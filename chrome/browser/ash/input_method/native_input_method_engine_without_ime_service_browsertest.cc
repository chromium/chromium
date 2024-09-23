// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/input_method/assistive_window_controller.h"
#include "chrome/browser/ash/input_method/native_input_method_engine.h"
#include "chrome/browser/ash/input_method/stub_input_method_engine_observer.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chrome/browser/ash/input_method/textinput_test_helper.h"
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
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {
namespace input_method {

namespace {

constexpr char kEmojiData[] = "happy,😀;😃;😄";

class TestObserver : public StubInputMethodEngineObserver {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  void OnKeyEvent(const std::string& engine_id,
                  const ui::KeyEvent& event,
                  TextInputMethod::KeyEventDoneCallback callback) override {
    std::move(callback).Run(ui::ime::KeyEventHandledState::kNotHandled);
  }
  void OnInputMethodOptionsChanged(const std::string& engine_id) override {
    changed_engine_id_ = engine_id;
  }
  void ClearChangedEngineId() { changed_engine_id_ = ""; }
  const std::string& changed_engine_id() const { return changed_engine_id_; }

 private:
  std::string changed_engine_id_;
};

class KeyProcessingWaiter {
 public:
  TextInputMethod::KeyEventDoneCallback CreateCallback() {
    return base::BindOnce(&KeyProcessingWaiter::OnKeyEventDone,
                          base::Unretained(this));
  }

  void OnKeyEventDone(ui::ime::KeyEventHandledState handled_state) {
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

// These use the browser test framework but tamper with the environment through
// global singletons, effectively bypassing CrOS IMF "input method management".
// Test subject is a bespoke NativeInputMethodEngine instance manually attached
// to the environment, shadowing those created and managed by CrOS IMF (an
// integral part of the "browser" environment set up by the browser test).
// TODO(crbug/1197005): Migrate all these to unit tests.
class NativeInputMethodEngineWithoutImeServiceTest
    : public InProcessBrowserTest,
      public ui::ImeKeyEventDispatcher {
 public:
  NativeInputMethodEngineWithoutImeServiceTest() : input_method_(this) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kOnDeviceGrammarCheck},
        /*disabled_features=*/{});
  }

 protected:
  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    // Passing |false| for |use_ime_service| so NativeInputMethodEngine won't
    // launch the IME Service which typically tries to load libimedecoder.so
    // unsupported in browser tests. None of these tests require the IME Service
    // so just avoid it outright instead of relying on implicit luck.
    engine_ =
        NativeInputMethodEngine::CreateForTesting(/*use_ime_service=*/false);
    IMEBridge::Get()->SetInputContextHandler(&input_method_);
    IMEBridge::Get()->SetCurrentEngineHandler(engine_.get());

    auto observer = std::make_unique<TestObserver>();
    observer_ = observer.get();

    profile_ = browser()->profile();
    prefs_ = profile_->GetPrefs();
    prefs_->Set(::prefs::kLanguageInputMethodSpecificSettings,
                base::Value(base::Value::Type::DICT));
    engine_->Initialize(std::move(observer), /*extension_id=*/"", profile_);
    engine_->get_assistive_suggester_for_testing()
        ->get_emoji_suggester_for_testing()
        ->LoadEmojiMapForTesting(kEmojiData);

    // Ensure predictive writing is off to stop tests from attempting to
    // load the shared library.
    prefs_->SetBoolean(prefs::kAssistPredictiveWritingEnabled, false);

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // Reset the engine before shutting down the browser because the engine
    // observes ChromeKeyboardControllerClient, which is tied to the browser
    // lifetime.
    engine_.reset();
    IMEBridge::Get()->SetInputContextHandler(nullptr);
    IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpTextInput(TextInputTestHelper& helper) {
    GURL url = ui_test_utils::GetTestUrl(
        base::FilePath(FILE_PATH_LITERAL("textinput")),
        base::FilePath(FILE_PATH_LITERAL("simple_textarea.html")));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    ASSERT_TRUE(
        content::ExecJs(tab, "document.getElementById('text_id').focus()"));
    helper.WaitForTextInputStateChanged(ui::TEXT_INPUT_TYPE_TEXT_AREA);

    SetFocus(helper.GetTextInputClient());
  }

  // Overridden from ui::ImeKeyEventDispatcher:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* event) override {
    return ui::EventDispatchDetails();
  }

  void DispatchKeyPress(ui::KeyboardCode code,
                        bool need_flush,
                        int flags = ui::EF_NONE) {
    KeyProcessingWaiter waiterPressed;
    KeyProcessingWaiter waiterReleased;
    engine_->ProcessKeyEvent({ui::EventType::kKeyPressed, code, flags},
                             waiterPressed.CreateCallback());
    engine_->ProcessKeyEvent({ui::EventType::kKeyReleased, code, flags},
                             waiterReleased.CreateCallback());
    if (need_flush) {
      engine_->FlushForTesting();
    }

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
  raw_ptr<Profile, DanglingUntriaged> profile_;
  raw_ptr<PrefService, DanglingUntriaged> prefs_;
  raw_ptr<TestObserver, DanglingUntriaged> observer_;

 private:
  InputMethodAsh input_method_;
  base::test::ScopedFeatureList feature_list_;
};

// ID is specified in google_xkb_manifest.json.
constexpr char kEngineIdUs[] = "xkb:us::eng";

}  // namespace

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
                       SuggestEmoji) {
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

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
                       DismissEmojiSuggestionWhenUsersContinueTyping) {
  base::HistogramTester histogram_tester;
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

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
                       EmojiSuggestionDisabledReasonkEnterpriseSettingsOff) {
  base::HistogramTester histogram_tester;
  prefs_->SetBoolean(prefs::kEmojiSuggestionEnterpriseAllowed, false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "happy ");

  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Disabled.Emoji",
                                      DisabledReason::kEnterpriseSettingsOff,
                                      1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
                       EmojiSuggestionDisabledReasonkUserSettingsOff) {
  base::HistogramTester histogram_tester;
  prefs_->SetBoolean(prefs::kEmojiSuggestionEnabled, false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "happy ");

  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Disabled.Emoji",
                                      DisabledReason::kUserSettingsOff, 1);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
                       EmojiSuggestionDisabledReasonkUrlOrAppNotAllowed) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  ui_test_utils::SendToOmniboxAndSubmit(browser(), "happy ");

  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Disabled.Emoji",
                                      DisabledReason::kUrlOrAppNotAllowed, 1);
}

IN_PROC_BROWSER_TEST_F(
    NativeInputMethodEngineWithoutImeServiceTest,
    OnLearnMoreButtonClickedOpensEmojiSuggestionSettingsPage) {
  base::UserActionTester user_action_tester;
  ui::ime::AssistiveWindowButton button;
  button.id = ui::ime::ButtonId::kLearnMore;
  button.window_type = ash::ime::AssistiveWindowType::kEmojiSuggestion;

  engine_->AssistiveWindowButtonClicked(button);

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ChromeOS.Settings.SmartInputs.EmojiSuggestions.Open"));
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
                       FiresOnInputMethodOptionsChangedEvent) {
  {
    base::Value::Dict settings;
    // Add key will trigger event.
    base::Value::Dict pinyin1;
    pinyin1.Set("foo", true);
    settings.SetByDottedPath("pinyin", std::move(pinyin1));
    prefs_->Set(::prefs::kLanguageInputMethodSpecificSettings,
                base::Value(std::move(settings)));
    EXPECT_EQ(observer_->changed_engine_id(), "pinyin");
    observer_->ClearChangedEngineId();
  }
  {
    base::Value::Dict settings;
    // Change key will trigger event.
    base::Value::Dict pinyin2;
    pinyin2.Set("foo", false);
    settings.SetByDottedPath("pinyin", std::move(pinyin2));
    prefs_->Set(::prefs::kLanguageInputMethodSpecificSettings,
                base::Value(std::move(settings)));
    EXPECT_EQ(observer_->changed_engine_id(), "pinyin");
  }
}

// TODO(pbos): Re-enable on all build configurations. This hits a
// DUMP_WILL_BE_NOTREACHED() in ~Profile as it's being destroyed with observers
// still present in its ObserverList. Usually this is a sign of UAFs waiting to
// happen (those observers will likely try to unregister themselves later). It's
// unclear if this is a quirk of the test or a bug in production code.
#if defined(OFFICIAL_BUILD) && !DCHECK_IS_ON()
#define MAYBE_DestroyProfile DestroyProfile
#else
#define MAYBE_DestroyProfile DISABLED_DestroyProfile
#endif  // defined(OFFICIAL_BUILD) && !DCHECK_IS_ON()

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
                       MAYBE_DestroyProfile) {
  EXPECT_NE(engine_->GetPrefChangeRegistrarForTesting(), nullptr);
  profile_->MaybeSendDestroyedNotification();
  EXPECT_EQ(engine_->GetPrefChangeRegistrarForTesting(), nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
                       HighlightsOnAutocorrectThenDismissesHighlight) {
  engine_->Enable(kEngineIdUs);
  TextInputTestHelper helper(GetBrowserInputMethod());
  ui::DummyTextInputClient text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  SetFocus(&text_input_client);

  engine_->OnAutocorrect(u"typed", u"corrected", 0);

  // Input the corrected word.
  helper.GetTextInputClient()->InsertText(
      u"corrected ",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);

  helper.WaitForSurroundingTextChanged(u"corrected ");

  EXPECT_FALSE(text_input_client.GetAutocorrectRange().is_empty());

  helper.GetTextInputClient()->InsertText(
      u"aa",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(u"corrected aa");

  // Highlighting should only go away after inserting 3 characters.
  EXPECT_FALSE(text_input_client.GetAutocorrectRange().is_empty());

  helper.GetTextInputClient()->InsertText(
      u"a",
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(u"corrected aaa");

  EXPECT_TRUE(text_input_client.GetAutocorrectRange().is_empty());

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
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
      ((AssistiveWindowController*)(IMEBridge::Get()
                                        ->GetAssistiveWindowHandler()));

  EXPECT_FALSE(controller->GetUndoWindowForTesting());

  // Move cursor back into the autocorrected word to show the window.
  helper.GetTextInputClient()->ExtendSelectionAndDelete(1, 0);
  helper.WaitForSurroundingTextChanged(u"corrected");

  EXPECT_TRUE(controller->GetUndoWindowForTesting());
  EXPECT_TRUE(controller->GetUndoWindowForTesting()->GetVisible());

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
                       RevertsAutocorrect) {
  engine_->Enable(kEngineIdUs);
  TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);
  const std::u16string corrected_text = u"hello corrected world";
  const std::u16string typed_text = u"hello typed world";
  helper.GetTextInputClient()->InsertText(
      corrected_text,
      ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  helper.WaitForSurroundingTextChanged(corrected_text);
  EXPECT_EQ(IMEBridge::Get()
                ->GetInputContextHandler()
                ->GetSurroundingTextInfo()
                .surrounding_text,
            corrected_text);

  engine_->OnAutocorrect(u"typed", u"corrected", 6);

  // Move cursor into the corrected word, sending VKEY_LEFT fails, so use JS.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      tab, "document.getElementById('text_id').setSelectionRange(8,8)"));

  helper.WaitForSurroundingTextChanged(corrected_text, gfx::Range(8, 8));

  engine_->get_autocorrect_manager_for_testing()->UndoAutocorrect();

  helper.WaitForSurroundingTextChanged(typed_text);

  EXPECT_EQ(IMEBridge::Get()
                ->GetInputContextHandler()
                ->GetSurroundingTextInfo()
                .surrounding_text,
            typed_text);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
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
  EXPECT_EQ(IMEBridge::Get()
                ->GetInputContextHandler()
                ->GetSurroundingTextInfo()
                .surrounding_text,
            corrected_text);

  engine_->OnAutocorrect(u"typed", u"corrected", 0);
  // Move cursor into the corrected word, sending VKEY_LEFT fails, so use JS.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      tab, "document.getElementById('text_id').setSelectionRange(2,2)"));
  helper.WaitForSurroundingTextChanged(corrected_text, gfx::Range(2, 2));

  DispatchKeyPress(ui::VKEY_UP, false);
  DispatchKeyPress(ui::VKEY_RETURN, false);

  helper.WaitForSurroundingTextChanged(typed_text);

  EXPECT_EQ(IMEBridge::Get()
                ->GetInputContextHandler()
                ->GetSurroundingTextInfo()
                .surrounding_text,
            typed_text);

  SetFocus(nullptr);
}

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
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
  EXPECT_EQ(IMEBridge::Get()
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
  ASSERT_TRUE(content::ExecJs(
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

IN_PROC_BROWSER_TEST_F(NativeInputMethodEngineWithoutImeServiceTest,
                       ReplaceSurroundingTextPerformsAtomicInsertText) {
  engine_->Enable(kEngineIdUs);

  TextInputTestHelper helper(GetBrowserInputMethod());
  SetUpTextInput(helper);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      tab, "document.getElementById('text_id').value = 'original'"));
  ASSERT_TRUE(content::ExecJs(
      tab, "document.getElementById('text_id').setSelectionRange(4,4)"));

  helper.GetTextInputClient()->ExtendSelectionAndReplace(3, 2, u"replaced");
  helper.WaitForSurroundingTextChanged(u"oreplacedal");

  EXPECT_EQ(helper.GetElementInnerText("text_events", tab),
            "replaced;insertText;false\n");

  SetFocus(nullptr);
}

}  // namespace input_method
}  // namespace ash
