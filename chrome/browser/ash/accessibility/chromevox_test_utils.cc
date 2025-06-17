// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/chromevox_test_utils.h"

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/common/constants.h"

namespace ash {

namespace {
Profile* GetProfile() {
  return AccessibilityManager::Get()->profile();
}
}  // namespace

ChromeVoxTestUtils::ChromeVoxTestUtils() {
  sm_ = std::make_unique<test::SpeechMonitor>();
}

ChromeVoxTestUtils::~ChromeVoxTestUtils() = default;

void ChromeVoxTestUtils::EnableChromeVox(bool check_for_intro) {
  // Enable ChromeVox, disable earcons and wait for key mappings to be fetched.
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
      GetProfile(), extension_misc::kChromeVoxExtensionId);

  // Load ChromeVox and block until it's fully loaded.
  extensions::ExtensionHostTestHelper host_helper(
      GetProfile(), extension_misc::kChromeVoxExtensionId);
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  host_helper.WaitForHostCompletedFirstLoad();

  sm()->ExpectSpeechPattern(
      check_for_intro ? "ChromeVox spoken feedback is ready" : "*");
  sm()->Call([this]() { GlobalizeModule("ChromeVox"); });
  sm()->Call([this]() { DisableEarcons(); });
  sm()->Call([this]() { WaitForReady(); });
}

void ChromeVoxTestUtils::GlobalizeModule(const std::string& name) {
  std::string script =
      "globalThis." + name + " = TestImportManager.getImports()." + name + ";";
  script += "window.domAutomationController.send('done');";
  extensions::browsertest_util::ExecuteScriptInBackgroundPageDeprecated(
      GetProfile(), extension_misc::kChromeVoxExtensionId, script);
}

void ChromeVoxTestUtils::DisableEarcons() {
  // Playing earcons from within a test is not only annoying if you're
  // running the test locally, but seems to cause crashes
  // (http://crbug.com/396507). Work around this by just telling
  // ChromeVox to not ever play earcons (prerecorded sound effects).
  extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      GetProfile(), extension_misc::kChromeVoxExtensionId,
      "ChromeVox.earcons.playEarcon = function() {};");
}

void ChromeVoxTestUtils::WaitForReady() {
  std::string script(R"JS(
      (async function() {
        const imports = TestImportManager.getImports();
        await imports.ChromeVoxState.ready();
        window.domAutomationController.send('done');
      })()
    )JS");

  RunJS(script);
}

void ChromeVoxTestUtils::WaitForValidRange() {
  std::string script(R"JS(
      (async function() {
        const imports = TestImportManager.getImports();
        await imports.ChromeVoxState.ready();

        const ChromeVoxRange = imports.ChromeVoxRange;
        if (!ChromeVoxRange.current) {
          await new Promise(resolve => {
              new (class {
                  constructor() {
                    ChromeVoxRange.addObserver(this);
                  }
                  onCurrentRangeChanged(newRange) {
                    if (newRange) {
                        ChromeVoxRange.removeObserver(this);
                        resolve();
                    }
                  }
              })();
          });
        }

        window.domAutomationController.send('done');
      })()
  )JS");

  RunJS(script);
}

void ChromeVoxTestUtils::ExecuteCommandHandlerCommand(std::string command) {
  GlobalizeModule("CommandHandlerInterface");
  RunJS("CommandHandlerInterface.instance.onCommand('" + command + "');");
}

void ChromeVoxTestUtils::RunJS(const std::string& script) {
  extensions::BackgroundScriptExecutor::ExecuteScriptAsync(
      GetProfile(), extension_misc::kChromeVoxExtensionId, script,
      extensions::browsertest_util::ScriptUserActivation::kDontActivate);
}

}  // namespace ash
