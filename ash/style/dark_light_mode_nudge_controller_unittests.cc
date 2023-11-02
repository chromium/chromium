// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/dark_light_mode_nudge_controller.h"

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/dark_mode/dark_mode_feature_pod_controller.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {

namespace {

const char kUser[] = "user@gmail.com";
const AccountId account_id = AccountId::FromUserEmailGaiaId(kUser, kUser);

}  // namespace

class DarkLightModeNudgeControllerTest : public NoSessionAshTestBase {
 public:
  DarkLightModeNudgeControllerTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kDarkLightMode);
  }
  DarkLightModeNudgeControllerTest(const DarkLightModeNudgeControllerTest&) =
      delete;
  DarkLightModeNudgeControllerTest& operator=(
      const DarkLightModeNudgeControllerTest&) = delete;
  ~DarkLightModeNudgeControllerTest() override = default;

  // NoSessionAshTestBase:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();
    Shell::Get()->dark_light_mode_controller()->SetShowNudgeForTesting(true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DarkLightModeNudgeControllerTest, NoNudgeInGuestSession) {
  SimulateGuestLogin();
  EXPECT_EQ(kDarkLightModeNudgeMaxShownCount,
            DarkLightModeNudgeController::GetRemainingShownCount());
}

TEST_F(DarkLightModeNudgeControllerTest, NoNudgeWhenSkippedByCommandLineFlag) {
  // Unit tests run with a scoped command line, so directly set the flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kAshNoNudges);
  SimulateUserLogin(account_id);
  EXPECT_EQ(kDarkLightModeNudgeMaxShownCount,
            DarkLightModeNudgeController::GetRemainingShownCount());
}

TEST_F(DarkLightModeNudgeControllerTest, NoNudgeInLockScreen) {
  SimulateUserLogin(account_id);
  EXPECT_EQ(kDarkLightModeNudgeMaxShownCount - 1,
            DarkLightModeNudgeController::GetRemainingShownCount());

  // Switch to lock screen should not show the nudge again.
  GetSessionControllerClient()->LockScreen();
  EXPECT_EQ(kDarkLightModeNudgeMaxShownCount - 1,
            DarkLightModeNudgeController::GetRemainingShownCount());
}

TEST_F(DarkLightModeNudgeControllerTest, NudgeShownCount) {
  // Login `kDarkLightModeNudgeMaxShownCount` times and verity the remaining
  // nudge shown count.
  for (int i = kDarkLightModeNudgeMaxShownCount; i > 0; i--) {
    SimulateUserLogin(account_id);
    EXPECT_EQ(i - 1, DarkLightModeNudgeController::GetRemainingShownCount());
    ClearLogin();
  }
  // The remaining nudge shown count should be 0 after
  // `kDarkLightModeNudgeMaxShownCount` times login, which means the nudge will
  // not be shown again in next time login.
  EXPECT_EQ(0, DarkLightModeNudgeController::GetRemainingShownCount());
}

// Flaky. https://crbug.com/1325224
TEST_F(DarkLightModeNudgeControllerTest,
       DISABLED_NoNudgeAfterColorModeToggled) {
  SimulateUserLogin(account_id);
  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  std::unique_ptr<DarkModeFeaturePodController>
      dark_mode_feature_pod_controller =
          std::make_unique<DarkModeFeaturePodController>(
              system_tray->bubble()->unified_system_tray_controller());

  EXPECT_EQ(kDarkLightModeNudgeMaxShownCount - 1,
            DarkLightModeNudgeController::GetRemainingShownCount());
  EXPECT_GT(DarkLightModeNudgeController::GetRemainingShownCount(), 0);
  // Toggle the "Dark theme" feature pod button inside quick settings.
  dark_mode_feature_pod_controller->CreateButton();
  dark_mode_feature_pod_controller->OnIconPressed();
  // The remaining nudge shown count should be 0 after toggling the "Dark theme"
  // feature pod button to switch color mode. Even though the nudge hasn't been
  // shown `kDarkLightModeNudgeMaxShownCount` yet.
  EXPECT_EQ(0, DarkLightModeNudgeController::GetRemainingShownCount());
}

}  // namespace ash
