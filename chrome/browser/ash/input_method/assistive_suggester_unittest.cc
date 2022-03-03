// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_suggester.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/assistive_suggester_client_filter.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/personal_info_suggester.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

using ime::TextSuggestion;
using ime::TextSuggestionMode;
using ime::TextSuggestionType;

const char kEmojiData[] = "happy,😀;😃;😄";
const char kUsEnglishEngineId[] = "xkb:us::eng";
const char kSpainSpanishEngineId[] = "xkb:es::spa";

ui::KeyEvent GenerateKeyEvent(const ui::DomCode& code,
                              const ui::EventType& event_type,
                              int flags) {
  return ui::KeyEvent(event_type, ui::VKEY_UNKNOWN, code, flags,
                      ui::DomKey::NONE, ui::EventTimeForNow());
}

ui::KeyEvent PressKey(const ui::DomCode& code) {
  return GenerateKeyEvent(code, ui::EventType::ET_KEY_PRESSED,
                          ui::EventFlags::EF_NONE);
}

ui::KeyEvent PressKeyWithAlt(const ui::DomCode& code) {
  return GenerateKeyEvent(code, ui::EventType::ET_KEY_PRESSED,
                          ui::EventFlags::EF_ALT_DOWN);
}

ui::KeyEvent PressKeyWithCtrl(const ui::DomCode& code) {
  return GenerateKeyEvent(code, ui::EventType::ET_KEY_PRESSED,
                          ui::EventFlags::EF_CONTROL_DOWN);
}

ui::KeyEvent PressKeyWithShift(const ui::DomCode& code) {
  return GenerateKeyEvent(code, ui::EventType::ET_KEY_PRESSED,
                          ui::EventFlags::EF_SHIFT_DOWN);
}

void SetInputMethodOptions(Profile& profile, bool predictive_writing_enabled) {
  base::Value input_method_setting(base::Value::Type::DICTIONARY);
  input_method_setting.SetPath(std::string(kUsEnglishEngineId) +
                                   ".physicalKeyboardEnablePredictiveWriting",
                               base::Value(predictive_writing_enabled));
  profile.GetPrefs()->Set(::prefs::kLanguageInputMethodSpecificSettings,
                          input_method_setting);
}

}  // namespace

class FakeSuggesterSwitch : public AssistiveSuggesterSwitch {
 public:
  explicit FakeSuggesterSwitch(EnabledSuggestions enabled_suggestions)
      : enabled_suggestions_(enabled_suggestions) {}
  ~FakeSuggesterSwitch() override = default;

  // AssistiveSuggesterDelegate overrides
  bool IsEmojiSuggestionAllowed() override {
    return enabled_suggestions_.emoji_suggestions;
  }

  bool IsMultiWordSuggestionAllowed() override {
    return enabled_suggestions_.multi_word_suggestions;
  }

  bool IsPersonalInfoSuggestionAllowed() override {
    return enabled_suggestions_.personal_info_suggestions;
  }

  void GetEnabledSuggestions(GetEnabledSuggestionsCallback callback) override {
    std::move(callback).Run(enabled_suggestions_);
  }

 private:
  EnabledSuggestions enabled_suggestions_;
};

class AssistiveSuggesterTest : public testing::Test {
 protected:
  AssistiveSuggesterTest() { profile_ = std::make_unique<TestingProfile>(); }

  void SetUp() override {
    engine_ = std::make_unique<InputMethodEngine>();
    assistive_suggester_ = std::make_unique<AssistiveSuggester>(
        engine_.get(), profile_.get(),
        std::make_unique<AssistiveSuggesterClientFilter>());

    histogram_tester_.ExpectUniqueSample(
        "InputMethod.Assistive.UserPref.PersonalInfo", true, 1);
    histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.UserPref.Emoji",
                                         true, 1);
    // Emoji is default to true now, so need to set emoji pref false to test
    // IsAssistiveFeatureEnabled correctly.
    profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, false);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AssistiveSuggester> assistive_suggester_;
  std::unique_ptr<InputMethodEngine> engine_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AssistiveSuggesterTest, EmojiSuggestion_UserPrefEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistPersonalInfo,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   true);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, false);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest, EmojiSuggestion_EnterprisePrefEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistPersonalInfo,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   false);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, true);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest, EmojiSuggestion_BothPrefsEnabledTrue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistPersonalInfo,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   true);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, true);

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest, EmojiSuggestion_BothPrefsEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistPersonalInfo,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   false);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, false);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       EnhancedEmojiSuggestDisabledWhenStandardEmojiDisabledAndPrefsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistEmojiEnhanced},
      /*disabled_features=*/{features::kAssistPersonalInfo,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   false);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, false);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       EnhancedEmojiSuggestEnabledWhenStandardEmojiEnabledAndPrefsEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistEmojiEnhanced},
      /*disabled_features=*/{features::kAssistPersonalInfo,
                             features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   true);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, true);

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(
    AssistiveSuggesterTest,
    AssistPersonalInfoEnabledPrefFalseFeatureFlagTrue_AssitiveFeatureEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfo},
      /*disabled_features=*/{features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kAssistPersonalInfoEnabled, false);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(
    AssistiveSuggesterTest,
    AssistPersonalInfoEnabledTrueFeatureFlagTrue_AssitiveFeatureEnabledTrue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistPersonalInfo},
      /*disabled_features=*/{features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kAssistPersonalInfoEnabled, true);

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       MultiWordEnabledWhenFeatureFlagEnabledAndPrefEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{features::kAssistPersonalInfo});

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       MultiWordDisabledWhenFeatureFlagEnabledAndPrefDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{features::kAssistPersonalInfo});

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       MultiWordDisabledWhenFeatureFlagDisabledAndPrefDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistPersonalInfo,
                             features::kAssistMultiWord});

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       QueriesAssistiveSuggesterSwitchWhenDeterminingIfFeatureAllowed) {
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      engine_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          FakeSuggesterSwitch::EnabledSuggestions{
              .emoji_suggestions = true,
              .multi_word_suggestions = true,
              .personal_info_suggestions = true}));

  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureAllowed(
      AssistiveSuggester::AssistiveFeature::kEmojiSuggestion));
  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureAllowed(
      AssistiveSuggester::AssistiveFeature::kMultiWordSuggestion));
  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureAllowed(
      AssistiveSuggester::AssistiveFeature::kPersonalInfoSuggestion));
}

TEST_F(AssistiveSuggesterTest, RecordPredictiveWritingPrefOnActivate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{});
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      engine_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          FakeSuggesterSwitch::EnabledSuggestions{}));

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.UserPref.MultiWord", true, 1);
}

TEST_F(AssistiveSuggesterTest, RecordsMultiWordTextInputAsNotAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{});
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      engine_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          FakeSuggesterSwitch::EnabledSuggestions{}));

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);

  histogram_tester_.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.InputState", 1);
  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.InputState",
      AssistiveTextInputState::kFeatureBlockedByDenylist, 1);
}

TEST_F(AssistiveSuggesterTest, RecordsMultiWordTextInputAsDisabledByUser) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{});
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      engine_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          FakeSuggesterSwitch::EnabledSuggestions{.multi_word_suggestions =
                                                      true}));

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);

  histogram_tester_.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.InputState", 1);
  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.InputState",
      AssistiveTextInputState::kFeatureBlockedByPreference, 1);
}

TEST_F(AssistiveSuggesterTest, RecordsMultiWordTextInputAsDisabledByLacros) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord,
                            features::kLacrosSupport},
      /*disabled_features=*/{});
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      engine_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          FakeSuggesterSwitch::EnabledSuggestions{.multi_word_suggestions =
                                                      true}));

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);

  histogram_tester_.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.InputState", 1);
  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.InputState",
      AssistiveTextInputState::kUnsupportedClient, 1);
}

TEST_F(AssistiveSuggesterTest,
       RecordMultiWordTextInputAsDisabledByUnsupportedLang) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{});
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      engine_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          FakeSuggesterSwitch::EnabledSuggestions{.multi_word_suggestions =
                                                      true}));

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true);
  assistive_suggester_->OnActivate(kSpainSpanishEngineId);
  assistive_suggester_->OnFocus(5);

  histogram_tester_.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.InputState", 1);
  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.InputState",
      AssistiveTextInputState::kUnsupportedLanguage, 1);
}

TEST_F(AssistiveSuggesterTest, RecordsMultiWordTextInputAsEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{});
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      engine_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          FakeSuggesterSwitch::EnabledSuggestions{.multi_word_suggestions =
                                                      true}));

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);

  histogram_tester_.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.InputState", 1);
  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.InputState",
      AssistiveTextInputState::kFeatureEnabled, 1);
}

class AssistiveSuggesterMultiWordTest : public testing::Test {
 protected:
  AssistiveSuggesterMultiWordTest() {
    profile_ = std::make_unique<TestingProfile>();
  }

  void SetUp() override {
    engine_ = std::make_unique<InputMethodEngine>();
    assistive_suggester_ = std::make_unique<AssistiveSuggester>(
        engine_.get(), profile_.get(),
        std::make_unique<FakeSuggesterSwitch>(
            FakeSuggesterSwitch::EnabledSuggestions{
                .multi_word_suggestions = true,
            }));

    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAssistMultiWord},
        /*disabled_features=*/{});

    SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AssistiveSuggester> assistive_suggester_;
  std::unique_ptr<InputMethodEngine> engine_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AssistiveSuggesterMultiWordTest,
       MatchMetricNotRecordedWhenZeroSuggestions) {
  assistive_suggester_->OnExternalSuggestionsUpdated({});

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Match", 0);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       MatchMetricRecordedWhenOneOrMoreSuggestions) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"", 0, 0);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Match", 1);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.Match",
                                       AssistiveType::kMultiWordPrediction, 1);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       MatchMetricNotRecordedWhenMultiWordFlagDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistMultiWord});
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"", 0, 0);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Match", 0);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       DisableMetricNotRecordedWhenNoSuggestionAndMultiWordBlocked) {
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      engine_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          FakeSuggesterSwitch::EnabledSuggestions{}));

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"", 0, 0);
  assistive_suggester_->OnExternalSuggestionsUpdated({});

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Disabled.MultiWord",
                                     0);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       DisableMetricRecordedWhenGivenSuggestionAndMultiWordBlocked) {
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      engine_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          FakeSuggesterSwitch::EnabledSuggestions{}));
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"", 0, 0);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Disabled.MultiWord",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.MultiWord",
      DisabledReason::kUrlOrAppNotAllowed, 1);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       CoverageMetricNotRecordedWhenNoSuggestionGiven) {
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"", 0, 0);
  assistive_suggester_->OnExternalSuggestionsUpdated({});

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Coverage", 0);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       CoverageMetricRecordedWhenSuggestionShown) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"", 0, 0);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Coverage", 1);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                       AssistiveType::kMultiWordPrediction, 1);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       CoverageMetricRecordedOnceWhenSuggestionShownAndTracked) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"", 0, 0);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);
  assistive_suggester_->OnSurroundingTextChanged(u"h", 1, 1);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);
  assistive_suggester_->OnSurroundingTextChanged(u"he", 2, 2);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);
  assistive_suggester_->OnSurroundingTextChanged(u"hel", 3, 3);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Coverage", 1);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                       AssistiveType::kMultiWordPrediction, 1);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       CoverageMetricRecordedForEverySuggestionShown) {
  std::vector<TextSuggestion> first_suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "hello there"}};
  std::vector<TextSuggestion> second_suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kPrediction,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "was"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"", 0, 0);
  assistive_suggester_->OnExternalSuggestionsUpdated(first_suggestions);
  assistive_suggester_->OnSurroundingTextChanged(u"h", 1, 1);
  assistive_suggester_->OnExternalSuggestionsUpdated(first_suggestions);
  assistive_suggester_->OnSurroundingTextChanged(u"he", 2, 2);
  assistive_suggester_->OnExternalSuggestionsUpdated(first_suggestions);
  assistive_suggester_->OnSurroundingTextChanged(u"he ", 3, 3);
  assistive_suggester_->OnExternalSuggestionsUpdated(second_suggestions);

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Coverage", 2);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                       AssistiveType::kMultiWordPrediction, 2);
}

TEST_F(AssistiveSuggesterMultiWordTest, PressingTabShouldAcceptSuggestion) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"why ar", 6, 6);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_TRUE(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::TAB)));
}

TEST_F(AssistiveSuggesterMultiWordTest, AltPlusTabShouldNotAcceptSuggestion) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"why ar", 6, 6);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_FALSE(
      assistive_suggester_->OnKeyEvent(PressKeyWithAlt(ui::DomCode::TAB)));
}

TEST_F(AssistiveSuggesterMultiWordTest, CtrlPlusTabShouldNotAcceptSuggestion) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"why ar", 6, 6);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_FALSE(
      assistive_suggester_->OnKeyEvent(PressKeyWithCtrl(ui::DomCode::TAB)));
}

TEST_F(AssistiveSuggesterMultiWordTest, ShiftPlusTabShouldNotAcceptSuggestion) {
  std::vector<TextSuggestion> suggestions = {
      TextSuggestion{.mode = TextSuggestionMode::kCompletion,
                     .type = TextSuggestionType::kMultiWord,
                     .text = "aren\'t you"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);
  assistive_suggester_->OnSurroundingTextChanged(u"why ar", 6, 6);
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions);

  EXPECT_FALSE(
      assistive_suggester_->OnKeyEvent(PressKeyWithShift(ui::DomCode::TAB)));
}

class AssistiveSuggesterEmojiTest : public testing::Test {
 protected:
  AssistiveSuggesterEmojiTest() {
    profile_ = std::make_unique<TestingProfile>();
  }

  void SetUp() override {
    engine_ = std::make_unique<InputMethodEngine>();
    assistive_suggester_ = std::make_unique<AssistiveSuggester>(
        engine_.get(), profile_.get(),
        std::make_unique<FakeSuggesterSwitch>(
            FakeSuggesterSwitch::EnabledSuggestions{
                .emoji_suggestions = true,
            }));
    assistive_suggester_->get_emoji_suggester_for_testing()
        ->LoadEmojiMapForTesting(kEmojiData);

    // Needed to ensure globals accessed by EmojiSuggester are available.
    chrome_keyboard_controller_client_ =
        ChromeKeyboardControllerClient::CreateForTest();
    chrome_keyboard_controller_client_->set_keyboard_visible_for_test(false);

    profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                     true);
    profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, true);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AssistiveSuggester> assistive_suggester_;
  std::unique_ptr<InputMethodEngine> engine_;
  base::HistogramTester histogram_tester_;

  // Needs to outlive the emoji_suggester under test.
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;
};

TEST_F(AssistiveSuggesterEmojiTest, ShouldReturnPrefixBasedEmojiSuggestions) {
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5);

  EXPECT_TRUE(assistive_suggester_->OnSurroundingTextChanged(u"happy ", 6, 6));
}

}  // namespace input_method
}  // namespace ash
