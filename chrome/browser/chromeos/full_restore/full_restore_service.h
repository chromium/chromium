// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace message_center {
class Notification;
}

namespace chromeos {
namespace full_restore {

class AppLaunchHandler;
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

// The FullRestoreService class calls AppService and Window Management
// interfaces to restore the app launchings and app windows.
class FullRestoreService : public KeyedService,
                           public message_center::NotificationObserver {
 public:
  static FullRestoreService* GetForProfile(Profile* profile);

  explicit FullRestoreService(Profile* profile);
  ~FullRestoreService() override;

  FullRestoreService(const FullRestoreService&) = delete;
  FullRestoreService& operator=(const FullRestoreService&) = delete;

  // Launches the browser, When the restore data is loaded, and the user chooses
  // to restore.
  void LaunchBrowserWhenReady();

  // message_center::NotificationObserver:
  void Close(bool by_user) override;
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

  void RestoreForTesting();

 private:
  void Init();

  // KeyedService overrides.
  void Shutdown() override;

  // Show the restore notification on startup.
  void ShowRestoreNotification(const std::string& id);

  // Implement the restoration.
  void Restore();

  void RecordRestoreAction(const std::string& notification_id,
                           RestoreAction restore_action);

  // Callback used when the pref |kRestoreAppsAndPagesPrefName| changes.
  void OnPreferenceChanged(const std::string& pref_name);

  Profile* profile_ = nullptr;
  PrefChangeRegistrar pref_change_registrar_;

  bool is_shut_down_ = false;

  // If the user clicks a notification button, set
  // |skip_notification_histogram_| as true to skip the notification close
  // histogram.
  bool skip_notification_histogram_ = false;

  std::unique_ptr<NewUserRestorePrefHandler> new_user_pref_handler_;

  // |app_launch_handler_| is responsible for launching apps based on the
  // restore data.
  std::unique_ptr<AppLaunchHandler> app_launch_handler_;

  std::unique_ptr<FullRestoreDataHandler> restore_data_handler_;

  std::unique_ptr<message_center::Notification> notification_;

  base::WeakPtrFactory<FullRestoreService> weak_ptr_factory_{this};
};

}  // namespace full_restore
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
namespace full_restore {
using ::chromeos::full_restore::FullRestoreService;
}
}  // namespace ash

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_H_
