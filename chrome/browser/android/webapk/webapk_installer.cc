// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_installer.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/android/webapk/webapk_install_service.h"
#include "chrome/browser/android/webapk/webapk_metrics.h"
#include "chrome/browser/android/webapk/webapk_ukm_recorder.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "components/webapk/webapk.pb.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_proto_builder.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "ui/android/color_utils_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/WebApkInstaller_jni.h"

namespace {

// The MIME type of the POST data sent to the server.
constexpr char kProtoMimeType[] = "application/x-protobuf";

// The default number of milliseconds to wait for the WebAPK download URL from
// the WebAPK server.
constexpr int kWebApkDownloadUrlTimeoutMs = 60000;

// Console message template for WebAPK installation failures.
constexpr char kWebApkFailureMessageTemplate[] =
    "Failed to install WebAPK for '%s'";

class CacheClearer : public content::BrowsingDataRemover::Observer {
 public:
  CacheClearer(const CacheClearer&) = delete;
  CacheClearer& operator=(const CacheClearer&) = delete;

  ~CacheClearer() override { remover_->RemoveObserver(this); }

  // Clear Chrome's cache. Run |callback| once clearing the cache is complete.
  static void FreeCacheAsync(content::BrowsingDataRemover* remover,
                             base::OnceClosure callback) {
    // CacheClearer manages its own lifetime and deletes itself when finished.
    auto* cache_clearer = new CacheClearer(remover, std::move(callback));
    remover->AddObserver(cache_clearer);
    remover->RemoveAndReply(base::Time(), base::Time::Max(),
                            content::BrowsingDataRemover::DATA_TYPE_CACHE,
                            chrome_browsing_data_remover::ALL_ORIGIN_TYPES,
                            cache_clearer);
  }

 private:
  CacheClearer(content::BrowsingDataRemover* remover,
               base::OnceClosure callback)
      : remover_(remover), install_callback_(std::move(callback)) {}

  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    std::move(install_callback_).Run();
    delete this;  // Matches the new in FreeCacheAsync()
  }

  raw_ptr<content::BrowsingDataRemover> remover_;

  base::OnceClosure install_callback_;
};

// Reads |file| and returns contents. Must be called on a background thread.
std::unique_ptr<std::string> ReadFileInBackground(const base::FilePath& file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  std::unique_ptr<std::string> update_request = std::make_unique<std::string>();
  base::ReadFileToString(file, update_request.get());
  return update_request;
}

// Returns task runner for running background tasks.
scoped_refptr<base::TaskRunner> GetBackgroundTaskRunner() {
  return base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

}  // anonymous namespace

WebApkInstaller::~WebApkInstaller() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebApkInstaller_destroy(env, java_ref_);
  java_ref_.Reset();
}

// static
void WebApkInstaller::InstallAsync(content::BrowserContext* context,
                                   content::WebContents* web_contents,
                                   const webapps::ShortcutInfo& shortcut_info,
                                   const SkBitmap& primary_icon,
                                   webapps::WebappInstallSource install_source,
                                   FinishCallback finish_callback) {
  // The installer will delete itself when it is done.
  WebApkInstaller* installer = new WebApkInstaller(context);
  installer->InstallAsync(web_contents, shortcut_info, primary_icon,
                          install_source, std::move(finish_callback));
}

// static
void WebApkInstaller::UpdateAsync(content::BrowserContext* context,
                                  const base::FilePath& update_request_path,
                                  FinishCallback finish_callback) {
  // The installer will delete itself when it is done.
  WebApkInstaller* installer = new WebApkInstaller(context);
  installer->UpdateAsync(update_request_path, std::move(finish_callback));
}

// static
void WebApkInstaller::InstallAsyncForTesting(
    WebApkInstaller* installer,
    content::WebContents* web_contents,
    const webapps::ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    webapps::WebappInstallSource install_source,
    FinishCallback callback) {
  installer->InstallAsync(web_contents, shortcut_info, primary_icon,
                          install_source, std::move(callback));
}

// static
void WebApkInstaller::UpdateAsyncForTesting(
    WebApkInstaller* installer,
    const base::FilePath& update_request_path,
    FinishCallback finish_callback) {
  installer->UpdateAsync(update_request_path, std::move(finish_callback));
}

void WebApkInstaller::SetTimeoutMs(int timeout_ms) {
  webapk_server_timeout_ms_ = timeout_ms;
}

void WebApkInstaller::OnInstallFinished(JNIEnv* env, jint result) {
  OnResult(static_cast<webapps::WebApkInstallResult>(result));
}

// static
void WebApkInstaller::StoreUpdateRequestToFile(
    const base::FilePath& update_request_path,
    const webapps::ShortcutInfo& shortcut_info,
    const GURL& app_key,
    std::unique_ptr<webapps::WebappIcon> primary_icon,
    std::unique_ptr<webapps::WebappIcon> splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<GURL, std::unique_ptr<webapps::WebappIcon>> icons,
    bool is_manifest_stale,
    bool is_app_identity_update_supported,
    std::vector<webapps::WebApkUpdateReason> update_reasons,
    base::OnceCallback<void(bool)> callback) {
  GetBackgroundTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&webapps::StoreUpdateRequestToFileInBackground,
                     update_request_path, shortcut_info, app_key,
                     std::move(primary_icon), std::move(splash_icon),
                     package_name, version, std::move(icons), is_manifest_stale,
                     is_app_identity_update_supported,
                     std::move(update_reasons)),
      std::move(callback));
}

void WebApkInstaller::InstallOrUpdateWebApk(const std::string& package_name,
                                            const std::string& token) {
  webapk_package_ = package_name;

  JNIEnv* env = base::android::AttachCurrentThread();

  if (task_type_ == WebApkInstaller::INSTALL) {
    webapk::TrackRequestTokenDuration(install_duration_timer_->Elapsed(),
                                      package_name);
    Java_WebApkInstaller_installWebApkAsync(
        env, java_ref_, webapk_package_, webapk_version_, short_name_, token,
        webapps::ShortcutInfo::SOURCE_ADD_TO_HOMESCREEN_PWA);
  } else {
    Java_WebApkInstaller_updateAsync(env, java_ref_, webapk_package_,
                                     webapk_version_, short_name_, token);
  }
}

void WebApkInstaller::OnResult(webapps::WebApkInstallResult result) {
  weak_ptr_factory_.InvalidateWeakPtrs();

  std::move(finish_callback_).Run(result, relax_updates_, webapk_package_);

  if (task_type_ == WebApkInstaller::INSTALL) {
    if (result == webapps::WebApkInstallResult::SUCCESS) {
      webapk::TrackInstallDuration(install_duration_timer_->Elapsed());
      webapk::TrackInstallEvent(webapk::INSTALL_COMPLETED);
      webapk::WebApkUkmRecorder::RecordInstall(manifest_id_, install_source_,
                                               install_shortcut_info_->display);
    } else {
      DVLOG(1) << "The WebAPK installation failed.";
      webapk::TrackInstallEvent(webapk::INSTALL_FAILED);
      if (web_contents_ && !web_contents_->IsBeingDestroyed()) {
        web_contents_->GetPrimaryMainFrame()->AddMessageToConsole(
            blink::mojom::ConsoleMessageLevel::kError,
            base::StringPrintf(kWebApkFailureMessageTemplate,
                               manifest_id_.spec().c_str()));
      }
    }
    webapk::TrackInstallResult(result);
  }

  delete this;
}

WebApkInstaller::WebApkInstaller(content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      webapk_server_timeout_ms_(kWebApkDownloadUrlTimeoutMs),
      webapk_version_(0),
      relax_updates_(false),
      task_type_(UNDEFINED) {
  CreateJavaRef();
  server_url_ = GetServerUrl();
}

void WebApkInstaller::CreateJavaRef() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(
      Java_WebApkInstaller_create(env, reinterpret_cast<intptr_t>(this)));
}

void WebApkInstaller::InstallAsync(content::WebContents* web_contents,
                                   const webapps::ShortcutInfo& shortcut_info,
                                   const SkBitmap& primary_icon,
                                   webapps::WebappInstallSource install_source,
                                   FinishCallback finish_callback) {
  install_duration_timer_ = std::make_unique<base::ElapsedTimer>();

  web_contents_ = web_contents->GetWeakPtr();
  install_shortcut_info_ =
      std::make_unique<webapps::ShortcutInfo>(shortcut_info);
  install_primary_icon_ = primary_icon;
  short_name_ = shortcut_info.short_name;
  finish_callback_ = std::move(finish_callback);
  manifest_id_ = install_shortcut_info_->manifest_id;
  install_source_ = install_source;
  task_type_ = INSTALL;

  if (!server_url_.is_valid()) {
    OnResult(webapps::WebApkInstallResult::SERVER_URL_INVALID);
    return;
  }

  CheckFreeSpace();
}

void WebApkInstaller::CheckFreeSpace() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebApkInstaller_checkFreeSpace(env, java_ref_);
}

void WebApkInstaller::OnGotSpaceStatus(JNIEnv* env, jint status) {
  SpaceStatus space_status = static_cast<SpaceStatus>(status);
  if (space_status == SpaceStatus::NOT_ENOUGH_SPACE) {
    OnResult(webapps::WebApkInstallResult::NOT_ENOUGH_SPACE);
    return;
  }

  if (space_status == SpaceStatus::ENOUGH_SPACE_AFTER_FREE_UP_CACHE) {
    CacheClearer::FreeCacheAsync(
        browser_context_->GetBrowsingDataRemover(),
        base::BindOnce(&WebApkInstaller::OnHaveSufficientSpaceForInstall,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnHaveSufficientSpaceForInstall();
  }
}

void WebApkInstaller::UpdateAsync(const base::FilePath& update_request_path,
                                  FinishCallback finish_callback) {
  finish_callback_ = std::move(finish_callback);
  task_type_ = UPDATE;

  if (!server_url_.is_valid()) {
    OnResult(webapps::WebApkInstallResult::SERVER_URL_INVALID);
    return;
  }

  GetBackgroundTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadFileInBackground, update_request_path),
      base::BindOnce(&WebApkInstaller::OnReadUpdateRequest,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstaller::OnReadUpdateRequest(
    std::unique_ptr<std::string> update_request) {
  std::unique_ptr<webapk::WebApk> proto(new webapk::WebApk);
  if (update_request->empty() || !proto->ParseFromString(*update_request)) {
    OnResult(webapps::WebApkInstallResult::REQUEST_INVALID);
    return;
  }

  webapk_package_ = proto->package_name();
  short_name_ = base::UTF8ToUTF16(proto->manifest().short_name());
  const net::NetworkTrafficAnnotationTag traffic_annotation_update_request =
      net::DefineNetworkTrafficAnnotation("webapk_update", R"(
        semantics {
          sender: "WebAPK"
          description:
            "When a user launches a previously-installed WebAPK (web app which "
            "was installed through Chrome), Chrome may check if the launched "
            "app is out of date. If an update is required, Chrome may send a "
            "background request to a Google server asking for a new version of "
            "that WebAPK. Both of these actions can be skipped if they have "
            "already been performed recently. In order for the server to "
            "create a new, updated WebAPK for the user, it first needs to know "
            "metadata about the web app that the user wants to update, as well "
            "as some details about the user's device. Upon successful creation "
            "of the new version of the WebAPK, the server will return a URL "
            "from which the updated WebAPK can be downloaded along with a few "
            "other details about the WebAPK (its size, version, and hash, for "
            "example)."
          trigger: "User launches a WebAPK, and Chrome determines that the "
                   "WebAPK is out of date."
          data:
            "The 'WebApk' message in components/webapk/webapk.proto lists the "
            "full contents of a WebAPK request. The proto includes:\n"
            "  * the Android package name and version of the "
            "currently-installed WebAPK that needs to be updated\n"
            "  * the reason(s) that the WebAPK needs to be updated (usually in "
            "the form of an attribute or attributes of the WebAPK that are out "
            "of date)\n"
            "  * the URL of the web app's Web Application Manifest (see "
            "https://www.w3.org/TR/appmanifest/ for details)\n"
            "  * the parsed contents of the web app's Web Application Manifest "
            "(includes things like the app's name and description, URLs to "
            "icons, and other app features)\n"
            "  * the Android package name and version string of the browser "
            "from which the user made the request\n"
            "  * the ABI and Android OS version of the device that made the "
            "request"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          policy_exception_justification: "Not implemented."
          setting: "This feature cannot be disabled."
        }
        )");

  SendRequest(traffic_annotation_update_request, *update_request);
}

void WebApkInstaller::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  timer_.Stop();

  int response_code = -1;
  if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers)
    response_code = loader_->ResponseInfo()->headers->response_code();

  if (!response_body || response_code != net::HTTP_OK) {
    LOG(WARNING) << base::StringPrintf(
        "WebAPK server returned response code %d.", response_code);
    OnResult(webapps::WebApkInstallResult::SERVER_ERROR);
    return;
  }

  std::unique_ptr<webapk::WebApkResponse> response(new webapk::WebApkResponse);
  if (!response_body || !response->ParseFromString(*response_body)) {
    LOG(WARNING) << "WebAPK server did not return proto.";
    OnResult(webapps::WebApkInstallResult::SERVER_ERROR);
    return;
  }

  base::StringToInt(response->version(), &webapk_version_);
  const std::string& token = response->token();
  if (task_type_ == UPDATE && token.empty()) {
    // https://crbug.com/680131. The server sends an empty URL if the server
    // does not have a newer WebAPK to update to.
    relax_updates_ = response->relax_updates();
    OnResult(webapps::WebApkInstallResult::SUCCESS);
    return;
  }

  if (token.empty() || response->package_name().empty()) {
    LOG(WARNING) << "WebAPK server returned incomplete proto.";
    OnResult(webapps::WebApkInstallResult::SERVER_ERROR);
    return;
  }

  InstallOrUpdateWebApk(response->package_name(), token);
}

network::SharedURLLoaderFactory* GetURLLoaderFactory(
    content::BrowserContext* browser_context) {
  return browser_context->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess()
      .get();
}

void WebApkInstaller::OnHaveSufficientSpaceForInstall() {
  // We need to take the hash of the bitmap at the icon URL prior to any
  // transformations being applied to the bitmap (such as encoding/decoding
  // the bitmap). The icon hash is used to determine whether an icon update is
  // needed
  //
  // We redownload the icon in order to take the Murmur2 hash. The redownload
  // should be fast because the icon should be in the HTTP cache.

  icon_hasher_ = std::make_unique<webapps::WebApkIconsHasher>();
  icon_hasher_->DownloadAndComputeMurmur2Hash(
      GetURLLoaderFactory(browser_context_), web_contents_,
      url::Origin::Create(install_shortcut_info_->url), *install_shortcut_info_,
      install_primary_icon_,
      base::BindOnce(&WebApkInstaller::OnGotIconMurmur2Hashes,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstaller::OnGotIconMurmur2Hashes(
    std::map<GURL, std::unique_ptr<webapps::WebappIcon>> icons) {
  if (icons.empty()) {
    OnResult(webapps::WebApkInstallResult::ICON_HASHER_ERROR);
    return;
  }

  DCHECK(install_shortcut_info_);

  // New WebAPK installs uses icon data from |icons|. |primary_icon| and
  // |splash_icon| are for updates only.
  webapps::BuildProto(*install_shortcut_info_,
                      install_shortcut_info_->manifest_id,
                      nullptr /* primary_icon */, nullptr /* splash_icon */,
                      "" /* package_name */, "" /* version */, std::move(icons),
                      false /* is_manifest_stale */,
                      false /* is_app_identity_update_supported */,
                      base::BindOnce(&WebApkInstaller::OnInstallProtoBuilt,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void WebApkInstaller::OnInstallProtoBuilt(
    std::unique_ptr<std::string> serialized_proto) {
  net::NetworkTrafficAnnotationTag traffic_annotation_install_from_chrome =
      net::DefineNetworkTrafficAnnotation("webapk_create", R"(
        semantics {
          sender: "WebAPK"
          description:
            "At the user's request, Chrome can install supported web apps on "
            "Android so that they show up in the user's app drawer and "
            "optionally home screen. Web apps installed in this way are called "
            "WebAPKs. See "
            "https://developers.google.com/web/fundamentals/integration/webapks "
            "for more details. WebAPKs are created on a Google server on "
            "behalf of the Chrome client and the user. In order for the server "
            "to create a WebAPK, it first needs to know metadata about the web "
            "app that the user wants to install, as well as some details about "
            "the user's device. Upon successful WebAPK creation, the server "
            "will return a URL from which the WebAPK can be downloaded along "
            "with a few other details about the WebAPK (its size, version, and "
            "hash, for example)."
          trigger: "User selects 'Add to home screen' or 'Install' items in "
                   "Chrome's app menu or an install prompt within Chrome."
          data:
            "The 'WebApk' message in components/webapk/webapk.proto lists the "
            "full contents of a WebAPK request. Note that 'package_name', "
            "'version', and 'update_reasons' will not be filled in for initial "
            "app installation requests, but only for future app updates. The "
            "proto includes:\n"
            "  * the URL of the web app's Web Application Manifest (see "
            "https://www.w3.org/TR/appmanifest/ for details)\n"
            "  * the parsed contents of the web app's Web Application Manifest "
            "(includes things like the app's name and description, URLs to "
            "icons, and other app features)\n"
            "  * the Android package name and version string of the browser "
            "from which the user made the request\n"
            "  * the ABI and Android OS version of the device that made the "
            "request"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          policy_exception_justification: "Not implemented."
          setting: "This feature cannot be disabled."
        }
        )");

  SendRequest(traffic_annotation_install_from_chrome, *serialized_proto);
}

void WebApkInstaller::SendRequest(
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& serialized_proto) {
  DCHECK(server_url_.is_valid());

  timer_.Start(
      FROM_HERE, base::Milliseconds(webapk_server_timeout_ms_),
      base::BindOnce(&WebApkInstaller::OnResult, weak_ptr_factory_.GetWeakPtr(),
                     webapps::WebApkInstallResult::REQUEST_TIMEOUT));

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = server_url_;
  request->method = "POST";
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  loader_->AttachStringForUpload(serialized_proto, kProtoMimeType);
  loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      GetURLLoaderFactory(browser_context_),
      base::BindOnce(&WebApkInstaller::OnURLLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

GURL WebApkInstaller::GetServerUrl() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  GURL command_line_url(
      command_line->GetSwitchValueASCII(switches::kWebApkServerUrl));

  if (command_line_url.is_valid())
    return command_line_url;

  JNIEnv* env = base::android::AttachCurrentThread();
  return GURL(Java_WebApkInstaller_getWebApkServerUrl(env, java_ref_));
}
