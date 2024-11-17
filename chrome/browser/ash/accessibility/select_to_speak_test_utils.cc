// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/select_to_speak_test_utils.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/automation_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace sts_test_utils {

void TurnOnSelectToSpeakForTest(Profile* profile) {
  // Pretend that enhanced network voices dialog has been accepted so that the
  // dialog does not block.
  profile->GetPrefs()->SetBoolean(
      prefs::kAccessibilitySelectToSpeakEnhancedVoicesDialogShown, true);
  extensions::ExtensionHostTestHelper host_helper(
      profile, extension_misc::kSelectToSpeakExtensionId);
  AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
  host_helper.WaitForHostCompletedFirstLoad();
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string script = base::StringPrintf(R"JS(
      (async function() {
        let module = await import('./select_to_speak_main.js');
        module.selectToSpeak.setOnLoadDesktopCallbackForTest(() => {
            chrome.test.sendScriptResult('ready');
          });
        // Set enhanced network voices dialog as shown, because the pref
        // change takes some time to propagate.
        module.selectToSpeak.prefsManager_.enhancedVoicesDialogShown_ = true;
      })();
    )JS");
  base::Value result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          profile, extension_misc::kSelectToSpeakExtensionId, script);
  CHECK_EQ("ready", result);
}

void StartSelectToSpeakInBrowserWithUrl(const std::string& url,
                                        AutomationTestUtils* test_utils,
                                        ui::test::EventGenerator* generator) {
  gfx::Rect bounds = test_utils->GetBoundsOfRootWebArea(url);
  StartSelectToSpeakWithBounds(bounds, generator);
}

void StartSelectToSpeakWithBounds(const gfx::Rect& bounds,
                                  ui::test::EventGenerator* generator) {
  generator->PressKey(ui::VKEY_LWIN, 0 /* flags */);
  generator->MoveMouseTo(bounds.x(), bounds.y());
  generator->PressLeftButton();
  generator->MoveMouseTo(bounds.x() + bounds.width(),
                         bounds.y() + bounds.height());
  generator->ReleaseLeftButton();
  generator->ReleaseKey(ui::VKEY_LWIN, 0 /* flags */);
}

}  // namespace sts_test_utils
}  // namespace ash
