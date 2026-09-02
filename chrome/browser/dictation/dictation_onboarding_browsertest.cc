// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/dictation/dictation_interactive_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/dictation/onboarding_dialog_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"

namespace dictation {

class DictationOnboardingInteractiveTest
    : public DictationInteractiveBrowserTestBase {
 public:
  DictationOnboardingInteractiveTest() = default;
  ~DictationOnboardingInteractiveTest() override = default;

  StepBuilder CheckHasCompletedOnboardingPref(bool expected) {
    return CheckResult(
        [this]() {
          return profile()->GetPrefs()->GetBoolean(
              prefs::kPrefDictationOnboardingCompleted);
        },
        expected);
  }

  StepBuilder SetHasCompletedOnboardingPref(bool completed) {
    return Do([this, completed]() {
      profile()->GetPrefs()->SetBoolean(
          prefs::kPrefDictationOnboardingCompleted, completed);
    });
  }

  void SetUpOnMainThread() override {
    DictationInteractiveBrowserTestBase::SetUpOnMainThread();
    // Ensure onboarding is NOT complete at the start of the test.
    profile()->GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                      false);
  }
};

class DictationOnboardingMetricsInteractiveTest
    : public DictationOnboardingInteractiveTest {
 public:
  DictationOnboardingMetricsInteractiveTest() = default;
  ~DictationOnboardingMetricsInteractiveTest() override = default;

  StepBuilder CheckNoFirstRunExitStatusSample() {
    return Do([this]() {
      histogram_tester_.ExpectTotalCount(kFirstRunExitStatusHistogramName, 0);
    });
  }

  StepBuilder CheckFirstRunExitStatus(
      DictationFirstRunExitStatus expected_status) {
    return Do([this, expected_status]() {
      histogram_tester_.ExpectUniqueSample(kFirstRunExitStatusHistogramName,
                                           expected_status, 1);
    });
  }

 protected:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(DictationOnboardingInteractiveTest,
                       InitialStateOfOnboardingPrefIsFalse) {
  RunTestSequence(CheckHasCompletedOnboardingPref(false));
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingInteractiveTest,
                       ShowOnboardingAndComplete) {
  // clang-format off
  RunTestSequence(
      // Try starting a session. It should show the onboarding dialog since it's
      // the first run.
      CheckHasSession(false),
      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),

      // The session must not be started.
      CheckHasSession(false),

      // Complete the onboarding
      PressButton(kDictationOnboardingOkButtonElementId),
      WaitForHide(kDictationOnboardingDialogElementId),

      // Verify the pref is set to completed and the session started.
      CheckHasCompletedOnboardingPref(true),
      CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingInteractiveTest,
                       ShowOnboardingAndCancel) {
  // clang-format off
  RunTestSequence(
      // Try starting a session. It should show the onboarding dialog since it's
      // the first run.
      CheckHasSession(false),
      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),

      // The session must not be started.
      CheckHasSession(false),

      // Cancel the onboarding
      PressButton(kDictationOnboardingCancelButtonElementId),
      WaitForHide(kDictationOnboardingDialogElementId),

      // Verify the pref is still false, and no session is started.
      CheckHasCompletedOnboardingPref(false),
      CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingInteractiveTest,
                       BypassOnboardingIfAlreadyCompleted) {
  // clang-format off
  RunTestSequence(
      // Set the pref to completed.
      SetHasCompletedOnboardingPref(true),
      CheckHasCompletedOnboardingPref(true),
      CheckHasSession(false),

      // Start the session. It should bypass onboarding and start immediately.
      StartSession(),
      CheckHasSession(true),

      // Verify the dialog was never shown.
      EnsureNotPresent(kDictationOnboardingDialogElementId)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingInteractiveTest,
                       RecordsMetricsOnFRECompletion) {
  base::HistogramTester histogram_tester;
  // clang-format off
  RunTestSequence(
      CheckHasSession(false),
      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),
      PressButton(kDictationOnboardingOkButtonElementId),
      WaitForHide(kDictationOnboardingDialogElementId),
      CheckHasSession(true)
  );
  // clang-format on
  histogram_tester.ExpectUniqueSample(kSessionStartSourceHistogramName,
                                      DictationSessionEntryPoint::kContextMenu,
                                      1);
  histogram_tester.ExpectUniqueSample(
      kStreamStartTriggerHistogramName,
      DictationStreamStartTrigger::kSessionStart, 1);
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingMetricsInteractiveTest,
                       RecordsFirstRunExitStatusOnCompletion) {
  // clang-format off
  RunTestSequence(
      CheckHasSession(false),
      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),
      CheckNoFirstRunExitStatusSample(),
      PressButton(kDictationOnboardingOkButtonElementId),
      WaitForHide(kDictationOnboardingDialogElementId),
      CheckFirstRunExitStatus(DictationFirstRunExitStatus::kCompleted),
      CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingMetricsInteractiveTest,
                       RecordsFirstRunExitStatusOnCancellation) {
  // clang-format off
  RunTestSequence(
      CheckHasSession(false),
      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),
      CheckNoFirstRunExitStatusSample(),
      PressButton(kDictationOnboardingCancelButtonElementId),
      WaitForHide(kDictationOnboardingDialogElementId),
      CheckFirstRunExitStatus(DictationFirstRunExitStatus::kCancelled),
      CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingMetricsInteractiveTest,
                       RecordsFirstRunExitStatusOnDisplacedBySecondTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);
  // clang-format off
  RunTestSequence(
      CheckHasSession(false),
      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),
      CheckNoFirstRunExitStatusSample(),

      AddInstrumentedTab(kSecondTabElementId, GURL("about:blank")),

      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),
      CheckFirstRunExitStatus(DictationFirstRunExitStatus::kAbandoned)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingMetricsInteractiveTest,
                       NoFirstRunExitStatusRecordedWhenFREIsShown) {
  // clang-format off
  RunTestSequence(
      CheckHasSession(false),
      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),
      CheckNoFirstRunExitStatusSample()
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingMetricsInteractiveTest,
                       RecordsFirstRunExitStatusOnTabClosed) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);
  // clang-format off
  RunTestSequence(
      CheckHasSession(false),
      // Add a second tab to keep the browser open when the first tab closes.
      AddInstrumentedTab(kSecondTabElementId, GURL("about:blank")),
      SelectTab(kTabStripElementId, 0),

      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),
      CheckNoFirstRunExitStatusSample(),

      // Close the tab hosting the FRE dialog.
      Do([this]() {
        browser()->tab_strip_model()->CloseWebContentsAt(
            0, TabCloseTypes::CLOSE_USER_GESTURE);
      }),

      CheckFirstRunExitStatus(DictationFirstRunExitStatus::kAbandoned)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingInteractiveTest,
                       ShowOnboardingOnSecondTabClosesFirst) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);
  // clang-format off
  RunTestSequence(
      // Try starting a session on Tab A. It shows onboarding dialog.
      CheckHasSession(false),
      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),

      // Open a second tab (Tab B).
      AddInstrumentedTab(kSecondTabElementId, GURL("about:blank")),

      // Start session on Tab B.
      // It should close FRE on Tab A and open a new FRE on Tab B.
      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),

      // Switch back to Tab A and ensure the FRE dialog on Tab A is
      // hidden/not present.
      SelectTab(kTabStripElementId, 0),
      EnsureNotPresent(kDictationOnboardingDialogElementId),

      // Switch back to Tab B (index 1).
      SelectTab(kTabStripElementId, 1),
      WaitForShow(kDictationOnboardingDialogElementId),

      // Complete onboarding on Tab B.
      PressButton(kDictationOnboardingOkButtonElementId),
      WaitForHide(kDictationOnboardingDialogElementId),

      // Verify pref is set to completed and session started.
      CheckHasCompletedOnboardingPref(true),
      CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationOnboardingInteractiveTest,
                       ResponsiveAfterTabSwitching) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabElementId);
  // clang-format off
  RunTestSequence(
      CheckHasSession(false),
      StartSession(),
      WaitForShow(kDictationOnboardingDialogElementId),

      AddInstrumentedTab(kSecondTabElementId, GURL("about:blank")),

      SelectTab(kTabStripElementId, 0),
      WaitForShow(kDictationOnboardingDialogElementId),

      // Verify focus is restored
      CheckViewProperty(kDictationOnboardingOkButtonElementId,
                        &views::View::HasFocus, true),

      PressButton(kDictationOnboardingOkButtonElementId),
      WaitForHide(kDictationOnboardingDialogElementId),

      CheckHasCompletedOnboardingPref(true),
      CheckHasSession(true)
  );
  // clang-format on
}

}  // namespace dictation
