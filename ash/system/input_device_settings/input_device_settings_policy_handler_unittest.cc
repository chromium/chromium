// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_policy_handler.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class InputDeviceSettingsPolicyHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    num_times_keyboard_policies_changed = 0;
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    handler_ =
        std::make_unique<InputDeviceSettingsPolicyHandler>(base::BindRepeating(
            &InputDeviceSettingsPolicyHandlerTest::OnKeyboardPoliciesChanged,
            base::Unretained(this)));

    pref_service_->registry()->RegisterBooleanPref(prefs::kSendFunctionKeys,
                                                   false);
  }

  void OnKeyboardPoliciesChanged() { num_times_keyboard_policies_changed++; }

 protected:
  int num_times_keyboard_policies_changed = 0;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<InputDeviceSettingsPolicyHandler> handler_;
};

TEST_F(InputDeviceSettingsPolicyHandlerTest, KeyboardNoPolicy) {
  handler_->Initialize(pref_service_.get());
  EXPECT_FALSE(handler_->keyboard_policies().top_row_are_fkeys_policy);
}

TEST_F(InputDeviceSettingsPolicyHandlerTest, KeyboardManagedPolicy) {
  pref_service_->SetManagedPref(prefs::kSendFunctionKeys, base::Value(false));
  handler_->Initialize(pref_service_.get());

  EXPECT_EQ(
      mojom::PolicyStatus::kManaged,
      handler_->keyboard_policies().top_row_are_fkeys_policy->policy_status);
  EXPECT_FALSE(handler_->keyboard_policies().top_row_are_fkeys_policy->value);
  EXPECT_EQ(0, num_times_keyboard_policies_changed);

  pref_service_->SetManagedPref(prefs::kSendFunctionKeys, base::Value(true));
  EXPECT_EQ(
      mojom::PolicyStatus::kManaged,
      handler_->keyboard_policies().top_row_are_fkeys_policy->policy_status);
  EXPECT_TRUE(handler_->keyboard_policies().top_row_are_fkeys_policy->value);
  EXPECT_EQ(1, num_times_keyboard_policies_changed);
}

TEST_F(InputDeviceSettingsPolicyHandlerTest, KeyboardRecommendedPolicy) {
  pref_service_->SetRecommendedPref(prefs::kSendFunctionKeys,
                                    base::Value(false));
  handler_->Initialize(pref_service_.get());

  EXPECT_EQ(
      mojom::PolicyStatus::kRecommended,
      handler_->keyboard_policies().top_row_are_fkeys_policy->policy_status);
  EXPECT_FALSE(handler_->keyboard_policies().top_row_are_fkeys_policy->value);
  EXPECT_EQ(0, num_times_keyboard_policies_changed);

  pref_service_->SetRecommendedPref(prefs::kSendFunctionKeys,
                                    base::Value(true));
  EXPECT_EQ(
      mojom::PolicyStatus::kRecommended,
      handler_->keyboard_policies().top_row_are_fkeys_policy->policy_status);
  EXPECT_TRUE(handler_->keyboard_policies().top_row_are_fkeys_policy->value);
  EXPECT_EQ(1, num_times_keyboard_policies_changed);
}

}  // namespace ash
