// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/app_manager_impl.h"

#include <memory>
#include <utility>

#include "apps/launcher.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/chromeos/lock_screen_apps/lock_screen_profile_creator.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_assets_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install_flag.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"

namespace lock_screen_apps {

namespace {

using ExtensionCallback = base::Callback<void(
    const scoped_refptr<const extensions::Extension>& extension)>;

// The max number of times the lock screen app can be relaoded if it gets
// terminated while the lock screen is active.
constexpr int kMaxLockScreenAppReloadsCount = 3;

// The lock screen note taking availability state.
// Used to report UMA histograms - the values should map to
// LockScreenActionAvailability UMA enum values, and the values assigned to
// enum states should NOT be changed.
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
// Used to report UMA histograms - the values should map to
// LockScreenAppUnloadStatus UMA enum values, and the values assigned to
// enum states should NOT be changed.
enum class AppUnloadStatus {
  kNotTerminated = 0,
  kTerminatedReloadable = 1,
  kTerminatedReloadAttemptsExceeded = 2,
  kCount = 3
};

ActionAvailability GetLockScreenNoteTakingAvailability(
    chromeos::NoteTakingAppInfo* app_info) {
  if (!app_info || !app_info->preferred)
    return ActionAvailability::kNoActionHandlerApp;

  switch (app_info->lock_screen_support) {
    case chromeos::NoteTakingLockScreenSupport::kNotSupported:
      return ActionAvailability::kAppNotSupportingLockScreen;
    case chromeos::NoteTakingLockScreenSupport::kSupported:
      return ActionAvailability::kActionNotEnabledOnLockScreen;
    case chromeos::NoteTakingLockScreenSupport::kNotAllowedByPolicy:
      return ActionAvailability::kDisallowedByPolicy;
    case chromeos::NoteTakingLockScreenSupport::kEnabled:
      return ActionAvailability::kAvailable;
  }

  return ActionAvailability::kAppNotSupportingLockScreen;
}

void InvokeCallbackOnTaskRunner(
    const ExtensionCallback& callback,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const scoped_refptr<const extensions::Extension>& extension) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(callback, extension));
}

// Loads extension with the provided |extension_id|, |location|, and
// |creation_flags| from the |version_dir| directory - directory to which the
// extension has been installed.
// |temp_copy| - scoped dir that contains the path from which extension
//     resources have been installed. Not used in this method, but passed around
//     to keep the directory in scope while the app is being installed.
// |callback| - callback to which the loaded app should be passed.
void LoadInstalledExtension(const std::string& extension_id,
                            extensions::Manifest::Location install_source,
                            int creation_flags,
                            std::unique_ptr<base::ScopedTempDir> temp_copy,
                            const ExtensionCallback& callback,
                            const base::FilePath& version_dir) {
  if (version_dir.empty()) {
    callback.Run(nullptr);
    return;
  }

  std::string error;
  scoped_refptr<const extensions::Extension> extension =
      extensions::file_util::LoadExtension(
          version_dir, extension_id, install_source, creation_flags, &error);
  callback.Run(extension);
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
    const ExtensionCallback& callback) {
  base::FilePath target_dir = target_install_dir.Append(extension->id());
  base::FilePath install_temp_dir =
      extensions::file_util::GetInstallTempDir(target_dir);
  auto extension_temp_dir = std::make_unique<base::ScopedTempDir>();
  if (install_temp_dir.empty() ||
      !extension_temp_dir->CreateUniqueTempDirUnderPath(install_temp_dir)) {
    callback.Run(nullptr);
    return;
  }

  // Copy the original extension path to a temp path to prevent
  // ExtensionAssetsManager from deleting it (as InstallExtension renames the
  // source path to a new location under the target install directory).
  base::FilePath temp_copy =
      extension_temp_dir->GetPath().Append(original_path.BaseName());
  if (!base::CopyDirectory(original_path, temp_copy, true /* recursive */)) {
    callback.Run(nullptr);
    return;
  }

  // Note: |extension_temp_dir| is passed around to ensure it stays in scope
  // until the app installation is done.
  extensions::ExtensionAssetsManager::GetInstance()->InstallExtension(
      extension.get(), temp_copy, target_install_dir, profile,
      base::Bind(&LoadInstalledExtension, extension->id(),
                 extension->location(), extension->creation_flags(),
                 base::Passed(std::move(extension_temp_dir)), callback));
}

}  // namespace

AppManagerImpl::AppManagerImpl(const base::TickClock* tick_clock)
    : tick_clock_(tick_clock),
      extensions_observer_(this),
      lock_screen_profile_extensions_observer_(this),
      note_taking_helper_observer_(this) {}

AppManagerImpl::~AppManagerImpl() = default;

void AppManagerImpl::Initialize(
    Profile* primary_profile,
    LockScreenProfileCreator* lock_screen_profile_creator) {
  DCHECK_EQ(State::kNotInitialized, state_);
  DCHECK(primary_profile);

  primary_profile_ = primary_profile;
  lock_screen_profile_creator_ = lock_screen_profile_creator;

  state_ = State::kInactive;

  note_taking_helper_observer_.Add(chromeos::NoteTakingHelper::Get());

  lock_screen_profile_creator_->AddCreateProfileCallback(
      base::Bind(&AppManagerImpl::OnLockScreenProfileLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void AppManagerImpl::OnLockScreenProfileLoaded() {
  if (!lock_screen_profile_creator_->lock_screen_profile())
    return;

  DCHECK_NE(primary_profile_,
            lock_screen_profile_creator_->lock_screen_profile());

  // Do not use OTR profile for lock screen apps. This is important for
  // profile usage in |LaunchNoteTaking| - lock screen app background page runs
  // in original, non off the record profile, so the launch event has to be
  // dispatched to that profile. For other |lock_screen_profile_|, it makes no
  // difference - the profile is used to get browser context keyed services, all
  // of which redirect OTR profile to the original one.
  lock_screen_profile_ =
      lock_screen_profile_creator_->lock_screen_profile()->GetOriginalProfile();

  CHECK(!chromeos::ProfileHelper::Get()->GetUserByProfile(lock_screen_profile_))
      << "Lock screen profile should not be associated with any users.";

  OnNoteTakingExtensionChanged();
}

void AppManagerImpl::Start(const base::Closure& note_taking_changed_callback) {
  DCHECK_NE(State::kNotInitialized, state_);

  note_taking_changed_callback_ = note_taking_changed_callback;

  if (state_ == State::kActive || state_ == State::kActivating)
    return;

  extensions_observer_.Add(
      extensions::ExtensionRegistry::Get(primary_profile_));

  lock_screen_app_id_.clear();
  std::string app_id = FindLockScreenNoteTakingApp();
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

  note_taking_changed_callback_.Reset();
  extensions_observer_.RemoveAll();
  available_lock_screen_app_reloads_ = 0;

  if (state_ == State::kInactive)
    return;

  RemoveAppFromLockScreenProfile(lock_screen_app_id_);
  lock_screen_app_id_.clear();
  state_ = State::kInactive;
}

bool AppManagerImpl::IsNoteTakingAppAvailable() const {
  return state_ == State::kActive && !lock_screen_app_id_.empty();
}

std::string AppManagerImpl::GetNoteTakingAppId() const {
  if (!IsNoteTakingAppAvailable())
    return std::string();
  return lock_screen_app_id_;
}

bool AppManagerImpl::LaunchNoteTaking() {
  if (!IsNoteTakingAppAvailable())
    return false;

  const extensions::Extension* app = GetAppForLockScreenAppLaunch();
  // If the app cannot be found at this point, it either got unexpectedly
  // disabled, or it failed to reload (in case it was previously terminated).
  // In either case, note taking should not be reported as available anymore.
  if (!app) {
    RemoveLockScreenAppDueToError();
    return false;
  }

  auto action_data =
      std::make_unique<extensions::api::app_runtime::ActionData>();
  action_data->action_type =
      extensions::api::app_runtime::ActionType::ACTION_TYPE_NEW_NOTE;
  action_data->is_lock_screen_action = std::make_unique<bool>(true);
  action_data->restore_last_action_state =
      std::make_unique<bool>(primary_profile_->GetPrefs()->GetBoolean(
          prefs::kRestoreLastLockScreenNote));
  apps::LaunchPlatformAppWithAction(lock_screen_profile_, app,
                                    std::move(action_data), base::FilePath());
  return true;
}

void AppManagerImpl::OnExtensionLoaded(content::BrowserContext* browser_context,
                                       const extensions::Extension* extension) {
  if (browser_context == primary_profile_ &&
      extension->id() ==
          primary_profile_->GetPrefs()->GetString(prefs::kNoteTakingAppId)) {
    OnNoteTakingExtensionChanged();
  }
}

void AppManagerImpl::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (extension->id() != lock_screen_app_id_)
    return;

  if (browser_context == primary_profile_) {
    OnNoteTakingExtensionChanged();
  } else if (browser_context == lock_screen_profile_) {
    HandleLockScreenAppUnload(reason);
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

  OnNoteTakingExtensionChanged();
}

void AppManagerImpl::OnNoteTakingExtensionChanged() {
  if (state_ == State::kInactive)
    return;

  std::string app_id = FindLockScreenNoteTakingApp();
  if (app_id == lock_screen_app_id_)
    return;

  RemoveAppFromLockScreenProfile(lock_screen_app_id_);
  lock_screen_app_id_.clear();

  state_ = AddAppToLockScreenProfile(app_id);
  if (state_ == State::kActive || state_ == State::kActivating)
    lock_screen_app_id_ = app_id;

  if (!note_taking_changed_callback_.is_null())
    note_taking_changed_callback_.Run();
}

std::string AppManagerImpl::FindLockScreenNoteTakingApp() const {
  // Note that lock screen does not currently support Android apps, so
  // it's enough to only check the state of the preferred Chrome app.
  std::unique_ptr<chromeos::NoteTakingAppInfo> note_taking_app =
      chromeos::NoteTakingHelper::Get()->GetPreferredChromeAppInfo(
          primary_profile_);
  ActionAvailability availability =
      GetLockScreenNoteTakingAvailability(note_taking_app.get());

  // |lock_screen_profile_| is created only if a note taking app is available
  // on the lock screen. If an app is not available, the profile is expected to
  // be nullptr.
  // If the app is available and the lock_screen_profile is not set, the profile
  // might still be loading, and |FindLockScreenNoteTakingApp| will be called
  // again when the profile is loaded - until then, report to UMA that lock
  // screen profile was not created at this point, and otherwise ignore the
  // available app.
  if (!lock_screen_profile_ && availability == ActionAvailability::kAvailable)
    availability = ActionAvailability::kLockScreenProfileNotCreated;

  UMA_HISTOGRAM_ENUMERATION(
      "Apps.LockScreen.NoteTakingApp.AvailabilityOnScreenLock", availability,
      ActionAvailability::kCount);

  if (availability != ActionAvailability::kAvailable)
    return std::string();

  return note_taking_app->app_id;
}

AppManagerImpl::State AppManagerImpl::AddAppToLockScreenProfile(
    const std::string& app_id) {
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
                                    *app->manifest()->value()->CreateDeepCopy(),
                                    app->creation_flags(), app->id(), &error);

  // While extension creation can fail in general, in this case the lock screen
  // profile extension creation arguments come from an app already installed in
  // a user profile. If the extension parameters were invalid, the app would not
  // exist in a user profile, and thus |app| would be nullptr, which is not the
  // case at this point.
  DCHECK(lock_profile_app);

  install_count_++;

  if (is_unpacked) {
    InstallAndEnableLockScreenAppInLockScreenProfile(lock_profile_app.get());
    return State::kActive;
  }

  extensions::ExtensionService* lock_screen_service =
      extensions::ExtensionSystem::Get(lock_screen_profile_)
          ->extension_service();

  extensions::GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InstallExtensionCopy, lock_profile_app, app->path(),
          lock_screen_service->install_directory(), lock_screen_profile_,
          base::Bind(&InvokeCallbackOnTaskRunner,
                     base::Bind(&AppManagerImpl::CompleteLockScreenAppInstall,
                                weak_ptr_factory_.GetWeakPtr(), install_count_,
                                tick_clock_->NowTicks()),
                     base::ThreadTaskRunnerHandle::Get())));
  return State::kActivating;
}

void AppManagerImpl::CompleteLockScreenAppInstall(
    int install_id,
    base::TimeTicks install_start_time,
    const scoped_refptr<const extensions::Extension>& app) {
  UMA_HISTOGRAM_TIMES(
      "Apps.LockScreen.NoteTakingApp.LockScreenInstallationDuration",
      tick_clock_->NowTicks() - install_start_time);

  // Bail out if the app manager is no longer waiting for this app's
  // installation - the copied resources will be cleaned up when the (ephemeral)
  // lock screen profile is destroyed.
  if (install_id != install_count_ || state_ != State::kActivating)
    return;

  if (app) {
    DCHECK_EQ(lock_screen_app_id_, app->id());
    InstallAndEnableLockScreenAppInLockScreenProfile(app.get());
    state_ = State::kActive;
  } else {
    state_ = State::kAppUnavailable;
  }

  if (!note_taking_changed_callback_.is_null())
    note_taking_changed_callback_.Run();
}

void AppManagerImpl::InstallAndEnableLockScreenAppInLockScreenProfile(
    const extensions::Extension* app) {
  extensions::ExtensionService* lock_screen_service =
      extensions::ExtensionSystem::Get(lock_screen_profile_)
          ->extension_service();

  lock_screen_service->OnExtensionInstalled(
      app, syncer::StringOrdinal(), extensions::kInstallFlagInstallImmediately);
  lock_screen_service->EnableExtension(app->id());

  available_lock_screen_app_reloads_ = kMaxLockScreenAppReloadsCount;

  lock_screen_profile_extensions_observer_.Add(
      extensions::ExtensionRegistry::Get(lock_screen_profile_));
}

void AppManagerImpl::RemoveAppFromLockScreenProfile(const std::string& app_id) {
  if (app_id.empty())
    return;

  lock_screen_profile_extensions_observer_.RemoveAll();

  extensions::ExtensionRegistry* lock_screen_registry =
      extensions::ExtensionRegistry::Get(lock_screen_profile_);
  if (!lock_screen_registry->GetExtensionById(
          app_id, extensions::ExtensionRegistry::EVERYTHING)) {
    return;
  }

  base::string16 error;
  extensions::ExtensionSystem::Get(lock_screen_profile_)
      ->extension_service()
      ->UninstallExtension(
          app_id, extensions::UNINSTALL_REASON_INTERNAL_MANAGEMENT, &error);
}

const extensions::Extension* AppManagerImpl::GetAppForLockScreenAppLaunch() {
  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(lock_screen_profile_);

  // Return the app, in case it's currently loaded.
  const extensions::Extension* app = extension_registry->GetExtensionById(
      lock_screen_app_id_, extensions::ExtensionRegistry::ENABLED);
  if (app) {
    ReportAppStatusOnAppLaunch(AppStatus::kEnabled);
    return app;
  }

  // If the app has been terminated (which can happen due to an app crash),
  // attempt a reload - otherwise, return nullptr to signal the app is
  // unavailable.
  app =
      extension_registry->terminated_extensions().GetByID(lock_screen_app_id_);
  if (!app) {
    ReportAppStatusOnAppLaunch(AppStatus::kNotLoadedNotTerminated);
    return nullptr;
  }

  if (available_lock_screen_app_reloads_ <= 0) {
    ReportAppStatusOnAppLaunch(AppStatus::kTerminatedReloadLimitExceeded);
    return nullptr;
  }

  available_lock_screen_app_reloads_--;

  std::string error;
  scoped_refptr<extensions::Extension> lock_profile_app =
      extensions::Extension::Create(app->path(), app->location(),
                                    *app->manifest()->value()->CreateDeepCopy(),
                                    app->creation_flags(), app->id(), &error);

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(lock_screen_profile_)
          ->extension_service();
  extension_service->AddExtension(lock_profile_app.get());
  extension_service->EnableExtension(lock_profile_app->id());

  app = extension_registry->GetExtensionById(
      lock_screen_app_id_, extensions::ExtensionRegistry::ENABLED);

  ReportAppStatusOnAppLaunch(app ? AppStatus::kAppReloaded
                                 : AppStatus::kAppReloadFailed);
  return app;
}

void AppManagerImpl::ReportAppStatusOnAppLaunch(AppStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Apps.LockScreen.NoteTakingApp.AppStatusOnNoteLaunch", status,
      AppStatus::kCount);
}

void AppManagerImpl::HandleLockScreenAppUnload(
    extensions::UnloadedExtensionReason reason) {
  if (state_ != State::kActive && state_ != State::kActivating)
    return;

  AppUnloadStatus status = AppUnloadStatus::kNotTerminated;
  if (reason == extensions::UnloadedExtensionReason::TERMINATE) {
    status = available_lock_screen_app_reloads_ > 0
                 ? AppUnloadStatus::kTerminatedReloadable
                 : AppUnloadStatus::kTerminatedReloadAttemptsExceeded;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "Apps.LockScreen.NoteTakingApp.LockScreenAppUnloaded", status,
      AppUnloadStatus::kCount);

  // If the app is terminated, it will be reloaded on the next app launch
  // request - if the app cannot be reloaded (e.g. if it was unloaded for a
  // different reason, or it was reloaded too many times already), change the
  // app managet to an error state. This will inform the app manager's user
  // that lock screen note action is not available anymore.
  if (status != AppUnloadStatus::kTerminatedReloadable)
    RemoveLockScreenAppDueToError();

  if (status != AppUnloadStatus::kNotTerminated) {
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.LockScreen.NoteTakingApp.ReloadCountOnAppTermination",
        kMaxLockScreenAppReloadsCount - available_lock_screen_app_reloads_,
        kMaxLockScreenAppReloadsCount + 1);
  }
}

void AppManagerImpl::RemoveLockScreenAppDueToError() {
  if (state_ != State::kActive && state_ != State::kActivating)
    return;

  RemoveAppFromLockScreenProfile(lock_screen_app_id_);
  lock_screen_app_id_.clear();
  state_ = State::kInactive;

  if (!note_taking_changed_callback_.is_null())
    note_taking_changed_callback_.Run();
}

}  // namespace lock_screen_apps
