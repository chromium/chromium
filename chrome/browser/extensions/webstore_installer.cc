// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_installer.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;
using content::NavigationController;
using download::DownloadItem;
using download::DownloadUrlParameters;

namespace {

// Key used to attach the Approval to the DownloadItem.
const char kApprovalKey[] = "extensions.webstore_installer";

const char kInvalidIdError[] = "Invalid id";
const char kDownloadDirectoryError[] = "Could not create download directory";
const char kDownloadCanceledError[] = "Download canceled";
const char kDownloadInterruptedError[] = "Download interrupted";
const char kInvalidDownloadError[] =
    "Download was not a valid extension or user script";
const char kDependencyNotFoundError[] = "Dependency not found";
const char kDependencyNotSharedModuleError[] =
    "Dependency is not shared module";
const char kInlineInstallSource[] = "inline";
const char kDefaultInstallSource[] = "ondemand";
const char kAppLauncherInstallSource[] = "applauncher";

// TODO(rockot): Share this duplicated constant with the extension updater.
// See http://crbug.com/371398.
const char kAuthUserQueryKey[] = "authuser";

constexpr base::TimeDelta kTimeRemainingThreshold = base::Seconds(1);

// Folder for downloading crx files from the webstore. This is used so that the
// crx files don't go via the usual downloads folder.
const base::FilePath::CharType kWebstoreDownloadFolder[] =
    FILE_PATH_LITERAL("Webstore Downloads");

base::FilePath* g_download_directory_for_tests = nullptr;

base::FilePath GetDownloadFilePath(const base::FilePath& download_directory,
                                   const extensions::ExtensionId& id) {
  // Ensure the download directory exists. TODO(asargent) - make this use
  // common code from the downloads system.
  if (!base::DirectoryExists(download_directory) &&
      !base::CreateDirectory(download_directory)) {
    return base::FilePath();
  }

  // This is to help avoid a race condition between when we generate this
  // filename and when the download starts writing to it (think concurrently
  // running sharded browser tests installing the same test file, for
  // instance).
  std::string random_number = base::NumberToString(
      base::RandGenerator(std::numeric_limits<uint16_t>::max()));

  base::FilePath file =
      download_directory.AppendASCII(id + "_" + random_number + ".crx");

  return base::GetUniquePath(file);
}

void MaybeAppendAuthUserParameter(const std::string& authuser, GURL* url) {
  if (authuser.empty())
    return;
  std::string old_query = url->query();
  url::Component query(0, old_query.length());
  url::Component key, value;
  // Ensure that the URL doesn't already specify an authuser parameter.
  while (url::ExtractQueryKeyValue(old_query, &query, &key, &value)) {
    std::string key_string = old_query.substr(key.begin, key.len);
    if (key_string == kAuthUserQueryKey) {
      return;
    }
  }
  if (!old_query.empty()) {
    old_query += "&";
  }
  std::string authuser_param = base::StringPrintf(
      "%s=%s",
      kAuthUserQueryKey,
      authuser.c_str());

  // TODO(rockot): Share this duplicated code with the extension updater.
  // See http://crbug.com/371398.
  std::string new_query_string = old_query + authuser_param;
  GURL::Replacements replacements;
  replacements.SetQueryStr(new_query_string);
  *url = url->ReplaceComponents(replacements);
}

std::string GetErrorMessageForDownloadInterrupt(
    download::DownloadInterruptReason reason) {
  switch (reason) {
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED:
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN:
      return l10n_util::GetStringUTF8(IDS_WEBSTORE_DOWNLOAD_ACCESS_DENIED);
    default:
      break;
  }
  return kDownloadInterruptedError;
}

}  // namespace

namespace extensions {

// static
GURL WebstoreInstaller::GetWebstoreInstallURL(
    const std::string& extension_id,
    InstallSource source) {
  std::string install_source;
  switch (source) {
    case INSTALL_SOURCE_INLINE:
      install_source = kInlineInstallSource;
      break;
    case INSTALL_SOURCE_APP_LAUNCHER:
      install_source = kAppLauncherInstallSource;
      break;
    case INSTALL_SOURCE_OTHER:
      install_source = kDefaultInstallSource;
  }

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(::switches::kAppsGalleryDownloadURL)) {
    std::string download_url =
        cmd_line->GetSwitchValueASCII(::switches::kAppsGalleryDownloadURL);
    return GURL(base::StringPrintfNonConstexpr(download_url.c_str(),
                                               extension_id.c_str()));
  }
  std::vector<std::string_view> params;
  std::string extension_param = "id=" + extension_id;
  std::string installsource_param = "installsource=" + install_source;
  params.push_back(extension_param);
  if (!install_source.empty())
    params.push_back(installsource_param);
  params.push_back("uc");
  std::string url_string = extension_urls::GetWebstoreUpdateUrl().spec();

  GURL url(
      url_string + "?response=redirect&" +
      update_client::UpdateQueryParams::Get(
          update_client::UpdateQueryParams::CRX) +
      "&x=" + base::EscapeQueryParamValue(base::JoinString(params, "&"), true));
  DCHECK(url.is_valid());

  return url;
}

WebstoreInstaller::Approval::Approval() = default;

std::unique_ptr<WebstoreInstaller::Approval>
WebstoreInstaller::Approval::CreateWithInstallPrompt(Profile* profile) {
  std::unique_ptr<Approval> result(new Approval());
  result->profile = profile;
  return result;
}

std::unique_ptr<WebstoreInstaller::Approval>
WebstoreInstaller::Approval::CreateForSharedModule(Profile* profile) {
  std::unique_ptr<Approval> result(new Approval());
  result->profile = profile;
  result->skip_install_dialog = true;
  result->skip_post_install_ui = true;
  result->manifest_check_level = MANIFEST_CHECK_LEVEL_NONE;
  return result;
}

std::unique_ptr<WebstoreInstaller::Approval>
WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
    Profile* profile,
    const extensions::ExtensionId& extension_id,
    base::Value::Dict parsed_manifest,
    bool strict_manifest_check) {
  std::unique_ptr<Approval> result(new Approval());
  result->extension_id = extension_id;
  result->profile = profile;
  result->manifest =
      std::make_unique<Manifest>(mojom::ManifestLocation::kInvalidLocation,
                                 std::move(parsed_manifest), extension_id);
  result->skip_install_dialog = true;
  result->manifest_check_level = strict_manifest_check ?
    MANIFEST_CHECK_LEVEL_STRICT : MANIFEST_CHECK_LEVEL_LOOSE;
  return result;
}

WebstoreInstaller::Approval::~Approval() {}

const WebstoreInstaller::Approval* WebstoreInstaller::GetAssociatedApproval(
    const DownloadItem& download) {
  return static_cast<const Approval*>(download.GetUserData(kApprovalKey));
}

WebstoreInstaller::WebstoreInstaller(Profile* profile,
                                     SuccessCallback success_callback,
                                     FailureCallback failure_callback,
                                     content::WebContents* web_contents,
                                     const extensions::ExtensionId& id,
                                     std::unique_ptr<Approval> approval,
                                     InstallSource source)
    : web_contents_(web_contents->GetWeakPtr()),
      profile_(profile),
      success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)),
      id_(id),
      install_source_(source),
      approval_(approval.release()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents);
  CHECK(success_callback_);
  CHECK(failure_callback_);

  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile));
}

void WebstoreInstaller::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AddRef();  // Balanced in ReportSuccess and ReportFailure.

  if (!crx_file::id_util::IdIsValid(id_)) {
    ReportFailure(kInvalidIdError, FAILURE_REASON_OTHER);
    return;
  }

  ExtensionService* extension_service =
    ExtensionSystem::Get(profile_)->extension_service();
  if (approval_.get() && approval_->dummy_extension.get()) {
    extension_service->shared_module_service()->CheckImports(
        approval_->dummy_extension.get(), &pending_modules_, &pending_modules_);
    // Do not check the return value of CheckImports, the CRX installer
    // will report appropriate error messages and fail to install if there
    // is an import error.
  }

  // Add the extension main module into the list.
  SharedModuleInfo::ImportInfo info;
  info.extension_id = id_;
  pending_modules_.push_back(info);

  total_modules_ = pending_modules_.size();

  std::set<extensions::ExtensionId> ids;
  std::list<SharedModuleInfo::ImportInfo>::const_iterator i;
  for (i = pending_modules_.begin(); i != pending_modules_.end(); ++i) {
    ids.insert(i->extension_id);
  }
  InstallVerifier::Get(profile_)->AddProvisional(ids);

  const std::string* name =
      approval_->manifest->available_values().FindString(manifest_keys::kName);
  if (!name) {
    NOTREACHED_IN_MIGRATION();
  }
  extensions::InstallTracker* tracker =
      extensions::InstallTrackerFactory::GetForBrowserContext(profile_);
  extensions::InstallObserver::ExtensionInstallParams params(
      id_, *name, approval_->installing_icon, approval_->manifest->is_app(),
      approval_->manifest->is_platform_app());
  tracker->OnBeginExtensionInstall(params);

  tracker->OnBeginExtensionDownload(id_);

  // TODO(crbug.com/41064141): Query manifest of dependencies before
  // downloading & installing those dependencies.
  DownloadNextPendingModule();
}

void WebstoreInstaller::OnInstallerDone(
    const std::optional<CrxInstallError>& error) {
  if (!error) {
    return;
  }

  // TODO(rdevlin.cronin): Continue removing std::string errors and
  // replacing with std::u16string. See crbug.com/71980.
  const std::string utf8_error = base::UTF16ToUTF8(error->message());
  crx_installer_ = nullptr;
  // ReportFailure releases a reference to this object so it must be the
  // last operation in this method.
  ReportFailure(utf8_error, FAILURE_REASON_OTHER);
}

void WebstoreInstaller::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  CHECK(profile_->IsSameOrParent(Profile::FromBrowserContext(browser_context)));
  if (pending_modules_.empty())
    return;
  SharedModuleInfo::ImportInfo info = pending_modules_.front();
  if (extension->id() != info.extension_id)
    return;
  pending_modules_.pop_front();

  // Clean up local state from the current download.
  if (download_item_) {
    download_item_->RemoveObserver(this);
    download_item_->Remove();
    download_item_ = nullptr;
  }
  crx_installer_.reset();

  if (pending_modules_.empty()) {
    CHECK_EQ(extension->id(), id_);
    ReportSuccess();
  } else {
    const base::Version version_required(info.minimum_version);
    if (version_required.IsValid() &&
        extension->version().CompareTo(version_required) < 0) {
      // It should not happen, CrxInstaller will make sure the version is
      // equal or newer than version_required.
      ReportFailure(kDependencyNotFoundError,
                    FAILURE_REASON_DEPENDENCY_NOT_FOUND);
    } else if (!SharedModuleInfo::IsSharedModule(extension)) {
      // It should not happen, CrxInstaller will make sure it is a shared
      // module.
      ReportFailure(kDependencyNotSharedModuleError,
                    FAILURE_REASON_DEPENDENCY_NOT_SHARED_MODULE);
    } else {
      DownloadNextPendingModule();
    }
  }
}

void WebstoreInstaller::SetDownloadDirectoryForTests(
    base::FilePath* directory) {
  g_download_directory_for_tests = directory;
}

WebstoreInstaller::~WebstoreInstaller() {
  if (download_item_) {
    download_item_->RemoveObserver(this);
    download_item_ = nullptr;
  }
}

void WebstoreInstaller::OnDownloadStarted(
    const extensions::ExtensionId& extension_id,
    DownloadItem* item,
    download::DownloadInterruptReason interrupt_reason) {
  if (!item || interrupt_reason != download::DOWNLOAD_INTERRUPT_REASON_NONE) {
    if (item)
      item->Remove();
    ReportFailure(download::DownloadInterruptReasonToString(interrupt_reason),
                  FAILURE_REASON_OTHER);
    return;
  }

  bool found = false;
  for (const auto& module : pending_modules_) {
    if (extension_id == module.extension_id) {
      found = true;
      break;
    }
  }
  if (!found) {
    // If this extension is not pending, it means another installer has
    // installed this extension and triggered OnExtensionInstalled(). In this
    // case, either it was the main module and success has already been
    // reported, or it was a dependency and either failed (ie. wrong version) or
    // the next download was triggered. In any case, the only thing that needs
    // to be done is to stop this download.
    item->Remove();
    return;
  }

  DCHECK_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE, interrupt_reason);
  DCHECK(!pending_modules_.empty());
  download_item_ = item;
  download_item_->AddObserver(this);
  if (pending_modules_.size() > 1) {
    // We are downloading a shared module. We need create an approval for it.
    std::unique_ptr<Approval> approval =
        Approval::CreateForSharedModule(profile_);
    const SharedModuleInfo::ImportInfo& info = pending_modules_.front();
    approval->extension_id = info.extension_id;
    const base::Version version_required(info.minimum_version);

    if (version_required.IsValid()) {
      approval->minimum_version =
          std::make_unique<base::Version>(version_required);
    }
    download_item_->SetUserData(kApprovalKey, std::move(approval));
  } else {
    // It is for the main module of the extension. We should use the provided
    // |approval_|.
    if (approval_)
      download_item_->SetUserData(kApprovalKey, std::move(approval_));
  }

  if (!download_started_) {
    download_started_ = true;
  }
}

void WebstoreInstaller::OnDownloadUpdated(DownloadItem* download) {
  CHECK_EQ(download_item_, download);

  switch (download->GetState()) {
    case DownloadItem::CANCELLED:
      ReportFailure(kDownloadCanceledError, FAILURE_REASON_CANCELLED);
      break;
    case DownloadItem::INTERRUPTED:
      ReportFailure(
          GetErrorMessageForDownloadInterrupt(download->GetLastReason()),
          FAILURE_REASON_OTHER);
      break;
    case DownloadItem::COMPLETE:
      // Stop the progress timer if it's running.
      download_progress_timer_.Stop();

      // Only wait for other notifications if the download is really
      // an extension.
      if (!download_crx_util::IsExtensionDownload(*download)) {
        ReportFailure(kInvalidDownloadError, FAILURE_REASON_OTHER);
        return;
      }

      if (crx_installer_.get())
        return;  // DownloadItemImpl calls the observer twice, ignore it.

      StartCrxInstaller(*download);

      if (pending_modules_.size() == 1) {
        // The download is the last module - the extension main module.
        extensions::InstallTracker* tracker =
            extensions::InstallTrackerFactory::GetForBrowserContext(profile_);
        tracker->OnDownloadProgress(id_, 100);
      }
      break;
    case DownloadItem::IN_PROGRESS: {
      UpdateDownloadProgress();
      break;
    }
    default:
      // Continue listening if the download is not in one of the above states.
      break;
  }
}

void WebstoreInstaller::OnDownloadDestroyed(DownloadItem* download) {
  CHECK_EQ(download_item_, download);
  download_item_->RemoveObserver(this);
  download_item_ = nullptr;
}

void WebstoreInstaller::DownloadNextPendingModule() {
  CHECK(!pending_modules_.empty());
  if (pending_modules_.size() == 1) {
    DCHECK_EQ(id_, pending_modules_.front().extension_id);
    DownloadCrx(id_, install_source_);
  } else {
    DownloadCrx(pending_modules_.front().extension_id, INSTALL_SOURCE_OTHER);
  }
}

void WebstoreInstaller::DownloadCrx(const extensions::ExtensionId& extension_id,
                                    InstallSource source) {
  download_url_ = GetWebstoreInstallURL(extension_id, source);
  MaybeAppendAuthUserParameter(approval_->authuser, &download_url_);

  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath download_path = user_data_dir.Append(kWebstoreDownloadFolder);

  base::FilePath download_directory(g_download_directory_for_tests ?
      *g_download_directory_for_tests : download_path);

  GetExtensionFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetDownloadFilePath, download_directory, extension_id),
      base::BindOnce(&WebstoreInstaller::StartDownload, this, extension_id));
}

// http://crbug.com/165634
// http://crbug.com/126013
// The current working theory is that one of the many pointers dereferenced in
// here is occasionally deleted before all of its referers are nullified,
// probably in a callback race. After this comment is released, the crash
// reports should narrow down exactly which pointer it is.  Collapsing all the
// early-returns into a single branch makes it hard to see exactly which pointer
// it is.
void WebstoreInstaller::StartDownload(
    const extensions::ExtensionId& extension_id,
    const base::FilePath& file) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (file.empty()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }

  DownloadManager* download_manager = profile_->GetDownloadManager();
  if (!download_manager) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }

  if (!web_contents_) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!web_contents_->GetPrimaryMainFrame()->GetRenderViewHost()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!web_contents_->GetPrimaryMainFrame()
           ->GetRenderViewHost()
           ->GetProcess()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }

  content::NavigationController& controller = web_contents_->GetController();
  if (!controller.GetBrowserContext()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }

  // The download url for the given extension is contained in |download_url_|.
  // We will navigate the current tab to this url to start the download. The
  // download system will then pass the crx to the CrxInstaller.
  int render_process_host_id = web_contents_->GetPrimaryMainFrame()
                                   ->GetRenderViewHost()
                                   ->GetProcess()
                                   ->GetID();

  content::RenderFrameHost* render_frame_host =
      web_contents_->GetPrimaryMainFrame();
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("webstore_installer", R"(
        semantics {
          sender: "Webstore Installer"
          description: "Downloads an extension for installation."
          trigger:
            "User initiates a webstore extension installation flow, including "
            "installing from the webstore, inline installation from a site, "
            "re-installing a corrupted extension, and others."
          data:
            "The id of the extension to be installed and information about the "
            "user's installation, including version, language, distribution "
            "(Chrome vs Chromium), NaCl architecture, installation source (as "
            "an enum), and accepted crx formats."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature cannot be disabled. It is only activated if the user "
            "triggers an extension installation."
          chrome_policy {
            ExtensionInstallBlocklist {
              ExtensionInstallBlocklist: {
                entries: '*'
              }
            }
          }
        })");
  std::unique_ptr<DownloadUrlParameters> params(new DownloadUrlParameters(
      download_url_, render_process_host_id, render_frame_host->GetRoutingID(),
      traffic_annotation));
  params->set_file_path(file);
  if (controller.GetVisibleEntry()) {
    content::Referrer referrer = content::Referrer::SanitizeForRequest(
        download_url_,
        content::Referrer(controller.GetVisibleEntry()->GetURL(),
                          network::mojom::ReferrerPolicy::kDefault));
    params->set_referrer(referrer.url);
    params->set_referrer_policy(
        content::Referrer::ReferrerPolicyForUrlRequest(referrer.policy));
  }
  params->set_callback(base::BindOnce(&WebstoreInstaller::OnDownloadStarted,
                                      this, extension_id));
  params->set_download_source(download::DownloadSource::EXTENSION_INSTALLER);
  download_manager->DownloadUrl(std::move(params));
}

void WebstoreInstaller::UpdateDownloadProgress() {
  // If the download has gone away, or isn't in progress (in which case we can't
  // give a good progress estimate), stop any running timers and return.
  if (!download_item_ ||
      download_item_->GetState() != DownloadItem::IN_PROGRESS) {
    download_progress_timer_.Stop();
    return;
  }

  int percent = download_item_->PercentComplete();
  // Only report progress if percent is more than 0 or we have finished
  // downloading at least one of the pending modules.
  int finished_modules = total_modules_ - pending_modules_.size();
  if (finished_modules > 0 && percent < 0)
    percent = 0;
  if (percent >= 0) {
    percent = (percent + (finished_modules * 100)) / total_modules_;
    extensions::InstallTracker* tracker =
        extensions::InstallTrackerFactory::GetForBrowserContext(profile_);
    tracker->OnDownloadProgress(id_, percent);
  }

  // If there's enough time remaining on the download to warrant an update,
  // set the timer (overwriting any current timers). Otherwise, stop the
  // timer.
  base::TimeDelta time_remaining;
  if (download_item_->TimeRemaining(&time_remaining) &&
      time_remaining > kTimeRemainingThreshold) {
    download_progress_timer_.Start(FROM_HERE, kTimeRemainingThreshold, this,
                                   &WebstoreInstaller::UpdateDownloadProgress);
  } else {
    download_progress_timer_.Stop();
  }
}

void WebstoreInstaller::StartCrxInstaller(const DownloadItem& download) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!crx_installer_.get());

  ExtensionService* service = ExtensionSystem::Get(profile_)->
      extension_service();
  CHECK(service);

  const Approval* approval = GetAssociatedApproval(download);
  DCHECK(approval);

  crx_installer_ = download_crx_util::CreateCrxInstaller(profile_, download);

  crx_installer_->set_expected_id(approval->extension_id);
  crx_installer_->set_is_gallery_install(true);
  crx_installer_->set_allow_silent_install(true);
  crx_installer_->AddInstallerCallback(base::BindOnce(
      &WebstoreInstaller::OnInstallerDone, weak_ptr_factory_.GetWeakPtr()));
  if (approval->withhold_permissions)
    crx_installer_->set_withhold_permissions();

  crx_installer_->InstallCrx(download.GetFullPath());
}

void WebstoreInstaller::ReportFailure(const std::string& error,
                                      FailureReason reason) {
  CHECK(failure_callback_);
  std::move(failure_callback_).Run(id_, error, reason);
  success_callback_ = base::NullCallback();
  extension_registry_observation_.Reset();

  extensions::InstallTracker* tracker =
      extensions::InstallTrackerFactory::GetForBrowserContext(profile_);
  tracker->OnInstallFailure(id_);

  Release();  // Balanced in Start().
}

void WebstoreInstaller::ReportSuccess() {
  CHECK(success_callback_);
  std::move(success_callback_).Run(id_);
  failure_callback_ = base::NullCallback();
  extension_registry_observation_.Reset();

  Release();  // Balanced in Start().
}

}  // namespace extensions
