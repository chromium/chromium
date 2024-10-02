// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_suggester.h"

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/input_method/assistive_suggester_client_filter.h"
#include "chrome/browser/ash/input_method/assistive_suggester_switch.h"
#include "chrome/browser/ash/input_method/fake_suggestion_handler.h"
#include "chrome/browser/ash/input_method/get_current_window_properties.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash::input_method {
namespace {

using ime::AssistiveSuggestion;
using ime::AssistiveSuggestionMode;
using ime::AssistiveSuggestionType;
using ime::SuggestionsTextContext;
using EnabledSuggestions = AssistiveSuggesterSwitch::EnabledSuggestions;

const char kUsEnglishEngineId[] = "xkb:us::eng";
const char kSpainSpanishEngineId[] = "xkb:es::spa";
const char kEmojiData[] = "arrow,←;↑;→";
const TextInputMethod::InputContext empty_context(ui::TEXT_INPUT_TYPE_NONE);
constexpr size_t kTakeLastNChars = 100;

ui::KeyEvent GenerateKeyEvent(const ui::DomCode& code,
                              const ui::EventType& event_type,
                              int flags) {
  return ui::KeyEvent(event_type, ui::VKEY_UNKNOWN, code, flags,
                      ui::DomKey::NONE, ui::EventTimeForNow());
}

ui::KeyEvent ReleaseKey(const ui::DomCode& code) {
  return GenerateKeyEvent(code, ui::EventType::kKeyReleased, ui::EF_NONE);
}

ui::KeyEvent PressKey(const ui::DomCode& code) {
  return GenerateKeyEvent(code, ui::EventType::kKeyPressed, ui::EF_NONE);
}

ui::KeyEvent PressKeyWithAlt(const ui::DomCode& code) {
  return GenerateKeyEvent(code, ui::EventType::kKeyPressed, ui::EF_ALT_DOWN);
}

ui::KeyEvent PressKeyWithCtrl(const ui::DomCode& code) {
  return GenerateKeyEvent(code, ui::EventType::kKeyPressed,
                          ui::EF_CONTROL_DOWN);
}

ui::KeyEvent PressKeyWithShift(const ui::DomCode& code) {
  return GenerateKeyEvent(code, ui::EventType::kKeyPressed, ui::EF_SHIFT_DOWN);
}

ui::KeyEvent CreateRepeatKeyEvent(const ui::DomCode& code) {
  return GenerateKeyEvent(code, ui::EventType::kKeyPressed, ui::EF_IS_REPEAT);
}

void SetInputMethodOptions(Profile& profile,
                           bool predictive_writing_enabled,
                           bool diacritics_on_longpress_enabled) {
  base::Value::Dict input_method_setting;
  input_method_setting.SetByDottedPath(
      std::string(kUsEnglishEngineId) +
          ".physicalKeyboardEnablePredictiveWriting",
      predictive_writing_enabled);
  profile.GetPrefs()->Set(::prefs::kLanguageInputMethodSpecificSettings,
                          base::Value(std::move(input_method_setting)));
  profile.GetPrefs()->Set(::ash::prefs::kLongPressDiacriticsEnabled,
                          base::Value(diacritics_on_longpress_enabled));
}

SuggestionsTextContext TextContext(const std::string& surrounding_text) {
  const size_t text_length = surrounding_text.length();
  const size_t trim_from =
      text_length > kTakeLastNChars ? text_length - kTakeLastNChars : 0;
  return SuggestionsTextContext{
      .last_n_chars = surrounding_text.substr(trim_from),
      .surrounding_text_length = text_length};
}

}  // namespace

class FakeSuggesterSwitch : public AssistiveSuggesterSwitch {
 public:
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  explicit FakeSuggesterSwitch(EnabledSuggestions enabled_suggestions)
      : enabled_suggestions_(enabled_suggestions) {}
  ~FakeSuggesterSwitch() override = default;

  // AssistiveSuggesterSwitch overrides
  void FetchEnabledSuggestionsThen(
      FetchEnabledSuggestionsCallback callback,
      const TextInputMethod::InputContext& context) override {
    std::move(callback).Run(enabled_suggestions_);
  }

 private:
  EnabledSuggestions enabled_suggestions_;
};

class AssistiveSuggesterTest : public testing::Test {
 protected:
  AssistiveSuggesterTest() { profile_ = std::make_unique<TestingProfile>(); }

  void SetUp() override {
    suggestion_handler_ = std::make_unique<FakeSuggestionHandler>();
    // TODO(b/242472734): Allow enabled suggestions passed without replace.
    assistive_suggester_ = std::make_unique<AssistiveSuggester>(
        suggestion_handler_.get(), profile_.get(),
        std::make_unique<AssistiveSuggesterClientFilter>(
            base::BindRepeating(&GetFocusedTabUrl),
            base::BindRepeating(&GetFocusedWindowProperties)));

    histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.UserPref.Emoji",
                                         true, 1);
    // Emoji is default to true now, so need to set emoji pref false to test
    // IsAssistiveFeatureEnabled correctly.
    profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, false);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AssistiveSuggester> assistive_suggester_;
  std::unique_ptr<FakeSuggestionHandler> suggestion_handler_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AssistiveSuggesterTest, EmojiSuggestion_UserPrefEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   true);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, false);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest, EmojiSuggestion_EnterprisePrefEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   false);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, true);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest, EmojiSuggestion_BothPrefsEnabledTrue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   true);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, true);

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest, EmojiSuggestion_BothPrefsEnabledFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistMultiWord});
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   false);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, false);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       MultiWordEnabledWhenFeatureFlagEnabledAndPrefEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{});

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true,
                        /*diacritics_on_longpress_enabled=*/false);

  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       MultiWordDisabledWhenFeatureFlagEnabledAndPrefDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{});

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/false);

  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       MultiWordDisabledWhenFeatureFlagDisabledAndPrefDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistMultiWord});

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/false);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       AssistiveDiacriticsLongpressFlagAndPrefEnabled_AssistiveFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kDiacriticsOnPhysicalKeyboardLongpress);
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       AssistiveDiacriticsLongpressFlagDisabled_AssistiveFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kDiacriticsOnPhysicalKeyboardLongpress);
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       AssistiveDiacriticsLongpressPrefDisabled_AssistiveFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kDiacriticsOnPhysicalKeyboardLongpress);
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/false);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       OnlyAssistiveControlVLongpressFlagEnabled_AssistiveFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kClipboardHistoryLongpress},
      /*disabled_features=*/{features::kAssistMultiWord,
                             features::kDiacriticsOnPhysicalKeyboardLongpress});
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/false);
  EXPECT_TRUE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest,
       AssistiveControlVLongpressFlagDisabled_AssistiveFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          features::kClipboardHistoryLongpress, features::kAssistMultiWord,
          features::kDiacriticsOnPhysicalKeyboardLongpress});
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/false);
  EXPECT_FALSE(assistive_suggester_->IsAssistiveFeatureEnabled());
}

TEST_F(AssistiveSuggesterTest, RecordPKDiacriticsPrefEnabledOnActivate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kDiacriticsOnPhysicalKeyboardLongpress);

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.UserPref.PhysicalKeyboardDiacriticsOnLongpress",
      true, 1);
}

TEST_F(AssistiveSuggesterTest, RecordPKDiacriticsPrefDisabledOnActivate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kDiacriticsOnPhysicalKeyboardLongpress);

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/false);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.UserPref.PhysicalKeyboardDiacriticsOnLongpress",
      false, 1);
}

TEST_F(AssistiveSuggesterTest, RecordPredictiveWritingPrefOnActivate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(EnabledSuggestions{}));

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true,
                        /*diacritics_on_longpress_enabled=*/false);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.UserPref.MultiWord", true, 1);
}

TEST_F(AssistiveSuggesterTest, RecordsMultiWordTextInputAsNotAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(EnabledSuggestions{}));

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true,
                        /*diacritics_on_longpress_enabled=*/false);

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

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
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.multi_word_suggestions = true}));

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/false);

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  histogram_tester_.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.InputState", 1);
  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.InputState",
      AssistiveTextInputState::kFeatureBlockedByPreference, 1);
}

TEST_F(AssistiveSuggesterTest,
       RecordMultiWordTextInputAsDisabledByUnsupportedLang) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAssistMultiWord},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.multi_word_suggestions = true}));

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true,
                        /*diacritics_on_longpress_enabled=*/false);

  assistive_suggester_->OnActivate(kSpainSpanishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

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
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.multi_word_suggestions = true}));

  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true,
                        /*diacritics_on_longpress_enabled=*/false);

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  histogram_tester_.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.InputState", 1);
  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.InputState",
      AssistiveTextInputState::kFeatureEnabled, 1);
}

TEST_F(AssistiveSuggesterTest,
       DiacriticsSuggestionNotTriggeredIfShiftDownAndShiftUp) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_EQ(
      assistive_suggester_->OnKeyEvent(PressKeyWithShift(ui::DomCode::US_A)),
      AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  EXPECT_EQ(assistive_suggester_->OnKeyEvent(ReleaseKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandled);
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(AssistiveSuggesterTest,
       DiacriticsSuggestionOnKeyDownLongpressForUSEnglish) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  assistive_suggester_->OnSurroundingTextChanged(u"a", gfx::Range(1));
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), u"à;á;â;ä;æ;ã;å;ā");
}

TEST_F(
    AssistiveSuggesterTest,
    DiacriticsSuggestionDisabledOnKeyDownLongpressForLastSurroundingTextEmpty) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(
    AssistiveSuggesterTest,
    DiacriticsSuggestionDisabledOnKeyDownLongpressForLastSurroundingTextBeforeCursorNotMatch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"xyz", gfx::Range(1));

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(
    AssistiveSuggesterTest,
    DiacriticsSuggestionDisabledOnKeyDownLongpressForLastSurroundingTextCursorPosTooLarge) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"xyz", gfx::Range(10));

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(
    AssistiveSuggesterTest,
    DiacriticsSuggestionDisabledOnKeyDownLongpressForLastSurroundingTextCursorPosZero) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"xyz", gfx::Range(0));

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(AssistiveSuggesterTest, DiacriticsSuggestionOnKeyDownRecordsSuccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  assistive_suggester_->OnSurroundingTextChanged(u"a", gfx::Range(1));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::DIGIT1)),
            AssistiveSuggesterKeyResult::kHandled);

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Success", 1);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.Success",
                                       AssistiveType::kLongpressDiacritics, 1);
}

TEST_F(AssistiveSuggesterTest,
       NoDiacriticsSuggestionOnKeyDownLongpressForUSEnglishOnPrefDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/false);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandled);
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(AssistiveSuggesterTest,
       NoDiacriticsSuggestionOnKeyDownLongpressForNonUSEnglish) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kSpainSpanishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandled);
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(AssistiveSuggesterTest,
       DiacriticsSuggestionOnKeyDownLongpressNotInterruptedByOtherKeys) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  assistive_suggester_->OnSurroundingTextChanged(u"a", gfx::Range(1));
  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::SHIFT_LEFT)),
            AssistiveSuggesterKeyResult::kNotHandled);
  EXPECT_EQ(
      assistive_suggester_->OnKeyEvent(ReleaseKey(ui::DomCode::SHIFT_LEFT)),
      AssistiveSuggesterKeyResult::kNotHandled);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), u"à;á;â;ä;æ;ã;å;ā");
}

TEST_F(AssistiveSuggesterTest,
       DiacriticsSuggestionWithoutContextIgnoresOnKeyDownLongpress) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  assistive_suggester_->OnActivate(kUsEnglishEngineId);

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandled);
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), u"");
}

TEST_F(AssistiveSuggesterTest, DiacriticsSuggestionInterruptedDoesNotSuggest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandled);
  task_environment_.FastForwardBy(
      base::Milliseconds(100));  // Not long enough to trigger longpress.
  EXPECT_EQ(assistive_suggester_->OnKeyEvent(ReleaseKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandled);
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), u"");
}

TEST_F(AssistiveSuggesterTest,
       DoNotPropagateAlphaRepeatKeyIfDiacriticsOnLongpressEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = true}));
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_EQ(
      assistive_suggester_->OnKeyEvent(CreateRepeatKeyEvent(ui::DomCode::US_A)),
      AssistiveSuggesterKeyResult::kHandled);
  task_environment_.FastForwardBy(
      base::Seconds(1));  // Long enough to trigger longpress.
  EXPECT_EQ(assistive_suggester_->OnKeyEvent(ReleaseKey(ui::DomCode::US_A)),
            AssistiveSuggesterKeyResult::kNotHandled);
  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), u"");
}

TEST_F(AssistiveSuggesterTest,
       PropagateAlphaRepeatKeyIfDiacriticsOnLongpressDisabledViaSettings) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/false);
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = false}));
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_EQ(
      assistive_suggester_->OnKeyEvent(CreateRepeatKeyEvent(ui::DomCode::US_A)),
      AssistiveSuggesterKeyResult::kNotHandled);
}

TEST_F(AssistiveSuggesterTest,
       PropagateAlphaRepeatKeyIfDiacriticsOnLongpressDisabledDenylist) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/false);
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(
          EnabledSuggestions{.diacritic_suggestions = false}));
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_EQ(
      assistive_suggester_->OnKeyEvent(CreateRepeatKeyEvent(ui::DomCode::US_A)),
      AssistiveSuggesterKeyResult::kNotHandled);
}

TEST_F(AssistiveSuggesterTest,
       IgnoreAndPropagateNonAlphaRepeatKeyIfDiacriticsOnLongpressEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(
                CreateRepeatKeyEvent(ui::DomCode::ARROW_DOWN)),
            AssistiveSuggesterKeyResult::kNotHandled);
}

TEST_F(AssistiveSuggesterTest, StoreLastEnabledSuggestionOnFocus) {
  EnabledSuggestions enabled_suggestions = EnabledSuggestions{
      .emoji_suggestions = true, .diacritic_suggestions = true};
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(enabled_suggestions));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  EXPECT_TRUE(assistive_suggester_
                  ->get_enabled_suggestion_from_last_onfocus_for_testing()
                  .has_value());
  EXPECT_EQ(*assistive_suggester_
                 ->get_enabled_suggestion_from_last_onfocus_for_testing(),
            enabled_suggestions);
}

TEST_F(AssistiveSuggesterTest, ClearLastEnabledSuggestionOnBlur) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kDiacriticsOnPhysicalKeyboardLongpress},
      /*disabled_features=*/{});
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(EnabledSuggestions{
          .emoji_suggestions = true, .diacritic_suggestions = true}));
  SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/false,
                        /*diacritics_on_longpress_enabled=*/true);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnBlur();

  EXPECT_FALSE(assistive_suggester_
                   ->get_enabled_suggestion_from_last_onfocus_for_testing()
                   .has_value());
}

class AssistiveSuggesterMultiWordTest : public testing::Test {
 protected:
  AssistiveSuggesterMultiWordTest() {
    profile_ = std::make_unique<TestingProfile>();
  }

  void SetUp() override {
    suggestion_handler_ = std::make_unique<FakeSuggestionHandler>();
    // TODO(b/242472734): Allow enabled suggestions passed without replace.
    assistive_suggester_ = std::make_unique<AssistiveSuggester>(
        suggestion_handler_.get(), profile_.get(),
        std::make_unique<FakeSuggesterSwitch>(EnabledSuggestions{
            .multi_word_suggestions = true,
        }));

    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAssistMultiWord},
        /*disabled_features=*/{});

    SetInputMethodOptions(*profile_, /*predictive_writing_enabled=*/true,
                          /*diacritics_on_longpress_enabled=*/false);
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AssistiveSuggester> assistive_suggester_;
  std::unique_ptr<FakeSuggestionHandler> suggestion_handler_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AssistiveSuggesterMultiWordTest,
       MatchMetricNotRecordedWhenZeroSuggestions) {
  assistive_suggester_->OnExternalSuggestionsUpdated({}, TextContext(""));

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Match", 0);
}

TEST_F(AssistiveSuggesterMultiWordTest, OnSuggestionExistShowSuggestion) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext(""));

  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), u"hello there");
}

TEST_F(AssistiveSuggesterMultiWordTest, OnDisabledFlagShouldNotShowSuggestion) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAssistMultiWord});
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext(""));

  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(AssistiveSuggesterMultiWordTest, ShouldNotSuggestWhenSwitchDisabled) {
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(EnabledSuggestions{
          .multi_word_suggestions = false,
      }));
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hello there"}};
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));

  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext(""));

  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(AssistiveSuggesterMultiWordTest,
       MatchMetricRecordedWhenOneOrMoreSuggestions) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext(""));

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
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext(""));

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Match", 0);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       DisableMetricNotRecordedWhenNoSuggestionAndMultiWordBlocked) {
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(EnabledSuggestions{}));

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));
  assistive_suggester_->OnExternalSuggestionsUpdated({}, TextContext(""));

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Disabled.MultiWord",
                                     0);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       DisableMetricRecordedWhenGivenSuggestionAndMultiWordBlocked) {
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(EnabledSuggestions{}));
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext(""));

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Disabled.MultiWord",
                                     1);
  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.MultiWord",
      DisabledReason::kUrlOrAppNotAllowed, 1);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       CoverageMetricNotRecordedWhenNoSuggestionGiven) {
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));
  assistive_suggester_->OnExternalSuggestionsUpdated({}, TextContext(""));

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Coverage", 0);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       CoverageMetricRecordedWhenSuggestionShown) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext(""));

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Coverage", 1);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                       AssistiveType::kMultiWordPrediction, 1);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       CoverageMetricRecordedOnceWhenSuggestionShownAndTracked) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hello there"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext(""));
  assistive_suggester_->OnSurroundingTextChanged(u"h", gfx::Range(1));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext("h"));
  assistive_suggester_->OnSurroundingTextChanged(u"he", gfx::Range(2));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext("he"));
  assistive_suggester_->OnSurroundingTextChanged(u"hel", gfx::Range(3));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext("hel"));

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Coverage", 1);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                       AssistiveType::kMultiWordPrediction, 1);
}

TEST_F(AssistiveSuggesterMultiWordTest,
       CoverageMetricRecordedForEverySuggestionShown) {
  std::vector<AssistiveSuggestion> first_suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "hello there"}};
  std::vector<AssistiveSuggestion> second_suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kPrediction,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "was"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"", gfx::Range(0));
  assistive_suggester_->OnExternalSuggestionsUpdated(first_suggestions,
                                                     TextContext(""));
  assistive_suggester_->OnSurroundingTextChanged(u"h", gfx::Range(1));
  assistive_suggester_->OnExternalSuggestionsUpdated(first_suggestions,
                                                     TextContext("h"));
  assistive_suggester_->OnSurroundingTextChanged(u"he", gfx::Range(2));
  assistive_suggester_->OnExternalSuggestionsUpdated(first_suggestions,
                                                     TextContext("he"));
  assistive_suggester_->OnSurroundingTextChanged(u"he ", gfx::Range(3));
  assistive_suggester_->OnExternalSuggestionsUpdated(second_suggestions,
                                                     TextContext("he "));

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Coverage", 2);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                       AssistiveType::kMultiWordPrediction, 2);
}

TEST_F(AssistiveSuggesterMultiWordTest, PressingTabShouldAcceptSuggestion) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"why ar", gfx::Range(6));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext("why ar"));

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKey(ui::DomCode::TAB)),
            AssistiveSuggesterKeyResult::kHandled);
}

TEST_F(AssistiveSuggesterMultiWordTest, AltPlusTabShouldNotAcceptSuggestion) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"why ar", gfx::Range(6));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext("why ar"));

  EXPECT_EQ(assistive_suggester_->OnKeyEvent(PressKeyWithAlt(ui::DomCode::TAB)),
            AssistiveSuggesterKeyResult::kNotHandled);
}

TEST_F(AssistiveSuggesterMultiWordTest, CtrlPlusTabShouldNotAcceptSuggestion) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"why ar", gfx::Range(6));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext("why ar"));

  EXPECT_EQ(
      assistive_suggester_->OnKeyEvent(PressKeyWithCtrl(ui::DomCode::TAB)),
      AssistiveSuggesterKeyResult::kNotHandled);
}

TEST_F(AssistiveSuggesterMultiWordTest, ShiftPlusTabShouldNotAcceptSuggestion) {
  std::vector<AssistiveSuggestion> suggestions = {
      AssistiveSuggestion{.mode = AssistiveSuggestionMode::kCompletion,
                          .type = AssistiveSuggestionType::kMultiWord,
                          .text = "aren\'t you"}};

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"why ar", gfx::Range(6));
  assistive_suggester_->OnExternalSuggestionsUpdated(suggestions,
                                                     TextContext("why ar"));

  EXPECT_EQ(
      assistive_suggester_->OnKeyEvent(PressKeyWithShift(ui::DomCode::TAB)),
      AssistiveSuggesterKeyResult::kNotHandled);
}

class AssistiveSuggesterEmojiTest : public testing::Test {
 protected:
  AssistiveSuggesterEmojiTest() {
    profile_ = std::make_unique<TestingProfile>();
  }

  void SetUp() override {
    suggestion_handler_ = std::make_unique<FakeSuggestionHandler>();
    // TODO(b/242472734): Allow enabled suggestions passed without replace.
    assistive_suggester_ = std::make_unique<AssistiveSuggester>(
        suggestion_handler_.get(), profile_.get(),
        std::make_unique<FakeSuggesterSwitch>(EnabledSuggestions{
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
  std::unique_ptr<FakeSuggestionHandler> suggestion_handler_;
  base::HistogramTester histogram_tester_;

  // Needs to outlive the emoji_suggester under test.
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;
};

TEST_F(AssistiveSuggesterEmojiTest, ShouldNotSuggestWhenEmojiDisabled) {
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   false);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, false);

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"arrow ", gfx::Range(6));

  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(AssistiveSuggesterEmojiTest, ShouldRecordDisabledWhenEmojiDisabled) {
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed,
                                   false);
  profile_->GetPrefs()->SetBoolean(prefs::kEmojiSuggestionEnabled, false);

  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"arrow ", gfx::Range(6));

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Disabled", 1);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.Disabled",
                                       AssistiveType::kEmoji, 1);
}

TEST_F(AssistiveSuggesterEmojiTest, ShouldNotSuggestWhenSwitchDisabled) {
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(EnabledSuggestions{
          .emoji_suggestions = false,
      }));
  assistive_suggester_->get_emoji_suggester_for_testing()
      ->LoadEmojiMapForTesting(kEmojiData);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  assistive_suggester_->OnSurroundingTextChanged(u"arrow ", gfx::Range(6));

  EXPECT_FALSE(suggestion_handler_->GetShowingSuggestion());
}

TEST_F(AssistiveSuggesterEmojiTest, ShouldRecordNotAllowedWhenSwitchDisabled) {
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(EnabledSuggestions{
          .emoji_suggestions = false,
      }));
  assistive_suggester_->get_emoji_suggester_for_testing()
      ->LoadEmojiMapForTesting(kEmojiData);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  assistive_suggester_->OnSurroundingTextChanged(u"arrow ", gfx::Range(6));

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.NotAllowed", 1);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.NotAllowed",
                                       AssistiveType::kEmoji, 1);
}

TEST_F(AssistiveSuggesterEmojiTest,
       ShouldRecordDisabledReasonWhenSwitchDisabled) {
  // TODO(b/242472734): Allow enabled suggestions passed without replace.
  assistive_suggester_ = std::make_unique<AssistiveSuggester>(
      suggestion_handler_.get(), profile_.get(),
      std::make_unique<FakeSuggesterSwitch>(EnabledSuggestions{
          .emoji_suggestions = false,
      }));
  assistive_suggester_->get_emoji_suggester_for_testing()
      ->LoadEmojiMapForTesting(kEmojiData);
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);

  assistive_suggester_->OnSurroundingTextChanged(u"arrow ", gfx::Range(6));

  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Disabled.Emoji", 1);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.Disabled.Emoji",
                                       DisabledReason::kUrlOrAppNotAllowed, 1);
}

TEST_F(AssistiveSuggesterEmojiTest, ShouldReturnPrefixBasedEmojiSuggestions) {
  assistive_suggester_->OnActivate(kUsEnglishEngineId);
  assistive_suggester_->OnFocus(5, empty_context);
  assistive_suggester_->OnSurroundingTextChanged(u"arrow ", gfx::Range(6));

  EXPECT_TRUE(suggestion_handler_->GetShowingSuggestion());
  EXPECT_EQ(suggestion_handler_->GetSuggestionText(), u"←;↑;→");
}

class AssistiveSuggesterControlVLongpressTest : public AshTestBase {
 protected:
  AssistiveSuggesterControlVLongpressTest()
      : AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))),
        assistive_suggester_(
            &suggestion_handler_,
            &profile_,
            std::make_unique<AssistiveSuggesterClientFilter>(
                base::BindRepeating(&GetFocusedTabUrl),
                base::BindRepeating(&GetFocusedWindowProperties))) {
    feature_list_.InitAndEnableFeature(features::kClipboardHistoryLongpress);
  }

  void SetUp() override {
    AshTestBase::SetUp();

    Shell::Get()
        ->clipboard_history_controller()
        ->set_confirmed_operation_callback_for_test(
            operation_confirmed_future_.GetRepeatingCallback());

    // Write content to the clipboard so that the clipboard history menu can
    // appear.
    SetClipboardText("B");
    SetClipboardText("A");

    // Create a textfield for the clipboard history controller to recognize as a
    // paste target.
    textfield_widget_ = CreateFramelessTestWidget();
    textfield_widget_->SetBounds(gfx::Rect(100, 100, 100, 100));
    textfield_ = textfield_widget_->SetContentsView(
        std::make_unique<views::Textfield>());

    // Set the textfield as the text input client so that its caret position can
    // be queried.
    IMEBridge::Get()
        ->GetInputContextHandler()
        ->GetInputMethod()
        ->SetFocusedTextInputClient(textfield_);
  }

  void SetClipboardText(const std::string& text) {
    ui::ScopedClipboardWriter(ui::ClipboardBuffer::kCopyPaste)
        .WriteText(base::UTF8ToUTF16(text));

    // Clipboard history will post a task to process clipboard data in order to
    // debounce multiple clipboard writes occurring in sequence. Here we give
    // clipboard history the chance to run its posted tasks before proceeding.
    EXPECT_TRUE(operation_confirmed_future_.Take());
  }

  ui::KeyEvent CreateControlVEvent(int extra_flags = ui::EF_NONE) {
    return ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_V,
                        ui::EF_CONTROL_DOWN | extra_flags);
  }

  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  FakeSuggestionHandler suggestion_handler_;
  AssistiveSuggester assistive_suggester_;
  base::test::TestFuture<bool> operation_confirmed_future_;
  std::unique_ptr<views::Widget> textfield_widget_;
  raw_ptr<views::Textfield> textfield_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AssistiveSuggesterControlVLongpressTest,
       ClipboardHistoryTriggeredOnControlVLongpress) {
  assistive_suggester_.OnFocus(5, empty_context);
  EXPECT_EQ(assistive_suggester_.OnKeyEvent(CreateControlVEvent()),
            AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  assistive_suggester_.OnSurroundingTextChanged(u"A", gfx::Range(1));
  task_environment()->FastForwardBy(base::Seconds(1));

  auto* const controller = Shell::Get()->clipboard_history_controller();
  EXPECT_TRUE(controller->IsMenuShowing());
  // Precise anchoring logic may change as time goes on, so this test only
  // assumes that the clipboard history menu should be left-aligned with the
  // input field's caret.
  EXPECT_EQ(controller->GetMenuBoundsInScreenForTest().x(),
            textfield_->GetCaretBounds().x());
  histogram_tester_.ExpectUniqueSample(
      "Ash.ClipboardHistory.ContextMenu.ShowMenu",
      crosapi::mojom::ClipboardHistoryControllerShowSource::kControlVLongpress,
      1);
}

TEST_F(AssistiveSuggesterControlVLongpressTest,
       ControlVLongpressPasteSuccessRecorded) {
  assistive_suggester_.OnFocus(5, empty_context);
  EXPECT_EQ(assistive_suggester_.OnKeyEvent(CreateControlVEvent()),
            AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  assistive_suggester_.OnSurroundingTextChanged(u"A", gfx::Range(1));
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(Shell::Get()->clipboard_history_controller()->IsMenuShowing());

  // Paste an item from the clipboard history menu.
  GetEventGenerator()->PressAndReleaseKey(ui::KeyboardCode::VKEY_DOWN);
  GetEventGenerator()->PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Success", 1);
  histogram_tester_.ExpectUniqueSample("InputMethod.Assistive.Success",
                                       AssistiveType::kLongpressControlV, 1);
}

TEST_F(AssistiveSuggesterControlVLongpressTest,
       ClipboardHistoryDismissedNoSuccessRecorded) {
  assistive_suggester_.OnFocus(5, empty_context);
  EXPECT_EQ(assistive_suggester_.OnKeyEvent(CreateControlVEvent()),
            AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  assistive_suggester_.OnSurroundingTextChanged(u"A", gfx::Range(1));
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_TRUE(Shell::Get()->clipboard_history_controller()->IsMenuShowing());

  // Dismiss the clipboard history menu without pasting.
  GetEventGenerator()->PressAndReleaseKey(ui::KeyboardCode::VKEY_ESCAPE);
  GetEventGenerator()->PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
  histogram_tester_.ExpectTotalCount("InputMethod.Assistive.Success", 0);
}

TEST_F(AssistiveSuggesterControlVLongpressTest,
       ClipboardHistoryNotTriggeredIfShiftDown) {
  assistive_suggester_.OnFocus(5, empty_context);
  EXPECT_EQ(assistive_suggester_.OnKeyEvent(
                CreateControlVEvent(/*extra_flags=*/ui::EF_SHIFT_DOWN)),
            AssistiveSuggesterKeyResult::kNotHandled);
  assistive_suggester_.OnSurroundingTextChanged(u"A", gfx::Range(1));
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_FALSE(Shell::Get()->clipboard_history_controller()->IsMenuShowing());
}

TEST_F(AssistiveSuggesterControlVLongpressTest,
       ClipboardHistoryNotTriggeredIfNoContextForControlVLongpress) {
  EXPECT_EQ(assistive_suggester_.OnKeyEvent(CreateControlVEvent()),
            AssistiveSuggesterKeyResult::kNotHandled);
  assistive_suggester_.OnSurroundingTextChanged(u"A", gfx::Range(1));
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_FALSE(Shell::Get()->clipboard_history_controller()->IsMenuShowing());
}

TEST_F(AssistiveSuggesterControlVLongpressTest,
       ClipboardHistoryNotTriggeredIfControlVLongpressInterrupted) {
  EXPECT_EQ(assistive_suggester_.OnKeyEvent(CreateControlVEvent()),
            AssistiveSuggesterKeyResult::kNotHandled);
  assistive_suggester_.OnSurroundingTextChanged(u"A", gfx::Range(1));
  task_environment()->FastForwardBy(
      base::Milliseconds(100));  // Not long enough to trigger longpress.

  EXPECT_EQ(assistive_suggester_.OnKeyEvent(ui::KeyEvent(
                ui::EventType::kKeyReleased, ui::VKEY_V, ui::EF_CONTROL_DOWN)),
            AssistiveSuggesterKeyResult::kNotHandled);
  EXPECT_FALSE(Shell::Get()->clipboard_history_controller()->IsMenuShowing());
}

TEST_F(AssistiveSuggesterControlVLongpressTest,
       RepeatedControlVNotPropagatedIfControlVLongpressEnabled) {
  assistive_suggester_.OnFocus(5, empty_context);
  EXPECT_EQ(assistive_suggester_.OnKeyEvent(CreateControlVEvent()),
            AssistiveSuggesterKeyResult::kNotHandledSuppressAutoRepeat);
  assistive_suggester_.OnSurroundingTextChanged(u"A", gfx::Range(1));

  EXPECT_EQ(assistive_suggester_.OnKeyEvent(
                CreateControlVEvent(/*extra_flags=*/ui::EF_IS_REPEAT)),
            AssistiveSuggesterKeyResult::kHandled);
}
}  // namespace ash::input_method
