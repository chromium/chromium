// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/crx_installer.h"

#include <map>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/blocklist_check.h"
#include "chrome/browser/extensions/convert_user_script.h"
#include "chrome/browser/extensions/extension_assets_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/crx_verifier.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/install_stage.h"
#include "extensions/browser/policy_check.h"
#include "extensions/browser/preload_check_group.h"
#include "extensions/browser/requirements_checker.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/file_util.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "extensions/common/verifier_formats.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user_manager.h"
#endif

using content::BrowserThread;

namespace extensions {

// static
scoped_refptr<CrxInstaller> CrxInstaller::CreateSilent(
    ExtensionService* frontend) {
  return new CrxInstaller(frontend->AsExtensionServiceWeakPtr(),
                          std::unique_ptr<ExtensionInstallPrompt>(), nullptr);
}

// static
scoped_refptr<CrxInstaller> CrxInstaller::Create(
    ExtensionService* frontend,
    std::unique_ptr<ExtensionInstallPrompt> client) {
  return new CrxInstaller(frontend->AsExtensionServiceWeakPtr(),
                          std::move(client), nullptr);
}

// static
scoped_refptr<CrxInstaller> CrxInstaller::Create(
    ExtensionService* service,
    std::unique_ptr<ExtensionInstallPrompt> client,
    const WebstoreInstaller::Approval* approval) {
  return new CrxInstaller(service->AsExtensionServiceWeakPtr(),
                          std::move(client), approval);
}

CrxInstaller::CrxInstaller(base::WeakPtr<ExtensionService> service_weak,
                           std::unique_ptr<ExtensionInstallPrompt> client,
                           const WebstoreInstaller::Approval* approval)
    : profile_(service_weak->profile()),
      install_directory_(service_weak->install_directory()),
      install_source_(mojom::ManifestLocation::kInternal),
      approved_(false),
      expected_manifest_check_level_(
          WebstoreInstaller::MANIFEST_CHECK_LEVEL_STRICT),
      fail_install_if_unexpected_version_(false),
      extensions_enabled_(service_weak->extensions_enabled()),
      delete_source_(false),
      service_weak_(service_weak),
      // See header file comment on |client_| for why we use a raw pointer here.
      client_(client.release()),
      apps_require_extension_mime_type_(false),
      allow_silent_install_(false),
      grant_permissions_(true),
      install_cause_(extension_misc::INSTALL_CAUSE_UNSET),
      creation_flags_(Extension::NO_FLAGS),
      off_store_install_allow_reason_(OffStoreInstallDisallowed),
      did_handle_successfully_(true),
      error_on_unsupported_requirements_(false),
      shared_file_task_runner_(GetExtensionFileTaskRunner()),
      update_from_settings_page_(false),
      install_flags_(kInstallFlagNone) {
  profile_observation_.Observe(profile_);

  if (!approval)
    return;

  CHECK(profile()->IsSameOrParent(approval->profile));
  if (client_) {
    client_->install_ui()->SetUseAppInstalledBubble(
        approval->use_app_installed_bubble);
    client_->install_ui()->SetSkipPostInstallUI(approval->skip_post_install_ui);
  }

  if (approval->skip_install_dialog) {
    // Mark the extension as approved, but save the expected manifest and ID
    // so we can check that they match the CRX's.
    approved_ = true;
    expected_manifest_check_level_ = approval->manifest_check_level;
    if (expected_manifest_check_level_ !=
        WebstoreInstaller::MANIFEST_CHECK_LEVEL_NONE) {
      expected_manifest_ = std::make_unique<base::Value::Dict>(
          approval->manifest->value()->Clone());
    }
    expected_id_ = approval->extension_id;
  }
  if (approval->minimum_version.get())
    minimum_version_ = base::Version(*approval->minimum_version);

  if (approval->bypassed_safebrowsing_friction)
    install_flags_ = kInstallFlagBypassedSafeBrowsingFriction;

  show_dialog_callback_ = approval->show_dialog_callback;
}

CrxInstaller::~CrxInstaller() {
  DCHECK(!service_weak_ || service_weak_->browser_terminating() ||
         installer_callbacks_.empty());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Ensure |client_| and |install_checker_| data members are destroyed on the
  // UI thread. The |client_| dialog has a weak reference as |this| is its
  // delegate, and |install_checker_| owns WeakPtrs, so must be destroyed on the
  // same thread that created it.
}

void CrxInstaller::InstallCrx(const base::FilePath& source_file) {
  crx_file::VerifierFormat format =
      off_store_install_allow_reason_ == OffStoreInstallDisallowed
          ? GetWebstoreVerifierFormat(
                base::CommandLine::ForCurrentProcess()->HasSwitch(
                    ::switches::kAppsGalleryURL))
          : GetExternalVerifierFormat();
  InstallCrxFile(CRXFileInfo(source_file, format));
}

void CrxInstaller::InstallCrxFile(const CRXFileInfo& source_file) {
  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  NotifyCrxInstallBegin();

  source_file_ = source_file.path;

  auto unpacker = base::MakeRefCounted<SandboxedUnpacker>(
      install_source_, creation_flags_, install_directory_,
      GetUnpackerTaskRunner(), this);

  if (!GetUnpackerTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(&SandboxedUnpacker::StartWithCrx, unpacker,
                                    source_file))) {
    NOTREACHED_IN_MIGRATION();
  }
}

void CrxInstaller::InstallUnpackedCrx(const ExtensionId& extension_id,
                                      const std::string& public_key,
                                      const base::FilePath& unpacked_dir) {
  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  NotifyCrxInstallBegin();

  source_file_ = unpacked_dir;

  auto unpacker = base::MakeRefCounted<SandboxedUnpacker>(
      install_source_, creation_flags_, install_directory_,
      GetUnpackerTaskRunner(), this);

  if (!GetUnpackerTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&SandboxedUnpacker::StartWithDirectory, unpacker,
                         extension_id, public_key, unpacked_dir))) {
    NOTREACHED_IN_MIGRATION();
  }
}

void CrxInstaller::InstallUserScript(const base::FilePath& source_file,
                                     const GURL& download_url) {
  DCHECK(!download_url.is_empty());

  NotifyCrxInstallBegin();

  source_file_ = source_file;
  download_url_ = download_url;

  if (!shared_file_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CrxInstaller::ConvertUserScriptOnSharedFileThread,
                         this)))
    NOTREACHED_IN_MIGRATION();
}

void CrxInstaller::ConvertUserScriptOnSharedFileThread() {
  std::u16string error;
  scoped_refptr<Extension> extension = ConvertUserScriptToExtension(
      source_file_, download_url_, install_directory_, &error);
  if (!extension.get()) {
    ReportFailureFromSharedFileThread(CrxInstallError(
        CrxInstallErrorType::OTHER,
        CrxInstallErrorDetail::CONVERT_USER_SCRIPT_TO_EXTENSION_FAILED, error));
    return;
  }

  OnUnpackSuccessOnSharedFileThread(extension->path(), extension->path(),
                                    nullptr, extension, SkBitmap(),
                                    /*ruleset_install_prefs=*/{});
}

void CrxInstaller::UpdateExtensionFromUnpackedCrx(
    const ExtensionId& extension_id,
    const std::string& public_key,
    const base::FilePath& unpacked_dir) {
  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  const Extension* extension = ExtensionRegistry::Get(service->profile())
                                   ->GetInstalledExtension(extension_id);
  if (!extension) {
    LOG(WARNING) << "Will not update extension " << extension_id
                 << " because it is not installed";
    if (delete_source_)
      temp_dir_ = unpacked_dir;
    if (installer_callbacks_.empty()) {
      shared_file_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&CrxInstaller::CleanupTempFiles, this));
    } else {
      shared_file_task_runner_->PostTaskAndReply(
          FROM_HERE, base::BindOnce(&CrxInstaller::CleanupTempFiles, this),
          base::BindOnce(
              &CrxInstaller::RunInstallerCallbacks, this,
              CrxInstallError(
                  CrxInstallErrorType::OTHER,
                  CrxInstallErrorDetail::UPDATE_NON_EXISTING_EXTENSION)));
    }
    return;
  }

  expected_id_ = extension_id;
  install_source_ = extension->location();
  install_cause_ = extension_misc::INSTALL_CAUSE_UPDATE;
  InitializeCreationFlagsForUpdate(extension, Extension::NO_FLAGS);

  const ExtensionPrefs* extension_prefs =
      ExtensionPrefs::Get(service->GetBrowserContext());
  DCHECK(extension_prefs);
  set_do_not_sync(extension_prefs->DoNotSync(extension_id));

  InstallUnpackedCrx(extension_id, public_key, unpacked_dir);
}

std::optional<CrxInstallError> CrxInstaller::CheckExpectations(
    const Extension* extension) {
  DCHECK(shared_file_task_runner_->RunsTasksInCurrentSequence());

  // Make sure the expected ID matches if one was supplied or if we want to
  // bypass the prompt.
  if ((approved_ || !expected_id_.empty()) &&
      expected_id_ != extension->id()) {
    return CrxInstallError(
        CrxInstallErrorType::OTHER, CrxInstallErrorDetail::UNEXPECTED_ID,
        l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALL_UNEXPECTED_ID,
                                   base::ASCIIToUTF16(expected_id_),
                                   base::ASCIIToUTF16(extension->id())));
  }

  if (expected_version_.IsValid() && fail_install_if_unexpected_version_ &&
      expected_version_ != extension->version()) {
    return CrxInstallError(
        CrxInstallErrorType::OTHER, CrxInstallErrorDetail::MISMATCHED_VERSION,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_INSTALL_UNEXPECTED_VERSION,
            base::ASCIIToUTF16(expected_version_.GetString()),
            base::ASCIIToUTF16(extension->version().GetString())));
  }

  return std::nullopt;
}

std::optional<CrxInstallError> CrxInstaller::AllowInstall(
    const Extension* extension) {
  DCHECK(shared_file_task_runner_->RunsTasksInCurrentSequence());

  if (minimum_version_.IsValid() &&
      extension->version().CompareTo(minimum_version_) < 0) {
    return CrxInstallError(
        CrxInstallErrorType::OTHER, CrxInstallErrorDetail::UNEXPECTED_VERSION,
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_INSTALL_UNEXPECTED_VERSION,
            base::ASCIIToUTF16(minimum_version_.GetString() + "+"),
            base::ASCIIToUTF16(extension->version().GetString())));
  }

  // Make sure the manifests match if we want to bypass the prompt.
  if (approved_) {
    bool valid = false;
    if (expected_manifest_check_level_ ==
        WebstoreInstaller::MANIFEST_CHECK_LEVEL_NONE) {
      // To skip manifest checking, the extension must be a shared module
      // and not request any permissions.
      if (SharedModuleInfo::IsSharedModule(extension) &&
          extension->permissions_data()->active_permissions().IsEmpty()) {
        valid = true;
      }
    } else {
      valid = *expected_manifest_ == *original_manifest_;
      if (!valid && expected_manifest_check_level_ ==
          WebstoreInstaller::MANIFEST_CHECK_LEVEL_LOOSE) {
        std::string error;
        scoped_refptr<Extension> dummy_extension = Extension::Create(
            base::FilePath(), install_source_, *expected_manifest_,
            creation_flags_, extension->id(), &error);
        if (error.empty()) {
          valid = !(PermissionMessageProvider::Get()->IsPrivilegeIncrease(
              dummy_extension->permissions_data()->active_permissions(),
              extension->permissions_data()->active_permissions(),
              extension->GetType()));
        }
      }
    }

    if (!valid)
      return CrxInstallError(
          CrxInstallErrorType::OTHER, CrxInstallErrorDetail::MANIFEST_INVALID,
          l10n_util::GetStringUTF16(IDS_EXTENSION_MANIFEST_INVALID));
  }

  // The checks below are skipped for themes and external installs.
  // TODO(pamg): After ManagementPolicy refactoring is complete, remove this
  // and other uses of install_source_ that are no longer needed now that the
  // SandboxedUnpacker sets extension->location.
  if (extension->is_theme() || Manifest::IsExternalLocation(install_source_)) {
    return std::nullopt;
  }

  if (!extensions_enabled_) {
    return CrxInstallError(
        CrxInstallErrorType::DECLINED,
        CrxInstallErrorDetail::INSTALL_NOT_ENABLED,
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_NOT_ENABLED));
  }

  if (install_cause_ == extension_misc::INSTALL_CAUSE_USER_DOWNLOAD &&
      !is_gallery_install() &&
      off_store_install_allow_reason_ == OffStoreInstallDisallowed) {
    // Don't delete source in this case so that the user can install
    // manually if they want.
    delete_source_ = false;
    did_handle_successfully_ = false;

    return CrxInstallError(
        CrxInstallErrorType::OTHER,
        CrxInstallErrorDetail::OFFSTORE_INSTALL_DISALLOWED,
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_DISALLOWED_ON_SITE));
  }

  if (extension_->is_app()) {
    // If the app was downloaded, apps_require_extension_mime_type_
    // will be set.  In this case, check that it was served with the
    // right mime type.  Make an exception for file URLs, which come
    // from the users computer and have no headers.
    if (!download_url_.SchemeIsFile() &&
        apps_require_extension_mime_type_ &&
        original_mime_type_ != Extension::kMimeType) {
      return CrxInstallError(
          CrxInstallErrorType::OTHER,
          CrxInstallErrorDetail::INCORRECT_APP_CONTENT_TYPE,
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_INSTALL_INCORRECT_APP_CONTENT_TYPE,
              base::ASCIIToUTF16(Extension::kMimeType)));
    }

    // If the client_ is NULL, then the app is either being installed via
    // an internal mechanism like sync, external_extensions, or default apps.
    // In that case, we don't want to enforce things like the install origin.
    if (!is_gallery_install() && client_) {
      // For apps with a gallery update URL, require that they be installed
      // from the gallery.
      // TODO(erikkay) Apply this rule for paid extensions and themes as well.
      ExtensionManagement* extension_management =
          ExtensionManagementFactory::GetForBrowserContext(profile_);
      if (extension_management->UpdatesFromWebstore(*extension)) {
        return CrxInstallError(
            CrxInstallErrorType::OTHER,
            CrxInstallErrorDetail::NOT_INSTALLED_FROM_GALLERY,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_INSTALL_GALLERY_ONLY,
                l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE)));
      }

      // For self-hosted apps, verify that the entire extent is on the same
      // host (or a subdomain of the host) the download happened from.  There's
      // no way for us to verify that the app controls any other hosts.
      URLPattern pattern(UserScript::ValidUserScriptSchemes());
      pattern.SetHost(download_url_.host());
      pattern.SetMatchSubdomains(true);

      const URLPatternSet& patterns = extension_->web_extent();
      for (auto i = patterns.begin(); i != patterns.end(); ++i) {
        if (!pattern.MatchesHost(i->host())) {
          return CrxInstallError(
              CrxInstallErrorType::OTHER,
              CrxInstallErrorDetail::INCORRECT_INSTALL_HOST,
              l10n_util::GetStringUTF16(
                  IDS_EXTENSION_INSTALL_INCORRECT_INSTALL_HOST));
        }
      }
    }
  }

  return std::nullopt;
}

void CrxInstaller::ShouldComputeHashesOnUI(
    scoped_refptr<const Extension> extension,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  extensions::ContentVerifier* content_verifier =
      extensions::ExtensionSystem::Get(profile_)->content_verifier();
  bool result = content_verifier &&
                content_verifier->ShouldComputeHashesOnInstall(*extension);
  GetUnpackerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void CrxInstaller::GetContentVerifierKeyOnUI(
    base::OnceCallback<void(ContentVerifierKey)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ContentVerifierKey key = ExtensionSystem::Get(profile_)
                               ->content_verifier()
                               ->GetContentVerifierKey();
  // Normally content verifier key is an std::span, so only a reference to the
  // real key. Hence we have to make a copy before passing it to another thread.
  std::vector<uint8_t> key_copy(key.begin(), key.end());
  GetUnpackerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(key_copy)));
}

void CrxInstaller::GetContentVerifierKey(
    base::OnceCallback<void(ContentVerifierKey)> callback) {
  if (!content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&CrxInstaller::GetContentVerifierKeyOnUI,
                                    this, std::move(callback)))) {
    NOTREACHED_IN_MIGRATION();
  }
}

void CrxInstaller::ShouldComputeHashesForOffWebstoreExtension(
    scoped_refptr<const Extension> extension,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(GetUnpackerTaskRunner()->RunsTasksInCurrentSequence());
  if (!content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&CrxInstaller::ShouldComputeHashesOnUI, this,
                         std::move(extension), std::move(callback)))) {
    NOTREACHED_IN_MIGRATION();
  }
}

void CrxInstaller::OnUnpackFailure(const CrxInstallError& error) {
  DCHECK(GetUnpackerTaskRunner()->RunsTasksInCurrentSequence());
  if (!content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&CrxInstaller::ReportFailureFromUIThread,
                                    this, error))) {
    NOTREACHED_IN_MIGRATION();
  }
}

void CrxInstaller::OnUnpackSuccess(
    const base::FilePath& temp_dir,
    const base::FilePath& extension_dir,
    std::unique_ptr<base::Value::Dict> original_manifest,
    const Extension* extension,
    const SkBitmap& install_icon,
    base::Value::Dict ruleset_install_prefs) {
  DCHECK(GetUnpackerTaskRunner()->RunsTasksInCurrentSequence());
  shared_file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CrxInstaller::OnUnpackSuccessOnSharedFileThread, this,
                     temp_dir, extension_dir, std::move(original_manifest),
                     scoped_refptr<const Extension>(extension), install_icon,
                     std::move(ruleset_install_prefs)));
}

void CrxInstaller::OnUnpackSuccessOnSharedFileThread(
    base::FilePath temp_dir,
    base::FilePath extension_dir,
    std::unique_ptr<base::Value::Dict> original_manifest,
    scoped_refptr<const Extension> extension,
    SkBitmap install_icon,
    base::Value::Dict ruleset_install_prefs) {
  DCHECK(shared_file_task_runner_->RunsTasksInCurrentSequence());

  extension_ = extension;
  temp_dir_ = temp_dir;
  ruleset_install_prefs_ = std::move(ruleset_install_prefs);
  ReportInstallationStage(InstallationStage::kCheckingExpectations);

  if (!install_icon.empty())
    install_icon_ = std::make_unique<SkBitmap>(install_icon);

  original_manifest_ = std::move(original_manifest);

  // We don't have to delete the unpack dir explicity since it is a child of
  // the temp dir.
  unpacked_extension_root_ = extension_dir;

  // Check whether the crx matches the set expectations.
  std::optional<CrxInstallError> expectations_error =
      CheckExpectations(extension.get());
  if (expectations_error) {
    DCHECK_NE(CrxInstallErrorType::NONE, expectations_error->type());
    ReportFailureFromSharedFileThread(*expectations_error);
    return;
  }

  // The |expectations_error| could be non-null in case of version mismatch if
  // |fail_install_if_unexpected_version_| is set to false.
  // If |expectations_passed_callback_| is set, the installer owns the crx file,
  // and there is no version mismatch, invoke the callback and transfer the
  // ownership. The responsibility to delete the crx file now lies with the
  // callback.
  if (!expectations_verified_callback_.is_null() && delete_source_ &&
      (!expected_version_.IsValid() ||
       expected_version_ == extension->version())) {
    delete_source_ = false;
    if (!content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, std::move(expectations_verified_callback_))) {
      NOTREACHED_IN_MIGRATION();
    }
  }

  std::optional<CrxInstallError> error = AllowInstall(extension.get());
  if (error) {
    DCHECK_NE(CrxInstallErrorType::NONE, error->type());
    ReportFailureFromSharedFileThread(*error);
    return;
  }

  if (!content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&CrxInstaller::CheckInstall, this)))
    NOTREACHED_IN_MIGRATION();
}

void CrxInstaller::OnStageChanged(InstallationStage stage) {
  ReportInstallationStage(stage);
}

void CrxInstaller::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK_EQ(profile, profile_);
  profile_keep_alive_.reset();
  profile_observation_.Reset();
}

void CrxInstaller::CheckInstall() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  // TODO(crbug.com/40387578): Move this code to a utility class to avoid
  // duplication of SharedModuleService::CheckImports code.
  if (SharedModuleInfo::ImportsModules(extension())) {
    const std::vector<SharedModuleInfo::ImportInfo>& imports =
        SharedModuleInfo::GetImports(extension());
    ExtensionRegistry* registry = ExtensionRegistry::Get(service->profile());
    for (const auto& import : imports) {
      const Extension* imported_module = registry->GetExtensionById(
          import.extension_id, ExtensionRegistry::EVERYTHING);
      if (!imported_module)
        continue;

      if (!SharedModuleInfo::IsSharedModule(imported_module)) {
        ReportFailureFromUIThread(CrxInstallError(
            CrxInstallErrorType::DECLINED,
            CrxInstallErrorDetail::DEPENDENCY_NOT_SHARED_MODULE,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_INSTALL_DEPENDENCY_NOT_SHARED_MODULE,
                base::UTF8ToUTF16(imported_module->name()))));
        return;
      }
      base::Version version_required(import.minimum_version);
      if (version_required.IsValid() &&
          imported_module->version().CompareTo(version_required) < 0) {
        ReportFailureFromUIThread(CrxInstallError(
            CrxInstallErrorType::DECLINED,
            CrxInstallErrorDetail::DEPENDENCY_OLD_VERSION,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_INSTALL_DEPENDENCY_OLD_VERSION,
                base::UTF8ToUTF16(imported_module->name()),
                base::ASCIIToUTF16(import.minimum_version),
                base::ASCIIToUTF16(imported_module->version().GetString()))));
        return;
      }
      if (!SharedModuleInfo::IsExportAllowedByAllowlist(imported_module,
                                                        extension()->id())) {
        ReportFailureFromUIThread(CrxInstallError(
            CrxInstallErrorType::DECLINED,
            CrxInstallErrorDetail::DEPENDENCY_NOT_ALLOWLISTED,
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_INSTALL_DEPENDENCY_NOT_ALLOWLISTED,
                base::UTF8ToUTF16(extension()->name()),
                base::UTF8ToUTF16(imported_module->name()))));
        return;
      }
    }
  }

  // Run the policy, requirements and blocklist checks in parallel.
  check_group_ = std::make_unique<PreloadCheckGroup>();

  policy_check_ = std::make_unique<PolicyCheck>(profile_, extension());
  requirements_check_ = std::make_unique<RequirementsChecker>(extension());
  blocklist_check_ =
      std::make_unique<BlocklistCheck>(Blocklist::Get(profile_), extension_);

  check_group_->AddCheck(policy_check_.get());
  check_group_->AddCheck(requirements_check_.get());
  check_group_->AddCheck(blocklist_check_.get());

  check_group_->Start(
      base::BindOnce(&CrxInstaller::OnInstallChecksComplete, this));
}

void CrxInstaller::OnInstallChecksComplete(const PreloadCheck::Errors& errors) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_weak_)
    return;

  if (errors.empty()) {
    ConfirmInstall();
    return;
  }

  // Check for requirement errors.
  if (!requirements_check_->GetErrorMessage().empty()) {
    if (error_on_unsupported_requirements_) {
      ReportFailureFromUIThread(
          CrxInstallError(CrxInstallErrorType::DECLINED,
                          CrxInstallErrorDetail::UNSUPPORTED_REQUIREMENTS,
                          requirements_check_->GetErrorMessage()));
      return;
    }
    install_flags_ |= kInstallFlagHasRequirementErrors;
  }

  // Check the blocklist state.
  if (errors.count(PreloadCheck::Error::kBlocklistedId) ||
      errors.count(PreloadCheck::Error::kBlocklistedUnknown)) {
    if (allow_silent_install_) {
      // NOTE: extension may still be blocklisted, but we're forced to silently
      // install it. In this case, ExtensionService::OnExtensionInstalled needs
      // to deal with it.
      if (errors.count(PreloadCheck::Error::kBlocklistedId))
        install_flags_ |= kInstallFlagIsBlocklistedForMalware;
    } else {
      // User tried to install a blocklisted extension. Show an error and
      // refuse to install it.
      ReportFailureFromUIThread(CrxInstallError(
          CrxInstallErrorType::DECLINED,
          CrxInstallErrorDetail::EXTENSION_IS_BLOCKLISTED,
          l10n_util::GetStringFUTF16(IDS_EXTENSION_IS_BLOCKLISTED,
                                     base::UTF8ToUTF16(extension()->name()))));
      UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.BlockCRX",
                                extension()->location());
      return;
    }
  }

  // Check for policy errors.
  if (errors.count(PreloadCheck::Error::kDisallowedByPolicy)) {
    // We don't want to show the error infobar for installs from the WebStore,
    // because the WebStore already shows an error dialog itself.
    // Note: |client_| can be NULL in unit_tests!
    if (extension()->from_webstore() && client_)
      client_->install_ui()->SetSkipPostInstallUI(true);

    ReportFailureFromUIThread(
        CrxInstallError(CrxInstallErrorType::DECLINED,
                        CrxInstallErrorDetail::DISALLOWED_BY_POLICY,
                        policy_check_->GetErrorMessage()));
    return;
  }

  ConfirmInstall();
}

void CrxInstaller::ConfirmInstall() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ReportInstallationStage(InstallationStage::kFinalizing);
  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  if (KioskModeInfo::IsKioskOnly(extension())) {
    bool in_kiosk_mode = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager::UserManager* user_manager = user_manager::UserManager::Get();
    in_kiosk_mode = user_manager && user_manager->IsLoggedInAsKioskApp();
#endif
    if (!in_kiosk_mode) {
      ReportFailureFromUIThread(CrxInstallError(
          CrxInstallErrorType::DECLINED, CrxInstallErrorDetail::KIOSK_MODE_ONLY,
          l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_KIOSK_MODE_ONLY)));
      return;
    }
  }

  // Check whether this install is initiated from the settings page to
  // update an existing extension or app.
  CheckUpdateFromSettingsPage();

  GURL overlapping_url;
  ExtensionRegistry* registry = ExtensionRegistry::Get(service->profile());
  const Extension* overlapping_extension =
      registry->enabled_extensions().GetHostedAppByOverlappingWebExtent(
          extension()->web_extent());
  if (overlapping_extension &&
      overlapping_extension->id() != extension()->id()) {
    ReportFailureFromUIThread(
        CrxInstallError(CrxInstallErrorType::OTHER,
                        CrxInstallErrorDetail::OVERLAPPING_WEB_EXTENT,
                        l10n_util::GetStringFUTF16(
                            IDS_EXTENSION_OVERLAPPING_WEB_EXTENT,
                            base::UTF8ToUTF16(extension()->name()),
                            base::UTF8ToUTF16(overlapping_extension->name()))));
    return;
  }

  current_version_ = base::Version(ExtensionPrefs::Get(service->profile())
                         ->GetVersionString(extension()->id()));

  if (client_ &&
      (!allow_silent_install_ || !approved_) &&
      !update_from_settings_page_) {
    AddRef();  // Balanced in OnInstallPromptDone().
    client_->ShowDialog(
        base::BindOnce(&CrxInstaller::OnInstallPromptDone, this), extension(),
        nullptr, show_dialog_callback_);
  } else {
    UpdateCreationFlagsAndCompleteInstall(kDontWithholdPermissions);
  }
}

void CrxInstaller::OnInstallPromptDone(
    ExtensionInstallPrompt::DoneCallbackPayload payload) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If update_from_settings_page_ boolean is true, this functions is
  // getting called in response to ExtensionInstallPrompt::ConfirmReEnable()
  // and if it is false, this function is called in response to
  // ExtensionInstallPrompt::ShowDialog().

  ExtensionService* service = service_weak_.get();
  switch (payload.result) {
    case ExtensionInstallPrompt::Result::ACCEPTED:
    case ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS:
      if (!service || service->browser_terminating())
        return;

      // Install (or re-enable) the extension with full permissions.
      if (update_from_settings_page_) {
        // TODO(crbug.com/40636075): Add support for withholding permissions on
        // the re-enable prompt here once we know how that should be handled.
        DCHECK_NE(
            payload.result,
            ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS);
        service->GrantPermissionsAndEnableExtension(extension());
      } else {
        WithholdingBehavior withholding_behavior =
            payload.result == ExtensionInstallPrompt::Result::
                                  ACCEPTED_WITH_WITHHELD_PERMISSIONS
                ? kWithholdPermissions
                : kDontWithholdPermissions;
        UpdateCreationFlagsAndCompleteInstall(withholding_behavior);
      }
      break;

    case ExtensionInstallPrompt::Result::USER_CANCELED:
      if (!update_from_settings_page_) {
        NotifyCrxInstallComplete(CrxInstallError(
            CrxInstallErrorType::OTHER, CrxInstallErrorDetail::USER_CANCELED));
      }
      break;
    case ExtensionInstallPrompt::Result::ABORTED:
      if (!update_from_settings_page_) {
        NotifyCrxInstallComplete(CrxInstallError(
            CrxInstallErrorType::OTHER, CrxInstallErrorDetail::USER_ABORTED));
      }
      break;
  }

  Release();  // balanced in ConfirmInstall() or ConfirmReEnable().
}

void CrxInstaller::InitializeCreationFlagsForUpdate(const Extension* extension,
                                                    const int initial_flags) {
  DCHECK(extension);

  creation_flags_ = initial_flags;

  // If the extension was installed from or has migrated to the webstore, or
  // its auto-update URL is from the webstore, treat it as a webstore install.
  // Note that we ignore some older extensions with blank auto-update URLs
  // because we are mostly concerned with restrictions on NaCl extensions,
  // which are newer. We need to check whether the update URL is from webstore
  // or not from |ExtensionManagement| because the extension update URL might be
  // overriden by policy.
  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile_);
  if (extension->from_webstore() ||
      extension_management->UpdatesFromWebstore(*extension)) {
    creation_flags_ |= Extension::FROM_WEBSTORE;
  }

  if (extension->was_installed_by_default())
    creation_flags_ |= Extension::WAS_INSTALLED_BY_DEFAULT;

  if (extension->was_installed_by_oem())
    creation_flags_ |= Extension::WAS_INSTALLED_BY_OEM;
}

void CrxInstaller::UpdateCreationFlagsAndCompleteInstall(
    WithholdingBehavior withholding_behavior) {
  creation_flags_ = extension()->creation_flags() | Extension::REQUIRE_KEY;
  // If the extension was already installed and had file access, also grant file
  // access to the updated extension.
  if (ExtensionPrefs::Get(profile())->AllowFileAccess(extension()->id()))
    creation_flags_ |= Extension::ALLOW_FILE_ACCESS;

  if (withholding_behavior == WithholdingBehavior::kWithholdPermissions)
    set_withhold_permissions();

  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile());
  const GURL update_url =
      extension_management->GetEffectiveUpdateURL(*(extension()));
  const bool updates_from_webstore_or_empty_update_url =
      update_url.is_empty() || extension_urls::IsWebstoreUpdateUrl(update_url);
  if (!shared_file_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CrxInstaller::CompleteInstall, this,
                         updates_from_webstore_or_empty_update_url))) {
    NOTREACHED_IN_MIGRATION();
  }
}

void CrxInstaller::CompleteInstall(
    bool updates_from_webstore_or_empty_update_url) {
  DCHECK(shared_file_task_runner_->RunsTasksInCurrentSequence());

  if (current_version_.IsValid() &&
      current_version_.CompareTo(extension()->version()) > 0) {
    ReportFailureFromSharedFileThread(CrxInstallError(
        CrxInstallErrorType::DECLINED,
        CrxInstallErrorDetail::CANT_DOWNGRADE_VERSION,
        l10n_util::GetStringUTF16(extension()->is_app()
                                      ? IDS_APP_CANT_DOWNGRADE_VERSION
                                      : IDS_EXTENSION_CANT_DOWNGRADE_VERSION)));
    return;
  }

  ExtensionAssetsManager* assets_manager =
      ExtensionAssetsManager::GetInstance();
  assets_manager->InstallExtension(
      extension(), unpacked_extension_root_, install_directory_, profile(),
      base::BindOnce(&CrxInstaller::ReloadExtensionAfterInstall, this),
      updates_from_webstore_or_empty_update_url);
}

void CrxInstaller::ReloadExtensionAfterInstall(
    const base::FilePath& version_dir) {
  DCHECK(shared_file_task_runner_->RunsTasksInCurrentSequence());

  if (version_dir.empty()) {
    ReportFailureFromSharedFileThread(
        CrxInstallError(CrxInstallErrorType::OTHER,
                        CrxInstallErrorDetail::MOVE_DIRECTORY_TO_PROFILE_FAILED,
                        l10n_util::GetStringUTF16(
                            IDS_EXTENSION_MOVE_DIRECTORY_TO_PROFILE_FAILED)));
    return;
  }

  // This is lame, but we must reload the extension because absolute paths
  // inside the content scripts are established inside InitFromValue() and we
  // just moved the extension.
  // TODO(aa): All paths to resources inside extensions should be created
  // lazily and based on the Extension's root path at that moment.
  // TODO(rdevlin.cronin): Continue removing std::string errors and replacing
  // with std::u16string
  ExtensionId extension_id = extension()->id();
  std::string error;
  extension_ = file_util::LoadExtension(
      version_dir, install_source_,
      // Note: modified by UpdateCreationFlagsAndCompleteInstall.
      creation_flags_, &error);

  if (extension()) {
    ReportSuccessFromSharedFileThread();
  } else {
    LOG(ERROR) << error << " " << extension_id << " " << download_url_;
    ReportFailureFromSharedFileThread(CrxInstallError(
        CrxInstallErrorType::OTHER, CrxInstallErrorDetail::CANT_LOAD_EXTENSION,
        base::UTF8ToUTF16(error)));
  }
}

void CrxInstaller::ReportFailureFromSharedFileThread(
    const CrxInstallError& error) {
  DCHECK(shared_file_task_runner_->RunsTasksInCurrentSequence());
  if (!content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&CrxInstaller::ReportFailureFromUIThread,
                                    this, error))) {
    NOTREACHED_IN_MIGRATION();
  }
}

void CrxInstaller::ReportFailureFromUIThread(const CrxInstallError& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(CrxInstallErrorType::NONE, error.type());

  if (!service_weak_.get() || service_weak_->browser_terminating())
    return;

  // This isn't really necessary, it is only used because unit tests expect to
  // see errors get reported via this interface.
  //
  // TODO(aa): Need to go through unit tests and clean them up too, probably get
  // rid of this line.
  LoadErrorReporter::GetInstance()->ReportError(error.message(),
                                                false);  // Be quiet.

  if (client_)
    client_->OnInstallFailure(error);

  NotifyCrxInstallComplete(error);

  // Delete temporary files.
  CleanupTempFiles();
}

void CrxInstaller::ReportSuccessFromSharedFileThread() {
  DCHECK(shared_file_task_runner_->RunsTasksInCurrentSequence());

  // Tracking number of extensions installed by users
  if (install_cause() == extension_misc::INSTALL_CAUSE_USER_DOWNLOAD)
    UMA_HISTOGRAM_ENUMERATION("Extensions.ExtensionInstalled", 1, 2);

  if (!content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&CrxInstaller::ReportSuccessFromUIThread, this)))
    NOTREACHED_IN_MIGRATION();

  // Delete temporary files.
  CleanupTempFiles();
}

void CrxInstaller::ReportSuccessFromUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!service_weak_.get() || service_weak_->browser_terminating())
    return;

  extension()->permissions_data()->BindToCurrentThread();

  if (!update_from_settings_page_) {
    // If there is a client, tell the client about installation.
    if (client_)
      client_->OnInstallSuccess(extension_, install_icon_.get());

    // We update the extension's granted permissions if the user already
    // approved the install (client_ is non NULL), or we are allowed to install
    // this silently.
    if ((client_ || allow_silent_install_) && grant_permissions_ &&
        (!expected_version_.IsValid() ||
         expected_version_ == extension()->version())) {
      PermissionsUpdater perms_updater(profile());
      perms_updater.InitializePermissions(extension());
      perms_updater.GrantActivePermissions(extension());
    }
  }

  service_weak_->OnExtensionInstalled(extension(), page_ordinal_,
                                      install_flags_,
                                      std::move(ruleset_install_prefs_));
  NotifyCrxInstallComplete(std::nullopt);
}

void CrxInstaller::ReportInstallationStage(InstallationStage stage) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(GetUnpackerTaskRunner()->RunsTasksInCurrentSequence() ||
           shared_file_task_runner_->RunsTasksInCurrentSequence());
    if (!content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(&CrxInstaller::ReportInstallationStage,
                                      this, stage))) {
      NOTREACHED_IN_MIGRATION();
    }
    return;
  }

  if (!service_weak_.get() || service_weak_->browser_terminating())
    return;
  // In case of force installed extensions, expected_id_ should always be set.
  // We do not want to report in case of other extensions.
  if (expected_id_.empty())
    return;
  InstallStageTracker* install_stage_tracker =
      InstallStageTracker::Get(profile_);
  install_stage_tracker->ReportCRXInstallationStage(expected_id_, stage);
}

void CrxInstaller::NotifyCrxInstallBegin() {
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kCrxInstaller);

  InstallTrackerFactory::GetForBrowserContext(profile())->OnBeginCrxInstall(
      *this, expected_id_);
}

void CrxInstaller::NotifyCrxInstallComplete(
    const std::optional<CrxInstallError>& error) {
  ReportInstallationStage(InstallationStage::kComplete);
  const ExtensionId extension_id =
      expected_id_.empty() && extension() ? extension()->id() : expected_id_;
  InstallStageTracker* install_stage_tracker =
      InstallStageTracker::Get(profile_);
  install_stage_tracker->ReportInstallationStage(
      extension_id, InstallStageTracker::Stage::COMPLETE);
  const bool success = !error.has_value();

  if (extension()) {
    install_stage_tracker->ReportExtensionType(extension_id,
                                               extension()->GetType());
  }

  if (!success && (!expected_id_.empty() || extension())) {
    switch (error->type()) {
      case CrxInstallErrorType::DECLINED:
        install_stage_tracker->ReportCrxInstallError(
            extension_id,
            InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_DECLINED,
            error->detail());
        break;
      case CrxInstallErrorType::SANDBOXED_UNPACKER_FAILURE:
        install_stage_tracker->ReportSandboxedUnpackerFailureReason(
            extension_id, error.value());
        break;
      case CrxInstallErrorType::OTHER:
        install_stage_tracker->ReportCrxInstallError(
            extension_id,
            InstallStageTracker::FailureReason::CRX_INSTALL_ERROR_OTHER,
            error->detail());
        break;
      case CrxInstallErrorType::NONE:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  InstallTrackerFactory::GetForBrowserContext(profile())->OnFinishCrxInstall(
      *this, success ? extension()->id() : expected_id_, success);

  if (success)
    ConfirmReEnable();

  RunInstallerCallbacks(error);

  profile_keep_alive_.reset();
}

void CrxInstaller::CleanupTempFiles() {
  if (!shared_file_task_runner_->RunsTasksInCurrentSequence()) {
    if (!shared_file_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&CrxInstaller::CleanupTempFiles, this))) {
      NOTREACHED_IN_MIGRATION();
    }
    return;
  }

  // Delete the temp directory and crx file as necessary.
  if (!temp_dir_.value().empty()) {
    base::DeletePathRecursively(temp_dir_);
    temp_dir_ = base::FilePath();
  }

  if (delete_source_ && !source_file_.value().empty()) {
    base::DeleteFile(source_file_);
    source_file_ = base::FilePath();
  }
}

void CrxInstaller::CheckUpdateFromSettingsPage() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  if (off_store_install_allow_reason_ != OffStoreInstallAllowedFromSettingsPage)
    return;

  const Extension* installed_extension =
      ExtensionRegistry::Get(service->profile())
          ->GetInstalledExtension(extension()->id());
  if (installed_extension) {
    // Previous version of the extension exists.
    update_from_settings_page_ = true;
    expected_id_ = installed_extension->id();
    install_source_ = installed_extension->location();
    install_cause_ = extension_misc::INSTALL_CAUSE_UPDATE;
  }
}

void CrxInstaller::ConfirmReEnable() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  if (!update_from_settings_page_)
    return;

  ExtensionPrefs* prefs = ExtensionPrefs::Get(service->profile());
  if (!prefs->DidExtensionEscalatePermissions(extension()->id()))
    return;

  if (client_) {
    AddRef();  // Balanced in OnInstallPromptDone().
    ExtensionInstallPrompt::PromptType type =
        ExtensionInstallPrompt::GetReEnablePromptTypeForExtension(
            service->profile(), extension());
    client_->ShowDialog(
        base::BindOnce(&CrxInstaller::OnInstallPromptDone, this), extension(),
        nullptr, std::make_unique<ExtensionInstallPrompt::Prompt>(type),
        ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  }
}

base::SequencedTaskRunner* CrxInstaller::GetUnpackerTaskRunner() {
  if (!unpacker_task_runner_) {
    bool low_priority =
        (creation_flags_ & Extension::WAS_INSTALLED_BY_DEFAULT) &&
        !(creation_flags_ & Extension::WAS_INSTALLED_BY_OEM);
    unpacker_task_runner_ = GetOneShotFileTaskRunner(
        low_priority ? base::TaskPriority::BEST_EFFORT
                     : base::TaskPriority::USER_VISIBLE);
  }
  return unpacker_task_runner_.get();
}

void CrxInstaller::set_withhold_permissions() {
  DCHECK(base::FeatureList::IsEnabled(
      extensions_features::kAllowWithholdingExtensionPermissionsOnInstall));
  creation_flags_ |= Extension::WITHHOLD_PERMISSIONS;
}

void CrxInstaller::AddInstallerCallback(InstallerResultCallback callback) {
  installer_callbacks_.emplace_back(std::move(callback));
}

void CrxInstaller::RunInstallerCallbacks(
    const std::optional<CrxInstallError>& error) {
  for (InstallerResultCallback& callback : installer_callbacks_) {
    if (!content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback), error))) {
      NOTREACHED_IN_MIGRATION();
    }
  }
  installer_callbacks_.clear();
}

void CrxInstaller::set_expectations_verified_callback(
    ExpectationsVerifiedCallback callback) {
  expectations_verified_callback_ = std::move(callback);
}

}  // namespace extensions
