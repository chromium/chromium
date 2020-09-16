// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/chromeos/input_method/suggestion_enums.h"
#include "chrome/browser/chromeos/input_method/textinput_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "chromeos/services/ime/decoder/decoder_engine.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/ime_engine_handler_interface.h"
#include "ui/base/ime/chromeos/input_method_chromeos.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

using chromeos::InputMethodEngineBase;

namespace {

constexpr char kEmojiData[] = "happy,😀;😃;😄";

class TestObserver : public InputMethodEngineBase::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void OnActivate(const std::string& engine_id) override {}
  void OnDeactivated(const std::string& engine_id) override {}
  void OnFocus(
      const ui::IMEEngineHandlerInterface::InputContext& context) override {}
  void OnBlur(int context_id) override {}
  void OnKeyEvent(
      const std::string& engine_id,
      const InputMethodEngineBase::KeyboardEvent& event,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) override {
    std::move(callback).Run(/*handled=*/false);
  }
  void OnInputContextUpdate(
      const ui::IMEEngineHandlerInterface::InputContext& context) override {}
  void OnCandidateClicked(
      const std::string& engine_id,
      int candidate_id,
      InputMethodEngineBase::MouseButtonEvent button) override {}
  void OnMenuItemActivated(const std::string& engine_id,
                           const std::string& menu_id) override {}
  void OnSurroundingTextChanged(const std::string& engine_id,
                                const base::string16& text,
                                int cursor_pos,
                                int anchor_pos,
                                int offset) override {}
  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {}
  void OnScreenProjectionChanged(bool is_projected) override {}
  void OnReset(const std::string& engine_id) override {}
  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override {}
  void OnInputMethodOptionsChanged(const std::string& engine_id) override {
    changed_engine_id_ = engine_id;
  }
  void ClearChangedEngineId() { changed_engine_id_ = ""; }
  std::string GetChangedEngineId() { return changed_engine_id_; }

 private:
  std::string changed_engine_id_ = "";
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestPersonalDataManagerObserver
    : public autofill::PersonalDataManagerObserver {
 public:
  explicit TestPersonalDataManagerObserver(Profile* profile)
      : profile_(profile) {
    autofill::PersonalDataManagerFactory::GetForProfile(profile_)->AddObserver(
        this);
  }
  ~TestPersonalDataManagerObserver() override {}

  // Waits for the PersonalDataManager's list of profiles to be updated.
  void Wait() {
    run_loop_.Run();
    autofill::PersonalDataManagerFactory::GetForProfile(profile_)
        ->RemoveObserver(this);
  }

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override { run_loop_.Quit(); }

 private:
  Profile* profile_;
  base::RunLoop run_loop_;
};

class KeyProcessingWaiter {
 public:
  ui::IMEEngineHandlerInterface::KeyEventDoneCallback CreateCallback() {
    return base::BindOnce(&KeyProcessingWaiter::OnKeyEventDone,
                          base::Unretained(this));
  }

  void OnKeyEventDone(bool consumed) { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

class NativeInputMethodEngineTest : public InProcessBrowserTest,
                                    public ui::internal::InputMethodDelegate {
 public:
  NativeInputMethodEngineTest() : input_method_(this) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kAssistPersonalInfo,
                              chromeos::features::kAssistPersonalInfoEmail,
                              chromeos::features::kAssistPersonalInfoName,
                              chromeos::features::kEmojiSuggestAddition},
        /*disabled_features=*/{});
  }

 protected:
  void SetUp() override {
    chromeos::ime::FakeEngineMainEntryForTesting();
    mojo::core::Init();
    InProcessBrowserTest::SetUp();
    ui::IMEBridge::Initialize();
  }

  void SetUpOnMainThread() override {
    ui::IMEBridge::Get()->SetInputContextHandler(&input_method_);
    ui::IMEBridge::Get()->SetCurrentEngineHandler(&engine_);

    auto observer = std::make_unique<TestObserver>();
    observer_ = observer.get();

    profile_ = browser()->profile();
    prefs_ = profile_->GetPrefs();
    prefs_->Set(prefs::kLanguageInputMethodSpecificSettings,
                base::DictionaryValue());
    engine_.Initialize(std::move(observer), "", profile_);
    engine_.get_assistive_suggester_for_testing()
        ->get_emoji_suggester_for_testing()
        ->LoadEmojiMapForTesting(kEmojiData);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override { engine_.Reset(); }

  void SetUpTextInput(chromeos::TextInputTestHelper& helper) {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("textinput")),
        base::FilePath(FILE_PATH_LITERAL("simple_textarea.html")));
    ui_test_utils::NavigateToURL(browser(), url);

    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    ASSERT_TRUE(content::ExecuteScript(
        tab, "document.getElementById('text_id').focus()"));
    helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT_AREA);

    SetFocus(helper.GetTextInputClient());
  }

  // Overridden from ui::internal::InputMethodDelegate:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override {
    return ui::EventDispatchDetails();
  }

  void DispatchKeyPress(ui::KeyboardCode code,
                        bool need_flush,
                        int flags = ui::EF_NONE) {
    KeyProcessingWaiter waiterPressed;
    KeyProcessingWaiter waiterReleased;
    engine_.ProcessKeyEvent({ui::ET_KEY_PRESSED, code, flags},
                            waiterPressed.CreateCallback());
    engine_.ProcessKeyEvent({ui::ET_KEY_RELEASED, code, flags},
                            waiterReleased.CreateCallback());
    if (need_flush)
      engine_.FlushForTesting();

    waiterPressed.Wait();
    waiterReleased.Wait();
  }

  void SetFocus(ui::TextInputClient* client) {
    input_method_.SetFocusedTextInputClient(client);
  }

  ui::InputMethod* GetBrowserInputMethod() {
    return browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod();
  }

  chromeos::NativeInputMethodEngine engine_;
  Profile* profile_;
  PrefService* prefs_;
  TestObserver* observer_;

 private:
  ui::InputMethodChromeOS input_method_;
  base::test::ScopedFeatureList feature_list_;
};

// ID is specified in google_xkb_manifest.json.
constexpr char kEngineIdArabic[] = "vkd_ar";
constexpr char kEngineIdUs[] = "xkb:us::eng";
constexpr char kEngineIdVietnameseTelex[] = "vkd_vi_telex";

}  // namespace

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       VietnameseTelex_SimpleTransform) {
  engine_.Enable(kEngineIdVietnameseTelex);
  engine_.FlushForTesting();
  EXPECT_TRUE(engine_.IsConnectedForTesting());

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true, ui::EF_SHIFT_DOWN);
  DispatchKeyPress(ui::VKEY_S, true);
  DispatchKeyPress(ui::VKEY_SPACE, true);

  // Expect to commit 'Á '.
  ASSERT_EQ(text_input_client.composition_history().size(), 2U);
  EXPECT_EQ(text_input_client.composition_history()[0].text,
            base::ASCIIToUTF16("A"));
  EXPECT_EQ(text_input_client.composition_history()[1].text,
            base::UTF8ToUTF16(u8"\u00c1"));
  ASSERT_EQ(text_input_client.insert_text_history().size(), 1U);
  EXPECT_EQ(text_input_client.insert_text_history()[0],
            base::UTF8ToUTF16(u8"\u00c1 "));

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, VietnameseTelex_Reset) {
  engine_.Enable(kEngineIdVietnameseTelex);
  engine_.FlushForTesting();
  EXPECT_TRUE(engine_.IsConnectedForTesting());

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true);
  engine_.Reset();
  DispatchKeyPress(ui::VKEY_S, true);

  // Expect to commit 's'.
  ASSERT_EQ(text_input_client.composition_history().size(), 1U);
  EXPECT_EQ(text_input_client.composition_history()[0].text,
            base::ASCIIToUTF16("a"));
  ASSERT_EQ(text_input_client.insert_text_history().size(), 1U);
  EXPECT_EQ(text_input_client.insert_text_history()[0],
            base::ASCIIToUTF16("s"));

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, SwitchActiveController) {
  // Swap between two controllers.
  engine_.Enable(kEngineIdVietnameseTelex);
  engine_.FlushForTesting();
  engine_.Disable();
  engine_.Enable(kEngineIdArabic);
  engine_.FlushForTesting();

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true);

  // Expect to commit 'ش'.
  ASSERT_EQ(text_input_client.composition_history().size(), 0U);
  ASSERT_EQ(text_input_client.insert_text_history().size(), 1U);
  EXPECT_EQ(text_input_client.insert_text_history()[0],
            base::UTF8ToUTF16(u8"ش"));

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, NoActiveController) {
  engine_.Enable(kEngineIdVietnameseTelex);
  engine_.FlushForTesting();
  engine_.Disable();

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true);
  engine_.Reset();

  // Expect no changes.
  ASSERT_EQ(text_input_client.composition_history().size(), 0U);
  ASSERT_EQ(text_input_client.insert_text_history().size(), 0U);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, SuggestUserEmail) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToAccept.PersonalInfo", 0);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile_);
  signin::SetPrimaryAccount(identity_manager, "johnwayne@me.xyz");

  engine_.Enable(kEngineIdUs);

  chromeos::TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);

  const base::string16 prefix_text = base::UTF8ToUTF16("my email is ");
  const base::string16 expected_result_text =
      base::UTF8ToUTF16("my email is johnwayne@me.xyz");

  helper.GetTextInputClient()->InsertText(prefix_text);
  helper.WaitForSurroundingTextChanged(prefix_text);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Match",
                                      chromeos::AssistiveType::kPersonalEmail,
                                      1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                      chromeos::AssistiveType::kPersonalEmail,
                                      1);
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToAccept.PersonalInfo", 0);

  DispatchKeyPress(ui::VKEY_DOWN, false);
  DispatchKeyPress(ui::VKEY_RETURN, false);
  helper.WaitForSurroundingTextChanged(expected_result_text);

  EXPECT_EQ(expected_result_text, helper.GetSurroundingText());
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Success",
                                      chromeos::AssistiveType::kPersonalEmail,
                                      1);
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToAccept.PersonalInfo", 1);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       DismissPersonalInfoSuggestion) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToDismiss.PersonalInfo", 0);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile_);
  signin::SetPrimaryAccount(identity_manager, "johnwayne@me.xyz");

  engine_.Enable(kEngineIdUs);

  chromeos::TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);

  const base::string16 prefix_text = base::UTF8ToUTF16("my email is ");
  const base::string16 expected_result_text =
      base::UTF8ToUTF16("my email is john@abc.com");

  helper.GetTextInputClient()->InsertText(prefix_text);
  helper.WaitForSurroundingTextChanged(prefix_text);
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToDismiss.PersonalInfo", 0);

  DispatchKeyPress(ui::VKEY_ESCAPE, false);
  // This down and enter should make no effect.
  DispatchKeyPress(ui::VKEY_DOWN, false);
  DispatchKeyPress(ui::VKEY_RETURN, false);
  helper.GetTextInputClient()->InsertText(base::UTF8ToUTF16("john@abc.com"));
  helper.WaitForSurroundingTextChanged(expected_result_text);

  EXPECT_EQ(expected_result_text, helper.GetSurroundingText());
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Success",
                                      chromeos::AssistiveType::kPersonalEmail,
                                      0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToDismiss.PersonalInfo", 1);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, SuggestUserName) {
  base::HistogramTester histogram_tester;

  TestPersonalDataManagerObserver personal_data_observer(profile_);
  autofill::AutofillProfile autofill_profile(base::GenerateGUID(),
                                             autofill::test::kEmptyOrigin);
  autofill_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL,
                              base::UTF8ToUTF16("John Wayne"));
  autofill::PersonalDataManagerFactory::GetForProfile(profile_)->AddProfile(
      autofill_profile);
  personal_data_observer.Wait();

  engine_.Enable(kEngineIdUs);

  chromeos::TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);

  const base::string16 prefix_text = base::UTF8ToUTF16("my name is ");
  const base::string16 expected_result_text =
      base::UTF8ToUTF16("my name is John Wayne");

  helper.GetTextInputClient()->InsertText(prefix_text);
  helper.WaitForSurroundingTextChanged(prefix_text);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.PersonalInfo",
      chromeos::DisabledReason::kNone, 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Match", chromeos::AssistiveType::kPersonalName, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                      chromeos::AssistiveType::kPersonalName,
                                      1);

  // Keep typing
  helper.GetTextInputClient()->InsertText(base::UTF8ToUTF16("jo"));
  helper.WaitForSurroundingTextChanged(base::UTF8ToUTF16("my name is jo"));

  DispatchKeyPress(ui::VKEY_DOWN, false);
  DispatchKeyPress(ui::VKEY_RETURN, false);
  helper.WaitForSurroundingTextChanged(expected_result_text);

  EXPECT_EQ(expected_result_text, helper.GetSurroundingText());

  // Make sure we do not emit multiple Coverage metrics when users keep typing.
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                      chromeos::AssistiveType::kPersonalName,
                                      1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Success",
                                      chromeos::AssistiveType::kPersonalName,
                                      1);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       PersonalInfoDisabledReasonkUserSettingsOff) {
  base::HistogramTester histogram_tester;
  prefs_->SetBoolean(chromeos::prefs::kAssistPersonalInfoEnabled, false);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "my name is ");

  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.PersonalInfo",
      chromeos::DisabledReason::kUserSettingsOff, 1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       PersonalInfoDisabledReasonkUrlOrAppNotAllowed) {
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "my name is ");

  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.PersonalInfo",
      chromeos::DisabledReason::kUrlOrAppNotAllowed, 1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, SuggestEmoji) {
  base::HistogramTester histogram_tester;
  engine_.Enable(kEngineIdUs);
  chromeos::TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);
  const base::string16 prefix_text = base::UTF8ToUTF16("happy ");
  const base::string16 expected_result_text = base::UTF8ToUTF16("happy 😀");

  helper.GetTextInputClient()->InsertText(prefix_text);
  helper.WaitForSurroundingTextChanged(prefix_text);
  // Selects first emoji.
  DispatchKeyPress(ui::VKEY_DOWN, false);
  DispatchKeyPress(ui::VKEY_RETURN, false);
  helper.WaitForSurroundingTextChanged(expected_result_text);

  EXPECT_EQ(expected_result_text, helper.GetSurroundingText());
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Match",
                                      chromeos::AssistiveType::kEmoji, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Disabled.Emoji",
                                      chromeos::DisabledReason::kNone, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                      chromeos::AssistiveType::kEmoji, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Success",
                                      chromeos::AssistiveType::kEmoji, 1);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       DismissEmojiSuggestionWhenUsersContinueTyping) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToDismiss.Emoji",
                                    0);
  engine_.Enable(kEngineIdUs);
  chromeos::TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);
  const base::string16 prefix_text = base::UTF8ToUTF16("happy ");
  const base::string16 expected_result_text = base::UTF8ToUTF16("happy a");

  helper.GetTextInputClient()->InsertText(prefix_text);
  helper.WaitForSurroundingTextChanged(prefix_text);
  // Types something random to dismiss emoji
  helper.GetTextInputClient()->InsertText(base::UTF8ToUTF16("a"));
  helper.WaitForSurroundingTextChanged(expected_result_text);

  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToDismiss.Emoji",
                                    1);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       EmojiSuggestionDisabledReasonkEnterpriseSettingsOff) {
  base::HistogramTester histogram_tester;
  prefs_->SetBoolean(chromeos::prefs::kEmojiSuggestionEnterpriseAllowed, false);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "happy ");

  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.Emoji",
      chromeos::DisabledReason::kEnterpriseSettingsOff, 1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       EmojiSuggestionDisabledReasonkUserSettingsOff) {
  base::HistogramTester histogram_tester;
  prefs_->SetBoolean(chromeos::prefs::kEmojiSuggestionEnabled, false);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "happy ");

  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.Emoji",
      chromeos::DisabledReason::kUserSettingsOff, 1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       EmojiSuggestionDisabledReasonkUrlOrAppNotAllowed) {
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "happy ");

  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.Emoji",
      chromeos::DisabledReason::kUrlOrAppNotAllowed, 1);
}

IN_PROC_BROWSER_TEST_F(
    NativeInputMethodEngineTest,
    OnLearnMoreButtonClickedOpensEmojiSuggestionSettingsPage) {
  base::UserActionTester user_action_tester;
  ui::ime::AssistiveWindowButton button;
  button.id = ui::ime::ButtonId::kLearnMore;
  button.window_type = ui::ime::AssistiveWindowType::kEmojiSuggestion;

  engine_.AssistiveWindowButtonClicked(button);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ChromeOS.Settings.SmartInputs.EmojiSuggestions.Open"));
}

IN_PROC_BROWSER_TEST_F(
    NativeInputMethodEngineTest,
    OnSettingLinkButtonClickedOpensPersonalInfoSuggestionSettingsPage) {
  base::UserActionTester user_action_tester;
  ui::ime::AssistiveWindowButton button;
  button.id = ui::ime::ButtonId::kSmartInputsSettingLink;

  engine_.AssistiveWindowButtonClicked(button);

  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                "ChromeOS.Settings.SmartInputs.PersonalInfoSuggestions.Open"));
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       FiresOnInputMethodOptionsChangedEvent) {
  base::DictionaryValue settings;

  // Add key will trigger event.
  base::Value pinyin1(base::Value::Type::DICTIONARY);
  pinyin1.SetBoolKey("foo", true);
  settings.SetPath("pinyin", std::move(pinyin1));
  prefs_->Set(prefs::kLanguageInputMethodSpecificSettings, settings);
  EXPECT_EQ(observer_->GetChangedEngineId(), "pinyin");
  observer_->ClearChangedEngineId();

  // Change key will trigger event.
  base::Value pinyin2(base::Value::Type::DICTIONARY);
  pinyin2.SetBoolKey("foo", false);
  settings.SetPath("pinyin", std::move(pinyin2));
  prefs_->Set(prefs::kLanguageInputMethodSpecificSettings, settings);
  EXPECT_EQ(observer_->GetChangedEngineId(), "pinyin");
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, DestroyProfile) {
  EXPECT_NE(engine_.GetPrefChangeRegistrarForTesting(), nullptr);
  profile_->MaybeSendDestroyedNotification();
  EXPECT_EQ(engine_.GetPrefChangeRegistrarForTesting(), nullptr);
}

class NativeInputMethodEngineAssistiveOff : public InProcessBrowserTest {
 public:
  NativeInputMethodEngineAssistiveOff() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kAssistPersonalInfoName},
        /*disabled_features=*/{chromeos::features::kAssistPersonalInfo,
                               chromeos::features::kEmojiSuggestAddition});
  }
  ~NativeInputMethodEngineAssistiveOff() override = default;

 protected:
  void SetUp() override {
    InProcessBrowserTest::SetUp();
    ui::IMEBridge::Initialize();
  }

  void SetUpOnMainThread() override {
    ui::IMEBridge::Get()->SetCurrentEngineHandler(&engine_);

    auto observer = std::make_unique<TestObserver>();
    observer_ = observer.get();

    profile_ = browser()->profile();
    engine_.Initialize(std::move(observer), "", profile_);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override { engine_.Reset(); }

  chromeos::NativeInputMethodEngine engine_;
  Profile* profile_;
  TestObserver* observer_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineAssistiveOff,
                       PersonalInfoSuggestionDisabledReasonkFeatureFlagOff) {
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "my name is ");

  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.PersonalInfo",
      chromeos::DisabledReason::kFeatureFlagOff, 1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineAssistiveOff,
                       EmojiSuggestionDisabledReasonkFeatureFlagOff) {
  base::HistogramTester histogram_tester;
  engine_.get_assistive_suggester_for_testing()
      ->get_emoji_suggester_for_testing()
      ->LoadEmojiMapForTesting(kEmojiData);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "happy ");

  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Disabled.Emoji",
                                      chromeos::DisabledReason::kFeatureFlagOff,
                                      1);
}
