// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_status_view.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/style/icon_button.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

namespace ash {

using PhoneStatusModel = phonehub::PhoneStatusModel;

class PhoneStatusViewTest : public AshTestBase,
                            public PhoneStatusView::Delegate {
 public:
  PhoneStatusViewTest() = default;
  ~PhoneStatusViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kPhoneHub);
    AshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    status_view_ = widget_->SetContentsView(
        std::make_unique<PhoneStatusView>(&phone_model_, this));
  }

  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  // PhoneStatusView::Delegate:
  bool CanOpenConnectedDeviceSettings() override {
    return can_open_connected_device_settings_;
  }

  void OpenConnectedDevicesSettings() override {
    connected_device_settings_opened_ = true;
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<PhoneStatusView, DanglingUntriaged> status_view_ = nullptr;
  phonehub::MutablePhoneModel phone_model_;
  base::test::ScopedFeatureList feature_list_;
  bool can_open_connected_device_settings_ = false;
  bool connected_device_settings_opened_ = false;
};

TEST_F(PhoneStatusViewTest, PhoneStatusLabelsContent) {
  std::u16string expected_name_text = u"Test Phone Name";
  std::u16string expected_provider_text = u"Test Provider";
  std::u16string expected_battery_text = u"10%";

  phone_model_.SetPhoneName(expected_name_text);

  PhoneStatusModel::MobileConnectionMetadata metadata = {
      .signal_strength = PhoneStatusModel::SignalStrength::kZeroBars,
      .mobile_provider = expected_provider_text,
  };
  auto phone_status =
      PhoneStatusModel(PhoneStatusModel::MobileStatus::kSimWithReception,
                       metadata, PhoneStatusModel::ChargingState::kNotCharging,
                       PhoneStatusModel::BatterySaverState::kOff, 10);
  phone_model_.SetPhoneStatusModel(phone_status);

  // All labels should display phone's status and information.
  EXPECT_EQ(expected_name_text, status_view_->phone_name_label_->GetText());
  EXPECT_EQ(expected_battery_text, status_view_->battery_label_->GetText());

  expected_name_text = u"New Phone Name";
  expected_provider_text = u"New Provider";
  expected_battery_text = u"20%";

  phone_model_.SetPhoneName(expected_name_text);
  metadata.mobile_provider = expected_provider_text;
  phone_status =
      PhoneStatusModel(PhoneStatusModel::MobileStatus::kSimWithReception,
                       metadata, PhoneStatusModel::ChargingState::kNotCharging,
                       PhoneStatusModel::BatterySaverState::kOff, 20);
  phone_model_.SetPhoneStatusModel(phone_status);

  // Changes in the model should be reflected in the labels.
  EXPECT_EQ(expected_name_text, status_view_->phone_name_label_->GetText());
  EXPECT_EQ(expected_battery_text, status_view_->battery_label_->GetText());

  // Simulate phone disconnected with a null |PhoneStatusModel| returned.
  phone_model_.SetPhoneStatusModel(std::nullopt);

  // Existing phone status will be cleared to reflect the model change.
  EXPECT_TRUE(status_view_->battery_label_->GetText().empty());
  EXPECT_TRUE(status_view_->battery_icon_->GetImage().isNull());
  EXPECT_TRUE(status_view_->signal_icon_->GetImage().isNull());
}

TEST_F(PhoneStatusViewTest, ClickOnSettings) {
  // The settings button is not visible if we can't open the settings.
  EXPECT_FALSE(status_view_->settings_button_->GetVisible());

  // The settings button is visible if we can open settings.
  can_open_connected_device_settings_ = true;
  status_view_ = widget_->SetContentsView(
      std::make_unique<PhoneStatusView>(&phone_model_, this));
  EXPECT_TRUE(status_view_->settings_button_->GetVisible());

  // Click on the settings button.
  views::test::ButtonTestApi(status_view_->settings_button_)
      .NotifyClick(ui::test::TestEvent());
  EXPECT_TRUE(connected_device_settings_opened_);
}

}  // namespace ash
