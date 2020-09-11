// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_tray.h"

#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/events/event.h"

namespace ash {

class PhoneHubTrayTest : public AshTestBase {
 public:
  PhoneHubTrayTest() = default;
  ~PhoneHubTrayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(chromeos::features::kPhoneHub);
    AshTestBase::SetUp();

    phone_hub_tray_ =
        StatusAreaWidgetTestHelper::GetStatusAreaWidget()->phone_hub_tray();

    GetFeatureStatusProvider()->SetStatus(
        chromeos::phonehub::FeatureStatus::kEnabledAndConnected);
    phone_hub_tray_->SetPhoneHubManager(&phone_hub_manager_);
  }

  chromeos::phonehub::FakeFeatureStatusProvider* GetFeatureStatusProvider() {
    return phone_hub_manager_.fake_feature_status_provider();
  }

  void ClickTrayButton() {
    ui::GestureEvent tap(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_TAP));
    phone_hub_tray_->PerformAction(tap);
  }

 protected:
  PhoneHubTray* phone_hub_tray_ = nullptr;
  chromeos::phonehub::FakePhoneHubManager phone_hub_manager_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PhoneHubTrayTest, SetPhoneHubManager) {
  // Set a new manager.
  chromeos::phonehub::FakePhoneHubManager new_manager;
  new_manager.fake_feature_status_provider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  phone_hub_tray_->SetPhoneHubManager(&new_manager);
  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  // Changing the old manager should have no effect.
  GetFeatureStatusProvider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kDisabled);
  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  // Only the new manager should work.
  new_manager.fake_feature_status_provider()->SetStatus(
      chromeos::phonehub::FeatureStatus::kDisabled);
  EXPECT_FALSE(phone_hub_tray_->GetVisible());

  // Set no manager.
  phone_hub_tray_->SetPhoneHubManager(nullptr);
  EXPECT_FALSE(phone_hub_tray_->GetVisible());
}

TEST_F(PhoneHubTrayTest, ClickTrayButton) {
  EXPECT_TRUE(phone_hub_tray_->GetVisible());

  EXPECT_FALSE(phone_hub_tray_->is_active());

  ClickTrayButton();
  EXPECT_TRUE(phone_hub_tray_->is_active());

  ClickTrayButton();
  EXPECT_FALSE(phone_hub_tray_->is_active());
}

}  // namespace ash
