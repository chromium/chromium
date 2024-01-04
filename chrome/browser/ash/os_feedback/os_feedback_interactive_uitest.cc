// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_tags.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsFeedbackWebContentsId);

class OsFeedbackInteractiveUiTest : public InteractiveAshTest {
 public:
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();
    // Ensure the Feedback SWA is installed.
    InstallSystemApps();
  }

 protected:
  // Query to pierce through Shadow DOM to find the feedback page title.
  const DeepQuery kFeedbackSearchPageTitleQuery = {
      "feedback-flow",
      "search-page",
      "h1.page-title",
  };

  // Clicks on an element in the DOM.
  auto ClickElement(const ui::ElementIdentifier& element_id,
                    const DeepQuery& element) {
    return Steps(MoveMouseTo(element_id, element), ClickMouse());
  }

  // Wait for the Feedback SWA to be present.
  auto WaitForFeedbackSWAReady(const ui::ElementIdentifier& element_id) {
    return Steps(
        Log("Waiting for the os feedback SWA to load"), WaitForShow(element_id),
        WaitForWebContentsReady(element_id),
        InAnyContext(EnsurePresent(element_id, kFeedbackSearchPageTitleQuery)),
        WaitForElementTextContains(element_id, kFeedbackSearchPageTitleQuery,
                                   "Send feedback"));
  }
};

IN_PROC_BROWSER_TEST_F(OsFeedbackInteractiveUiTest,
                       OpenFromAltShiftIShortCuts) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-3f028d06-0100-4b5b-b1f3-99ceeaf3d62b");

  ui::Accelerator open_feedback_accelerator(
      ui::VKEY_I, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);

  GURL blank_url("about:blank");
  ASSERT_TRUE(CreateBrowserWindow(blank_url));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabWebContentsId);

  RunTestSequence(
      InstrumentTab(kNewTabWebContentsId),
      InstrumentNextTab(kOsFeedbackWebContentsId, AnyBrowser()),
      Log("Pressing Alt+Shift+I"),
      SendAccelerator(kNewTabWebContentsId, open_feedback_accelerator),
      FlushEvents(), WaitForFeedbackSWAReady(kOsFeedbackWebContentsId));
}

}  // namespace
}  // namespace ash
