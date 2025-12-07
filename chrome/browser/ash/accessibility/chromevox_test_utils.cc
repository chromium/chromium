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
#include "extensions/browser/extension_registry_test_helper.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/accessibility_features.h"

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

void ChromeVoxTestUtils::EnableChromeVox(bool check_for_speech) {
  // Enable ChromeVox, disable earcons and wait for key mappings to be fetched.
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  Profile* profile = GetProfile();
  console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
      profile, extension_misc::kChromeVoxExtensionId);
  AccessibilityManager::Get()->EnableSpokenFeedback(true);

  if (::features::IsAccessibilityManifestV3EnabledForChromeVox()) {
    if (!profile->IsOffTheRecord()) {
      // Watch events from an MV3 extension which runs in a service worker.
      // Note: this class doesn't work with off the record profiles. For off
      // the record profiles, we use the SpeechMonitor to signal that ChromeVox
      // has loaded by waiting for text to speech to play.
      extensions::ExtensionRegistryTestHelper observer(
          extension_misc::kChromeVoxExtensionId, profile);
      ASSERT_EQ(3, observer.WaitForManifestVersion());
      observer.WaitForServiceWorkerStart();
    }
  } else {
    // Watch events from an MV2 extension which runs in a background page.
    extensions::ExtensionHostTestHelper host_helper(
        profile, extension_misc::kChromeVoxExtensionId);
    host_helper.WaitForHostCompletedFirstLoad();
  }

  if (check_for_speech) {
    sm()->ExpectSpeechPattern("ChromeVox spoken feedback is ready");
    sm()->Call([this]() { WaitForReady(); });
    sm()->Call([this]() { GlobalizeModule("ChromeVox"); });
    sm()->Call([this]() { DisableEarcons(); });
  } else {
    WaitForReady();
    GlobalizeModule("ChromeVox");
    DisableEarcons();
  }
}

void ChromeVoxTestUtils::GlobalizeModule(const std::string& name) {
  std::string script =
      "globalThis." + name + " = TestImportManager.getImports()." + name + ";";
  script += "chrome.test.sendScriptResult('done');";
  RunJS(script);
}

void ChromeVoxTestUtils::DisableEarcons() {
  // Playing earcons from within a test is not only annoying if you're
  // running the test locally, but seems to cause crashes
  // (http://crbug.com/396507). Work around this by just telling
  // ChromeVox to not ever play earcons (prerecorded sound effects).
  std::string script(R"JS(
    ChromeVox.earcons.playEarcon = function() {};
    chrome.test.sendScriptResult('done');
  )JS");
  RunJS(script);
}

void ChromeVoxTestUtils::WaitForReady() {
  std::string script(R"JS(
      (async function() {
        const imports = TestImportManager.getImports();
        await imports.ChromeVoxState.ready();
        chrome.test.sendScriptResult('done');
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

        chrome.test.sendScriptResult('done');
      })()
  )JS");

  RunJS(script);
}

void ChromeVoxTestUtils::ExecuteCommandHandlerCommand(std::string command) {
  GlobalizeModule("CommandHandlerInterface");
  std::string script =
      "CommandHandlerInterface.instance.onCommand('" + command + "');";
  script += "chrome.test.sendScriptResult('done');";
  RunJS(script);
}

void ChromeVoxTestUtils::RunJS(const std::string& script) {
  base::Value value =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          GetProfile(), extension_misc::kChromeVoxExtensionId, script,
          extensions::browsertest_util::ScriptUserActivation::kDontActivate);
  ASSERT_EQ(value, "done");
}

}  // namespace ash
