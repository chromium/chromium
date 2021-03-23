// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/guid.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/chromeos/input_method/assistive_window_controller.h"
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

namespace chromeos {
namespace {

constexpr char kEmojiData[] = "happy,😀;😃;😄";

// TODO(crbug.com/1148157): Use StubInputMethodEngineObserver.
class TestObserver : public InputMethodEngineBase::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  void OnActivate(const std::string& engine_id) override {}
  void OnDeactivated(const std::string& engine_id) override {}
  void OnFocus(
      int context_id,
      const ui::IMEEngineHandlerInterface::InputContext& context) override {}
  void OnBlur(int context_id) override {}
  void OnKeyEvent(
      const std::string& engine_id,
      const ui::KeyEvent& event,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback) override {
    std::move(callback).Run(/*handled=*/false);
  }
  void OnCandidateClicked(
      const std::string& engine_id,
      int candidate_id,
      InputMethodEngineBase::MouseButtonEvent button) override {}
  void OnMenuItemActivated(const std::string& engine_id,
                           const std::string& menu_id) override {}
  void OnSurroundingTextChanged(const std::string& engine_id,
                                const std::u16string& text,
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
  const std::string& changed_engine_id() const { return changed_engine_id_; }

 private:
  std::string changed_engine_id_;
};

class TestPersonalDataManagerObserver
    : public autofill::PersonalDataManagerObserver {
 public:
  explicit TestPersonalDataManagerObserver(Profile* profile) {
    observed_personal_data_manager_.Observe(
        autofill::PersonalDataManagerFactory::GetForProfile(profile));
  }
  ~TestPersonalDataManagerObserver() override = default;

  // Waits for the PersonalDataManager's list of profiles to be updated.
  void Wait() {
    run_loop_.Run();
  }

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
  base::ScopedObservation<autofill::PersonalDataManager,
                          autofill::PersonalDataManagerObserver>
      observed_personal_data_manager_{this};
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
        /*enabled_features=*/{features::kAssistPersonalInfo,
                              features::kAssistPersonalInfoEmail,
                              features::kAssistPersonalInfoName,
                              features::kEmojiSuggestAddition},
        /*disabled_features=*/{});
  }

 protected:
  void SetUp() override {
    mojo::core::Init();
    InProcessBrowserTest::SetUp();
    ui::IMEBridge::Initialize();
  }

  void SetUpOnMainThread() override {
    engine_ = std::make_unique<NativeInputMethodEngine>();
    ui::IMEBridge::Get()->SetInputContextHandler(&input_method_);
    ui::IMEBridge::Get()->SetCurrentEngineHandler(engine_.get());

    auto observer = std::make_unique<TestObserver>();
    observer_ = observer.get();

    profile_ = browser()->profile();
    prefs_ = profile_->GetPrefs();
    prefs_->Set(::prefs::kLanguageInputMethodSpecificSettings,
                base::DictionaryValue());
    engine_->Initialize(std::move(observer), /*extension_id=*/"", profile_);
    engine_->get_assistive_suggester_for_testing()
        ->get_emoji_suggester_for_testing()
        ->LoadEmojiMapForTesting(kEmojiData);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // Reset the engine before shutting down the browser because the engine
    // observes ChromeKeyboardControllerClient, which is tied to the browser
    // lifetime.
    engine_.reset();
    ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
    ui::IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpTextInput(TextInputTestHelper& helper) {
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
    engine_->ProcessKeyEvent({ui::ET_KEY_PRESSED, code, flags},
                             waiterPressed.CreateCallback());
    engine_->ProcessKeyEvent({ui::ET_KEY_RELEASED, code, flags},
                             waiterReleased.CreateCallback());
    if (need_flush)
      engine_->FlushForTesting();

    waiterPressed.Wait();
    waiterReleased.Wait();
  }

  void DispatchKeyPresses(const std::vector<ui::KeyboardCode>& codes,
                          bool need_flush) {
    for (const ui::KeyboardCode& code : codes) {
      DispatchKeyPress(code, need_flush);
    }
  }

  void SetFocus(ui::TextInputClient* client) {
    input_method_.SetFocusedTextInputClient(client);
  }

  ui::InputMethod* GetBrowserInputMethod() {
    return browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod();
  }

  std::unique_ptr<NativeInputMethodEngine> engine_;
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
  engine_->Enable(kEngineIdVietnameseTelex);
  engine_->FlushForTesting();
  EXPECT_TRUE(engine_->IsConnectedForTesting());

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true, ui::EF_SHIFT_DOWN);
  DispatchKeyPress(ui::VKEY_S, true);
  DispatchKeyPress(ui::VKEY_SPACE, true);

  // Expect to commit 'Á '.
  ASSERT_EQ(text_input_client.composition_history().size(), 2U);
  EXPECT_EQ(text_input_client.composition_history()[0].text, u"A");
  EXPECT_EQ(text_input_client.composition_history()[1].text,
            base::UTF8ToUTF16(u8"\u00c1"));
  ASSERT_EQ(text_input_client.insert_text_history().size(), 1U);
  EXPECT_EQ(text_input_client.insert_text_history()[0],
            base::UTF8ToUTF16(u8"\u00c1 "));

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, VietnameseTelex_Reset) {
  engine_->Enable(kEngineIdVietnameseTelex);
  engine_->FlushForTesting();
  EXPECT_TRUE(engine_->IsConnectedForTesting());

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true);
  engine_->Reset();
  DispatchKeyPress(ui::VKEY_S, true);

  // Expect to commit 's'.
  ASSERT_EQ(text_input_client.composition_history().size(), 1U);
  EXPECT_EQ(text_input_client.composition_history()[0].text, u"a");
  ASSERT_EQ(text_input_client.insert_text_history().size(), 1U);
  EXPECT_EQ(text_input_client.insert_text_history()[0], u"s");

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, SwitchActiveController) {
  // Swap between two controllers.
  engine_->Enable(kEngineIdVietnameseTelex);
  engine_->FlushForTesting();
  engine_->Disable();
  engine_->Enable(kEngineIdArabic);
  engine_->FlushForTesting();

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
  engine_->Enable(kEngineIdVietnameseTelex);
  engine_->FlushForTesting();
  engine_->Disable();

  // Create a fake text field.
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  DispatchKeyPress(ui::VKEY_A, true);
  engine_->Reset();

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

  engine_->Enable(kEngineIdUs);

  TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);

  const std::u16string prefix_text = u"my email is ";
  const std::u16string expected_result_text = u"my email is johnwayne@me.xyz";

  helper.GetTextInputClient()->InsertText(
      prefix_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(prefix_text);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Match",
                                      AssistiveType::kPersonalEmail, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                      AssistiveType::kPersonalEmail, 1);
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToAccept.PersonalInfo", 0);

  DispatchKeyPress(ui::VKEY_DOWN, false);
  DispatchKeyPress(ui::VKEY_RETURN, false);
  helper.WaitForSurroundingTextChanged(expected_result_text);

  EXPECT_EQ(expected_result_text, helper.GetSurroundingText());
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Success",
                                      AssistiveType::kPersonalEmail, 1);
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

  engine_->Enable(kEngineIdUs);

  TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);

  const std::u16string prefix_text = u"my email is ";
  const std::u16string expected_result_text = u"my email is john@abc.com";

  helper.GetTextInputClient()->InsertText(
      prefix_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(prefix_text);
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.TimeToDismiss.PersonalInfo", 0);

  DispatchKeyPress(ui::VKEY_ESCAPE, false);
  // This down and enter should make no effect.
  DispatchKeyPress(ui::VKEY_DOWN, false);
  DispatchKeyPress(ui::VKEY_RETURN, false);
  helper.GetTextInputClient()->InsertText(
      u"john@abc.com",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(expected_result_text);

  EXPECT_EQ(expected_result_text, helper.GetSurroundingText());
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Success",
                                      AssistiveType::kPersonalEmail, 0);
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
                              u"John Wayne");
  autofill::PersonalDataManagerFactory::GetForProfile(profile_)->AddProfile(
      autofill_profile);
  personal_data_observer.Wait();

  engine_->Enable(kEngineIdUs);

  TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);

  const std::u16string prefix_text = u"my name is ";
  const std::u16string expected_result_text = u"my name is John Wayne";

  helper.GetTextInputClient()->InsertText(
      prefix_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(prefix_text);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.PersonalInfo", DisabledReason::kNone, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Match",
                                      AssistiveType::kPersonalName, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                      AssistiveType::kPersonalName, 1);

  // Keep typing
  helper.GetTextInputClient()->InsertText(
      u"jo",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(u"my name is jo");

  DispatchKeyPress(ui::VKEY_DOWN, false);
  DispatchKeyPress(ui::VKEY_RETURN, false);
  helper.WaitForSurroundingTextChanged(expected_result_text);

  EXPECT_EQ(expected_result_text, helper.GetSurroundingText());

  // Make sure we do not emit multiple Coverage metrics when users keep typing.
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                      AssistiveType::kPersonalName, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Success",
                                      AssistiveType::kPersonalName, 1);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       PersonalInfoDisabledReasonkUserSettingsOff) {
  base::HistogramTester histogram_tester;
  prefs_->SetBoolean(prefs::kAssistPersonalInfoEnabled, false);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "my name is ");

  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.PersonalInfo",
      DisabledReason::kUserSettingsOff, 1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       PersonalInfoDisabledReasonkUrlOrAppNotAllowed) {
  base::HistogramTester histogram_tester;

  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.PersonalInfo",
      DisabledReason::kUrlOrAppNotAllowed, 0);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.NotAllowed",
                                      chromeos::AssistiveType::kPersonalName,
                                      0);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "my name is ");

  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.Disabled.PersonalInfo",
      DisabledReason::kUrlOrAppNotAllowed, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.NotAllowed",
                                      chromeos::AssistiveType::kPersonalName,
                                      1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, SuggestEmoji) {
  base::HistogramTester histogram_tester;
  engine_->Enable(kEngineIdUs);
  TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);
  const std::u16string prefix_text = u"happy ";
  const std::u16string expected_result_text = u"happy 😀";

  helper.GetTextInputClient()->InsertText(
      prefix_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(prefix_text);
  // Selects first emoji.
  DispatchKeyPress(ui::VKEY_DOWN, false);
  DispatchKeyPress(ui::VKEY_RETURN, false);
  helper.WaitForSurroundingTextChanged(expected_result_text);

  EXPECT_EQ(expected_result_text, helper.GetSurroundingText());
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Match",
                                      AssistiveType::kEmoji, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Disabled.Emoji",
                                      DisabledReason::kNone, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Coverage",
                                      AssistiveType::kEmoji, 1);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Success",
                                      AssistiveType::kEmoji, 1);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       DismissEmojiSuggestionWhenUsersContinueTyping) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToDismiss.Emoji",
                                    0);
  engine_->Enable(kEngineIdUs);
  TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);
  const std::u16string prefix_text = u"happy ";
  const std::u16string expected_result_text = u"happy a";

  helper.GetTextInputClient()->InsertText(
      prefix_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(prefix_text);
  // Types something random to dismiss emoji
  helper.GetTextInputClient()->InsertText(
      u"a",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(expected_result_text);

  histogram_tester.ExpectTotalCount("InputMethod.Assistive.TimeToDismiss.Emoji",
                                    1);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       EmojiSuggestionDisabledReasonkEnterpriseSettingsOff) {
  base::HistogramTester histogram_tester;
  prefs_->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed, false);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "happy ");

  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Disabled.Emoji",
                                      DisabledReason::kEnterpriseSettingsOff,
                                      1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       EmojiSuggestionDisabledReasonkUserSettingsOff) {
  base::HistogramTester histogram_tester;
  prefs_->SetBoolean(prefs::kEmojiSuggestionEnabled, false);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "happy ");

  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Disabled.Emoji",
                                      DisabledReason::kUserSettingsOff, 1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       EmojiSuggestionDisabledReasonkUrlOrAppNotAllowed) {
  base::HistogramTester histogram_tester;

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "happy ");

  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Disabled.Emoji",
                                      DisabledReason::kUrlOrAppNotAllowed, 1);
}

IN_PROC_BROWSER_TEST_F(
    NativeInputMethodEngineTest,
    OnLearnMoreButtonClickedOpensEmojiSuggestionSettingsPage) {
  base::UserActionTester user_action_tester;
  ui::ime::AssistiveWindowButton button;
  button.id = ui::ime::ButtonId::kLearnMore;
  button.window_type = ui::ime::AssistiveWindowType::kEmojiSuggestion;

  engine_->AssistiveWindowButtonClicked(button);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ChromeOS.Settings.SmartInputs.EmojiSuggestions.Open"));
}

IN_PROC_BROWSER_TEST_F(
    NativeInputMethodEngineTest,
    OnSettingLinkButtonClickedOpensPersonalInfoSuggestionSettingsPage) {
  base::UserActionTester user_action_tester;
  ui::ime::AssistiveWindowButton button;
  button.id = ui::ime::ButtonId::kSmartInputsSettingLink;

  engine_->AssistiveWindowButtonClicked(button);

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
  prefs_->Set(::prefs::kLanguageInputMethodSpecificSettings, settings);
  EXPECT_EQ(observer_->changed_engine_id(), "pinyin");
  observer_->ClearChangedEngineId();

  // Change key will trigger event.
  base::Value pinyin2(base::Value::Type::DICTIONARY);
  pinyin2.SetBoolKey("foo", false);
  settings.SetPath("pinyin", std::move(pinyin2));
  prefs_->Set(::prefs::kLanguageInputMethodSpecificSettings, settings);
  EXPECT_EQ(observer_->changed_engine_id(), "pinyin");
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, DestroyProfile) {
  EXPECT_NE(engine_->GetPrefChangeRegistrarForTesting(), nullptr);
  profile_->MaybeSendDestroyedNotification();
  EXPECT_EQ(engine_->GetPrefChangeRegistrarForTesting(), nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       HighlightsOnAutocorrectThenDismissesHighlight) {
  engine_->Enable(kEngineIdUs);
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);
  // Input the corrected word.
  DispatchKeyPresses(
      {
          ui::VKEY_C,
          ui::VKEY_O,
          ui::VKEY_R,
          ui::VKEY_R,
          ui::VKEY_E,
          ui::VKEY_C,
          ui::VKEY_T,
          ui::VKEY_E,
          ui::VKEY_D,
      },
      false);

  engine_->OnAutocorrect(u"typed", u"corrected", 0);

  EXPECT_FALSE(engine_->GetAutocorrectRange().is_empty());

  DispatchKeyPress(ui::KeyboardCode::VKEY_A, false);
  DispatchKeyPress(ui::KeyboardCode::VKEY_A, false);
  DispatchKeyPress(ui::KeyboardCode::VKEY_A, false);

  // Highlighting should only go away after 4 keypresses.
  EXPECT_FALSE(engine_->GetAutocorrectRange().is_empty());

  DispatchKeyPress(ui::KeyboardCode::VKEY_A, false);

  EXPECT_TRUE(engine_->GetAutocorrectRange().is_empty());

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       ShowsAndHidesAutocorrectUndoWindow) {
  engine_->Enable(kEngineIdUs);
  TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);
  const std::u16string prefix_text = u"corrected ";
  helper.GetTextInputClient()->InsertText(
      prefix_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(prefix_text);

  engine_->OnAutocorrect(u"typed", u"corrected", 0);

  auto* controller =
      ((input_method::
            AssistiveWindowController*)(ui::IMEBridge::Get()
                                            ->GetAssistiveWindowHandler()));

  EXPECT_FALSE(controller->GetUndoWindowForTesting());

  // Move cursor back into the autocorrected word to show the window.
  helper.GetTextInputClient()->ExtendSelectionAndDelete(1, 0);
  helper.WaitForSurroundingTextChanged(u"corrected");

  EXPECT_TRUE(controller->GetUndoWindowForTesting());
  EXPECT_TRUE(controller->GetUndoWindowForTesting()->GetVisible());

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest, RevertsAutocorrect) {
  engine_->Enable(kEngineIdUs);
  TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);
  const std::u16string corrected_text = u"hello corrected world";
  const std::u16string typed_text = u"hello typed world";
  helper.GetTextInputClient()->InsertText(
      corrected_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(corrected_text);
  EXPECT_EQ(ui::IMEBridge::Get()
                ->GetInputContextHandler()
                ->GetSurroundingTextInfo()
                .surrounding_text,
            corrected_text);

  engine_->OnAutocorrect(u"typed", u"corrected", 6);

  // Move cursor into the corrected word, sending VKEY_LEFT fails, so use JS.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      tab, "document.getElementById('text_id').setSelectionRange(8,8)"));

  helper.WaitForSurroundingTextChanged(corrected_text, gfx::Range(8, 8));

  engine_->get_autocorrect_manager_for_testing()->UndoAutocorrect();

  helper.WaitForSurroundingTextChanged(typed_text);

  EXPECT_EQ(ui::IMEBridge::Get()
                ->GetInputContextHandler()
                ->GetSurroundingTextInfo()
                .surrounding_text,
            typed_text);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       RevertsAutocorrectWithKeyboard) {
  engine_->Enable(kEngineIdUs);

  TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);
  const std::u16string corrected_text = u"corrected";
  const std::u16string typed_text = u"typed";
  helper.GetTextInputClient()->InsertText(
      corrected_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(corrected_text);
  EXPECT_EQ(ui::IMEBridge::Get()
                ->GetInputContextHandler()
                ->GetSurroundingTextInfo()
                .surrounding_text,
            corrected_text);

  engine_->OnAutocorrect(u"typed", u"corrected", 0);
  // Move cursor into the corrected word, sending VKEY_LEFT fails, so use JS.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      tab, "document.getElementById('text_id').setSelectionRange(2,2)"));
  helper.WaitForSurroundingTextChanged(corrected_text, gfx::Range(2, 2));

  DispatchKeyPress(ui::VKEY_UP, false);
  DispatchKeyPress(ui::VKEY_RETURN, false);

  helper.WaitForSurroundingTextChanged(typed_text);

  EXPECT_EQ(ui::IMEBridge::Get()
                ->GetInputContextHandler()
                ->GetSurroundingTextInfo()
                .surrounding_text,
            typed_text);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineTest,
                       SendsAutocorrectMetricsforUnderline) {
  base::HistogramTester histogram_tester;
  engine_->Enable(kEngineIdUs);

  TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);
  const std::u16string corrected_text = u"corrected";
  const std::u16string typed_text = u"typed";
  helper.GetTextInputClient()->InsertText(
      corrected_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(corrected_text);
  EXPECT_EQ(ui::IMEBridge::Get()
                ->GetInputContextHandler()
                ->GetSurroundingTextInfo()
                .surrounding_text,
            corrected_text);

  histogram_tester.ExpectBucketCount("InputMethod.Assistive.Coverage",
                                     AssistiveType::kAutocorrectWindowShown, 0);
  engine_->OnAutocorrect(u"typed", u"corrected", 0);
  histogram_tester.ExpectBucketCount("InputMethod.Assistive.Coverage",
                                     AssistiveType::kAutocorrectUnderlined, 1);

  histogram_tester.ExpectBucketCount("InputMethod.Assistive.Coverage",
                                     AssistiveType::kAutocorrectWindowShown, 0);
  // Move cursor into the corrected word, sending VKEY_LEFT fails, so use JS.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      tab, "document.getElementById('text_id').setSelectionRange(2,2)"));
  helper.WaitForSurroundingTextChanged(corrected_text, gfx::Range(2, 2));
  histogram_tester.ExpectBucketCount("InputMethod.Assistive.Coverage",
                                     AssistiveType::kAutocorrectWindowShown, 1);

  histogram_tester.ExpectBucketCount("InputMethod.Assistive.Coverage",
                                     AssistiveType::kAutocorrectReverted, 0);
  DispatchKeyPress(ui::VKEY_UP, false);
  DispatchKeyPress(ui::VKEY_RETURN, false);

  helper.WaitForSurroundingTextChanged(typed_text);

  histogram_tester.ExpectBucketCount("InputMethod.Assistive.Coverage",
                                     AssistiveType::kAutocorrectReverted, 1);

  SetFocus(nullptr);
}

class NativeInputMethodEngineAssistiveOff : public InProcessBrowserTest {
 public:
  NativeInputMethodEngineAssistiveOff() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAssistPersonalInfoName},
        /*disabled_features=*/{features::kAssistPersonalInfo,
                               features::kEmojiSuggestAddition});
  }
  ~NativeInputMethodEngineAssistiveOff() override = default;

 protected:
  void SetUp() override {
    InProcessBrowserTest::SetUp();
    ui::IMEBridge::Initialize();
  }

  void SetUpOnMainThread() override {
    engine_ = std::make_unique<NativeInputMethodEngine>();
    ui::IMEBridge::Get()->SetCurrentEngineHandler(engine_.get());

    auto observer = std::make_unique<TestObserver>();
    observer_ = observer.get();

    profile_ = browser()->profile();
    engine_->Initialize(std::move(observer), "", profile_);
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // Reset the engine before shutting down the browser because the engine
    // observes ChromeKeyboardControllerClient, which is tied to the browser
    // lifetime.
    engine_.reset();
    ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
    ui::IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<NativeInputMethodEngine> engine_;
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
      DisabledReason::kFeatureFlagOff, 1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineAssistiveOff,
                       EmojiSuggestionDisabledReasonkFeatureFlagOff) {
  base::HistogramTester histogram_tester;
  engine_->get_assistive_suggester_for_testing()
      ->get_emoji_suggester_for_testing()
      ->LoadEmojiMapForTesting(kEmojiData);

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "happy ");

  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Disabled.Emoji",
                                      DisabledReason::kFeatureFlagOff, 1);
}  // namespace
}  // namespace chromeos
