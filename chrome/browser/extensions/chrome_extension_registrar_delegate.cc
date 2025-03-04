// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_registrar_delegate.h"

#include <set>
#include <string>

#include "base/barrier_closure.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/data_deleter.h"
#include "chrome/browser/extensions/delayed_install_manager.h"
#include "chrome/browser/extensions/extension_assets_manager.h"
#include "chrome/browser/extensions/extension_disabled_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/installed_loader.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/webui_url_constants.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
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

using extensions::mojom::ManifestLocation;

namespace extensions {

using LoadErrorBehavior = ExtensionRegistrar::LoadErrorBehavior;

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
    Profile* profile,
    ExtensionService* extension_service,
    ComponentLoader* component_loader,
    const base::FilePath& install_directory,
    const base::FilePath& unpacked_install_directory)
    : profile_(profile),
      system_(ExtensionSystem::Get(profile_)),
      extension_service_(extension_service),
      extension_prefs_(ExtensionPrefs::Get(profile_)),
      registry_(ExtensionRegistry::Get(profile_)),
      component_loader_(component_loader),
      install_directory_(install_directory),
      unpacked_install_directory_(unpacked_install_directory) {}

ChromeExtensionRegistrarDelegate::~ChromeExtensionRegistrarDelegate() = default;

void ChromeExtensionRegistrarDelegate::Init(
    ExtensionRegistrar* registrar,
    DelayedInstallManager* delayed_install) {
  extension_registrar_ = registrar;
  delayed_install_manager_ = delayed_install;
}

void ChromeExtensionRegistrarDelegate::Shutdown() {
  // Avoid dangling pointers. The Profile outlives this object but some other
  // classes don't.
  extension_prefs_ = nullptr;
  system_ = nullptr;
  registry_ = nullptr;
  extension_registrar_ = nullptr;
  delayed_install_manager_ = nullptr;
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

void ChromeExtensionRegistrarDelegate::PostActivateExtension(
    scoped_refptr<const Extension> extension) {
  // Update policy permissions in case they were changed while extension was not
  // active.
  PermissionsUpdater(profile_).ApplyPolicyHostRestrictions(*extension);

  // TODO(kalman): Convert ExtensionSpecialStoragePolicy to a
  // BrowserContextKeyedService and use ExtensionRegistryObserver.
  profile_->GetExtensionSpecialStoragePolicy()->GrantRightsForExtension(
      extension.get(), profile_);

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
    content::URLDataSource::Add(profile_,
                                std::make_unique<ThemeSource>(profile_));
  }
}

void ChromeExtensionRegistrarDelegate::PostDeactivateExtension(
    scoped_refptr<const Extension> extension) {
  // TODO(kalman): Convert ExtensionSpecialStoragePolicy to a
  // BrowserContextKeyedService and use ExtensionRegistryObserver.
  profile_->GetExtensionSpecialStoragePolicy()->RevokeRightsForExtension(
      extension.get(), profile_);

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
  InstallVerifier::Get(profile_)->Remove(extension->id());
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
        is_unpacked_location ? unpacked_install_directory_ : install_directory_;

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

void ChromeExtensionRegistrarDelegate::PostNotifyUninstallExtension(
    scoped_refptr<const Extension> extension) {
  delayed_install_manager_->Remove(extension->id());
}

void ChromeExtensionRegistrarDelegate::LoadExtensionForReload(
    const ExtensionId& extension_id,
    const base::FilePath& path,
    ExtensionRegistrar::LoadErrorBehavior load_error_behavior) {
  if (delayed_install_manager_->Contains(extension_id) &&
      delayed_install_manager_->FinishDelayedInstallationIfReady(
          extension_id, true /*install_immediately*/)) {
    return;
  }

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
    InstalledLoader(extension_service_).Load(*installed_extension, false);
  } else {
    // Otherwise, the extension is unpacked (location LOAD). We must load it
    // from the path.
    CHECK(!path.empty()) << "ExtensionRegistrar should never ask to load an "
                            "unknown extension with no path";
    scoped_refptr<UnpackedInstaller> unpacked_installer =
        UnpackedInstaller::Create(extension_service_);
    unpacked_installer->set_be_noisy_on_failure(load_error_behavior ==
                                                LoadErrorBehavior::kNoisy);
    unpacked_installer->set_completion_callback(base::BindOnce(
        &ChromeExtensionRegistrarDelegate::OnUnpackedReloadFailure,
        weak_factory_.GetWeakPtr()));
    unpacked_installer->Load(path);
  }
}

void ChromeExtensionRegistrarDelegate::ShowExtensionDisabledError(
    const Extension* extension,
    bool is_remote_install) {
  // TODO(crbug.com/399680111): Android will need a different implementation of
  // this function (e.g. an extension_disabled_ui_android.cc file) as it cannot
  // use the views implementation of this bubble.
  AddExtensionDisabledError(profile_, extension, is_remote_install);
}

void ChromeExtensionRegistrarDelegate::FinishDelayedInstallationsIfAny() {
  delayed_install_manager_->MaybeFinishDelayedInstallations();
}

bool ChromeExtensionRegistrarDelegate::CanAddExtension(
    const Extension* extension) {
  // TODO(jstritar): We may be able to get rid of this branch by overriding the
  // default extension state to DISABLED when the --disable-extensions flag
  // is set (http://crbug.com/29067).
  std::set<std::string> disable_flag_exempted_extensions =
      extension_service_->disable_flag_exempted_extensions();
  if (!extension_service_->extensions_enabled() &&
      !Manifest::ShouldAlwaysLoadExtension(extension->location(),
                                           extension->is_theme()) &&
      disable_flag_exempted_extensions.count(extension->id()) == 0) {
    return false;
  }
  return true;
}

bool ChromeExtensionRegistrarDelegate::CanEnableExtension(
    const Extension* extension) {
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

  return system_->management_policy()->UserMayModifySettings(extension,
                                                             nullptr);
}

bool ChromeExtensionRegistrarDelegate::ShouldBlockExtension(
    const Extension* extension) {
  if (!extension_service_->block_extensions()) {
    return false;
  }

  // Blocked extensions aren't marked as such in prefs, thus if
  // |block_extensions_| is true then CanBlockExtension() must be called with an
  // Extension object. If |extension| is not loaded, assume it should be
  // blocked.
  return !extension || extension_registrar_->CanBlockExtension(extension);
}

void ChromeExtensionRegistrarDelegate::GrantActivePermissions(
    const Extension* extension) {
  PermissionsUpdater(profile_).GrantActivePermissions(extension);
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
    const std::string& error) {
  if (!error.empty()) {
    extension_registrar_->OnUnpackedExtensionReloadFailed(file_path);
  }
}

}  // namespace extensions
