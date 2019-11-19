// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_LOCK_TO_SINGLE_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_LOCK_TO_SINGLE_USER_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/vm_starting_observer.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "components/user_manager/user_manager.h"

namespace policy {

// This class observes the UserManager session state, ArcSessionManager,
// CrostiniManager and PluginVmManager and checks if the device must be locked
// to a single user mount, if the policy forces it.
class LockToSingleUserManager final
    : public user_manager::UserManager::UserSessionStateObserver,
      public arc::ArcSessionManager::Observer,
      public chromeos::ConciergeClient::VmObserver,
      public chromeos::VmStartingObserver {
 public:
  static LockToSingleUserManager* GetLockToSingleUserManagerInstance();

  LockToSingleUserManager();
  ~LockToSingleUserManager() override;

  // Notify that a VM is being started from outside of Chrome
  void DbusNotifyVmStarting();

 private:
  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* user) override;

  // arc::ArcSessionManager::Observer:
  void OnArcStarted() override;

  // ConciergeClient::VmObserver:
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;

  // chromeos::VmStartingObserver:
  void OnVmStarting() override;

  // Add observers for VM starting events
  void AddVmStartingObservers(user_manager::User* user);

  // Sends D-Bus request to lock device to single user mount.
  void LockToSingleUser();

  // Processes the response from D-Bus call.
  void OnLockToSingleUserMountUntilRebootDone(
      base::Optional<cryptohome::BaseReply> reply);

  // true if locking is required when DbusNotifyVmStarting() is called
  bool lock_to_single_user_on_dbus_call_ = false;

  // true if it is expected that the device is already locked to a single user
  bool expect_to_be_locked_ = false;

  ScopedObserver<arc::ArcSessionManager, arc::ArcSessionManager::Observer>
      arc_session_observer_{this};

  base::WeakPtrFactory<LockToSingleUserManager> weak_factory_{this};

  friend class LockToSingleUserManagerTest;

  DISALLOW_COPY_AND_ASSIGN(LockToSingleUserManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_LOCK_TO_SINGLE_USER_MANAGER_H_
