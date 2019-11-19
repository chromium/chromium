// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_devices_controller.h"

#include "ash/accelerators/debug_commands.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@test.com";
constexpr char kUser2Email[] = "user2@test.com";

bool GetUserPrefTouchpadEnabled() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return prefs && prefs->GetBoolean(prefs::kTouchpadEnabled);
}

bool GetGlobalTouchpadEnabled() {
  return Shell::Get()->touch_devices_controller()->GetTouchpadEnabled(
      TouchDeviceEnabledSource::GLOBAL);
}

bool GetUserPrefTouchscreenEnabled() {
  return Shell::Get()->touch_devices_controller()->GetTouchscreenEnabled(
      TouchDeviceEnabledSource::USER_PREF);
}

bool GetGlobalTouchscreenEnabled() {
  return Shell::Get()->touch_devices_controller()->GetTouchscreenEnabled(
      TouchDeviceEnabledSource::GLOBAL);
}

void SetTapDraggingEnabled(bool enabled) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetBoolean(prefs::kTapDraggingEnabled, enabled);
  prefs->CommitPendingWrite();
}

class TouchDevicesControllerSigninTest : public NoSessionAshTestBase {
 public:
  TouchDevicesControllerSigninTest() = default;
  ~TouchDevicesControllerSigninTest() override = default;

  // NoSessionAshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshDebugShortcuts);
    NoSessionAshTestBase::SetUp();
    CreateTestUserSessions();

    // Simulate user 1 login.
    SwitchActiveUser(kUser1Email);

    ASSERT_TRUE(debug::DebugAcceleratorsEnabled());
  }

  void CreateTestUserSessions() {
    GetSessionControllerClient()->Reset();
    GetSessionControllerClient()->AddUserSession(kUser1Email);
    GetSessionControllerClient()->AddUserSession(kUser2Email);
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchDevicesControllerSigninTest);
};

TEST_F(TouchDevicesControllerSigninTest, PrefsAreRegistered) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  EXPECT_TRUE(prefs->FindPreference(prefs::kTapDraggingEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kTouchpadEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kTouchscreenEnabled));
}

TEST_F(TouchDevicesControllerSigninTest, SetTapDraggingEnabled) {
  auto* controller = Shell::Get()->touch_devices_controller();
  ASSERT_FALSE(controller->tap_dragging_enabled_for_test());
  SetTapDraggingEnabled(true);
  EXPECT_TRUE(controller->tap_dragging_enabled_for_test());

  // Switch to user 2 and switch back.
  SwitchActiveUser(kUser2Email);
  EXPECT_FALSE(controller->tap_dragging_enabled_for_test());
  SwitchActiveUser(kUser1Email);
  EXPECT_TRUE(controller->tap_dragging_enabled_for_test());

  SetTapDraggingEnabled(false);
  EXPECT_FALSE(controller->tap_dragging_enabled_for_test());
}

// Tests that touchpad enabled user pref works properly under debug accelerator.
TEST_F(TouchDevicesControllerSigninTest, ToggleTouchpad) {
  ASSERT_TRUE(GetUserPrefTouchpadEnabled());
  debug::PerformDebugActionIfEnabled(DEBUG_TOGGLE_TOUCH_PAD);
  EXPECT_FALSE(GetUserPrefTouchpadEnabled());

  // Switch to user 2 and switch back.
  SwitchActiveUser(kUser2Email);
  EXPECT_TRUE(GetUserPrefTouchpadEnabled());
  SwitchActiveUser(kUser1Email);
  EXPECT_FALSE(GetUserPrefTouchpadEnabled());

  debug::PerformDebugActionIfEnabled(DEBUG_TOGGLE_TOUCH_PAD);
  EXPECT_TRUE(GetUserPrefTouchpadEnabled());
}

TEST_F(TouchDevicesControllerSigninTest, SetTouchpadEnabled) {
  ASSERT_TRUE(GetUserPrefTouchpadEnabled());
  ASSERT_TRUE(GetGlobalTouchpadEnabled());

  Shell::Get()->touch_devices_controller()->SetTouchpadEnabled(
      false, TouchDeviceEnabledSource::GLOBAL);
  ASSERT_TRUE(GetUserPrefTouchpadEnabled());
  ASSERT_FALSE(GetGlobalTouchpadEnabled());

  Shell::Get()->touch_devices_controller()->SetTouchpadEnabled(
      false, TouchDeviceEnabledSource::USER_PREF);
  ASSERT_FALSE(GetUserPrefTouchpadEnabled());
  ASSERT_FALSE(GetGlobalTouchpadEnabled());

  Shell::Get()->touch_devices_controller()->SetTouchpadEnabled(
      true, TouchDeviceEnabledSource::GLOBAL);
  ASSERT_FALSE(GetUserPrefTouchpadEnabled());
  ASSERT_TRUE(GetGlobalTouchpadEnabled());
}

// Tests that touchscreen enabled user pref works properly under debug
// accelerator.
TEST_F(TouchDevicesControllerSigninTest, SetTouchscreenEnabled) {
  ASSERT_TRUE(GetGlobalTouchscreenEnabled());
  ASSERT_TRUE(GetUserPrefTouchscreenEnabled());

  debug::PerformDebugActionIfEnabled(DEBUG_TOGGLE_TOUCH_SCREEN);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
  EXPECT_FALSE(GetUserPrefTouchscreenEnabled());

  // Switch to user 2 and switch back.
  SwitchActiveUser(kUser2Email);
  EXPECT_TRUE(GetUserPrefTouchscreenEnabled());
  SwitchActiveUser(kUser1Email);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
  EXPECT_FALSE(GetUserPrefTouchscreenEnabled());

  debug::PerformDebugActionIfEnabled(DEBUG_TOGGLE_TOUCH_SCREEN);
  EXPECT_TRUE(GetUserPrefTouchscreenEnabled());
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());

  // The global setting should be preserved when switching users.
  Shell::Get()->touch_devices_controller()->SetTouchscreenEnabled(
      false, TouchDeviceEnabledSource::GLOBAL);
  EXPECT_FALSE(GetGlobalTouchscreenEnabled());
  SwitchActiveUser(kUser2Email);
  EXPECT_FALSE(GetGlobalTouchscreenEnabled());
}

using TouchDevicesControllerPrefsTest = NoSessionAshTestBase;

// Tests that "Touchpad.TapDragging.Started" is recorded on user session added
// and pref service is ready and "Touchpad.TapDragging.Changed" is recorded each
// time pref changes.
TEST_F(TouchDevicesControllerPrefsTest, RecordUma) {
  auto* controller = Shell::Get()->touch_devices_controller();
  ASSERT_FALSE(controller->tap_dragging_enabled_for_test());

  TestSessionControllerClient* session = GetSessionControllerClient();
  // Disable auto-provision of PrefService.
  constexpr bool kEnableSettings = true;
  constexpr bool kProvidePrefService = false;
  // Add and switch to |kUser1Email|, but user pref service is not ready.
  session->AddUserSession(kUser1Email, user_manager::USER_TYPE_REGULAR,
                          kEnableSettings, kProvidePrefService);
  const AccountId kUserAccount1 = AccountId::FromUserEmail(kUser1Email);
  session->SwitchActiveUser(kUserAccount1);

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Touchpad.TapDragging.Started", 0);
  histogram_tester.ExpectTotalCount("Touchpad.TapDragging.Changed", 0);

  // Simulate active user pref service is changed.
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(pref_service->registry(), true /* for_test */);
  GetSessionControllerClient()->SetUserPrefService(kUserAccount1,
                                                   std::move(pref_service));
  histogram_tester.ExpectTotalCount("Touchpad.TapDragging.Started", 1);
  histogram_tester.ExpectTotalCount("Touchpad.TapDragging.Changed", 0);

  EXPECT_FALSE(controller->tap_dragging_enabled_for_test());
  SetTapDraggingEnabled(true);
  histogram_tester.ExpectTotalCount("Touchpad.TapDragging.Started", 1);
  histogram_tester.ExpectTotalCount("Touchpad.TapDragging.Changed", 1);
}

}  // namespace
}  // namespace ash
