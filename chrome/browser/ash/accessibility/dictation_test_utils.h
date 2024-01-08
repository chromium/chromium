// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_TEST_UTILS_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/speech/speech_recognition_test_helper.h"

class GURL;
class Profile;

namespace speech {
enum class SpeechRecognitionType;
}  // namespace speech

namespace ui {
namespace test {
class EventGenerator;
}  // namespace test
}  // namespace ui

namespace ash {

class AutomationTestUtils;
class ExtensionConsoleErrorObserver;
class MockIMEInputContextHandler;

// A class that can be used to exercise Dictation in browsertests.
class DictationTestUtils {
 public:
  // The type of editable field to use in Dictation tests.
  enum class EditableType {
    kContentEditable,
    kFormattedContentEditable,
    kInput,
    kTextArea
  };

  DictationTestUtils(speech::SpeechRecognitionType speech_recognition_type,
                     EditableType editable_type);
  ~DictationTestUtils();
  DictationTestUtils(const DictationTestUtils&) = delete;
  DictationTestUtils& operator=(const DictationTestUtils&) = delete;

  // Enables and sets up Dictation.
  void EnableDictation(Profile* profile,
                       base::OnceCallback<void(const GURL&)> navigate_to_url);
  // Toggles Dictation on or off depending on Dictation's current state.
  void ToggleDictationWithKeystroke();

  // Convenience methods for faking speech and waiting for a condition.
  void SendFinalResultAndWaitForEditableValue(
      const std::string& result,
      const std::string& value);
  void SendFinalResultAndWaitForSelection(const std::string& result,
                                          int start,
                                          int end);
  void SendFinalResultAndWaitForClipboardChanged(const std::string& result);

  // Routers to SpeechRecognitionTestHelper methods.
  void WaitForRecognitionStarted();
  void WaitForRecognitionStopped();
  void SendInterimResultAndWait(const std::string& transcript);
  void SendFinalResultAndWait(const std::string& transcript);
  void SendErrorAndWait();
  std::vector<base::test::FeatureRef> GetEnabledFeatures();
  std::vector<base::test::FeatureRef> GetDisabledFeatures();

  // Script-related methods.
  void ExecuteAccessibilityCommonScript(const std::string& script);
  void DisablePumpkin();

  std::string GetUrlForEditableType();

  // Methods for interacting with the editable.
  std::string GetEditableValue();
  void WaitForEditableValue(const std::string& value);
  void WaitForSelection(int start, int end);

  // IME-related methods.
  void InstallMockInputContextHandler();
  // Retrieves the number of times commit text is updated.
  int GetCommitTextCallCount();
  void WaitForCommitText(const std::u16string& value);

  // TODO(b:259352600): Instead of disabling the observer, change this to
  // allow specific messages.
  void DisableConsoleObserver() { console_observer_.reset(); }

  // Sets whether or not we should wait for the accessibility common extension
  // to load when enabling Dictation. This should be true in almost all cases.
  // However, there are times when we don't want to wait for accessibility
  // common to load (e.g. if it's already loaded because another accessibility
  // common extension is active). This only has an effect if EnableDictation()
  // hasn't been called yet.
  void set_wait_for_accessibility_common_extension_load_(bool wait) {
    wait_for_accessibility_common_extension_load_ = wait;
  }

  AutomationTestUtils* automation_test_utils() {
    return automation_test_utils_.get();
  }

  ui::test::EventGenerator* generator() { return generator_.get(); }

 private:
  // Set up helper methods.
  void SetUpPumpkinDir();
  void SetUpTestSupport();
  void WaitForDictationJSReady();
  void WaitForEditableFocus();
  void WaitForPumpkinTaggerReady();
  void WaitForFocusHandler();

  bool wait_for_accessibility_common_extension_load_;
  speech::SpeechRecognitionType speech_recognition_type_;
  EditableType editable_type_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<AutomationTestUtils> automation_test_utils_;
  std::unique_ptr<SpeechRecognitionTestHelper> test_helper_;
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<MockIMEInputContextHandler> input_context_handler_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_TEST_UTILS_H_
