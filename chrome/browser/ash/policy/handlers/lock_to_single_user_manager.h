// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_LOCK_TO_SINGLE_USER_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_LOCK_TO_SINGLE_USER_MANAGER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/guest_os/vm_starting_observer.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "components/user_manager/user_manager.h"

namespace policy {

// This class observes the UserManager session state, ArcSessionManager,
// CrostiniManager and PluginVmManager and checks if the device must be locked
// to a single user mount, if the policy forces it.
class LockToSingleUserManager final
    : public user_manager::UserManager::UserSessionStateObserver,
      public arc::ArcSessionManagerObserver,
      public ash::ConciergeClient::VmObserver,
      public ash::VmStartingObserver {
 public:
  static LockToSingleUserManager* GetLockToSingleUserManagerInstance();

  LockToSingleUserManager();

  LockToSingleUserManager(const LockToSingleUserManager&) = delete;
  LockToSingleUserManager& operator=(const LockToSingleUserManager&) = delete;

  ~LockToSingleUserManager() override;

  // Notify that a VM is being started from outside of Chrome
  void DbusNotifyVmStarting();

 private:
  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* user) override;

  // arc::ArcSessionManagerObserver:
  void OnArcStarted() override;

  // ConciergeClient::VmObserver:
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;

  // ash::VmStartingObserver:
  void OnVmStarting() override;

  // On affiliation established of the active user.
  void OnUserAffiliationEstablished(user_manager::User* user,
                                    bool is_affiliated);

  // Add observers for VM starting events
  void AddVmStartingObservers(user_manager::User* user);

  // Sends D-Bus request to lock device to single user mount.
  void LockToSingleUser();

  // Processes the response from D-Bus call.
  void OnLockToSingleUserMountUntilRebootDone(
      std::optional<user_data_auth::LockToSingleUserMountUntilRebootReply>
          reply);

  // true if locking is required when DbusNotifyVmStarting() is called
  bool lock_to_single_user_on_dbus_call_ = false;

  // true if it is expected that the device is already locked to a single user
  bool expect_to_be_locked_ = false;

  base::ScopedObservation<arc::ArcSessionManager,
                          arc::ArcSessionManagerObserver>
      arc_session_observation_{this};

  base::WeakPtrFactory<LockToSingleUserManager> weak_factory_{this};

  friend class LockToSingleUserManagerTest;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_LOCK_TO_SINGLE_USER_MANAGER_H_
