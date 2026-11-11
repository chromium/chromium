// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_view_impl.h"

#include "base/test/run_until.h"
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
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/system/privacy/privacy_indicators_controller.h"
#include "base/test/scoped_feature_list.h"
#include "ui/message_center/message_center.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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

  void SetTabFullscreen(bool fullscreen) {
    content::WebContents* const tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    using ui_test_utils::FullscreenWaiter;
    const FullscreenWaiter::Expectation expectation =
        fullscreen ? FullscreenWaiter::Expectation{.tab_fullscreen = true}
                   : FullscreenWaiter::kNoFullscreen;
    FullscreenWaiter waiter(browser(), expectation);
    if (fullscreen) {
      tab->GetDelegate()->EnterFullscreenModeForTab(tab->GetPrimaryMainFrame(),
                                                    {});
    } else {
      tab->ExitFullscreen(/*will_cause_resize=*/true);
    }
    waiter.Wait();
  }

  views::View* GetBrowserView(ui::ElementIdentifier id) {
    return views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
        id, BrowserElements::From(browser())->GetContext());
  }

  // The plate's window bounds, including its shadow.
  gfx::Rect GetPlateBounds() {
    return GetBrowserView(TtcVoicePlate::kViewElementId)
        ->GetWidget()
        ->GetWindowBoundsInScreen();
  }

  // Whether the plate's top edge is inside the browser window, which in
  // fullscreen is the screen. Only the bubble shadow (`kShadowBlur`) may extend
  // past it, as with toasts in fullscreen. Uses the browser view's widget: on
  // Mac immersive the top container lives in a separate overlay widget.
  bool IsPlateTopInWindow() {
    const gfx::Rect window = GetBrowserView(kBrowserViewElementId)
                                 ->GetWidget()
                                 ->GetWindowBoundsInScreen();
    const int min_top = window.y() - views::BubbleBorder::kShadowBlur;
    return GetPlateBounds().y() >= min_top;
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

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest, ToastShowsOnSystemMicMuted) {
  // clang-format off
  RunTestSequence(
      StartSession(),
      Do([this]() {
        session_controller().OnSystemMicMuteChanged(
            /*muted=*/false, /*at_stream_start=*/false);
      }),
      EnsureNotPresent(toasts::ToastView::kToastViewId),
      Do([this]() {
        session_controller().OnSystemMicMuteChanged(
            /*muted=*/true, /*at_stream_start=*/false);
      }),
      WaitForShow(toasts::ToastView::kToastViewId)
  );
  // clang-format on
}

#if BUILDFLAG(IS_CHROMEOS)
// Runs with the video conference tray off and on: Ash hides privacy indicators
// for most apps when the tray is on, but not for TTC.
class SessionViewImplMicIndicatorBrowserTest
    : public SessionViewImplBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  SessionViewImplMicIndicatorBrowserTest() {
    feature_list_.InitWithFeatureState(
        ash::features::kFeatureManagementVideoConference, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SessionViewImplMicIndicatorBrowserTest,
                         /*video_conference_tray=*/testing::Bool());

// TTC opens the microphone itself, so it reports capture to the Ash privacy
// indicator directly, and ending the session hides the indicator.
IN_PROC_BROWSER_TEST_P(SessionViewImplMicIndicatorBrowserTest,
                       MicCaptureShowsPrivacyIndicator) {
  auto has_notification = []() {
    return message_center::MessageCenter::Get()->FindNotificationById(
               ash::GetPrivacyIndicatorsNotificationId(
                   ash::kTtcPrivacyIndicatorsAppId)) != nullptr;
  };
  // The notification stays up briefly after capture stops, so the stop is
  // checked on the controller's list of apps using the microphone.
  auto is_ttc_using_mic = []() {
    return ash::PrivacyIndicatorsController::Get()
        ->apps_using_microphone()
        .contains(ash::kTtcPrivacyIndicatorsAppId);
  };
  // clang-format off
  RunTestSequence(
      StartSession(),
      CheckResult(has_notification, false),
      Do([this]() { session_controller().OnMicCaptureChanged(true); }),
      CheckResult(has_notification, true),
      Do([this]() { session_controller().OnMicCaptureChanged(false); }),
      CheckResult(is_ttc_using_mic, false),
      Do([this]() { session_controller().OnMicCaptureChanged(true); }),
      CheckResult(is_ttc_using_mic, true),
      Do([this]() { ttc_service().EndSession(); }),
      CheckResult(is_ttc_using_mic, false)
  );
  // clang-format on
}

// Sets the video conference tray explicitly: the testing field trial config
// enables it on bots.
class SessionViewImplMutedAtStartBrowserTest
    : public SessionViewImplBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  SessionViewImplMutedAtStartBrowserTest() {
    feature_list_.InitWithFeatureState(
        ash::features::kFeatureManagementVideoConference, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SessionViewImplMutedAtStartBrowserTest,
                         /*video_conference_tray=*/testing::Bool());

// Without the video conference tray, ChromeOS notifies the user itself when a
// stream opens muted, so there is no toast. With the tray, nothing else tells
// the user, so the toast shows.
IN_PROC_BROWSER_TEST_P(SessionViewImplMutedAtStartBrowserTest,
                       ToastOnlyWithVideoConferenceTray) {
  const bool video_conference_tray = GetParam();
  // clang-format off
  RunTestSequence(
      StartSession(),
      Do([this]() {
        session_controller().OnSystemMicMuteChanged(
            /*muted=*/true, /*at_stream_start=*/true);
      }),
      video_conference_tray
          ? WaitForShow(toasts::ToastView::kToastViewId)
          : EnsureNotPresent(toasts::ToastView::kToastViewId)
  );
  // clang-format on
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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

// Closing the last tabbed window ends the session, even with a popup still
// open: no window is left to show the plate in or to supply page context.
IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest,
                       ClosingLastTabbedWindowEndsSession) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(TtcVoicePlate::kViewElementId)
  );
  // clang-format on

  BrowserWindowInterface* const popup_browser =
      CreateBrowserForPopup(profile());
  ui_test_utils::WaitForBrowserSetLastActive(popup_browser);

  TtcKeyedService& service = ttc_service();
  CloseBrowserSynchronously(browser());
  // `browser()` is gone; point the fixture at the popup.
  SetBrowser(popup_browser);

  EXPECT_TRUE(base::test::RunUntil(
      [&service]() { return !service.is_session_active(); }));
}

// Tab fullscreen hides the toolbar the plate normally overlaps.
IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest,
                       TabFullscreenKeepsPlateOnScreen) {
  SetTabFullscreen(true);

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(TtcVoicePlate::kViewElementId)
  );
  // clang-format on

  EXPECT_TRUE(IsPlateTopInWindow());
}

IN_PROC_BROWSER_TEST_F(SessionViewImplBrowserTest,
                       PlateFollowsTabFullscreenChanges) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(TtcVoicePlate::kViewElementId)
  );
  // clang-format on
  const int windowed_y = GetPlateBounds().y();

  SetTabFullscreen(true);
  // The plate is already inside the window before fullscreen, so also wait
  // for it to move up.
  EXPECT_TRUE(base::test::RunUntil([this, windowed_y]() {
    return IsPlateTopInWindow() && GetPlateBounds().y() < windowed_y;
  }));

  SetTabFullscreen(false);
  EXPECT_TRUE(base::test::RunUntil(
      [this, windowed_y]() { return GetPlateBounds().y() == windowed_y; }));
}

}  // namespace

}  // namespace ttc
