// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_LOCKER_TESTER_H_
#define CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_LOCKER_TESTER_H_

#include <string>

#include "base/memory/raw_ref.h"

class AccountId;

namespace ash {

class FakeSessionManagerClient;
class ScreenLockerController;

// ScreenLockerTester provides a high-level API to test the lock screen.
// Must be created after the SessionManager is initialized.
class ScreenLockerTester {
 public:
  // RAII object to set/reset a fake callback for
  // FakeSessionManagerClient::RequestLockScreen. In production,
  // `RequestLockScreen` sends a request to the session_manager daemon, then it
  // will call back into `ScreenLockServiceProvider`. This class enables the
  // plumbing for browser tests.
  class ScopedRequestLockScreenOverride {
   public:
    // FakeSessionManagerClient and ScreenLockerController must outlive this
    // instance.
    ScopedRequestLockScreenOverride();
    ScopedRequestLockScreenOverride(const ScopedRequestLockScreenOverride&) =
        delete;
    ScopedRequestLockScreenOverride& operator=(
        const ScopedRequestLockScreenOverride&) = delete;
    ~ScopedRequestLockScreenOverride();

   private:
    const raw_ref<FakeSessionManagerClient> fake_session_manager_client_;
    const raw_ref<ScreenLockerController> screen_locker_controller_;
  };

  ScreenLockerTester();
  ScreenLockerTester(const ScreenLockerTester&) = delete;
  ScreenLockerTester& operator=(const ScreenLockerTester&) = delete;
  ~ScreenLockerTester();

  // Synchronously lock the device.
  // Session state must be ACTIVE before calling this.
  void Lock();

  // Not necessary when using Lock() because it does this internally, this is
  // used when triggering a lock via some other means.
  void WaitForLock();

  void WaitForUnlock();

  // Injects authenticators that only authenticate with the given password.
  void SetUnlockPassword(const AccountId& account_id,
                         const std::string& password);

  // Returns true if the screen is locked.
  bool IsLocked();

  // Returns true if Restart button is visible.
  bool IsLockRestartButtonShown();

  // Returns true if Shutdown button is visible.
  bool IsLockShutdownButtonShown();

  // Enters and submits the given password for the given account. This does not
  // wait for the unlock to complete, call WaitForUnlock() to synchronize.
  void UnlockWithPassword(const AccountId& account_id,
                          const std::string& password);

  // Same as UnlockWithPassword but submits even if the password auth disabled.
  void ForceSubmitPassword(const AccountId& account_id,
                           const std::string& password);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOCK_SCREEN_LOCKER_TESTER_H_
