// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_view_impl.h"

#include "build/build_config.h"
#include "chrome/browser/ttc/core/ttc_interactive_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "url/gurl.h"

namespace ttc {

namespace {

class SessionViewImplBrowserTest : public TtcInteractiveBrowserTestBase {
 public:
  SessionViewImplBrowserTest() = default;
  ~SessionViewImplBrowserTest() override = default;

 protected:
  StepBuilder StartSession() {
    return Do([this]() { ttc_service().StartSession(); });
  }
};

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, StartSessionShowsUI) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),
    EnsureNotPresent(dictation::DictationBubbleUi::kViewElementIdForTesting),

    StartSession(),
    CheckHasSession(true),
    WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, EndSessionTearsDownUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting),

    PressButton(dictation::DictationBubbleUi::kCloseButtonElementIdForTesting),

    WaitForHide(dictation::DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest,
                       EndSessionFromServiceTearsDownUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting),

    Do([this]() { ttc_service().EndSession(); }),

    WaitForHide(dictation::DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, TabSwitchLeavesUIInPlace) {
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));

  // clang-format off
  RunTestSequence(
    SelectTab(kTabStripElementId, 0),
    StartSession(),
    WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting),

    SelectTab(kTabStripElementId, 1),

    EnsurePresent(dictation::DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(true)
  );
  // clang-format on
}

// Opening a new window makes it the active one, so the plate should move into
// it.
IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, NewWindowMovesUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting)
  );
  // clang-format on

  BrowserWindowInterface* const second_browser = CreateBrowser(profile());

  // clang-format off
  RunTestSequence(
    InContext(
        BrowserElements::From(second_browser)->GetContext(),
        WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting)),
    InContext(
        BrowserElements::From(browser())->GetContext(),
        EnsureNotPresent(
            dictation::DictationBubbleUi::kViewElementIdForTesting)),
    CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, ActivatingWindowMovesUI) {
#if BUILDFLAG(IS_LINUX)
  if (views::test::InteractionTestUtilSimulatorViews::IsWayland()) {
    GTEST_SKIP()
        << "Programmatic window activation is not supported in the Weston "
           "reference implementation of Wayland used by test bots.";
  }
#endif

  BrowserWindowInterface* const second_browser = CreateBrowser(profile());

  // clang-format off
  RunTestSequence(
    // Creating the second window activated it, so hand activation back before
    // starting the session.
    InContext(BrowserElements::From(browser())->GetContext(),
              ActivateSurface(kBrowserViewElementId)),

    StartSession(),
    WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting),

    InContext(BrowserElements::From(second_browser)->GetContext(),
              ActivateSurface(kBrowserViewElementId)),

    InContext(
        BrowserElements::From(second_browser)->GetContext(),
        WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting)),
    InContext(
        BrowserElements::From(browser())->GetContext(),
        EnsureNotPresent(
            dictation::DictationBubbleUi::kViewElementIdForTesting)),
    CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, DetachedTabMovesUI) {
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->GetTabStripModel()->ActivateTabAt(0);

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting)
  );
  // clang-format on

  // Detaching a tab into a new window activates that window, which should bring
  // the plate along. Done outside of a sequence because waiting on the new
  // window blocks on a nested run loop.
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  chrome::MoveTabsToNewWindow(browser(), {1});
  BrowserWindowInterface* const detached_browser =
      browser_created_observer.Wait();
  ASSERT_TRUE(detached_browser);

  // clang-format off
  RunTestSequence(
    InContext(
        BrowserElements::From(detached_browser)->GetContext(),
        WaitForShow(dictation::DictationBubbleUi::kViewElementIdForTesting)),
    InContext(
        BrowserElements::From(browser())->GetContext(),
        EnsureNotPresent(
            dictation::DictationBubbleUi::kViewElementIdForTesting)),
    CheckHasSession(true)
  );
  // clang-format on
}

}  // namespace

}  // namespace ttc
