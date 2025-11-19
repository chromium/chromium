// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_registrar_delegate.h"

#include <set>
#include <string>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/corrupted_extension_reinstaller.h"
#include "chrome/browser/extensions/data_deleter.h"
#include "chrome/browser/extensions/extension_allowlist.h"
#include "chrome/browser/extensions/extension_assets_manager.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/external_install_manager.h"
#include "chrome/browser/extensions/install_verifier_factory.h"
#include "chrome/browser/extensions/installed_loader.h"
#include "chrome/browser/extensions/profile_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/common/webui_url_constants.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "extensions/browser/delayed_install_manager.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/install_verifier.h"
#include "extensions/browser/pending_extension_manager.h"
#include "extensions/browser/permissions/permissions_updater.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/crash_keys.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/file_system/file_system_context.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/sync/extension_sync_service.h"
#include "chrome/browser/ui/webui/theme_source.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

// When uninstalling an extension, determine if the extension's directory
// should be deleted when uninstalling. Returns `true` iff extension is
// unpacked and installed outside the unpacked extensions installations dir.
// Example: packed extensions are always deleted. But unpacked extensions are
// in a folder outside the profile dir are not deleted.
bool SkipDeleteExtensionDir(const Extension& extension,
                            const base::FilePath& profile_path) {
  bool is_unpacked_location =
      Manifest::IsUnpackedLocation(extension.location());
  bool extension_dir_not_direct_subdir_of_unpacked_extensions_install_dir =
      extension.path().DirName() !=
      profile_path.AppendASCII(extensions::kUnpackedInstallDirectoryName);
  return is_unpacked_location &&
         extension_dir_not_direct_subdir_of_unpacked_extensions_install_dir;
}

}  // namespace

ChromeExtensionRegistrarDelegate::ChromeExtensionRegistrarDelegate(
    Profile* profile)
    : profile_(profile),
      system_(ExtensionSystem::Get(profile_)),
      extension_prefs_(ExtensionPrefs::Get(profile_)),
      registry_(ExtensionRegistry::Get(profile_)),
      component_loader_(ComponentLoader::Get(profile_)) {}

ChromeExtensionRegistrarDelegate::~ChromeExtensionRegistrarDelegate() = default;

void ChromeExtensionRegistrarDelegate::Init(ExtensionRegistrar* registrar) {
  extension_registrar_ = registrar;
}

void ChromeExtensionRegistrarDelegate::Shutdown() {
  // Avoid dangling pointers.
  profile_ = nullptr;
  extension_prefs_ = nullptr;
  system_ = nullptr;
  registry_ = nullptr;
  extension_registrar_ = nullptr;
  component_loader_ = nullptr;
}

void ChromeExtensionRegistrarDelegate::PreAddExtension(
    const Extension* extension,
    const Extension* old_extension) {
  // An extension may have updated to no longer support incognito. When this
  // is the case, we don't show the toggle in the chrome://extensions page.
  // In order to ensure an extension doesn't keep an unrevokable permission,
  // reset the stored pref.
  if (old_extension && !IncognitoInfo::IsIncognitoAllowed(extension)) {
    extension_prefs_->SetIsIncognitoEnabled(extension->id(), false);
  }

  // Check if the extension's privileges have changed and mark the
  // extension disabled if necessary.
  CheckPermissionsIncrease(extension, !!old_extension);
}

void ChromeExtensionRegistrarDelegate::OnAddNewOrUpdatedExtension(
    const Extension* extension) {
  if (InstallVerifier::NeedsVerification(*extension, profile_)) {
    InstallVerifierFactory::GetForBrowserContext(profile_)->VerifyExtension(
        extension->id());
  }
}

void ChromeExtensionRegistrarDelegate::PostActivateExtension(
    scoped_refptr<const Extension> extension) {
  // Update policy permissions in case they were changed while extension was not
  // active.
  PermissionsUpdater(profile_).ApplyPolicyHostRestrictions(*extension);

  // TODO(kalman): Convert ExtensionSpecialStoragePolicy to a
  // BrowserContextKeyedService and use ExtensionRegistryObserver.
  auto* special_storage_policy = profile_->GetExtensionSpecialStoragePolicy();
  CHECK(special_storage_policy);
  special_storage_policy->GrantRightsForExtension(extension.get(), profile_);

  // TODO(kalman): This is broken. The crash reporter is process-wide so doesn't
  // work properly multi-profile. Besides which, it should be using
  // ExtensionRegistryObserver. See http://crbug.com/355029.
  UpdateActiveExtensionsInCrashReporter();

  const PermissionsData* permissions_data = extension->permissions_data();

  // If the extension has permission to load chrome://favicon/ resources we need
  // to make sure that the FaviconSource is registered with the
  // ChromeURLDataManager.
  if (permissions_data->HasHostPermission(GURL(chrome::kChromeUIFaviconURL))) {
    content::URLDataSource::Add(
        profile_, std::make_unique<FaviconSource>(
                      profile_, chrome::FaviconUrlFormat::kFaviconLegacy));
  }

  // Same for chrome://theme/ resources.
  if (permissions_data->HasHostPermission(GURL(chrome::kChromeUIThemeURL))) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    content::URLDataSource::Add(profile_,
                                std::make_unique<ThemeSource>(profile_));
#else
    // TODO(crbug.com/408507365): Figure out the theme story on desktop Android
    // and port ThemeSource if necessary.
    NOTIMPLEMENTED() << "Themes not yet supported on desktop Android.";
#endif
  }
}

void ChromeExtensionRegistrarDelegate::PostDeactivateExtension(
    scoped_refptr<const Extension> extension) {
  // TODO(kalman): Convert ExtensionSpecialStoragePolicy to a
  // BrowserContextKeyedService and use ExtensionRegistryObserver.
  auto* special_storage_policy = profile_->GetExtensionSpecialStoragePolicy();
  CHECK(special_storage_policy);
  special_storage_policy->RevokeRightsForExtension(extension.get(), profile_);

#if BUILDFLAG(IS_CHROMEOS)
  // Revoke external file access for the extension from its file system context.
  // It is safe to access the extension's storage partition at this point. The
  // storage partition may get destroyed only after the extension gets unloaded.
  storage::FileSystemContext* filesystem_context =
      util::GetStoragePartitionForExtensionId(extension->id(), profile_)
          ->GetFileSystemContext();
  if (filesystem_context && ash::FileSystemBackend::Get(*filesystem_context)) {
    ash::FileSystemBackend::Get(*filesystem_context)
        ->RevokeAccessForOrigin(extension->origin());
  }
#endif

  // TODO(kalman): This is broken. The crash reporter is process-wide so doesn't
  // work properly multi-profile. Besides which, it should be using
  // ExtensionRegistryObserver::OnExtensionLoaded. See http://crbug.com/355029.
  UpdateActiveExtensionsInCrashReporter();
}

void ChromeExtensionRegistrarDelegate::PreUninstallExtension(
    scoped_refptr<const Extension> extension) {
  InstallVerifierFactory::GetForBrowserContext(profile_)->Remove(
      extension->id());
}

void ChromeExtensionRegistrarDelegate::PostUninstallExtension(
    scoped_refptr<const Extension> extension,
    base::OnceClosure done_callback) {
  // Prepare barrier closure for UninstallExtensionOnFileThread() task (if
  // applicable) and DataDeleter::StartDeleting().
  bool is_unpacked_location =
      Manifest::IsUnpackedLocation(extension->location());
  base::RepeatingClosure subtask_done_callback = base::DoNothing();
  if (!done_callback.is_null()) {
    int num_tasks = is_unpacked_location ? 1 : 2;
    subtask_done_callback =
        base::BarrierClosure(num_tasks, std::move(done_callback));
  }

  // Delete extensions in profile directory (from webstore, or from .crx), but
  // do not delete unpacked in a folder outside the profile directory.
  if (!SkipDeleteExtensionDir(*extension, profile_->GetPath())) {
    // Extensions installed from webstore or .crx are versioned in subdirs so we
    // delete the parent dir. Unpacked (installed from .zip rather than folder)
    // are not versioned so we just delete the single installation directory.
    base::FilePath extension_dir_to_delete =
        is_unpacked_location ? extension->path() : extension->path().DirName();

    base::FilePath extensions_install_dir =
        is_unpacked_location
            ? extension_registrar_->unpacked_install_directory()
            : extension_registrar_->install_directory();

    // Tell the backend to start deleting the installed extension on the file
    // thread.
    if (!GetExtensionFileTaskRunner()->PostTaskAndReply(
            FROM_HERE,
            base::BindOnce(&ChromeExtensionRegistrarDelegate::
                               UninstallExtensionOnFileThread,
                           extension->id(), profile_->GetProfileUserName(),
                           std::move(extensions_install_dir),
                           std::move(extension_dir_to_delete),
                           profile_->GetPath()),
            subtask_done_callback)) {
      NOTREACHED();
    }
  }

  DataDeleter::StartDeleting(profile_, extension.get(), subtask_done_callback);
}

void ChromeExtensionRegistrarDelegate::DoLoadExtensionForReload(
    const ExtensionId& extension_id,
    const base::FilePath& path,
    bool load_error_behavior_noisy) {
  // If we're reloading a component extension, use the component extension
  // loader's reloader.
  if (component_loader_->Exists(extension_id)) {
    component_loader_->Reload(extension_id);
    return;
  }

  // Check the installed extensions to see if what we're reloading was already
  // installed.
  std::optional<ExtensionInfo> installed_extension(
      extension_prefs_->GetInstalledExtensionInfo(extension_id));
  if (installed_extension && installed_extension->extension_manifest.get()) {
    InstalledLoader(profile_).Load(*installed_extension, false);
  } else {
    // Otherwise, the extension is unpacked (location LOAD). We must load it
    // from the path.
    CHECK(!path.empty()) << "ExtensionRegistrar should never ask to load an "
                            "unknown extension with no path";
    scoped_refptr<UnpackedInstaller> unpacked_installer =
        UnpackedInstaller::Create(profile_);
    unpacked_installer->set_be_noisy_on_failure(load_error_behavior_noisy);
    unpacked_installer->set_completion_callback(base::BindOnce(
        &ChromeExtensionRegistrarDelegate::OnUnpackedReloadFailure,
        weak_factory_.GetWeakPtr()));
    unpacked_installer->Load(path);
  }
}
void ChromeExtensionRegistrarDelegate::LoadExtensionForReload(
    const ExtensionId& extension_id,
    const base::FilePath& path) {
  DoLoadExtensionForReload(extension_id, path, true);
}
void ChromeExtensionRegistrarDelegate::LoadExtensionForReloadWithQuietFailure(
    const ExtensionId& extension_id,
    const base::FilePath& path) {
  DoLoadExtensionForReload(extension_id, path, false);
}

void ChromeExtensionRegistrarDelegate::ShowExtensionDisabledError(
    const Extension* extension,
    bool is_remote_install) {
  AddExtensionDisabledError(profile_, extension, is_remote_install);
}

bool ChromeExtensionRegistrarDelegate::CanEnableExtension(
    const Extension* extension) {
  CHECK(system_->management_policy());
  return !system_->management_policy()->MustRemainDisabled(extension, nullptr);
}

bool ChromeExtensionRegistrarDelegate::CanDisableExtension(
    const Extension* extension) {
  // Some extensions cannot be disabled by users:
  // - |extension| can be null if sync disables an extension that is not
  //   installed yet; allow disablement in this case.
  if (!extension) {
    return true;
  }

  // - Shared modules are just resources used by other extensions, and are not
  //   user-controlled.
  if (SharedModuleInfo::IsSharedModule(extension)) {
    return false;
  }

  // - EXTERNAL_COMPONENT extensions are not generally modifiable by users, but
  //   can be uninstalled by the browser if the user sets extension-specific
  //   preferences.
  if (extension->location() == ManifestLocation::kExternalComponent) {
    return true;
  }

  CHECK(system_->management_policy());
  return system_->management_policy()->UserMayModifySettings(extension,
                                                             nullptr);
}

void ChromeExtensionRegistrarDelegate::GrantActivePermissions(
    const Extension* extension) {
  PermissionsUpdater(profile_).GrantActivePermissions(extension);
}

void ChromeExtensionRegistrarDelegate::UpdateExternalExtensionAlert() {
  ExternalInstallManager::Get(profile_)->UpdateExternalExtensionAlert();
}

void ChromeExtensionRegistrarDelegate::OnExtensionInstalled(
    const Extension* extension,
    const syncer::StringOrdinal& page_ordinal,
    int install_flags,
    base::Value::Dict ruleset_install_prefs) {
  const std::string& id = extension->id();
  base::flat_set<int> disable_reasons =
      extension_registrar_->GetDisableReasonsOnInstalled(extension);
  std::string install_parameter;
  auto* pending_extension_manager = PendingExtensionManager::Get(profile_);
  const PendingExtensionInfo* pending_extension_info =
      pending_extension_manager->GetById(id);
  auto* corrupted_extension_reinstaller =
      CorruptedExtensionReinstaller::Get(profile_);
  bool is_reinstall_for_corruption =
      corrupted_extension_reinstaller->IsReinstallForCorruptionExpected(id);

  if (is_reinstall_for_corruption) {
    corrupted_extension_reinstaller->MarkResolved(id);
  }

  if (pending_extension_info) {
    if (!pending_extension_info->ShouldAllowInstall(extension, profile_)) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
      // Note: Theme is unsupported on desktop Android.
      // Hack for crbug.com/558299, see comment on DeleteThemeDoNotUse.
      if (extension->is_theme() && pending_extension_info->is_from_sync()) {
        ExtensionSyncService::Get(profile_)->DeleteThemeDoNotUse(*extension);
      }
#endif

      pending_extension_manager->Remove(id);

      ExtensionManagement* management =
          ExtensionManagementFactory::GetForBrowserContext(profile_);
      LOG(WARNING) << "ShouldAllowInstall() returned false for " << id
                   << " of type " << extension->GetType() << " and update URL "
                   << management->GetEffectiveUpdateURL(*extension).spec()
                   << "; not installing";

      // Delete the extension directory since we're not going to
      // load it.
      if (!GetExtensionFileTaskRunner()->PostTask(
              FROM_HERE,
              base::GetDeletePathRecursivelyCallback(extension->path()))) {
        NOTREACHED();
      }
      return;
    }

    install_parameter = pending_extension_info->install_parameter();
    pending_extension_manager->Remove(id);
  } else if (!is_reinstall_for_corruption) {
    // We explicitly want to re-enable an uninstalled external
    // extension; if we're here, that means the user is manually
    // installing the extension.
    if (extension_prefs_->IsExternalExtensionUninstalled(id)) {
      disable_reasons.clear();
    }
  }

  // If the old version of the extension was disabled due to corruption, this
  // new install may correct the problem.
  disable_reasons.erase(disable_reason::DISABLE_CORRUPTED);

  // Unsupported requirements overrides the management policy.
  if (install_flags & kInstallFlagHasRequirementErrors) {
    disable_reasons.insert(disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT);
  } else {
    // Requirement is supported now, remove the corresponding disable reason
    // instead.
    disable_reasons.erase(disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT);
  }

  // Check if the extension was disabled because of the minimum version
  // requirements from enterprise policy, and satisfies it now.
  if (ExtensionManagementFactory::GetForBrowserContext(profile_)
          ->CheckMinimumVersion(extension, nullptr)) {
    // And remove the corresponding disable reason.
    disable_reasons.erase(disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY);
  }

  if (install_flags & kInstallFlagIsBlocklistedForMalware) {
    // Installation of a blocklisted extension can happen from sync, policy,
    // etc, where to maintain consistency we need to install it, just never
    // load it (see AddExtension). Usually it should be the job of callers to
    // intercept blocklisted extensions earlier (e.g. CrxInstaller, before even
    // showing the install dialogue).
    extension_prefs_->AcknowledgeBlocklistedExtension(id);
    UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.SilentInstall",
                              extension->location());
  }

  RecordInstallHistograms(extension);

  ExtensionAllowlist::Get(profile_)->OnExtensionInstalled(id, install_flags);

  DelayedInstallManager* delayed_install_manager =
      DelayedInstallManager::Get(profile_);

  ExtensionPrefs::DelayReason delay_reason;
  InstallGate::Action action =
      delayed_install_manager->ShouldDelayExtensionInstall(
          extension, !!(install_flags & kInstallFlagInstallImmediately),
          &delay_reason);
  switch (action) {
    case InstallGate::INSTALL:
      extension_registrar_->AddNewOrUpdatedExtension(
          extension, disable_reasons, install_flags, page_ordinal,
          install_parameter, std::move(ruleset_install_prefs));
      return;
    case InstallGate::DELAY:
      extension_prefs_->SetDelayedInstallInfo(
          extension, disable_reasons, install_flags, delay_reason, page_ordinal,
          install_parameter, std::move(ruleset_install_prefs));

      // Transfer ownership of |extension|.
      delayed_install_manager->Insert(extension);

      if (delay_reason == ExtensionPrefs::DelayReason::kWaitForIdle) {
        ExtensionUpdater::Get(profile_)->NotifyAppUpdateAvailable(*extension);
      }
      return;
    case InstallGate::ABORT:
      // Do nothing to abort the install. One such case is the shared module
      // service gets IMPORT_STATUS_UNRECOVERABLE status for the pending
      // install.
      return;
  }

  NOTREACHED() << "Unknown action for delayed install: " << action;
}

void ChromeExtensionRegistrarDelegate::CheckPermissionsIncrease(
    const Extension* extension,
    bool is_extension_loaded) {
  PermissionsUpdater(profile_).InitializePermissions(extension);

  // We keep track of all permissions the user has granted each extension.
  // This allows extensions to gracefully support backwards compatibility
  // by including unknown permissions in their manifests. When the user
  // installs the extension, only the recognized permissions are recorded.
  // When the unknown permissions become recognized (e.g., through browser
  // upgrade), we can prompt the user to accept these new permissions.
  // Extensions can also silently upgrade to less permissions, and then
  // silently upgrade to a version that adds these permissions back.
  //
  // For example, pretend that Chrome 10 includes a permission "omnibox"
  // for an API that adds suggestions to the omnibox. An extension can
  // maintain backwards compatibility while still having "omnibox" in the
  // manifest. If a user installs the extension on Chrome 9, the browser
  // will record the permissions it recognized, not including "omnibox."
  // When upgrading to Chrome 10, "omnibox" will be recognized and Chrome
  // will disable the extension and prompt the user to approve the increase
  // in privileges. The extension could then release a new version that
  // removes the "omnibox" permission. When the user upgrades, Chrome will
  // still remember that "omnibox" had been granted, so that if the
  // extension once again includes "omnibox" in an upgrade, the extension
  // can upgrade without requiring this user's approval.

  // Silently grant all active permissions to pre-installed apps and apps
  // installed in kiosk mode.
  bool auto_grant_permission =
      extension->was_installed_by_default() ||
      ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode();
  if (auto_grant_permission) {
    PermissionsUpdater(profile_).GrantActivePermissions(extension);
  }

  bool is_privilege_increase = false;
  // We only need to compare the granted permissions to the current permissions
  // if the extension has not been auto-granted its permissions above and is
  // installed internally.
  if (extension->location() == ManifestLocation::kInternal &&
      !auto_grant_permission) {
    // Add all the recognized permissions if the granted permissions list
    // hasn't been initialized yet.
    std::unique_ptr<const PermissionSet> granted_permissions =
        extension_prefs_->GetGrantedPermissions(extension->id());
    CHECK(granted_permissions.get());
    // We check the union of both granted permissions and runtime granted
    // permissions as it is possible for permissions which were withheld during
    // installation to have never entered the granted set, but to have later
    // been granted as runtime permissions.
    std::unique_ptr<const PermissionSet> runtime_granted_permissions =
        extension_prefs_->GetRuntimeGrantedPermissions(extension->id());
    std::unique_ptr<const PermissionSet> total_permissions =
        PermissionSet::CreateUnion(*granted_permissions,
                                   *runtime_granted_permissions);

    // Here, we check if an extension's privileges have increased in a manner
    // that requires the user's approval. This could occur because the browser
    // upgraded and recognized additional privileges, or an extension upgrades
    // to a version that requires additional privileges.
    is_privilege_increase =
        PermissionMessageProvider::Get()->IsPrivilegeIncrease(
            *total_permissions,
            extension->permissions_data()->active_permissions(),
            extension->GetType());

    // If there was no privilege increase, the extension might still have new
    // permissions (which either don't generate a warning message, or whose
    // warning messages are suppressed by existing permissions). Grant the new
    // permissions.
    if (!is_privilege_increase) {
      PermissionsUpdater(profile_).GrantActivePermissions(extension);
    }
  }

  const DisableReasonSet disable_reasons =
      extension_prefs_->GetDisableReasons(extension->id());

  // If the extension is disabled due to a permissions increase, but does in
  // fact have all permissions, remove that disable reason.
  if (disable_reasons.contains(disable_reason::DISABLE_PERMISSIONS_INCREASE) &&
      !is_privilege_increase) {
    extension_prefs_->RemoveDisableReason(
        extension->id(), disable_reason::DISABLE_PERMISSIONS_INCREASE);
  }

  // Extension has changed permissions significantly. Disable it. A
  // notification should be sent by the caller. If the extension is already
  // disabled because it was installed remotely, don't add another disable
  // reason.
  if (is_privilege_increase &&
      !disable_reasons.contains(disable_reason::DISABLE_REMOTE_INSTALL)) {
    extension_prefs_->AddDisableReason(
        extension->id(), disable_reason::DISABLE_PERMISSIONS_INCREASE);
  }
}

void ChromeExtensionRegistrarDelegate::UpdateActiveExtensionsInCrashReporter() {
  std::set<std::string> extension_ids;
  for (const auto& extension : registry_->enabled_extensions()) {
    if (!extension->is_theme() &&
        extension->location() != ManifestLocation::kComponent) {
      extension_ids.insert(extension->id());
    }
  }

  // TODO(kalman): This is broken. ExtensionService is per-profile.
  // crash_keys::SetActiveExtensions is per-process. See
  // http://crbug.com/355029.
  crash_keys::SetActiveExtensions(extension_ids);
}

// static
void ChromeExtensionRegistrarDelegate::UninstallExtensionOnFileThread(
    const std::string& id,
    const std::string& profile_user_name,
    const base::FilePath& extensions_install_dir,
    const base::FilePath& extension_dir_to_delete,
    const base::FilePath& profile_dir) {
  ExtensionAssetsManager* assets_manager =
      ExtensionAssetsManager::GetInstance();
  assets_manager->UninstallExtension(id, profile_user_name,
                                     extensions_install_dir,
                                     extension_dir_to_delete, profile_dir);
}

void ChromeExtensionRegistrarDelegate::OnUnpackedReloadFailure(
    const Extension* extension,
    const base::FilePath& file_path,
    const std::u16string& error) {
  if (!error.empty()) {
    extension_registrar_->OnUnpackedExtensionReloadFailed(file_path);
  }
}

void ChromeExtensionRegistrarDelegate::RecordInstallHistograms(
    const Extension* extension) {
  bool is_user_profile =
      extensions::profile_util::ProfileCanUseNonComponentExtensions(profile_);

  if (!registry_->GetInstalledExtension(extension->id())) {
    if (is_user_profile) {
      UMA_HISTOGRAM_ENUMERATION("Extensions.InstallType.User",
                                extension->GetType(), 100);
      UMA_HISTOGRAM_ENUMERATION("Extensions.InstallSource.User2",
                                extension->location(), 100);
      InstalledLoader::RecordPermissionMessagesHistogram(extension, "Install",
                                                         profile_);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Extensions.InstallType.NonUser",
                                extension->GetType(), 100);
      UMA_HISTOGRAM_ENUMERATION("Extensions.InstallSource.NonUser2",
                                extension->location(), 100);
    }
  }
}

}  // namespace extensions
