// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation_test_utils.h"

#include <string_view>

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
#include "chrome/browser/ash/accessibility/automation_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_constants.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
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
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kContentEditableUrl[] = R"(
    data:text/html;charset=utf-8,<div id='input' class='editableForDictation'
        contenteditable autofocus></div>
)";
constexpr char kFormattedContentEditableUrl[] = R"(
    data:text/html;charset=utf-8,<div id='input' class='editableForDictation'
        contenteditable autofocus>
    <p><strong>This</strong> <b>is</b> a <em>test</em></p></div>
)";
constexpr char kInputUrl[] = R"(
    data:text/html;charset=utf-8,<input id='input' class='editableForDictation'
        type='text' autofocus></input>
)";
constexpr char kTextAreaUrl[] = R"(
    data:text/html;charset=utf-8,<textarea id='input'
        class='editableForDictation' autofocus></textarea>
)";
constexpr char kPumpkinTestFilePath[] =
    "resources/chromeos/accessibility/accessibility_common/third_party/pumpkin";
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
  automation_test_utils_ = std::make_unique<AutomationTestUtils>(
      extension_misc::kAccessibilityCommonExtensionId);
  test_helper_ = std::make_unique<SpeechRecognitionTestHelper>(
      speech_recognition_type, media::mojom::RecognizerClientType::kDictation);
}

DictationTestUtils::~DictationTestUtils() {
  if (speech_recognition_type_ == speech::SpeechRecognitionType::kNetwork) {
    content::SpeechRecognitionManager::SetManagerForTesting(nullptr);
  }
}

void DictationTestUtils::EnableDictation(
    Profile* profile,
    base::OnceCallback<void(const GURL&)> navigate_to_url) {
  profile_ = profile;
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

  std::string url = GetUrlForEditableType();
  std::move(navigate_to_url).Run(GURL(url));

  // Dictation test support references the main Dictation object, so wait for
  // the main object to be created before installing test support.
  WaitForDictationJSReady();

  // Setup automation test support.
  automation_test_utils_->SetUpTestSupport();

  // Create an instance of the DictationTestSupport JS class, which can be
  // used from these tests to interact with Dictation JS. For more
  // information, see kTestSupportPath.
  SetUpTestSupport();

  // Wait for focus to propagate.
  WaitForEditableFocus();

  // Increase Dictation's NO_FOCUSED_IME timeout to reduce flakiness on slower
  // builds.
  std::string script =
      "dictationTestSupport.setNoFocusedImeTimeout(1000 * 1000);";
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
    const std::string& result,
    const std::string& value) {
  SendFinalResultAndWait(result);
  if (speech_recognition_type_ == speech::SpeechRecognitionType::kNetwork) {
    automation_test_utils_->WaitForValueChangedEvent();
  }
  WaitForEditableValue(value);
}

void DictationTestUtils::SendFinalResultAndWaitForSelection(
    const std::string& result,
    int start,
    int end) {
  SendFinalResultAndWait(result);
  automation_test_utils_->WaitForTextSelectionChangedEvent();
  WaitForSelection(start, end);
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

void DictationTestUtils::ExecuteAccessibilityCommonScript(
    const std::string& script) {
  extensions::browsertest_util::ExecuteScriptInBackgroundPage(
      /*context=*/profile_,
      /*extension_id=*/extension_misc::kAccessibilityCommonExtensionId,
      /*script=*/script);
}

void DictationTestUtils::DisablePumpkin() {
  std::string script = "dictationTestSupport.disablePumpkin();";
  ExecuteAccessibilityCommonScript(script);
}

std::string DictationTestUtils::GetUrlForEditableType() {
  switch (editable_type_) {
    case EditableType::kTextArea:
      return kTextAreaUrl;
    case EditableType::kFormattedContentEditable:
      return kFormattedContentEditableUrl;
    case EditableType::kInput:
      return kInputUrl;
    case EditableType::kContentEditable:
      return kContentEditableUrl;
  }
}

std::string DictationTestUtils::GetEditableValue() {
  return automation_test_utils_->GetValueForNodeWithClassName(
      "editableForDictation");
}

void DictationTestUtils::WaitForEditableValue(const std::string& value) {
  std::string script = base::StringPrintf(
      "dictationTestSupport.waitForEditableValue(`%s`);", value.c_str());
  ExecuteAccessibilityCommonScript(script);
}

void DictationTestUtils::WaitForSelection(int start, int end) {
  std::string script = base::StringPrintf(
      "dictationTestSupport.waitForSelection(%d, %d);", start, end);
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
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
  auto test_support_path = source_dir.AppendASCII(kTestSupportPath);
  std::string script;
  ASSERT_TRUE(base::ReadFileToString(test_support_path, &script))
      << test_support_path;
  ExecuteAccessibilityCommonScript(script);
}

void DictationTestUtils::WaitForDictationJSReady() {
  std::string script = base::StringPrintf(R"JS(
    (async function() {
      window.accessibilityCommon.setFeatureLoadCallbackForTest('dictation',
          () => {
            chrome.test.sendScriptResult('ready');
          });
    })();
  )JS");
  ExecuteAccessibilityCommonScript(script);
}

void DictationTestUtils::WaitForEditableFocus() {
  std::string script = "dictationTestSupport.waitForEditableFocus();";
  ExecuteAccessibilityCommonScript(script);
}

void DictationTestUtils::WaitForPumpkinTaggerReady() {
  std::string locale =
      profile_->GetPrefs()->GetString(prefs::kAccessibilityDictationLocale);
  static constexpr auto kPumpkinLocales =
      base::MakeFixedFlatSet<std::string_view>(
          {"en-US", "fr-FR", "it-IT", "de-DE", "es-ES"});
  if (!base::Contains(kPumpkinLocales, locale)) {
    // If Pumpkin doesn't support the dictation locale, then it will never
    // initialize.
    return;
  }

  std::string script = "dictationTestSupport.waitForPumpkinTaggerReady();";
  ExecuteAccessibilityCommonScript(script);
}

void DictationTestUtils::WaitForFocusHandler() {
  std::string script = R"(
    dictationTestSupport.waitForFocusHandler('editableForDictation');
  )";
  ExecuteAccessibilityCommonScript(script);
}

}  // namespace ash
