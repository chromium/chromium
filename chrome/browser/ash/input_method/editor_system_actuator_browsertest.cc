// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_system_actuator.h"

#include "base/strings/string_util.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/chromevox_test_utils.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ash/input_method/editor_geolocation_mock_provider.h"
#include "chrome/browser/ash/input_method/editor_geolocation_provider.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::input_method {
namespace {

class EditorSystemActuatorAccessibilityTest : public InProcessBrowserTest {
 public:
  EditorSystemActuatorAccessibilityTest() = default;
  ~EditorSystemActuatorAccessibilityTest() override = default;
  EditorSystemActuatorAccessibilityTest(
      const EditorSystemActuatorAccessibilityTest&) = delete;
  EditorSystemActuatorAccessibilityTest& operator=(
      const EditorSystemActuatorAccessibilityTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    chromevox_test_utils_ = std::make_unique<ash::ChromeVoxTestUtils>();
    chromevox_test_utils_->EnableChromeVox();
  }

  void TearDownOnMainThread() override {
    chromevox_test_utils_.reset();
    ash::AccessibilityManager::Get()->EnableSpokenFeedback(false);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  ChromeVoxTestUtils* chromevox_test_utils() {
    return chromevox_test_utils_.get();
  }

  test::SpeechMonitor* sm() { return chromevox_test_utils()->sm(); }

 protected:
  std::unique_ptr<ash::ChromeVoxTestUtils> chromevox_test_utils_;
};

IN_PROC_BROWSER_TEST_F(EditorSystemActuatorAccessibilityTest,
                       AnnounceFeedbackSubmitted) {
  EditorMediator editor_mediator(
      ash::AccessibilityManager::Get()->profile(),
      std::make_unique<EditorGeolocationMockProvider>("testing_country"));
  EditorSystemActuator system_actuator(
      ash::AccessibilityManager::Get()->profile(),
      mojo::PendingAssociatedRemote<orca::mojom::SystemActuator>()
          .InitWithNewEndpointAndPassReceiver(),
      &editor_mediator);

  sm()->Call([&]() { system_actuator.SubmitFeedback("dummy feedback"); });
  sm()->ExpectSpeechPattern(
      l10n_util::GetStringUTF8(IDS_EDITOR_ANNOUNCEMENT_TEXT_FOR_FEEDBACK));
  sm()->Replay();
}

IN_PROC_BROWSER_TEST_F(EditorSystemActuatorAccessibilityTest,
                       AnnounceTextInsertion) {
  EditorMediator editor_mediator(
      ash::AccessibilityManager::Get()->profile(),
      std::make_unique<EditorGeolocationMockProvider>("testing_country"));
  EditorSystemActuator system_actuator(
      ash::AccessibilityManager::Get()->profile(),
      mojo::PendingAssociatedRemote<orca::mojom::SystemActuator>()
          .InitWithNewEndpointAndPassReceiver(),
      &editor_mediator);

  sm()->Call([&]() { system_actuator.InsertText("dummy text"); });
  sm()->ExpectSpeechPattern(
      l10n_util::GetStringUTF8(IDS_EDITOR_ANNOUNCEMENT_TEXT_FOR_INSERTION));
  sm()->Replay();
}

}  // namespace
}  // namespace ash::input_method
