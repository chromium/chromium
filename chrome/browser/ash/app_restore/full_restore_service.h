// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_SERVICE_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_SERVICE_H_

#include <memory>

#include "ash/public/cpp/accelerators.h"
#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash::full_restore {

class FullRestoreAppLaunchHandler;
class FullRestoreDataHandler;
class NewUserRestorePrefHandler;

extern const char kRestoreForCrashNotificationId[];
extern const char kRestoreNotificationId[];
extern const char kSetRestorePrefNotificationId[];

// The restore notification button index.
enum class RestoreNotificationButtonIndex {
  kRestore = 0,
  kCancel,
};

// This is used to record histograms, so do not remove or reorder existing
// entries.
enum class RestoreAction {
  kRestore = 0,
  kCancel = 1,
  kCloseByUser = 2,
  kCloseNotByUser = 3,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kCloseNotByUser,
};

// Returns true if FullRestoreService can be created to restore/launch Lacros
// during the system startup phase when all of the below conditions are met:
// 1. The FullRestoreForLacros flag is enabled.
// 2. Lacros is enabled.
// 3. FullRestoreService can be created for the primary profile.
bool MaybeCreateFullRestoreServiceForLacros();

// The FullRestoreService class calls AppService and Window Management
// interfaces to restore the app launchings and app windows.
class FullRestoreService : public KeyedService,
                           public message_center::NotificationObserver,
                           public AcceleratorController::Observer {
 public:
  static FullRestoreService* GetForProfile(Profile* profile);
  static void MaybeCloseNotification(Profile* profile);

  explicit FullRestoreService(Profile* profile);
  FullRestoreService(const FullRestoreService&) = delete;
  FullRestoreService& operator=(const FullRestoreService&) = delete;
  ~FullRestoreService() override;

  // Initialize the full restore service. |show_notification| indicates whether
  // a full restore notification has been shown.
  void Init(bool& show_notification);

  void OnTransitionedToNewActiveUser(Profile* profile);

  // Launches the browser, When the restore data is loaded, and the user chooses
  // to restore.
  void LaunchBrowserWhenReady();

  void MaybeCloseNotification(bool allow_save = true);

  // Implement the restoration.
  void Restore();

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

  // AcceleratorController::Observer:
  void OnActionPerformed(AcceleratorAction action) override;
  void OnAcceleratorControllerWillBeDestroyed(
      AcceleratorController* controller) override;

  FullRestoreAppLaunchHandler* app_launch_handler() {
    return app_launch_handler_.get();
  }

  void SetAppLaunchHandlerForTesting(
      std::unique_ptr<FullRestoreAppLaunchHandler> app_launch_handler);

 private:
  friend class FullRestoreServiceMultipleUsersTest;
  FRIEND_TEST_ALL_PREFIXES(FullRestoreAppLaunchHandlerChromeAppBrowserTest,
                           RestoreChromeApp);
  FRIEND_TEST_ALL_PREFIXES(FullRestoreAppLaunchHandlerArcAppBrowserTest,
                           RestoreArcApp);

  // KeyedService overrides.
  void Shutdown() override;

  // Returns true if `Init` can be called to show the notification or restore
  // apps. Otherwise, returns false.
  bool CanBeInited();

  // Show the restore notification on startup.
  void MaybeShowRestoreNotification(const std::string& id,
                                    bool& show_notification);

  void RecordRestoreAction(const std::string& notification_id,
                           RestoreAction restore_action);

  // Callback used when the pref |kRestoreAppsAndPagesPrefName| changes.
  void OnPreferenceChanged(const std::string& pref_name);

  // Returns true if there are some restore data and this is not the first time
  // Chrome is run. Otherwise, returns false.
  bool ShouldShowNotification();

  void OnAppTerminating();

  raw_ptr<Profile, ExperimentalAsh> profile_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;

  // If the user of `profile_` is not the primary user, and hasn't been the
  // active user yet, don't init to restore. Because if the restore setting is
  // 'Always', the app could be launched directly after restart, and the app
  // windows could be added to the primary user's profile path. This may cause
  // the non-primary user lost some restored windows data.
  //
  // If the non primary user becomes the active user, set `can_be_inited_` as
  // true to init and restore app. Otherwise, if `can_be_inited_` is false for
  // the non primary user, defer the init and app restoration.
  bool can_be_inited_ = false;

  bool is_shut_down_ = false;
  bool close_notification_ = false;

  // Specifies whether it is the first time to run the full restore feature.
  bool first_run_full_restore_ = false;

  // If the user clicks a notification button, set
  // |skip_notification_histogram_| as true to skip the notification close
  // histogram.
  bool skip_notification_histogram_ = false;

  std::unique_ptr<NewUserRestorePrefHandler> new_user_pref_handler_;

  // |app_launch_handler_| is responsible for launching apps based on the
  // restore data.
  std::unique_ptr<FullRestoreAppLaunchHandler> app_launch_handler_;

  std::unique_ptr<FullRestoreDataHandler> restore_data_handler_;

  std::unique_ptr<message_center::Notification> notification_;

  base::CallbackListSubscription on_app_terminating_subscription_;

  // Browser session restore exit type service lock. This is created when the
  // system is restored from crash to help set the browser saving flag.
  std::unique_ptr<ExitTypeService::CrashedLock> crashed_lock_;

  base::ScopedObservation<AcceleratorController,
                          AcceleratorController::Observer>
      accelerator_controller_observer_{this};

  base::WeakPtrFactory<FullRestoreService> weak_ptr_factory_{this};
};

class ScopedRestoreForTesting {
 public:
  ScopedRestoreForTesting();
  ScopedRestoreForTesting(const ScopedRestoreForTesting&) = delete;
  ScopedRestoreForTesting& operator=(const ScopedRestoreForTesting&) = delete;
  ~ScopedRestoreForTesting();
};

}  // namespace ash::full_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_FULL_RESTORE_SERVICE_H_
