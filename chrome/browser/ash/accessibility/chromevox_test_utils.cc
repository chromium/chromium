// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/chromevox_test_utils.h"

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/common/constants.h"

namespace ash {

ChromeVoxTestUtils::ChromeVoxTestUtils(Profile* profile) {
  profile_ = profile;
  sm_ = std::make_unique<test::SpeechMonitor>();
}

ChromeVoxTestUtils::~ChromeVoxTestUtils() = default;

void ChromeVoxTestUtils::EnableChromeVox(bool check_for_intro) {
  // Enable ChromeVox, disable earcons and wait for key mappings to be fetched.
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  // TODO(crbug.com/388867840): fix console error/warnings and instantiate
  // |console_observer_| here.

  // Load ChromeVox and block until it's fully loaded.
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  sm()->ExpectSpeechPattern(
      check_for_intro ? "ChromeVox spoken feedback is ready" : "*");
  sm()->Call([this]() { GlobalizeModule("ChromeVox"); });
  sm()->Call([this]() { DisableEarcons(); });
}

void ChromeVoxTestUtils::GlobalizeModule(const std::string& name) {
  std::string script =
      "globalThis." + name + " = TestImportManager.getImports()." + name + ";";
  script += "window.domAutomationController.send('done');";
  extensions::browsertest_util::ExecuteScriptInBackgroundPageDeprecated(
      profile_, extension_misc::kChromeVoxExtensionId, script);
}

void ChromeVoxTestUtils::DisableEarcons() {
  // Playing earcons from within a test is not only annoying if you're
  // running the test locally, but seems to cause crashes
  // (http://crbug.com/396507). Work around this by just telling
  // ChromeVox to not ever play earcons (prerecorded sound effects).
  extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      profile_, extension_misc::kChromeVoxExtensionId,
      "ChromeVox.earcons.playEarcon = function() {};");
}

void ChromeVoxTestUtils::RunJS(const std::string& script) {
  extensions::BackgroundScriptExecutor::ExecuteScriptAsync(
      profile_, extension_misc::kChromeVoxExtensionId, script,
      extensions::browsertest_util::ScriptUserActivation::kDontActivate);
}

}  // namespace ash
