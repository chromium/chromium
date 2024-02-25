// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/system/unified/quick_settings_footer.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user_type.h"

namespace ash {

// Pixel tests for the quick settings footer.
class QuickSettingsFooterPixelTest : public AshTestBase {
 public:
  QuickSettingsFooterPixelTest() {
    feature_list_.InitWithFeatures({chromeos::features::kJelly},
                                   {features::kAdaptiveCharging});
  }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    chromeos::PowerManagerClient::Shutdown();
  }

  void InitPowerStatusAndOpenBubble() {
    power_manager::PowerSupplyProperties prop;
    prop.set_external_power(power_manager::PowerSupplyProperties::AC);
    prop.set_battery_state(power_manager::PowerSupplyProperties::CHARGING);
    prop.set_battery_time_to_full_sec(6000);
    prop.set_battery_percent(30.0);
    PowerStatus::Get()->SetProtoForTesting(prop);

    UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
    system_tray->ShowBubble();
    footer_ =
        system_tray->bubble()->quick_settings_view()->footer_for_testing();
  }

  void CloseBubble() { GetPrimaryUnifiedSystemTray()->CloseBubble(); }

 protected:
  QuickSettingsFooter* GetFooter() { return footer_; }

 private:
  base::test::ScopedFeatureList feature_list_;

  // Owned by view hierarchy.
  raw_ptr<QuickSettingsFooter, DanglingUntriaged> footer_ = nullptr;
};

TEST_F(QuickSettingsFooterPixelTest, FooterShouldBeRenderedCorrectly) {
  InitPowerStatusAndOpenBubble();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "with_no_extra_button",
      /*revision_number=*/6, GetFooter()));
  CloseBubble();

  // Regression test for b/293484037: The settings button is missing when
  // there's no enough space for the battery label.
  SimulateUserLogin("test@gmail.com", user_manager::UserType::kPublicAccount);
  InitPowerStatusAndOpenBubble();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "with_exit_button",
      /*revision_number=*/6, GetFooter()));
  CloseBubble();
}

}  // namespace ash
