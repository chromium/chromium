// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation_test_utils.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/base_paths.h"
#include "base/containers/fixed_flat_set.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/caret_bounds_changed_waiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

constexpr char kContentEditableUrl[] =
    "data:text/html;charset=utf-8,<div id='input' contenteditable></div>";
constexpr char kFormattedContentEditableUrl[] = R"(
    data:text/html;charset=utf-8,<div id='input' contenteditable>
    <p><strong>This</strong> <b>is</b> a <em>test</em></p></div>
)";
constexpr char kInputUrl[] =
    "data:text/html;charset=utf-8,<input id='input' type='text'></input>";
constexpr char kTextAreaUrl[] =
    "data:text/html;charset=utf-8,<textarea id='input'></textarea>";
constexpr char kPumpkinTestFilePath[] =
    "resources/chromeos/accessibility/accessibility_common/dictation/parse/"
    "pumpkin";
constexpr char kTestSupportPath[] =
    "chrome/browser/resources/chromeos/accessibility/accessibility_common/"
    "dictation/dictation_test_support.js";

// Listens for changes to the clipboard. This class only allows `Wait()` to be
// called once. If you need to call `Wait()` multiple times, create multiple
// instances of this class.
class ClipboardChangedWaiter : public ui::ClipboardObserver {
 public:
  ClipboardChangedWaiter() {
    ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  }
  ClipboardChangedWaiter(const ClipboardChangedWaiter&) = delete;
  ClipboardChangedWaiter& operator=(const ClipboardChangedWaiter&) = delete;
  ~ClipboardChangedWaiter() override {
    ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

 private:
  // ui::ClipboardObserver:
  void OnClipboardDataChanged() override { run_loop_.Quit(); }

  base::RunLoop run_loop_;
};

// Listens to when the IME commits text. This class only allows `Wait()` to be
// called once. If you need to call `Wait()` multiple times, create multiple
// instances of this class.
class CommitTextWaiter : public MockIMEInputContextHandler::Observer {
 public:
  CommitTextWaiter() = default;
  CommitTextWaiter(const CommitTextWaiter&) = delete;
  CommitTextWaiter& operator=(const CommitTextWaiter&) = delete;
  ~CommitTextWaiter() override = default;

  void Wait(const std::u16string& expected_commit_text) {
    expected_commit_text_ = expected_commit_text;
    run_loop_.Run();
  }

 private:
  // MockIMEInputContextHandler::Observer
  void OnCommitText(const std::u16string& text) override {
    if (text == expected_commit_text_) {
      run_loop_.Quit();
    }
  }

  std::u16string expected_commit_text_;
  base::RunLoop run_loop_;
};

}  // namespace

DictationTestUtils::DictationTestUtils(
    speech::SpeechRecognitionType speech_recognition_type,
    EditableType editable_type)
    : wait_for_accessibility_common_extension_load_(true),
      speech_recognition_type_(speech_recognition_type),
      editable_type_(editable_type) {
  test_helper_ =
      std::make_unique<SpeechRecognitionTestHelper>(speech_recognition_type);
}

DictationTestUtils::~DictationTestUtils() {
  if (speech_recognition_type_ == speech::SpeechRecognitionType::kNetwork) {
    content::SpeechRecognitionManager::SetManagerForTesting(nullptr);
  }
}

void DictationTestUtils::EnableDictation(Browser* browser) {
  profile_ = browser->profile();
  console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
      profile_, extension_misc::kAccessibilityCommonExtensionId);
  generator_ = std::make_unique<ui::test::EventGenerator>(
      Shell::Get()->GetPrimaryRootWindow());

  // Set up the Pumpkin dir before turning on Dictation because the
  // extension will immediately request a Pumpkin installation once activated.
  DictationTestUtils::SetUpPumpkinDir();
  test_helper_->SetUp(profile_);
  ASSERT_FALSE(AccessibilityManager::Get()->IsDictationEnabled());
  profile_->GetPrefs()->SetBoolean(
      prefs::kDictationAcceleratorDialogHasBeenAccepted, true);

  if (wait_for_accessibility_common_extension_load_) {
    // Use ExtensionHostTestHelper to detect when the accessibility common
    // extension loads.
    extensions::ExtensionHostTestHelper host_helper(
        profile_, extension_misc::kAccessibilityCommonExtensionId);
    AccessibilityManager::Get()->SetDictationEnabled(true);
    host_helper.WaitForHostCompletedFirstLoad();
  } else {
    // In some cases (e.g. DictationWithAutoclickTest) the accessibility
    // common extension is already setup and loaded. For these cases, simply
    // toggle Dictation.
    AccessibilityManager::Get()->SetDictationEnabled(true);
  }

  std::string url;
  switch (editable_type_) {
    case EditableType::kTextArea:
      url = kTextAreaUrl;
      break;
    case EditableType::kFormattedContentEditable:
      url = kFormattedContentEditableUrl;
      break;
    case EditableType::kInput:
      url = kInputUrl;
      break;
    case EditableType::kContentEditable:
      url = kContentEditableUrl;
      break;
  }
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, GURL(url)));
  // Put focus in the text box.
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::KeyboardCode::VKEY_TAB, false, false, false, false)));

  // Create an instance of the DictationTestSupport JS class, which can be
  // used from these tests to interact with Dictation JS. For more
  // information, see kTestSupportPath.
  SetUpTestSupport();

  // Increase Dictation's NO_FOCUSED_IME timeout to reduce flakiness on slower
  // builds.
  std::string script = "testSupport.increaseNoFocusedImeTimeout();";
  ExecuteAccessibilityCommonScript(script);

  // Dictation will request a Pumpkin install when it starts up. Wait for
  // the install to succeed.
  WaitForPumpkinTaggerReady();
}

void DictationTestUtils::ToggleDictationWithKeystroke() {
  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::SendKeyPressToWindowSync(
      nullptr, ui::KeyboardCode::VKEY_D, false, false, false, true)));
}

void DictationTestUtils::SendFinalResultAndWaitForEditableValue(
    content::WebContents* web_contents,
    const std::string& result,
    const std::string& value) {
  // Ensure that the accessibility tree and the text area value are updated.
  content::AccessibilityNotificationWaiter waiter(
      web_contents, ui::kAXModeComplete, ax::mojom::Event::kValueChanged);
  SendFinalResultAndWait(result);
  ASSERT_TRUE(waiter.WaitForNotification());
  WaitForEditableValue(value);
}

void DictationTestUtils::SendFinalResultAndWaitForSelectionChanged(
    content::WebContents* web_contents,
    const std::string& result) {
  content::AccessibilityNotificationWaiter selection_waiter(
      web_contents, ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  content::BoundingBoxUpdateWaiter bounding_box_waiter(web_contents);
  SendFinalResultAndWait(result);
  bounding_box_waiter.Wait();
  ASSERT_TRUE(selection_waiter.WaitForNotification());
}

// TODO(b:259353252): Update this method to use testSupport JS, similar to
// what's done in DictationFormattedContentEditableTest::WaitForSelection.
void DictationTestUtils::SendFinalResultAndWaitForCaretBoundsChanged(
    content::WebContents* web_contents,
    ui::InputMethod* input_method,
    const std::string& result) {
  content::AccessibilityNotificationWaiter selection_waiter(
      web_contents, ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  CaretBoundsChangedWaiter caret_waiter(input_method);
  SendFinalResultAndWait(result);
  caret_waiter.Wait();
  ASSERT_TRUE(selection_waiter.WaitForNotification());
}

void DictationTestUtils::SendFinalResultAndWaitForClipboardChanged(
    const std::string& result) {
  ClipboardChangedWaiter waiter;
  SendFinalResultAndWait(result);
  waiter.Wait();
}

void DictationTestUtils::WaitForRecognitionStarted() {
  test_helper_->WaitForRecognitionStarted();
  // Dictation initializes FocusHandler when speech recognition starts.
  // Several tests require FocusHandler logic, so wait for it to initialize
  // before proceeding.
  WaitForFocusHandler();
}

void DictationTestUtils::WaitForRecognitionStopped() {
  test_helper_->WaitForRecognitionStopped();
}

void DictationTestUtils::SendInterimResultAndWait(
    const std::string& transcript) {
  test_helper_->SendInterimResultAndWait(transcript);
}

void DictationTestUtils::SendFinalResultAndWait(const std::string& transcript) {
  test_helper_->SendFinalResultAndWait(transcript);
}

void DictationTestUtils::SendErrorAndWait() {
  test_helper_->SendErrorAndWait();
}

std::vector<base::test::FeatureRef> DictationTestUtils::GetEnabledFeatures() {
  return test_helper_->GetEnabledFeatures();
}

std::vector<base::test::FeatureRef> DictationTestUtils::GetDisabledFeatures() {
  return test_helper_->GetDisabledFeatures();
}

std::string DictationTestUtils::ExecuteAccessibilityCommonScript(
    const std::string& script) {
  return extensions::browsertest_util::ExecuteScriptInBackgroundPageDeprecated(
      /*context=*/profile_,
      /*extension_id=*/extension_misc::kAccessibilityCommonExtensionId,
      /*script=*/script);
}

void DictationTestUtils::DisablePumpkin() {
  std::string script = "testSupport.disablePumpkin();";
  ExecuteAccessibilityCommonScript(script);
}

std::string DictationTestUtils::GetEditableValue(
    content::WebContents* web_contents) {
  std::string script;
  switch (editable_type_) {
    case EditableType::kTextArea:
    case EditableType::kInput:
      script = "document.getElementById('input').value";
      break;
    case EditableType::kContentEditable:
    case EditableType::kFormattedContentEditable:
      // Replace all non-breaking spaces with regular spaces. Otherwise,
      // string comparisons will unexpectedly fail.
      script =
          "document.getElementById('input').innerText.replaceAll("
          "'\u00a0', ' ');";
      break;
  }
  return content::EvalJs(web_contents, script).ExtractString();
}

void DictationTestUtils::WaitForEditableValue(const std::string& value) {
  std::string script = base::StringPrintf(
      "testSupport.waitForEditableValue(`%s`);", value.c_str());
  ExecuteAccessibilityCommonScript(script);
}

void DictationTestUtils::WaitForSelection(int start, int end) {
  std::string script =
      base::StringPrintf("testSupport.waitForSelection(%d, %d);", start, end);
  ExecuteAccessibilityCommonScript(script);
}

void DictationTestUtils::InstallMockInputContextHandler() {
  input_context_handler_ = std::make_unique<MockIMEInputContextHandler>();
  IMEBridge::Get()->SetInputContextHandler(input_context_handler_.get());
}

// Retrieves the number of times commit text is updated.
int DictationTestUtils::GetCommitTextCallCount() {
  EXPECT_TRUE(input_context_handler_);
  return input_context_handler_->commit_text_call_count();
}

void DictationTestUtils::WaitForCommitText(const std::u16string& value) {
  if (value == input_context_handler_->last_commit_text()) {
    return;
  }

  CommitTextWaiter waiter;
  input_context_handler_->AddObserver(&waiter);
  waiter.Wait(value);
  input_context_handler_->RemoveObserver(&waiter);
}

void DictationTestUtils::SetUpPumpkinDir() {
  // Set the path to the Pumpkin test files. For more details, see the
  // `pumpkin_test_files` rule in the accessibility_common BUILD file.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath gen_root_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &gen_root_dir));
  base::FilePath pumpkin_test_file_path =
      gen_root_dir.AppendASCII(kPumpkinTestFilePath);
  ASSERT_TRUE(base::PathExists(pumpkin_test_file_path));
  AccessibilityManager::Get()->SetDlcPathForTest(pumpkin_test_file_path);
}

void DictationTestUtils::SetUpTestSupport() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir));
  auto test_support_path = source_dir.AppendASCII(kTestSupportPath);
  std::string script;
  ASSERT_TRUE(base::ReadFileToString(test_support_path, &script))
      << test_support_path;
  ExecuteAccessibilityCommonScript(script);
}

void DictationTestUtils::WaitForPumpkinTaggerReady() {
  std::string locale =
      profile_->GetPrefs()->GetString(prefs::kAccessibilityDictationLocale);
  static constexpr auto kPumpkinLocales =
      base::MakeFixedFlatSet<base::StringPiece>(
          {"en-US", "fr-FR", "it-IT", "de-DE", "es-ES"});
  if (!base::Contains(kPumpkinLocales, locale)) {
    // If Pumpkin doesn't support the dictation locale, then it will never
    // initialize.
    return;
  }

  std::string script = "testSupport.waitForPumpkinTaggerReady();";
  ExecuteAccessibilityCommonScript(script);
}

void DictationTestUtils::WaitForFocusHandler() {
  std::string script = "testSupport.waitForFocusHandler();";
  ExecuteAccessibilityCommonScript(script);
}

}  // namespace ash
