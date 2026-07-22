// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/generated_find_and_fill_with_gemini_pref.h"

#include <memory>

#include "chrome/browser/extensions/api/settings_private/generated_pref_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings_api = extensions::api::settings_private;
namespace settings_private = extensions::settings_private;

namespace autofill {
namespace {

class GeneratedFindAndFillWithGeminiPrefTest : public testing::Test {
 public:
  GeneratedFindAndFillWithGeminiPrefTest() = default;
  ~GeneratedFindAndFillWithGeminiPrefTest() override = default;

  TestingProfile* profile() { return profile_.get(); }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_->GetTestingPrefService();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
};

// Tests that GetPrefObject returns the normal backing pref values when no
// policy is set.
TEST_F(GeneratedFindAndFillWithGeminiPrefTest, GetPrefObject_NoPolicy) {
  GeneratedFindAndFillWithGeminiPref pref(profile());

  prefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(true));
  settings_api::PrefObject pref_object = pref.GetPrefObject();

  EXPECT_EQ(pref_object.key, kGeneratedFindAndFillWithGeminiPref);
  EXPECT_EQ(pref_object.type, settings_api::PrefType::kBoolean);
  EXPECT_EQ(pref_object.value->GetBool(), true);
  EXPECT_EQ(pref_object.enforcement, settings_api::Enforcement::kNone);

  prefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(false));
  pref_object = pref.GetPrefObject();

  EXPECT_EQ(pref_object.value->GetBool(), false);
  EXPECT_EQ(pref_object.enforcement, settings_api::Enforcement::kNone);
}

// Tests that SetPref updates the backing user pref when no policy is set.
TEST_F(GeneratedFindAndFillWithGeminiPrefTest, SetPref_NoPolicy) {
  GeneratedFindAndFillWithGeminiPref pref(profile());

  base::Value value_true(true);
  EXPECT_EQ(pref.SetPref(&value_true),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(
      prefs()->GetBoolean(personal_context::prefs::
                              kPersonalContextInAutofillSettingsToggleStatus),
      true);

  base::Value value_false(false);
  EXPECT_EQ(pref.SetPref(&value_false),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(
      prefs()->GetBoolean(personal_context::prefs::
                              kPersonalContextInAutofillSettingsToggleStatus),
      false);
}

// Tests that when the enterprise policy is set to disabled (2), the pref is
// enforced disabled.
TEST_F(GeneratedFindAndFillWithGeminiPrefTest, PolicyDisabled) {
  GeneratedFindAndFillWithGeminiPref pref(profile());

  // Set the backing pref to true initially.
  prefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(true));

  // Set the enterprise policy to kDisable (2).
  prefs()->SetManagedPref(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings,
      base::Value(
          static_cast<int>(optimization_guide::model_execution::prefs::
                               ModelExecutionEnterprisePolicyValue::kDisable)));

  settings_api::PrefObject pref_object = pref.GetPrefObject();
  EXPECT_EQ(pref_object.value->GetBool(), false);
  EXPECT_EQ(pref_object.enforcement, settings_api::Enforcement::kEnforced);
  EXPECT_EQ(pref_object.controlled_by,
            settings_api::ControlledBy::kDevicePolicy);

  // Attempting to set the pref should return PREF_NOT_MODIFIABLE.
  base::Value value_true(true);
  EXPECT_EQ(pref.SetPref(&value_true),
            settings_private::SetPrefResult::PREF_NOT_MODIFIABLE);
}

// Tests that when the enterprise policy is allowed (0 or 1), the pref behaves
// normally.
TEST_F(GeneratedFindAndFillWithGeminiPrefTest, PolicyAllowed) {
  GeneratedFindAndFillWithGeminiPref pref(profile());

  prefs()->SetUserPref(
      personal_context::prefs::kPersonalContextInAutofillSettingsToggleStatus,
      base::Value(true));

  // Set the enterprise policy to kAllow (0).
  prefs()->SetManagedPref(
      optimization_guide::prefs::kFindAndFillWithGeminiSettings,
      base::Value(
          static_cast<int>(optimization_guide::model_execution::prefs::
                               ModelExecutionEnterprisePolicyValue::kAllow)));

  settings_api::PrefObject pref_object = pref.GetPrefObject();
  EXPECT_EQ(pref_object.value->GetBool(), true);
  EXPECT_EQ(pref_object.enforcement, settings_api::Enforcement::kNone);

  base::Value value_false(false);
  EXPECT_EQ(pref.SetPref(&value_false),
            settings_private::SetPrefResult::SUCCESS);
  EXPECT_EQ(
      prefs()->GetBoolean(personal_context::prefs::
                              kPersonalContextInAutofillSettingsToggleStatus),
      false);
}

}  // namespace
}  // namespace autofill
