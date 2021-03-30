// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"

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

// The FullRestoreService class calls AppService and Window Management
// interfaces to restore the app launchings and app windows.
class FullRestoreService : public KeyedService {
 public:
  static FullRestoreService* GetForProfile(Profile* profile);

  explicit FullRestoreService(Profile* profile);
  ~FullRestoreService() override;

  FullRestoreService(const FullRestoreService&) = delete;
  FullRestoreService& operator=(const FullRestoreService&) = delete;

  // Launches the browser, When the restore data is loaded, and the user chooses
  // to restore.
  void LaunchBrowserWhenReady();

  void RestoreForTesting();

 private:
  void Init();

  // KeyedService overrides.
  void Shutdown() override;

  // Show the restore notification on startup.
  void ShowRestoreNotification(const std::string& id);

  void HandleRestoreNotificationClicked(base::Optional<int> button_index);

  // Implement the restoration.
  void Restore();

  Profile* profile_ = nullptr;

  bool is_shut_down_ = false;

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

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_H_
