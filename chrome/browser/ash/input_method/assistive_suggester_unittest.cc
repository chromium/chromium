// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_suggester.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/personal_info_suggester.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/ime_bridge.h"

namespace ash {
namespace input_method {
namespace {

using ::chromeos::ime::TextSuggestion;
using ::chromeos::ime::TextSuggestionMode;
using ::chromeos::ime::TextSuggestionType;

}  // namespace

class AssistiveSuggesterTest : public testing::Test {
 protected:
  AssistiveSuggesterTest() { profile_ = std::make_unique<TestingProfile>(); }

  void SetUp() override {
    engine_ = std::make_unique<InputMethodEngine>();
    assistive_suggester_ =
        std::make_unique<AssistiveSuggester>(engine_.get(), profile_.get());
    histogram_tester_.ExpectUniqueSample(
        "InputMethod.Assistive.UserPref.PersonalInfo", true, 1);
    histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.UserPref.Emoji",
                                         true, 1);
    histogram_tester_.ExpectUniqueSample(
        "InputMethod.Assistive.UserPref.MultiWord", true, 1);
    ui::IMEBridge::Initialize();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AssistiveSuggester> assistive_suggester_;
  std::unique_ptr<InputMethodEngine> engine_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AssistiveSuggesterTest,
       EmojiSuggestionPrefFalseFeatureFlagTrue_UserPrefEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEmojiSuggestAddition},
      /*disabled_features=*/{features::kAssistPersonalInfo,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   true);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, false);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       EmojiSuggestionPrefFalseFeatureFlagTrue_EnterprisePrefEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEmojiSuggestAddition},
      /*disabled_features=*/{features::kAssistPersonalInfo,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   false);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, true);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       EmojiSuggestionPrefTrueFeatureFlagTrue_BothPrefsEnabledTrue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEmojiSuggestAddition},
      /*disabled_features=*/{features::kAssistPersonalInfo,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   true);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, true);

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       EmojiSuggestionPrefTrueFeatureFlagTrue_BothPrefsEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kEmojiSuggestAddition},
      /*disabled_features=*/{features::kAssistPersonalInfo,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   false);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, false);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(
    AssistiveSuggesterTest,
    AssistPersonalInfoEnabledPrefFalseFeatureFlagTrue_AssitiveFeatureEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfo},
      /*disabled_features=*/{features::kEmojiSuggestAddition,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kAssistPersonalInfoEnabled, false);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(
    AssistiveSuggesterTest,
    AssistPersonalInfoEnabledTrueFeatureFlagTrue_AssitiveFeatureEnabledTrue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfo},
      /*disabled_features=*/{features::kEmojiSuggestAddition,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kAssistPersonalInfoEnabled, true);

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       MultiWordEnabledWhenFeatureFlagEnabledAndPrefEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{features::kEmojiSuggestAddition,
                             features::kAssistPersonalInfo});
  profile_->GetPrefs()->SetBoolean(prefs::kAssistPredictiveWritingEnabled,
                                   true);

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       MultiWordDisabledWhenFeatureFlagEnabledAndPrefDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{features::kEmojiSuggestAddition,
                             features::kAssistPersonalInfo});
  profile_->GetPrefs()->SetBoolean(prefs::kAssistPredictiveWritingEnabled,
                                   false);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       MultiWordDisabledWhenFeatureFlagDisabledAndPrefDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kEmojiSuggestAddition,
                             features::kAssistPersonalInfo,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kAssistPredictiveWritingEnabled,
                                   false);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       MultiWordDisabledWhenFeatureFlagEnabledButImeServiceDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{features::kEmojiSuggestAddition,
                             features::kAssistPersonalInfo,
                             features::kImeMojoDecoder});

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       MultiWordDisabledWhenFeatureFlagAndImeServiceEnableButSystemPkDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord,
                            features::kImeMojoDecoder},
      /*disabled_features=*/{features::kEmojiSuggestAddition,
                             features::kAssistPersonalInfo,
                             features::kSystemLatinPhysicalTyping});

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest, MultiWordEnabledWhenFeatureFlagAndDepsEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord,
                            features::kImeMojoDecoder,
                            features::kSystemLatinPhysicalTyping},
      /*disabled_features=*/{features::kEmojiSuggestAddition,
                             features::kAssistPersonalInfo});

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

}  // namespace input_method
}  // namespace ash
