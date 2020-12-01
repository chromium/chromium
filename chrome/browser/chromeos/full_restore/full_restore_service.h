// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace chromeos {
namespace full_restore {

// The FullRestoreService class calls AppService and Window Management
// interfaces to restore the app launchings and app windows.
//
// 1. If the system is recovered from the crash, creates the notification to let
// the user select restore or not.
// 2. For normal reboot, read the restore setting fromt the user pref, and based
// on the setting to decide restore or not.
//
// TODO(crbug.com/909794):
// 1. If the system crashed before reboot, show the notification notification.
// Otherwise, read |kRestoreAppsAndPagesPrefName|
// 2. Observe the AppRegistryCache to read the app info, and restore apps and
// app windows.
class FullRestoreService : public KeyedService {
 public:
  explicit FullRestoreService(Profile* profile);
  ~FullRestoreService() override;

  FullRestoreService(const FullRestoreService&) = delete;
  FullRestoreService& operator=(const FullRestoreService&) = delete;

 private:
  // KeyedService overrides.
  void Shutdown() override;

  // Show the restore notification on startup.
  void ShowRestoreNotification(const std::string& id);

  void HandleRestoreNotificationClicked(const std::string& id,
                                        base::Optional<int> button_index);

  Profile* profile_ = nullptr;

  bool is_shut_down_ = false;

  base::WeakPtrFactory<FullRestoreService> weak_ptr_factory_{this};
};

}  // namespace full_restore
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_SERVICE_H_
