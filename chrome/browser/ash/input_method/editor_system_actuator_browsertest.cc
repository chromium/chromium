// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_system_actuator.h"

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
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
  EditorSystemActuatorAccessibilityTest() {}
  ~EditorSystemActuatorAccessibilityTest() override = default;
  EditorSystemActuatorAccessibilityTest(
      const EditorSystemActuatorAccessibilityTest&) = delete;
  EditorSystemActuatorAccessibilityTest& operator=(
      const EditorSystemActuatorAccessibilityTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ash::AccessibilityManager::Get()->EnableSpokenFeedback(true);
    // Ignore the intro.
    sm_.ExpectSpeechPattern("ChromeVox*");
    // Disable earcons which can be annoying in tests.
    sm_.Call([this]() {
      ImportJSModuleForChromeVox("ChromeVox",
                                 "/chromevox/background/chromevox.js");
      DisableEarcons();
    });
  }

  void TearDownOnMainThread() override {
    ash::AccessibilityManager::Get()->EnableSpokenFeedback(false);
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  ash::test::SpeechMonitor sm_;

 private:
  void ImportJSModuleForChromeVox(std::string_view name,
                                  std::string_view path) {
    extensions::browsertest_util::ExecuteScriptInBackgroundPageDeprecated(
        ash::AccessibilityManager::Get()->profile(),
        extension_misc::kChromeVoxExtensionId,
        base::ReplaceStringPlaceholders(
            R"(import('$1').then(mod => {
            globalThis.$2 = mod.$2;
            window.domAutomationController.send('done');
          }))",
            {std::string(path), std::string(name)}, nullptr));
  }

  void DisableEarcons() {
    extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        ash::AccessibilityManager::Get()->profile(),
        extension_misc::kChromeVoxExtensionId,
        "ChromeVox.earcons.playEarcon = function() {};");
  }
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

  sm_.Call([&]() { system_actuator.SubmitFeedback("dummy feedback"); });
  sm_.ExpectSpeechPattern(
      l10n_util::GetStringUTF8(IDS_EDITOR_ANNOUNCEMENT_TEXT_FOR_FEEDBACK));
  sm_.Replay();
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

  sm_.Call([&]() { system_actuator.InsertText("dummy text"); });
  sm_.ExpectSpeechPattern(
      l10n_util::GetStringUTF8(IDS_EDITOR_ANNOUNCEMENT_TEXT_FOR_INSERTION));
  sm_.Replay();
}

}  // namespace
}  // namespace ash::input_method
