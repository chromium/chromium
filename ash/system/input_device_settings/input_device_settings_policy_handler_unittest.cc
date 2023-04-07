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
    num_times_keyboard_policies_changed_ = 0;
    num_times_mouse_policies_changed_ = 0;
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    handler_ = std::make_unique<InputDeviceSettingsPolicyHandler>(
        base::BindRepeating(
            &InputDeviceSettingsPolicyHandlerTest::OnKeyboardPoliciesChanged,
            base::Unretained(this)),
        base::BindRepeating(
            &InputDeviceSettingsPolicyHandlerTest::OnMousePoliciesChanged,
            base::Unretained(this)));

    pref_service_->registry()->RegisterBooleanPref(prefs::kSendFunctionKeys,
                                                   false);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kPrimaryMouseButtonRight, false);
  }

  void OnKeyboardPoliciesChanged() { ++num_times_keyboard_policies_changed_; }
  void OnMousePoliciesChanged() { ++num_times_mouse_policies_changed_; }

 protected:
  int num_times_keyboard_policies_changed_ = 0;
  int num_times_mouse_policies_changed_ = 0;

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
  EXPECT_EQ(0, num_times_keyboard_policies_changed_);

  pref_service_->SetManagedPref(prefs::kSendFunctionKeys, base::Value(true));
  EXPECT_EQ(
      mojom::PolicyStatus::kManaged,
      handler_->keyboard_policies().top_row_are_fkeys_policy->policy_status);
  EXPECT_TRUE(handler_->keyboard_policies().top_row_are_fkeys_policy->value);
  EXPECT_EQ(1, num_times_keyboard_policies_changed_);
}

TEST_F(InputDeviceSettingsPolicyHandlerTest, KeyboardRecommendedPolicy) {
  pref_service_->SetRecommendedPref(prefs::kSendFunctionKeys,
                                    base::Value(false));
  handler_->Initialize(pref_service_.get());

  EXPECT_EQ(
      mojom::PolicyStatus::kRecommended,
      handler_->keyboard_policies().top_row_are_fkeys_policy->policy_status);
  EXPECT_FALSE(handler_->keyboard_policies().top_row_are_fkeys_policy->value);
  EXPECT_EQ(0, num_times_keyboard_policies_changed_);

  pref_service_->SetRecommendedPref(prefs::kSendFunctionKeys,
                                    base::Value(true));
  EXPECT_EQ(
      mojom::PolicyStatus::kRecommended,
      handler_->keyboard_policies().top_row_are_fkeys_policy->policy_status);
  EXPECT_TRUE(handler_->keyboard_policies().top_row_are_fkeys_policy->value);
  EXPECT_EQ(1, num_times_keyboard_policies_changed_);
}

TEST_F(InputDeviceSettingsPolicyHandlerTest, MouseNoPolicy) {
  handler_->Initialize(pref_service_.get());
  EXPECT_FALSE(handler_->mouse_policies().swap_right_policy);
}

TEST_F(InputDeviceSettingsPolicyHandlerTest, MouseManagedPolicy) {
  pref_service_->SetManagedPref(prefs::kPrimaryMouseButtonRight,
                                base::Value(false));
  handler_->Initialize(pref_service_.get());

  EXPECT_EQ(mojom::PolicyStatus::kManaged,
            handler_->mouse_policies().swap_right_policy->policy_status);
  EXPECT_FALSE(handler_->mouse_policies().swap_right_policy->value);
  EXPECT_EQ(0, num_times_mouse_policies_changed_);

  pref_service_->SetManagedPref(prefs::kPrimaryMouseButtonRight,
                                base::Value(true));
  EXPECT_EQ(mojom::PolicyStatus::kManaged,
            handler_->mouse_policies().swap_right_policy->policy_status);
  EXPECT_TRUE(handler_->mouse_policies().swap_right_policy->value);
  EXPECT_EQ(1, num_times_mouse_policies_changed_);
}

TEST_F(InputDeviceSettingsPolicyHandlerTest, MouseRecommendedPolicy) {
  pref_service_->SetRecommendedPref(prefs::kPrimaryMouseButtonRight,
                                    base::Value(false));
  handler_->Initialize(pref_service_.get());

  EXPECT_EQ(mojom::PolicyStatus::kRecommended,
            handler_->mouse_policies().swap_right_policy->policy_status);
  EXPECT_FALSE(handler_->mouse_policies().swap_right_policy->value);
  EXPECT_EQ(0, num_times_mouse_policies_changed_);

  pref_service_->SetRecommendedPref(prefs::kPrimaryMouseButtonRight,
                                    base::Value(true));
  EXPECT_EQ(mojom::PolicyStatus::kRecommended,
            handler_->mouse_policies().swap_right_policy->policy_status);
  EXPECT_TRUE(handler_->mouse_policies().swap_right_policy->value);
  EXPECT_EQ(1, num_times_mouse_policies_changed_);
}

}  // namespace ash
