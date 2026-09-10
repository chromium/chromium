// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_interactive_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/dictation/onboarding_dialog_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace dictation {

class DictationOnboardingInteractiveUiTest
    : public DictationInteractiveBrowserTestBase {
 public:
  DictationOnboardingInteractiveUiTest() = default;
  ~DictationOnboardingInteractiveUiTest() override = default;

  StepBuilder CheckHasCompletedOnboardingPref(bool expected) {
    return CheckResult(
        [this]() {
          return profile()->GetPrefs()->GetBoolean(
              prefs::kPrefDictationOnboardingCompleted);
        },
        expected);
  }

  void SetUpOnMainThread() override {
    DictationInteractiveBrowserTestBase::SetUpOnMainThread();
    // Ensure onboarding is NOT complete at the start of the test.
    profile()->GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                      false);
  }
};

IN_PROC_BROWSER_TEST_F(DictationOnboardingInteractiveUiTest,
                       ShowOnboardingAndCompleteWithKeyboard) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  // clang-format off
  RunTestSequence(
      InstrumentTab(kWebContentsElementId),
      NavigateWebContents(kWebContentsElementId, url),
      CheckHasSession(false),
      StartSessionWithTarget(kWebContentsElementId, "#text_id"),
      WaitForShow(kDictationOnboardingDialogElementId),
      WaitForShow(kDictationOnboardingOkButtonElementId),

      // Complete onboarding by pressing Enter on the initially focused OK
      // button.
      SendKeyPress(kBrowserViewElementId, ui::VKEY_RETURN),
      WaitForHide(kDictationOnboardingDialogElementId),

      CheckHasCompletedOnboardingPref(true),
      CheckHasSession(true),

      // Ensure that focus returns to the target field in the page.
      CheckElement(
          kWebContentsElementId,
          [](ui::TrackedElement* el) {
            content::WebContents* wc =
                AsInstrumentedWebContents(el)->web_contents();
            return wc->IsFocusedElementEditable();
          })
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingInteractiveUiTest,
                       ShowOnboardingAndCancelWithEscapeKey) {
  // clang-format off
  RunTestSequence(
      CheckHasSession(false),
      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),

      // The escape key should cancel onboarding.
      SendKeyPress(kBrowserViewElementId, ui::VKEY_ESCAPE),
      WaitForHide(kDictationOnboardingDialogElementId),

      CheckHasCompletedOnboardingPref(false),
      CheckHasSession(false)
  );
  // clang-format on
}

}  // namespace dictation
