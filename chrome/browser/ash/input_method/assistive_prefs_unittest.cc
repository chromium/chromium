// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_prefs.h"

#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace input_method {
namespace {

constexpr char kUsEnglish[] = "xkb:us::eng";

void SetManagedPkPredictiveWritingAllowed(Profile& profile, bool allowed) {
  profile.GetPrefs()->SetBoolean(
      prefs::kManagedPhysicalKeyboardPredictiveWritingAllowed, allowed);
}

void SetPredictiveWritingEnabled(Profile& profile, bool enabled) {
  base::Value::Dict input_method_setting;
  input_method_setting.SetByDottedPath(
      "xkb:us::eng.physicalKeyboardEnabledPredictiveWriting", enabled);
  profile.GetPrefs()->Set(::prefs::kLanguageInputMethodSpecificSettings,
                          base::Value(std::move(input_method_setting)));
}

class AssistivePrefsTest : public ::testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(AssistivePrefsTest, PredictiveWritingIsEnabledByDefault) {
  EXPECT_TRUE(IsPredictiveWritingPrefEnabled(*profile_.GetPrefs(), kUsEnglish));
}

TEST_F(AssistivePrefsTest,
       WhenAdminDisallowsPredictiveWritingItIsAlwaysDisabled) {
  SetPredictiveWritingEnabled(profile_, true);
  SetManagedPkPredictiveWritingAllowed(profile_, false);

  EXPECT_FALSE(
      IsPredictiveWritingPrefEnabled(*profile_.GetPrefs(), kUsEnglish));
}

TEST_F(AssistivePrefsTest, WhenAdminAllowsPredictiveWritingItCanBeEnabled) {
  SetPredictiveWritingEnabled(profile_, true);
  SetManagedPkPredictiveWritingAllowed(profile_, true);

  EXPECT_TRUE(IsPredictiveWritingPrefEnabled(*profile_.GetPrefs(), kUsEnglish));
}

}  // namespace
}  // namespace input_method
}  // namespace ash
