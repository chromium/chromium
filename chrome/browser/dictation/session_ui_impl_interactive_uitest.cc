// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/test/run_until.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/dictation/dictation_interactive_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "url/gurl.h"

namespace dictation {

class DictationSessionUiImplInteractiveUiTest
    : public DictationInteractiveBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplInteractiveUiTest,
                       SecondWindowInvokesDictationHotkeyMovesUI) {
  // Create a second browser window.
  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
  content::WebContents* window2_contents =
      second_browser->GetTabStripModel()->GetActiveWebContents();
  ASSERT_NE(window2_contents, nullptr);

  // Navigate Window 2 to test page and wait for paint.
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(content::NavigateToURL(window2_contents, url));
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(window2_contents);
  content::MainThreadFrameObserver frame_observer(
      window2_contents->GetPrimaryMainFrame()->GetRenderWidgetHost());
  frame_observer.Wait();

  // clang-format off
  RunTestSequence(
    // Start dictation session in Window 1.
    // Explicitly use Window 1 context for starting session and waiting for UI.
    InContext(BrowserElements::From(browser())->GetContext(),
              StartSession()),
    InContext(BrowserElements::From(browser())->GetContext(),
              WaitForShow(DictationBubbleUi::kViewElementIdForTesting)),

    // Activate Window 2.
    InContext(BrowserElements::From(second_browser)->GetContext(),
              ActivateSurface(kBrowserViewElementId)),

    // Invoke dictation from the hotkey in Window 2.
    // Explicitly use Window 2 context.
    InContext(BrowserElements::From(second_browser)->GetContext(),
              Do([this, window2_contents] {
                FocusTextarea(window2_contents);
                dictation_service().ToggleHotkeyHandler();
              })),

    // Verify UI is showing in Window 2 and not showing in Window 1.
    InContext(BrowserElements::From(second_browser)->GetContext(),
              WaitForShow(DictationBubbleUi::kViewElementIdForTesting)),
    InContext(BrowserElements::From(browser())->GetContext(),
              EnsureNotPresent(DictationBubbleUi::kViewElementIdForTesting)),
    CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplInteractiveUiTest,
                       FocusUiByCyclingPaneFocus) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  ui::Accelerator next_pane;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_NEXT_PANE, &next_pane));
  views::Widget* bubble_widget = nullptr;

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    ObserveState(views::test::kCurrentWidgetFocus),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),
    WithView(
        DictationBubbleUi::kViewElementIdForTesting,
        [&](views::View* view) { bubble_widget = view->GetWidget(); }),
    CheckViewProperty(DictationBubbleUi::kDoneButtonElementIdForTesting,
                      &views::View::HasFocus, false),
    SendAccelerator(kBrowserViewElementId, next_pane),
    WaitForState(views::test::kCurrentWidgetFocus, std::ref(bubble_widget)),
    CheckViewProperty(DictationBubbleUi::kDoneButtonElementIdForTesting,
                      &views::View::HasFocus, true),
    SendKeyPress(kBrowserViewElementId, ui::VKEY_TAB),
    CheckViewProperty(DictationBubbleUi::kCloseButtonElementIdForTesting,
                      &views::View::HasFocus, true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplInteractiveUiTest,
                       FocusUiByInactivePopupAccelerator) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  ui::Accelerator focus_inactive_popup;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY, &focus_inactive_popup));
  views::Widget* bubble_widget = nullptr;

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    ObserveState(views::test::kCurrentWidgetFocus),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),
    WithView(
        DictationBubbleUi::kViewElementIdForTesting,
        [&](views::View* view) { bubble_widget = view->GetWidget(); }),
    CheckViewProperty(DictationBubbleUi::kDoneButtonElementIdForTesting,
                      &views::View::HasFocus, false),
    SendAccelerator(kBrowserViewElementId, focus_inactive_popup),
    WaitForState(views::test::kCurrentWidgetFocus, std::ref(bubble_widget)),
    CheckViewProperty(DictationBubbleUi::kDoneButtonElementIdForTesting,
                      &views::View::HasFocus, true),
    SendKeyPress(kBrowserViewElementId, ui::VKEY_TAB),
    CheckViewProperty(DictationBubbleUi::kCloseButtonElementIdForTesting,
                      &views::View::HasFocus, true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplInteractiveUiTest,
                       ReviewingPageStatusShownOnFirstSessionStart) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  const GURL other_url =
      embedded_test_server()->GetURL("/textinput/simple_textinput.html");

  dictation_service().SetReviewingPageStatusEnabledForTesting(true);

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    // First session shows "Reviewing page" and hides "Done".
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),
    WaitForShow(
        DictationBubbleUi::kReviewingPageStatusLabelElementIdForTesting),
    WaitForShow(DictationBubbleUi::kSeparatorElementIdForTesting),
    EnsureNotPresent(DictationBubbleUi::kDoneButtonElementIdForTesting),

    // End first session.
    Do([this] {
      dictation_service().session_controller()->EndSessionSynchronously();
    }),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),

    // Second session on the same document within the cooldown period shows
    // "Done" immediately and does NOT show "Reviewing page". Check "Done"
    // without waiting so this can't pass by the status timing out.
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),
    EnsurePresent(DictationBubbleUi::kDoneButtonElementIdForTesting),
    EnsureNotPresent(
        DictationBubbleUi::kReviewingPageStatusLabelElementIdForTesting),
    EnsureNotPresent(DictationBubbleUi::kSeparatorElementIdForTesting),

    // End second session.
    Do([this] {
      dictation_service().session_controller()->EndSessionSynchronously();
    }),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),

    // Third session on a different document shows "Reviewing page" again.
    NavigateWebContents(kWebContentsElementId, other_url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),
    EnsurePresent(
        DictationBubbleUi::kReviewingPageStatusLabelElementIdForTesting),
    EnsurePresent(DictationBubbleUi::kSeparatorElementIdForTesting),
    EnsureNotPresent(DictationBubbleUi::kDoneButtonElementIdForTesting)
  );
  // clang-format on
}

// While the reviewing page status is shown the done button is hidden. The
// bubble must still be reachable by keyboard (b/559265158), with focus going to
// the close button instead.
IN_PROC_BROWSER_TEST_F(DictationSessionUiImplInteractiveUiTest,
                       FocusUiByCyclingPaneFocusWhileReviewingPageStatusShown) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  dictation_service().SetReviewingPageStatusEnabledForTesting(true);

  ui::Accelerator next_pane;
  ASSERT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())->GetAccelerator(
      IDC_FOCUS_NEXT_PANE, &next_pane));
  views::Widget* bubble_widget = nullptr;

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    ObserveState(views::test::kCurrentWidgetFocus),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),
    WaitForShow(
        DictationBubbleUi::kReviewingPageStatusLabelElementIdForTesting),
    WithView(
        DictationBubbleUi::kViewElementIdForTesting,
        [&](views::View* view) { bubble_widget = view->GetWidget(); }),
    SendAccelerator(kBrowserViewElementId, next_pane),
    WaitForState(views::test::kCurrentWidgetFocus, std::ref(bubble_widget)),
    EnsureNotPresent(DictationBubbleUi::kDoneButtonElementIdForTesting),
    CheckViewProperty(DictationBubbleUi::kCloseButtonElementIdForTesting,
                      &views::View::HasFocus, true)
  );
  // clang-format on
}

}  // namespace dictation
