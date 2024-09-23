// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/system/power/power_event_observer_test_api.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/chromeos/crosier/power_manager_emitter.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/user_manager/user_names.h"

namespace {

using LockScreen = AshIntegrationTest;

}  // namespace

// Tests that dbus messages for lid close trigger screen lock. This only tests
// the "lock on lid close" pref state because it's difficult to test the inverse
// (that the screen didn't lock) without a long timeout. The pref behavior is
// tested separately in the CloseLidPref test below.
//
// Bug Component b:1207311
//     (ChromeOS > Software > Commercial (Enterprise) > Identity > LURS)
// Contacts:
//     cros-lurs@google.com
//     antrim@chromium.org
//     chromeos-sw-engprod@google.com
//     cros-exp-wg+testresults@google.com (for fieldtrial_testing_config)
IN_PROC_BROWSER_TEST_F(LockScreen, CloseLidDbusIntegration) {
  PowerManagerEmitter emitter;
  ash::ScreenLockerTester locker_tester;

  auto* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);
  PrefService* prefs = profile->GetPrefs();
  ASSERT_TRUE(prefs);

  // Set to lock when the lid is closed.
  prefs->SetBoolean(ash::prefs::kEnableAutoScreenLock, true);

  // This should lock the screen.
  emitter.EmitInputEvent(power_manager::InputEvent_Type_LID_CLOSED);
  locker_tester.WaitForLock();
  ASSERT_TRUE(locker_tester.IsLocked());

  // Unlock.
  const std::string kPassword = "password";
  locker_tester.SetUnlockPassword(user_manager::StubAccountId(), kPassword);
  locker_tester.UnlockWithPassword(user_manager::StubAccountId(), kPassword);
  locker_tester.WaitForUnlock();
  EXPECT_FALSE(locker_tester.IsLocked());
}

// Tests that the preference for screen lock on lid close is hooked up. This
// test is less end-to-end than the previous one so there are no asynchronous
// dbus events that we need to deal with:
//
//  • It injects a "lid close" event directly into the PowerEventObserver,
//    bypassing dbus on the way in.
//
//  • It observes whether the screen was locked by watching for
//    SessionManagerClient::RequestLockScreen calls. These calls normally
//    trigger a dbus call to lock the screen, the success of which then triggers
//    the UI. Watching here using the fake SessionManagerClient lets us see
//    these calls after all the prefs are checked, but before the async dbus
//    gets involved.
IN_PROC_BROWSER_TEST_F(LockScreen, CloseLidPref) {
  ash::PowerEventObserverTestApi test_api(
      ash::Shell::Get()->power_event_observer());

  auto* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);
  PrefService* prefs = profile->GetPrefs();
  ASSERT_TRUE(prefs);

  // Set to not lock the screen when the lid is closed.
  prefs->SetBoolean(ash::prefs::kEnableAutoScreenLock, false);

  // Install the fake to catch the dbus requests.
  ash::SessionManagerClient::InitializeFakeInMemory();
  ASSERT_EQ(
      0,
      ash::FakeSessionManagerClient::Get()->request_lock_screen_call_count());

  // This should NOT lock the screen.
  test_api.SendLidEvent(chromeos::PowerManagerClient::LidState::CLOSED);
  EXPECT_EQ(
      0,
      ash::FakeSessionManagerClient::Get()->request_lock_screen_call_count());

  // Reset the lid state.
  test_api.SendLidEvent(chromeos::PowerManagerClient::LidState::OPEN);

  // Set to lock the screen when the lid is closed.
  prefs->SetBoolean(ash::prefs::kEnableAutoScreenLock, true);

  // This should lock the screen.
  test_api.SendLidEvent(chromeos::PowerManagerClient::LidState::CLOSED);
  EXPECT_EQ(
      1,
      ash::FakeSessionManagerClient::Get()->request_lock_screen_call_count());
}
