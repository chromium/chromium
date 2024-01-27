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
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    handler_ = std::make_unique<InputDeviceSettingsPolicyHandler>(
        base::BindRepeating(
            &InputDeviceSettingsPolicyHandlerTest::OnKeyboardPoliciesChanged,
            base::Unretained(this)),
        base::BindRepeating(
            &InputDeviceSettingsPolicyHandlerTest::OnMousePoliciesChanged,
            base::Unretained(this)));

    local_state_->registry()->RegisterBooleanPref(
        prefs::kOwnerPrimaryMouseButtonRight, false);
    local_state_->registry()->RegisterBooleanPref(
        prefs::kDeviceSwitchFunctionKeysBehaviorEnabled, false);
    pref_service_->registry()->RegisterBooleanPref(prefs::kSendFunctionKeys,
                                                   false);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kPrimaryMouseButtonRight, false);
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kF11KeyModifier,
        static_cast<int>(ui::mojom::ExtendedFkeysModifier::kDisabled));
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kF12KeyModifier,
        static_cast<int>(ui::mojom::ExtendedFkeysModifier::kDisabled));
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kHomeAndEndKeysModifier,
        static_cast<int>(ui::mojom::SixPackShortcutModifier::kNone));
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kPageUpAndPageDownKeysModifier,
        static_cast<int>(ui::mojom::SixPackShortcutModifier::kNone));
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kDeleteKeyModifier,
        static_cast<int>(ui::mojom::SixPackShortcutModifier::kNone));
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kInsertKeyModifier,
        static_cast<int>(ui::mojom::SixPackShortcutModifier::kNone));
  }

  void OnKeyboardPoliciesChanged() { ++num_times_keyboard_policies_changed_; }
  void OnMousePoliciesChanged() { ++num_times_mouse_policies_changed_; }

 protected:
  int num_times_keyboard_policies_changed_ = 0;
  int num_times_mouse_policies_changed_ = 0;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<InputDeviceSettingsPolicyHandler> handler_;
};

TEST_F(InputDeviceSettingsPolicyHandlerTest, KeyboardNoPolicy) {
  handler_->Initialize(local_state_.get(), pref_service_.get());
  EXPECT_FALSE(handler_->keyboard_policies().top_row_are_fkeys_policy);
  EXPECT_FALSE(handler_->keyboard_policies().enable_meta_fkey_rewrites_policy);
  EXPECT_FALSE(handler_->keyboard_policies().f11_key_policy);
  EXPECT_FALSE(handler_->keyboard_policies().f12_key_policy);
  EXPECT_FALSE(handler_->keyboard_policies().home_and_end_keys_policy);
  EXPECT_FALSE(handler_->keyboard_policies().page_up_and_page_down_keys_policy);
  EXPECT_FALSE(handler_->keyboard_policies().delete_key_policy);
  EXPECT_FALSE(handler_->keyboard_policies().insert_key_policy);
}

TEST_F(InputDeviceSettingsPolicyHandlerTest, KeyboardManagedPolicy) {
  pref_service_->SetManagedPref(prefs::kSendFunctionKeys, base::Value(false));
  local_state_->SetManagedPref(prefs::kDeviceSwitchFunctionKeysBehaviorEnabled,
                               base::Value(false));
  pref_service_->SetManagedPref(
      prefs::kF11KeyModifier,
      base::Value(
          static_cast<int>(ui::mojom::ExtendedFkeysModifier::kCtrlShift)));
  pref_service_->SetManagedPref(
      prefs::kF12KeyModifier,
      base::Value(static_cast<int>(ui::mojom::ExtendedFkeysModifier::kAlt)));
  pref_service_->SetManagedPref(
      prefs::kHomeAndEndKeysModifier,
      base::Value(static_cast<int>(ui::mojom::SixPackShortcutModifier::kAlt)));
  pref_service_->SetManagedPref(
      prefs::kPageUpAndPageDownKeysModifier,
      base::Value(static_cast<int>(ui::mojom::SixPackShortcutModifier::kAlt)));
  pref_service_->SetManagedPref(
      prefs::kDeleteKeyModifier,
      base::Value(static_cast<int>(ui::mojom::SixPackShortcutModifier::kAlt)));
  pref_service_->SetManagedPref(
      prefs::kInsertKeyModifier,
      base::Value(
          static_cast<int>(ui::mojom::SixPackShortcutModifier::kSearch)));
  handler_->Initialize(local_state_.get(), pref_service_.get());

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

  EXPECT_EQ(mojom::PolicyStatus::kManaged,
            handler_->keyboard_policies()
                .enable_meta_fkey_rewrites_policy->policy_status);
  EXPECT_FALSE(
      handler_->keyboard_policies().enable_meta_fkey_rewrites_policy->value);
  EXPECT_EQ(1, num_times_keyboard_policies_changed_);

  local_state_->SetManagedPref(prefs::kDeviceSwitchFunctionKeysBehaviorEnabled,
                               base::Value(true));
  EXPECT_EQ(mojom::PolicyStatus::kManaged,
            handler_->keyboard_policies()
                .enable_meta_fkey_rewrites_policy->policy_status);
  EXPECT_TRUE(
      handler_->keyboard_policies().enable_meta_fkey_rewrites_policy->value);
  EXPECT_EQ(2, num_times_keyboard_policies_changed_);

  EXPECT_EQ(mojom::PolicyStatus::kManaged,
            handler_->keyboard_policies().f11_key_policy->policy_status);
  EXPECT_EQ(
      static_cast<int>(ui::mojom::ExtendedFkeysModifier::kCtrlShift),
      static_cast<int>(handler_->keyboard_policies().f11_key_policy->value));
  pref_service_->SetManagedPref(
      prefs::kF11KeyModifier,
      base::Value(static_cast<int>(ui::mojom::ExtendedFkeysModifier::kAlt)));
  EXPECT_EQ(
      static_cast<int>(ui::mojom::ExtendedFkeysModifier::kAlt),
      static_cast<int>(handler_->keyboard_policies().f11_key_policy->value));
  EXPECT_EQ(3, num_times_keyboard_policies_changed_);

  EXPECT_EQ(mojom::PolicyStatus::kManaged,
            handler_->keyboard_policies().f12_key_policy->policy_status);
  EXPECT_EQ(
      static_cast<int>(ui::mojom::ExtendedFkeysModifier::kAlt),
      static_cast<int>(handler_->keyboard_policies().f12_key_policy->value));
  pref_service_->SetManagedPref(
      prefs::kF12KeyModifier,
      base::Value(
          static_cast<int>(ui::mojom::ExtendedFkeysModifier::kCtrlShift)));
  EXPECT_EQ(
      static_cast<int>(ui::mojom::ExtendedFkeysModifier::kCtrlShift),
      static_cast<int>(handler_->keyboard_policies().f12_key_policy->value));
  EXPECT_EQ(4, num_times_keyboard_policies_changed_);

  EXPECT_EQ(
      mojom::PolicyStatus::kManaged,
      handler_->keyboard_policies().home_and_end_keys_policy->policy_status);
  EXPECT_EQ(static_cast<int>(ui::mojom::SixPackShortcutModifier::kAlt),
            static_cast<int>(
                handler_->keyboard_policies().home_and_end_keys_policy->value));
  pref_service_->SetManagedPref(
      prefs::kHomeAndEndKeysModifier,
      base::Value(
          static_cast<int>(ui::mojom::SixPackShortcutModifier::kSearch)));
  EXPECT_EQ(static_cast<int>(ui::mojom::SixPackShortcutModifier::kSearch),
            static_cast<int>(
                handler_->keyboard_policies().home_and_end_keys_policy->value));
  EXPECT_EQ(5, num_times_keyboard_policies_changed_);

  EXPECT_EQ(mojom::PolicyStatus::kManaged,
            handler_->keyboard_policies()
                .page_up_and_page_down_keys_policy->policy_status);
  EXPECT_EQ(static_cast<int>(ui::mojom::SixPackShortcutModifier::kAlt),
            static_cast<int>(handler_->keyboard_policies()
                                 .page_up_and_page_down_keys_policy->value));
  pref_service_->SetManagedPref(
      prefs::kPageUpAndPageDownKeysModifier,
      base::Value(
          static_cast<int>(ui::mojom::SixPackShortcutModifier::kSearch)));
  EXPECT_EQ(static_cast<int>(ui::mojom::SixPackShortcutModifier::kSearch),
            static_cast<int>(handler_->keyboard_policies()
                                 .page_up_and_page_down_keys_policy->value));
  EXPECT_EQ(6, num_times_keyboard_policies_changed_);

  EXPECT_EQ(mojom::PolicyStatus::kManaged,
            handler_->keyboard_policies().delete_key_policy->policy_status);
  EXPECT_EQ(
      static_cast<int>(ui::mojom::SixPackShortcutModifier::kAlt),
      static_cast<int>(handler_->keyboard_policies().delete_key_policy->value));
  pref_service_->SetManagedPref(
      prefs::kDeleteKeyModifier,
      base::Value(
          static_cast<int>(ui::mojom::SixPackShortcutModifier::kSearch)));
  EXPECT_EQ(
      static_cast<int>(ui::mojom::SixPackShortcutModifier::kSearch),
      static_cast<int>(handler_->keyboard_policies().delete_key_policy->value));
  EXPECT_EQ(7, num_times_keyboard_policies_changed_);

  EXPECT_EQ(mojom::PolicyStatus::kManaged,
            handler_->keyboard_policies().insert_key_policy->policy_status);
  EXPECT_EQ(
      static_cast<int>(ui::mojom::SixPackShortcutModifier::kSearch),
      static_cast<int>(handler_->keyboard_policies().insert_key_policy->value));
  pref_service_->SetManagedPref(
      prefs::kInsertKeyModifier,
      base::Value(static_cast<int>(ui::mojom::SixPackShortcutModifier::kNone)));
  EXPECT_EQ(
      static_cast<int>(ui::mojom::SixPackShortcutModifier::kNone),
      static_cast<int>(handler_->keyboard_policies().insert_key_policy->value));
  EXPECT_EQ(8, num_times_keyboard_policies_changed_);
}

TEST_F(InputDeviceSettingsPolicyHandlerTest, KeyboardRecommendedPolicy) {
  pref_service_->SetRecommendedPref(prefs::kSendFunctionKeys,
                                    base::Value(false));
  pref_service_->SetRecommendedPref(
      prefs::kHomeAndEndKeysModifier,
      base::Value(static_cast<int>(ui::mojom::SixPackShortcutModifier::kAlt)));
  pref_service_->SetRecommendedPref(
      prefs::kPageUpAndPageDownKeysModifier,
      base::Value(static_cast<int>(ui::mojom::SixPackShortcutModifier::kAlt)));
  pref_service_->SetRecommendedPref(
      prefs::kDeleteKeyModifier,
      base::Value(static_cast<int>(ui::mojom::SixPackShortcutModifier::kAlt)));
  pref_service_->SetRecommendedPref(
      prefs::kInsertKeyModifier,
      base::Value(static_cast<int>(ui::mojom::SixPackShortcutModifier::kNone)));
  local_state_->SetRecommendedPref(
      prefs::kDeviceSwitchFunctionKeysBehaviorEnabled, base::Value(false));
  pref_service_->SetRecommendedPref(
      prefs::kF11KeyModifier,
      base::Value(static_cast<int>(ui::mojom::ExtendedFkeysModifier::kAlt)));
  pref_service_->SetRecommendedPref(
      prefs::kF12KeyModifier,
      base::Value(static_cast<int>(ui::mojom::ExtendedFkeysModifier::kAlt)));
  handler_->Initialize(local_state_.get(), pref_service_.get());

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

  EXPECT_EQ(mojom::PolicyStatus::kRecommended,
            handler_->keyboard_policies().f11_key_policy->policy_status);

  EXPECT_EQ(mojom::PolicyStatus::kRecommended,
            handler_->keyboard_policies().f12_key_policy->policy_status);

  EXPECT_EQ(
      mojom::PolicyStatus::kRecommended,
      handler_->keyboard_policies().home_and_end_keys_policy->policy_status);

  EXPECT_EQ(mojom::PolicyStatus::kRecommended,
            handler_->keyboard_policies().delete_key_policy->policy_status);

  EXPECT_EQ(mojom::PolicyStatus::kRecommended,
            handler_->keyboard_policies().insert_key_policy->policy_status);

  EXPECT_EQ(mojom::PolicyStatus::kRecommended,
            handler_->keyboard_policies()
                .page_up_and_page_down_keys_policy->policy_status);

  EXPECT_EQ(mojom::PolicyStatus::kRecommended,
            handler_->keyboard_policies()
                .enable_meta_fkey_rewrites_policy->policy_status);
  EXPECT_FALSE(
      handler_->keyboard_policies().enable_meta_fkey_rewrites_policy->value);
  EXPECT_EQ(1, num_times_keyboard_policies_changed_);

  local_state_->SetRecommendedPref(
      prefs::kDeviceSwitchFunctionKeysBehaviorEnabled, base::Value(true));
  EXPECT_EQ(mojom::PolicyStatus::kRecommended,
            handler_->keyboard_policies()
                .enable_meta_fkey_rewrites_policy->policy_status);
  EXPECT_TRUE(
      handler_->keyboard_policies().enable_meta_fkey_rewrites_policy->value);
  EXPECT_EQ(2, num_times_keyboard_policies_changed_);
}

TEST_F(InputDeviceSettingsPolicyHandlerTest, MouseNoPolicy) {
  handler_->Initialize(local_state_.get(), pref_service_.get());
  EXPECT_FALSE(handler_->mouse_policies().swap_right_policy);
}

TEST_F(InputDeviceSettingsPolicyHandlerTest, MouseManagedPolicy) {
  pref_service_->SetManagedPref(prefs::kPrimaryMouseButtonRight,
                                base::Value(false));
  handler_->Initialize(local_state_.get(), pref_service_.get());

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
  handler_->Initialize(local_state_.get(), pref_service_.get());

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

TEST_F(InputDeviceSettingsPolicyHandlerTest, MouseLoginScreenPolicy) {
  local_state_->SetManagedPref(prefs::kOwnerPrimaryMouseButtonRight,
                               base::Value(false));
  handler_->Initialize(local_state_.get(), nullptr);

  EXPECT_EQ(mojom::PolicyStatus::kManaged,
            handler_->mouse_policies().swap_right_policy->policy_status);
  EXPECT_FALSE(handler_->mouse_policies().swap_right_policy->value);
  EXPECT_EQ(0, num_times_mouse_policies_changed_);

  local_state_->SetManagedPref(prefs::kOwnerPrimaryMouseButtonRight,
                               base::Value(true));
  EXPECT_EQ(mojom::PolicyStatus::kManaged,
            handler_->mouse_policies().swap_right_policy->policy_status);
  EXPECT_TRUE(handler_->mouse_policies().swap_right_policy->value);
  EXPECT_EQ(1, num_times_mouse_policies_changed_);
}

}  // namespace ash
