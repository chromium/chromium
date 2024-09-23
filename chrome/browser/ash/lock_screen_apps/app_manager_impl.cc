// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/app_manager_impl.h"

#include <atomic>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "apps/launcher.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/lock_screen_apps/lock_screen_apps.h"
#include "chrome/browser/ash/lock_screen_apps/lock_screen_profile_creator.h"
#include "chrome/browser/ash/note_taking/note_taking_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_assets_manager.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install_flag.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "url/gurl.h"

namespace lock_screen_apps {

namespace {

using ExtensionCallback = base::OnceCallback<void(
    const scoped_refptr<const extensions::Extension>& extension)>;

// The max number of times the lock screen app can be relaoded if it gets
// terminated while the lock screen is active.
constexpr int kMaxLockScreenAppReloadsCount = 3;

// The lock screen note taking availability state.
enum class ActionAvailability {
  kAvailable = 0,
  kNoActionHandlerApp = 1,
  kAppNotSupportingLockScreen = 2,
  kActionNotEnabledOnLockScreen = 3,
  kDisallowedByPolicy = 4,
  kLockScreenProfileNotCreated = 5,
  kCount,
};

// The reason the note taking app was unloaded from the lock screen apps
// profile.
enum class AppUnloadStatus {
  kNotTerminated = 0,
  kTerminatedReloadable = 1,
  kTerminatedReloadAttemptsExceeded = 2,
  kCount = 3
};

ActionAvailability ToActionAvailability(
    ash::LockScreenAppSupport lock_screen_support) {
  switch (lock_screen_support) {
    case ash::LockScreenAppSupport::kNotSupported:
      return ActionAvailability::kAppNotSupportingLockScreen;
    case ash::LockScreenAppSupport::kSupported:
      return ActionAvailability::kActionNotEnabledOnLockScreen;
    case ash::LockScreenAppSupport::kNotAllowedByPolicy:
      return ActionAvailability::kDisallowedByPolicy;
    case ash::LockScreenAppSupport::kEnabled:
      return ActionAvailability::kAvailable;
  }

  return ActionAvailability::kAppNotSupportingLockScreen;
}

void InvokeCallbackOnTaskRunner(
    ExtensionCallback callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<const extensions::Extension>& extension) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), extension));
}

// Loads extension with the provided |extension_id|, |location|, and
// |creation_flags| from the |version_dir| directory - directory to which the
// extension has been installed.
// |temp_copy| - scoped dir that contains the path from which extension
//     resources have been installed. Not used in this method, but passed around
//     to keep the directory in scope while the app is being installed.
// |callback| - callback to which the loaded app should be passed.
void LoadInstalledExtension(const std::string& extension_id,
                            extensions::mojom::ManifestLocation install_source,
                            int creation_flags,
                            std::unique_ptr<base::ScopedTempDir> temp_copy,
                            ExtensionCallback callback,
                            const base::FilePath& version_dir) {
  if (version_dir.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::string error;
  scoped_refptr<const extensions::Extension> extension =
      extensions::file_util::LoadExtension(
          version_dir, extension_id, install_source, creation_flags, &error);
  std::move(callback).Run(extension);
}

// Installs |extension| as a copy of an extension unpacked at |original_path|
// into |target_install_dir|.
// |profile| is the profile to which the extension is being installed.
// |callback| - called with the app loaded from the final installation path.
void InstallExtensionCopy(
    const scoped_refptr<const extensions::Extension>& extension,
    const base::FilePath& original_path,
    const base::FilePath& target_install_dir,
    Profile* profile,
    bool updates_from_webstore_or_empty_update_url,
    ExtensionCallback callback) {
  base::FilePath target_dir = target_install_dir.Append(extension->id());
  base::FilePath install_temp_dir =
      extensions::file_util::GetInstallTempDir(target_dir);
  auto extension_temp_dir = std::make_unique<base::ScopedTempDir>();
  if (install_temp_dir.empty() ||
      !extension_temp_dir->CreateUniqueTempDirUnderPath(install_temp_dir)) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Copy the original extension path to a temp path to prevent
  // ExtensionAssetsManager from deleting it (as InstallExtension renames the
  // source path to a new location under the target install directory).
  base::FilePath temp_copy =
      extension_temp_dir->GetPath().Append(original_path.BaseName());
  if (!base::CopyDirectory(original_path, temp_copy, true /* recursive */)) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Note: |extension_temp_dir| is passed around to ensure it stays in scope
  // until the app installation is done.
  extensions::ExtensionAssetsManager::GetInstance()->InstallExtension(
      extension.get(), temp_copy, target_install_dir, profile,
      base::BindOnce(&LoadInstalledExtension, extension->id(),
                     extension->location(), extension->creation_flags(),
                     std::move(extension_temp_dir), std::move(callback)),
      updates_from_webstore_or_empty_update_url);
}

}  // namespace

AppManagerImpl::AppManagerImpl(const base::TickClock* tick_clock)
    : tick_clock_(tick_clock) {}

AppManagerImpl::~AppManagerImpl() = default;

void AppManagerImpl::Initialize(
    Profile* primary_profile,
    LockScreenProfileCreator* lock_screen_profile_creator) {
  DCHECK_EQ(State::kNotInitialized, state_);
  DCHECK(primary_profile);

  primary_profile_ = primary_profile;
  lock_screen_profile_creator_ = lock_screen_profile_creator;

  state_ = State::kInactive;

  note_taking_helper_observation_.Observe(ash::NoteTakingHelper::Get());

  lock_screen_profile_creator_->AddCreateProfileCallback(
      base::BindOnce(&AppManagerImpl::OnLockScreenProfileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppManagerImpl::OnLockScreenProfileLoaded() {
  if (!lock_screen_profile_creator_->lock_screen_profile())
    return;

  DCHECK_NE(primary_profile_,
            lock_screen_profile_creator_->lock_screen_profile());

  // Do not use OTR profile for lock screen Chrome apps. This is important for
  // profile usage in |LaunchLockScreenApp| - lock screen app background page
  // runs in original, non off the record profile, so the launch event has to be
  // dispatched to that profile. For other |lock_screen_profile_|, it makes no
  // difference - the profile is used to get browser context keyed services, all
  // of which redirect OTR profile to the original one.
  lock_screen_profile_ =
      lock_screen_profile_creator_->lock_screen_profile()->GetOriginalProfile();

  CHECK(!ash::ProfileHelper::Get()->GetUserByProfile(lock_screen_profile_))
      << "Lock screen profile should not be associated with any users.";

  UpdateLockScreenAppState();
}

void AppManagerImpl::Start(
    const base::RepeatingClosure& note_taking_changed_callback) {
  DCHECK_NE(State::kNotInitialized, state_);

  app_changed_callback_ = note_taking_changed_callback;

  if (state_ == State::kActive || state_ == State::kActivating)
    return;

  extensions_observation_.Observe(
      extensions::ExtensionRegistry::Get(primary_profile_));

  lock_screen_app_id_.clear();
  std::string app_id = FindLockScreenAppId();
  if (app_id.empty()) {
    state_ = State::kAppUnavailable;
    return;
  }

  state_ = AddAppToLockScreenProfile(app_id);
  if (state_ == State::kActive || state_ == State::kActivating)
    lock_screen_app_id_ = app_id;
}

void AppManagerImpl::Stop() {
  DCHECK_NE(State::kNotInitialized, state_);

  app_changed_callback_.Reset();
  extensions_observation_.Reset();
  available_lock_screen_app_reloads_ = 0;

  if (state_ == State::kInactive)
    return;

  RemoveChromeAppFromLockScreenProfile(lock_screen_app_id_);
  lock_screen_app_id_.clear();
  state_ = State::kInactive;
}

bool AppManagerImpl::IsLockScreenAppAvailable() const {
  return state_ == State::kActive && !lock_screen_app_id_.empty();
}

std::string AppManagerImpl::GetLockScreenAppId() const {
  if (!IsLockScreenAppAvailable())
    return std::string();
  return lock_screen_app_id_;
}

bool AppManagerImpl::LaunchLockScreenApp() {
  if (!IsLockScreenAppAvailable())
    return false;

  // TODO(crbug.com/40099955): Handle web apps here.

  const extensions::Extension* app = GetChromeAppForLockScreenAppLaunch();
  // If the app cannot be found at this point, it either got unexpectedly
  // disabled, or it failed to reload (in case it was previously terminated).
  // In either case, note taking should not be reported as available anymore.
  if (!app) {
    RemoveLockScreenAppDueToError();
    return false;
  }

  extensions::api::app_runtime::ActionData action_data;
  action_data.action_type = extensions::api::app_runtime::ActionType::kNewNote;
  action_data.is_lock_screen_action = true;
  action_data.restore_last_action_state =
      primary_profile_->GetPrefs()->GetBoolean(
          prefs::kRestoreLastLockScreenNote);
  apps::LaunchPlatformAppWithAction(lock_screen_profile_, app,
                                    std::move(action_data));
  return true;
}

void AppManagerImpl::OnExtensionLoaded(content::BrowserContext* browser_context,
                                       const extensions::Extension* extension) {
  if (browser_context == primary_profile_ &&
      extension->id() ==
          primary_profile_->GetPrefs()->GetString(prefs::kNoteTakingAppId)) {
    UpdateLockScreenAppState();
  }
}

void AppManagerImpl::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (extension->id() != lock_screen_app_id_)
    return;

  if (browser_context == primary_profile_) {
    UpdateLockScreenAppState();
  } else if (browser_context == lock_screen_profile_) {
    HandleLockScreenChromeAppUnload(reason);
  }
}

void AppManagerImpl::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  // If the app is uninstalled from the lock screen apps profile, make sure
  // it's not reported as available anymore.
  if (browser_context == lock_screen_profile_ &&
      extension->id() == lock_screen_app_id_) {
    RemoveLockScreenAppDueToError();
  }
}

void AppManagerImpl::OnAvailableNoteTakingAppsUpdated() {}

void AppManagerImpl::OnPreferredNoteTakingAppUpdated(Profile* profile) {
  if (profile != primary_profile_)
    return;

  UpdateLockScreenAppState();
}

void AppManagerImpl::UpdateLockScreenAppState() {
  if (state_ == State::kInactive)
    return;

  std::string app_id = FindLockScreenAppId();
  if (app_id == lock_screen_app_id_)
    return;

  RemoveChromeAppFromLockScreenProfile(lock_screen_app_id_);
  lock_screen_app_id_.clear();

  state_ = AddAppToLockScreenProfile(app_id);
  if (state_ == State::kActive || state_ == State::kActivating)
    lock_screen_app_id_ = app_id;

  if (!app_changed_callback_.is_null())
    app_changed_callback_.Run();
}

std::string AppManagerImpl::FindLockScreenAppId() const {
  ash::NoteTakingHelper* helper = ash::NoteTakingHelper::Get();
  std::string app_id = helper->GetPreferredAppId(primary_profile_);
  // Lock screen apps service should always exist on the primary profile.
  DCHECK(primary_profile_);
  DCHECK(ash::LockScreenAppsFactory::IsSupportedProfile(primary_profile_));
  ash::LockScreenAppSupport lock_screen_support =
      ash::LockScreenApps::GetSupport(primary_profile_, app_id);

  ActionAvailability availability =
      app_id.empty() ? ActionAvailability::kNoActionHandlerApp
                     : ToActionAvailability(lock_screen_support);

  // |lock_screen_profile_| is created only if a note taking app is available
  // on the lock screen. If an app is not available, the profile is expected to
  // be nullptr.
  // If the app is available and the lock_screen_profile is not set, the profile
  // might still be loading, and |FindLockScreenAppId| will be called
  // again when the profile is loaded - until then, ignore the available app.
  if (!lock_screen_profile_ && availability == ActionAvailability::kAvailable)
    availability = ActionAvailability::kLockScreenProfileNotCreated;

  if (availability != ActionAvailability::kAvailable)
    return std::string();

  return app_id;
}

AppManagerImpl::State AppManagerImpl::AddAppToLockScreenProfile(
    const std::string& app_id) {
  // TODO(crbug.com/40099955): First check if app_id is an installed web app.

  extensions::ExtensionRegistry* primary_registry =
      extensions::ExtensionRegistry::Get(primary_profile_);
  const extensions::Extension* app =
      primary_registry->enabled_extensions().GetByID(app_id);
  if (!app)
    return State::kAppUnavailable;

  bool is_unpacked = extensions::Manifest::IsUnpackedLocation(app->location());

  // Unpacked apps in lock screen profile will be loaded from their original
  // file path, so their path will be the same as the primary profile app's.
  // For the rest, the app will be copied to a location in the lock screen
  // profile's extension install directory (using |InstallExtensionCopy|) - the
  // exact final path is not known at this point, and will be set as part of
  // |InstallExtensionCopy|.
  base::FilePath lock_profile_app_path =
      is_unpacked ? app->path() : base::FilePath();

  std::string error;
  scoped_refptr<extensions::Extension> lock_profile_app =
      extensions::Extension::Create(lock_profile_app_path, app->location(),
                                    app->manifest()->value()->Clone(),
                                    app->creation_flags(), app->id(), &error);

  // While extension creation can fail in general, in this case the lock screen
  // profile extension creation arguments come from an app already installed in
  // a user profile. If the extension parameters were invalid, the app would not
  // exist in a user profile, and thus |app| would be nullptr, which is not the
  // case at this point.
  DCHECK(lock_profile_app);

  install_count_++;

  if (is_unpacked) {
    InstallAndEnableLockScreenChromeAppInLockScreenProfile(
        lock_profile_app.get());
    return State::kActive;
  }

  extensions::ExtensionService* lock_screen_service =
      extensions::ExtensionSystem::Get(lock_screen_profile_)
          ->extension_service();

  const GURL update_url =
      extensions::ExtensionManagementFactory::GetForBrowserContext(
          lock_screen_profile_)
          ->GetEffectiveUpdateURL(*lock_profile_app);
  bool updates_from_webstore_or_empty_update_url =
      update_url.is_empty() || extension_urls::IsWebstoreUpdateUrl(update_url);

  extensions::GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InstallExtensionCopy, lock_profile_app, app->path(),
          lock_screen_service->install_directory(), lock_screen_profile_,
          updates_from_webstore_or_empty_update_url,
          base::BindOnce(
              &InvokeCallbackOnTaskRunner,
              base::BindOnce(
                  &AppManagerImpl::CompleteLockScreenChromeAppInstall,
                  weak_ptr_factory_.GetWeakPtr(), install_count_,
                  tick_clock_->NowTicks()),
              base::SingleThreadTaskRunner::GetCurrentDefault())));
  return State::kActivating;
}

void AppManagerImpl::CompleteLockScreenChromeAppInstall(
    int install_id,
    base::TimeTicks install_start_time,
    const scoped_refptr<const extensions::Extension>& app) {
  // Bail out if the app manager is no longer waiting for this app's
  // installation - the copied resources will be cleaned up when the (ephemeral)
  // lock screen profile is destroyed.
  if (install_id != install_count_ || state_ != State::kActivating)
    return;

  if (app) {
    DCHECK_EQ(lock_screen_app_id_, app->id());
    InstallAndEnableLockScreenChromeAppInLockScreenProfile(app.get());
    state_ = State::kActive;
  } else {
    state_ = State::kAppUnavailable;
  }

  if (!app_changed_callback_.is_null())
    app_changed_callback_.Run();
}

void AppManagerImpl::InstallAndEnableLockScreenChromeAppInLockScreenProfile(
    const extensions::Extension* app) {
  extensions::ExtensionService* lock_screen_service =
      extensions::ExtensionSystem::Get(lock_screen_profile_)
          ->extension_service();

  lock_screen_service->OnExtensionInstalled(
      app, syncer::StringOrdinal(), extensions::kInstallFlagInstallImmediately);
  lock_screen_service->EnableExtension(app->id());

  available_lock_screen_app_reloads_ = kMaxLockScreenAppReloadsCount;

  lock_screen_profile_extensions_observation_.Observe(
      extensions::ExtensionRegistry::Get(lock_screen_profile_));
}

void AppManagerImpl::RemoveChromeAppFromLockScreenProfile(
    const std::string& app_id) {
  if (app_id.empty())
    return;

  lock_screen_profile_extensions_observation_.Reset();

  extensions::ExtensionRegistry* lock_screen_registry =
      extensions::ExtensionRegistry::Get(lock_screen_profile_);
  if (!lock_screen_registry->GetExtensionById(
          app_id, extensions::ExtensionRegistry::EVERYTHING)) {
    return;
  }

  std::u16string error;
  extensions::ExtensionSystem::Get(lock_screen_profile_)
      ->extension_service()
      ->UninstallExtension(
          app_id, extensions::UNINSTALL_REASON_INTERNAL_MANAGEMENT, &error);
}

const extensions::Extension*
AppManagerImpl::GetChromeAppForLockScreenAppLaunch() {
  // TODO(crbug.com/40099955): First check if app_id is an installed web app.

  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(lock_screen_profile_);

  // Return the app, in case it's currently loaded.
  const extensions::Extension* app =
      extension_registry->enabled_extensions().GetByID(lock_screen_app_id_);
  if (app) {
    return app;
  }

  // If the app has been terminated (which can happen due to an app crash),
  // attempt a reload - otherwise, return nullptr to signal the app is
  // unavailable.
  app =
      extension_registry->terminated_extensions().GetByID(lock_screen_app_id_);
  if (!app) {
    return nullptr;
  }

  if (available_lock_screen_app_reloads_ <= 0) {
    return nullptr;
  }

  available_lock_screen_app_reloads_--;

  std::string error;
  scoped_refptr<extensions::Extension> lock_profile_app =
      extensions::Extension::Create(app->path(), app->location(),
                                    app->manifest()->value()->Clone(),
                                    app->creation_flags(), app->id(), &error);

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(lock_screen_profile_)
          ->extension_service();
  extension_service->AddExtension(lock_profile_app.get());
  extension_service->EnableExtension(lock_profile_app->id());

  app = extension_registry->enabled_extensions().GetByID(lock_screen_app_id_);

  return app;
}

void AppManagerImpl::HandleLockScreenChromeAppUnload(
    extensions::UnloadedExtensionReason reason) {
  if (state_ != State::kActive && state_ != State::kActivating)
    return;

  AppUnloadStatus status = AppUnloadStatus::kNotTerminated;
  if (reason == extensions::UnloadedExtensionReason::TERMINATE) {
    status = available_lock_screen_app_reloads_ > 0
                 ? AppUnloadStatus::kTerminatedReloadable
                 : AppUnloadStatus::kTerminatedReloadAttemptsExceeded;
  }

  // If the app is terminated, it will be reloaded on the next app launch
  // request - if the app cannot be reloaded (e.g. if it was unloaded for a
  // different reason, or it was reloaded too many times already), change the
  // app managet to an error state. This will inform the app manager's user
  // that lock screen note action is not available anymore.
  if (status != AppUnloadStatus::kTerminatedReloadable)
    RemoveLockScreenAppDueToError();
}

void AppManagerImpl::RemoveLockScreenAppDueToError() {
  if (state_ != State::kActive && state_ != State::kActivating)
    return;

  RemoveChromeAppFromLockScreenProfile(lock_screen_app_id_);
  lock_screen_app_id_.clear();
  state_ = State::kInactive;

  if (!app_changed_callback_.is_null())
    app_changed_callback_.Run();
}

}  // namespace lock_screen_apps
