// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_status_view.h"

#include "ash/test/ash_test_base.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/phonehub/mutable_phone_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

using PhoneStatusModel = chromeos::phonehub::PhoneStatusModel;

class PhoneStatusViewTest : public AshTestBase {
 public:
  PhoneStatusViewTest() = default;
  ~PhoneStatusViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(chromeos::features::kPhoneHub);
    AshTestBase::SetUp();

    phone_status_view_ = std::make_unique<PhoneStatusView>(&phone_model_);
  }

  void TearDown() override {
    phone_status_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  PhoneStatusView* status_view() { return phone_status_view_.get(); }
  chromeos::phonehub::MutablePhoneModel* phone_model() { return &phone_model_; }

 private:
  std::unique_ptr<PhoneStatusView> phone_status_view_;
  chromeos::phonehub::MutablePhoneModel phone_model_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PhoneStatusViewTest, MobileProviderVisibility) {
  PhoneStatusModel::MobileConnectionMetadata metadata = {
      .signal_strength = PhoneStatusModel::SignalStrength::kZeroBars,
      .mobile_provider = base::UTF8ToUTF16("Test Provider"),
  };
  auto phone_status =
      PhoneStatusModel(PhoneStatusModel::MobileStatus::kNoSim, metadata,
                       PhoneStatusModel::ChargingState::kNotCharging,
                       PhoneStatusModel::BatterySaverState::kOff, 0);
  phone_model()->SetPhoneStatusModel(phone_status);
  // Mobile provider should not be shown when there is no sim.
  EXPECT_FALSE(status_view()->mobile_provider_label_->GetVisible());

  phone_status =
      PhoneStatusModel(PhoneStatusModel::MobileStatus::kSimButNoReception,
                       metadata, PhoneStatusModel::ChargingState::kNotCharging,
                       PhoneStatusModel::BatterySaverState::kOff, 0);
  phone_model()->SetPhoneStatusModel(phone_status);
  // Mobile provider should not be shown when there is no connection.
  EXPECT_FALSE(status_view()->mobile_provider_label_->GetVisible());

  phone_status =
      PhoneStatusModel(PhoneStatusModel::MobileStatus::kSimWithReception,
                       metadata, PhoneStatusModel::ChargingState::kNotCharging,
                       PhoneStatusModel::BatterySaverState::kOff, 0);
  phone_model()->SetPhoneStatusModel(phone_status);
  // Mobile provider should be shown when there is a connection.
  EXPECT_TRUE(status_view()->mobile_provider_label_->GetVisible());
}

TEST_F(PhoneStatusViewTest, PhoneStatusLabelsContent) {
  base::string16 expected_name_text = base::UTF8ToUTF16("Test Phone Name");
  base::string16 expected_provider_text = base::UTF8ToUTF16("Test Provider");
  base::string16 expected_battery_text = base::UTF8ToUTF16("10%");

  phone_model()->SetPhoneName(expected_name_text);

  PhoneStatusModel::MobileConnectionMetadata metadata = {
      .signal_strength = PhoneStatusModel::SignalStrength::kZeroBars,
      .mobile_provider = expected_provider_text,
  };
  auto phone_status =
      PhoneStatusModel(PhoneStatusModel::MobileStatus::kSimWithReception,
                       metadata, PhoneStatusModel::ChargingState::kNotCharging,
                       PhoneStatusModel::BatterySaverState::kOff, 10);
  phone_model()->SetPhoneStatusModel(phone_status);

  // All labels should display phone's status and information.
  EXPECT_EQ(expected_name_text, status_view()->phone_name_label_->GetText());
  EXPECT_EQ(expected_provider_text,
            status_view()->mobile_provider_label_->GetText());
  EXPECT_EQ(expected_battery_text, status_view()->battery_label_->GetText());

  expected_name_text = base::UTF8ToUTF16("New Phone Name");
  expected_provider_text = base::UTF8ToUTF16("New Provider");
  expected_battery_text = base::UTF8ToUTF16("20%");

  phone_model()->SetPhoneName(expected_name_text);
  metadata.mobile_provider = expected_provider_text;
  phone_status =
      PhoneStatusModel(PhoneStatusModel::MobileStatus::kSimWithReception,
                       metadata, PhoneStatusModel::ChargingState::kNotCharging,
                       PhoneStatusModel::BatterySaverState::kOff, 20);
  phone_model()->SetPhoneStatusModel(phone_status);

  // Changes in the model should be reflected in the labels.
  EXPECT_EQ(expected_name_text, status_view()->phone_name_label_->GetText());
  EXPECT_EQ(expected_provider_text,
            status_view()->mobile_provider_label_->GetText());
  EXPECT_EQ(expected_battery_text, status_view()->battery_label_->GetText());

  // Simulate phone disconnected with a null |PhoneStatusModel| returned.
  phone_model()->SetPhoneStatusModel(base::nullopt);

  // Existing phone status will be cleared to reflect the model change.
  EXPECT_TRUE(status_view()->mobile_provider_label_->GetText().empty());
  EXPECT_TRUE(status_view()->battery_label_->GetText().empty());
  EXPECT_TRUE(status_view()->battery_icon_->GetImage().isNull());
  EXPECT_TRUE(status_view()->signal_icon_->GetImage().isNull());
}

}  // namespace ash
