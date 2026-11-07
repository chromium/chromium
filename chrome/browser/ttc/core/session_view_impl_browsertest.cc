// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_view_impl.h"

#include "build/build_config.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/browser/ttc/core/ttc_interactive_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/ttc/ttc_voice_plate.h"
#include "chrome/browser/ui/views/voice/waveform_mode.h"
#include "chrome/browser/ui/views/voice/waveform_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ttc/app/public/error_codes.h"
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

  StepBuilder SetSessionLifecycle(SessionLifecycle lifecycle) {
    return Do([this, lifecycle]() {
      session_controller().SetSessionLifecycle(lifecycle);
    });
  }
};

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, StartSessionShowsUI) {
  // clang-format off
  RunTestSequence(
    CheckHasSession(false),
    EnsureNotPresent(TtcVoicePlate::kViewElementId),

    StartSession(),
    CheckHasSession(true),
    WaitForShow(TtcVoicePlate::kViewElementId)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest,
                       VoicePlateFollowsSessionLifecycle) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(TtcVoicePlate::kViewElementId),
    CheckViewProperty(TtcVoicePlate::kWaveformElementId,
                      &voice::WaveformView::mode,
                      voice::WaveformMode::kProcessing),

    SetSessionLifecycle(SessionLifecycle::kLive),
    CheckViewProperty(TtcVoicePlate::kWaveformElementId,
                      &voice::WaveformView::mode, voice::WaveformMode::kLive)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, EndSessionTearsDownUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(TtcVoicePlate::kViewElementId),

    PressButton(TtcVoicePlate::kCloseButtonElementId),

    WaitForHide(TtcVoicePlate::kViewElementId),
    CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest,
                       EndSessionFromServiceTearsDownUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(TtcVoicePlate::kViewElementId),

    Do([this]() { ttc_service().EndSession(); }),

    WaitForHide(TtcVoicePlate::kViewElementId),
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
    WaitForShow(TtcVoicePlate::kViewElementId),

    SelectTab(kTabStripElementId, 1),

    EnsurePresent(TtcVoicePlate::kViewElementId),
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
    WaitForShow(TtcVoicePlate::kViewElementId)
  );
  // clang-format on

  BrowserWindowInterface* const second_browser = CreateBrowser(profile());

  // clang-format off
  RunTestSequence(
    InContext(
        BrowserElements::From(second_browser)->GetContext(),
        WaitForShow(TtcVoicePlate::kViewElementId)),
    InContext(
        BrowserElements::From(browser())->GetContext(),
        EnsureNotPresent(TtcVoicePlate::kViewElementId)),
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
    WaitForShow(TtcVoicePlate::kViewElementId),

    InContext(BrowserElements::From(second_browser)->GetContext(),
              ActivateSurface(kBrowserViewElementId)),

    InContext(
        BrowserElements::From(second_browser)->GetContext(),
        WaitForShow(TtcVoicePlate::kViewElementId)),
    InContext(
        BrowserElements::From(browser())->GetContext(),
        EnsureNotPresent(TtcVoicePlate::kViewElementId)),
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
    WaitForShow(TtcVoicePlate::kViewElementId)
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
        WaitForShow(TtcVoicePlate::kViewElementId)),
    InContext(
        BrowserElements::From(browser())->GetContext(),
        EnsureNotPresent(TtcVoicePlate::kViewElementId)),
    CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, ClosingWindowKeepsUIState) {
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
    InContext(BrowserElements::From(second_browser)->GetContext(),
              ActivateSurface(kBrowserViewElementId)),

    StartSession(),
    InContext(BrowserElements::From(second_browser)->GetContext(),
              WaitForShow(TtcVoicePlate::kViewElementId)),

    Do([this]() {
      session_controller().SetSessionLifecycle(SessionLifecycle::kLive);
    }),
    InContext(BrowserElements::From(second_browser)->GetContext(),
              CheckViewProperty(TtcVoicePlate::kWaveformElementId,
                                &voice::WaveformView::mode,
                                voice::WaveformMode::kLive))
  );
  // clang-format on

  ui_test_utils::BrowserDestroyedObserver browser_destroyed_observer(
      second_browser);
  second_browser->GetTabStripModel()->CloseAllTabs();
  browser_destroyed_observer.Wait();

  // clang-format off
  RunTestSequence(
    InContext(BrowserElements::From(browser())->GetContext(),
              ActivateSurface(kBrowserViewElementId),
              WaitForShow(TtcVoicePlate::kViewElementId),
              CheckViewProperty(TtcVoicePlate::kWaveformElementId,
                                &voice::WaveformView::mode,
                                voice::WaveformMode::kLive)),
    CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, ToastShowsOnError) {
  // clang-format off
  RunTestSequence(
      StartSession(),
      Do([this]() {
        session_controller().OnError(ErrorCode::kUnknown);
      }),
      WaitForShow(toasts::ToastView::kToastViewId)
  );
  // clang-format on
}

// Opening a popup or app window does not move the voice plate away from the
// normal browser window.
IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest,
                       NonNormalWindowDoesNotMoveUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(TtcVoicePlate::kViewElementId)
  );
  // clang-format on

  BrowserWindowInterface* const popup_browser =
      CreateBrowserForPopup(profile());
  ui_test_utils::WaitForBrowserSetLastActive(popup_browser);
  ASSERT_EQ(popup_browser->GetType(), BrowserWindowInterface::TYPE_POPUP);

  BrowserWindowInterface* const app_browser =
      CreateBrowserForApp("TestApp", profile());
  ui_test_utils::WaitForBrowserSetLastActive(app_browser);
  ASSERT_EQ(app_browser->GetType(), BrowserWindowInterface::TYPE_APP);

  // clang-format off
  RunTestSequence(
    InContext(
        BrowserElements::From(popup_browser)->GetContext(),
        EnsureNotPresent(TtcVoicePlate::kViewElementId)),
    InContext(
        BrowserElements::From(app_browser)->GetContext(),
        EnsureNotPresent(TtcVoicePlate::kViewElementId)),
    InContext(
        BrowserElements::From(browser())->GetContext(),
        EnsurePresent(TtcVoicePlate::kViewElementId)),
    CheckHasSession(true)
  );
  // clang-format on
}

// Closing the plate's window moves the plate to the next tabbed window, not to
// an open popup window.
IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest,
                       ClosingWindowMovesUIToNextTabbedWindow) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(TtcVoicePlate::kViewElementId)
  );
  // clang-format on

  BrowserWindowInterface* const second_browser = CreateBrowser(profile());
  // clang-format off
  RunTestSequence(
    InContext(
        BrowserElements::From(second_browser)->GetContext(),
        WaitForShow(TtcVoicePlate::kViewElementId))
  );
  // clang-format on

  BrowserWindowInterface* const popup_browser =
      CreateBrowserForPopup(profile());
  ui_test_utils::WaitForBrowserSetLastActive(popup_browser);
  CloseBrowserSynchronously(second_browser);

  // clang-format off
  RunTestSequence(
    InContext(
        BrowserElements::From(browser())->GetContext(),
        WaitForShow(TtcVoicePlate::kViewElementId)),
    InContext(
        BrowserElements::From(popup_browser)->GetContext(),
        EnsureNotPresent(TtcVoicePlate::kViewElementId)),
    CheckHasSession(true)
  );
  // clang-format on
}

}  // namespace

}  // namespace ttc
