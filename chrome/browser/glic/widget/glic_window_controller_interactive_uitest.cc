// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/memory_pressure_monitor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/background/glic/glic_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/glic/widget/glic_window_controller_impl.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/test/ui_controls.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/virtual_display_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/test/button_test_api.h"

namespace glic {

namespace {

#if !BUILDFLAG(IS_CHROMEOS)
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
#endif  // !BUILDFLAG(IS_CHROMEOS)

const InteractiveBrowserTestApi::DeepQuery kMockGlicClientHangButton = {
    "#hang"};

}  // anonymous namespace

class GlicWindowControllerUiTest : public test::InteractiveGlicTest {
 public:
  GlicWindowControllerUiTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kGlicHostLogging);
    features_.InitWithFeaturesAndParameters(
        {{features::kTabstripComboButton, {}},
         {features::kGlicActor, {}},
         {features::kGlicActorUi,
          {{features::kGlicActorUiTaskIconName, "true"}}}},
        {});
    TrackFloatingGlicInstance();
  }
  ~GlicWindowControllerUiTest() override = default;

  auto SimulateGlicHotkey() {
    // TODO: Actually implement the hotkey when we know what it is.
    return Do([this]() {
      glic_service()->ToggleUI(nullptr, /*prevent_close=*/false,
                               mojom::InvocationSource::kOsHotkey);
    });
  }

  auto SimulateOpenMenuItem() {
    return Do([this]() {
      glic_controller_->Show(mojom::InvocationSource::kOsButtonMenu);
    });
  }

  auto SimulateOsButton() {
    return Do([this]() {
      glic_controller_->Toggle(mojom::InvocationSource::kOsButton);
    });
  }

  auto ForceInvalidateAccount() {
    return Do([this]() { InvalidateAccount(window_controller().profile()); });
  }

  auto ForceReauthAccount() {
    return Do([this]() { ReauthAccount(window_controller().profile()); });
  }

  bool IsWorkAreaTooSmallForTest() {
    gfx::Rect work_area_bounds =
        display::Screen::Get()->GetPrimaryDisplay().work_area();
    gfx::Size glic_expected_size = GlicWidget::GetInitialSize();
    gfx::Size cell_size = {work_area_bounds.width() / 3,
                           work_area_bounds.height() / 3};
    // Set browser bounds to the center cell of the work area bounds.
    gfx::Rect browser_bounds = gfx::Rect(
        gfx::Point(work_area_bounds.width() / 3 + work_area_bounds.x(),
                   work_area_bounds.height() / 3 + work_area_bounds.y()),
        cell_size);
    browser()->window()->SetBounds(browser_bounds);
    browser_bounds = browser()->window()->GetBounds();

    // The test places the browser in the center cell. For the test to be valid,
    // there must be enough space around the browser for the GlicWidget to
    // appear without being clipped or overlapping in a way that breaks the test
    // logic.
    return cell_size.width() <= glic_expected_size.width() / 2 ||
           cell_size.height() <= glic_expected_size.height() / 2 ||
           work_area_bounds.width() <=
               browser_bounds.width() + glic_expected_size.width() ||
           work_area_bounds.height() <=
               browser_bounds.height() + glic_expected_size.height();
  }

 private:
  std::unique_ptr<GlicController> glic_controller_ =
      std::make_unique<GlicController>();

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, ShowAndCloseDetachedWidget) {
  RunTestSequence(OpenGlicFloatingWindow(), CloseGlicWindow());
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, DoNotCrashOnBrowserClose) {
  RunTestSequence(OpenGlicFloatingWindow());
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, DoNotCrashWhenReopening) {
  RunTestSequence(OpenGlicFloatingWindow(), CloseGlicWindow(),
                  OpenGlicFloatingWindow());
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, ButtonTogglesGlicWindow) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(OpenGlicFloatingWindow(), PressButton(kGlicButtonElementId),
                  WaitForGlicClose(), PressButton(kGlicButtonElementId),

                  CheckControllerWidgetMode(GlicWindowMode::kDetached));
}

constexpr char kActivateSurfaceIncompatibilityNotice[] =
    "Programmatic window activation does not work on the Weston reference "
    "implementation of Wayland used on Linux testbots. It also doesn't work "
    "reliably on Linux in general. For this reason, some of these tests which "
    "use ActivateSurface() may be skipped on machine configurations which do "
    "not reliably support them.";

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ButtonWhenAttachedToActiveBrowserCloses) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    GTEST_SKIP() << "N/A for multi-instance";
  }
  RunTestSequence(
      OpenGlicFloatingWindow(),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId),
      // Glic should close.
      PressButton(kGlicButtonElementId), WaitForGlicClose(),
      CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyWhenDetachedActiveCloses) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      OpenGlicFloatingWindow(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateGlicHotkey(), WaitForGlicClose(),
      CheckControllerHasWidget(false));
}

// TODO(393203136): Once tests can observe window controller state rather than
// polling, make a test like this one with glic initially attached.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyDetachedWithNotNormalBrowser) {
  RunTestSequence(
      Do([&]() {
        Browser* const pwa =
            CreateBrowserForApp("app name", browser()->profile());
        pwa->window()->Activate();
      }),
      SimulateGlicHotkey(),
      InAnyContext(WaitForShow(kGlicViewElementId).SetMustRemainVisible(false)),
      CheckControllerWidgetMode(GlicWindowMode::kDetached));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyOpensDetachedWithMinimizedBrowser) {
  RunTestSequence(
      // Glic should open attached to active browser.
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId));
  browser()->window()->Minimize();
  ASSERT_TRUE(ui_test_utils::WaitForMinimized(browser()));
  RunTestSequence(SimulateGlicHotkey(), WaitForGlicOpen());
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyOpensDetachedWithNonActiveBrowser) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      // Glic should open attached to active browser.
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId));

  // This will make some other window the foreground window.
  browser()->window()->Deactivate();

  RunTestSequence(
      SimulateGlicHotkey(),
      InAnyContext(WaitForShow(kGlicViewElementId).SetMustRemainVisible(false)),

      CheckControllerWidgetMode(GlicWindowMode::kDetached));
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ESCWhenDetachedActiveCloses) {
  RunTestSequence(
      OpenGlicFloatingWindow(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateAcceleratorPress(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForGlicClose());
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ESCWhenAttachedActiveCloses) {
  RunTestSequence(
      OpenGlicFloatingWindow(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateAcceleratorPress(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)),
      WaitForGlicClose());
}

// TODO(crbug.com/454088252): Re-enable this test when there is a glic state for
// post-resize.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       DISABLED_CloseWithContextMenu) {
  RunTestSequence(
      OpenGlicFloatingWindow(), MoveMouseTo(kGlicViewElementId),
      MayInvolveNativeContextMenu(
          ClickMouse(ui_controls::RIGHT), WaitForHide(kBrowserViewElementId),
          InAnyContext(
              SelectMenuItem(RenderViewContextMenu::kGlicCloseMenuItem))),
      CheckControllerHasWidget(false));
}

// TODO(crbug.com/401158115): Flaky on macOS
#if BUILDFLAG(IS_MAC)
#define MAYBE_OpenMenuItemShows DISABLED_OpenMenuItemShows
#else
#define MAYBE_OpenMenuItemShows OpenMenuItemShows
#endif
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, MAYBE_OpenMenuItemShows) {
  if (!base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    RunTestSequence(SimulateOpenMenuItem(),
                    WaitForAndInstrumentGlic(kHostAndContents),
                    CheckControllerWidgetMode(GlicWindowMode::kDetached),
                    CloseGlicWindow());
  } else {
    TrackGlicInstanceWithTabIndex(0);
    RunTestSequence(SimulateOpenMenuItem(),
                    WaitForAndInstrumentGlic(kHostAndContents),
                    CheckControllerWidgetMode(GlicWindowMode::kAttached),
                    CloseGlicWindow());
  }
}

#if BUILDFLAG(IS_WIN)
// On Windows, the OsButton toggles opening and closing floaty, because floaty
// will never be active when the os button is clicked.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, OsButtonToggles) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(SimulateOsButton(),
                  WaitForAndInstrumentGlic(kHostAndContents),
                  CheckControllerWidgetMode(GlicWindowMode::kDetached),
                  SimulateOsButton(), WaitForHide(test::kGlicHostElementId));
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenMenuItemWhenAttachedToActiveBrowserDoesNotClose) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    GTEST_SKIP() << "N/A for multi-instance";
  }
  RunTestSequence(
      OpenGlicFloatingWindow(),
      // Glic should close.
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId), SimulateOpenMenuItem(),
      CheckControllerShowing(true));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenMenuItemWhenDetachedActiveDoesNotClose) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    GTEST_SKIP() << "N/A for multi-instance";
  }
  RunTestSequence(
      OpenGlicFloatingWindow(),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateOpenMenuItem(), CheckControllerShowing(true));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpeningProfilePickerClosesPanel) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance. This behavior may be
    // obsolete.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      OpenGlicFloatingWindow(),
      CheckControllerWidgetMode(GlicWindowMode::kDetached), Do([&]() {
        glic::GlicProfileManager::GetInstance()->ShowProfilePicker();
      }),
      WaitForGlicClose());
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ClientUnresponsiveThenError) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  base::HistogramTester histogram_tester;
  RunTestSequence(
      OpenGlicFloatingWindow(),
      ClickMockGlicElement(kMockGlicClientHangButton, true),
      ObserveState(test::internal::kGlicAppState, GetHost()),
      WaitForState(test::internal::kGlicAppState,
                   mojom::WebUiState::kUnresponsive),
      // Client should show error after showing the unresponsive UI for 5s.
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kError));
  histogram_tester.ExpectTotalCount(
      "Glic.Host.WebClientUnresponsiveState.Duration", 1);
  histogram_tester.ExpectTotalCount("Glic.Host.WebClientUnresponsiveState", 2);
  // One sample in the WebClientUnresponsiveState.ENTERED_FROM_CUSTOM_HEARTBEAT
  // (1) bucket.
  histogram_tester.ExpectBucketCount("Glic.Host.WebClientUnresponsiveState", 1,
                                     1);
  // One sample in the WebClientUnresponsiveState.EXITED (4) bucket.
  histogram_tester.ExpectBucketCount("Glic.Host.WebClientUnresponsiveState", 4,
                                     1);
}

// ASAN builds are slow enough that the responsiveness check actually sometimes
// triggers just while setting this up, before we deactivate the window.
// Rather than adjust the timeouts to make this test even slower, just disable
// it for those builds (as well as similarly slow sanitizer builds).
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(MEMORY_SANITIZER)
#define MAYBE_ClientUnresponsiveWhileBrowserNotActive \
  DISABLED_ClientUnresponsiveWhileBrowserNotActive
#else
#define MAYBE_ClientUnresponsiveWhileBrowserNotActive \
  ClientUnresponsiveWhileBrowserNotActive
#endif
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       MAYBE_ClientUnresponsiveWhileBrowserNotActive) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  const base::TimeDelta kTimeToWait = base::Seconds(7);
  ASSERT_GT(kTimeToWait,
            base::Milliseconds(
                features::kGlicClientResponsivenessCheckTimeoutMs.Get() +
                features::kGlicClientResponsivenessCheckIntervalMs.Get()));

  // This is another window to which we can give focus. It's not otherwise
  // guaranteed that we can reliably make all relevant windows inactive
  // (this is subtle and platform-specific, unfortunately).
  auto other_widget = std::make_unique<views::Widget>();

  base::HistogramTester histogram_tester;
  RunTestSequence(
      ObserveState(test::internal::kGlicAppState, GetHost()),
      OpenGlicFloatingWindow(),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kReady),
      ObserveState(views::test::kCurrentWidgetFocus),
      WithView(kBrowserViewElementId,
               [&](BrowserView* browser_view) {
                 views::Widget::InitParams params{
                     views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW};
                 params.bounds = gfx::Rect(0, 0, 200, 200);
                 params.context = browser_view->GetWidget()->GetNativeWindow();
                 other_widget->Init(std::move(params));
                 other_widget->Show();
                 browser_view->GetWidget()->Deactivate();
                 other_widget->Activate();
               }),
      WaitForState(views::test::kCurrentWidgetFocus, other_widget.get()),
      // This click dispatches via JavaScript and doesn't change focus.
      ClickMockGlicElement(kMockGlicClientHangButton, true), Wait(kTimeToWait),
      CheckState(test::internal::kGlicAppState, mojom::WebUiState::kReady),
      Do([&] {
        histogram_tester.ExpectTotalCount(
            "Glic.Host.WebClientUnresponsiveState", 0);
      }),
      Do([&] { browser()->window()->Activate(); }),
      WaitForState(test::internal::kGlicAppState,
                   mojom::WebUiState::kUnresponsive),
      Do([&] {
        // ENTERED_FROM_CUSTOM_HEARTBEAT (1)
        histogram_tester.ExpectBucketCount(
            "Glic.Host.WebClientUnresponsiveState", 1, 1);
      }));
}

// Note: ChromeOS maintains account auth as a part of OS User session.
// So invalidation is not supported.
// TODO(crbug.com/450629835): Revisit if we figure out actual flow we need
// reauth.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       InvalidatedAccountWhileLoadingGlic) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(
      SimulateGlicHotkey(), ForceInvalidateAccount(),
      WaitForAndInstrumentGlic(kHostOnly),
      WaitForWebUIState(mojom::WebUiState::kSignIn),
      InAnyContext(ClickElement(test::kGlicHostElementId, {"#signInButton"},
                                ui_controls::LEFT, ui_controls::kNoAccelerator,
                                ExecuteJsMode::kFireAndForget)),
      WaitForHide(test::kGlicHostElementId),
      // Without a pause here, we will 'sign-in' before the callback is
      // registered to listen for it. This isn't a bug because it takes real
      // users finite time to actually sign-in.
      Wait(base::Milliseconds(500)), ForceReauthAccount(),
      WaitForWebUIState(mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       InvalidatedAccountSignInOnGlicOpenFlow) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance, requirements have changed.
    // Update this test.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  RunTestSequence(ForceInvalidateAccount(), SimulateGlicHotkey(),
                  CheckControllerHasWidget(false), InstrumentTab(kFirstTab),
                  WaitForWebContentsReady(kFirstTab),
                  // Without a pause here, we will 'sign-in' before the callback
                  // is registered to listen for it. This isn't a bug because it
                  // takes real users finite time to actually sign-in.
                  Wait(base::Milliseconds(500)), ForceReauthAccount(),
                  WaitForAndInstrumentGlic(kHostOnly),
                  WaitForWebUIState(mojom::WebUiState::kReady));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       AccountInvalidatedWhileGlicOpen) {
  TrackGlicInstanceWithTabIndex(0);
  RunTestSequence(
      SimulateGlicHotkey(), WaitForWebUIState(mojom::WebUiState::kReady),
      ForceInvalidateAccount(), WaitForWebUIState(mojom::WebUiState::kSignIn),
      ForceReauthAccount(), WaitForWebUIState(mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       DetachedWidgetIsTrackedByOcclusionTracker) {
  RunTestSequence(OpenGlicFloatingWindow(), CheckOcclusionTracked(true));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, TestInitialBounds) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  // The GlicButton and Tabstrip are not actually shown until a tab is created.
  chrome::AddTabAt(browser(), GURL("about:blank"), 0, true);
  // Calculate default location offset from work area.
  gfx::Point top_right =
      display::Screen::Get()->GetPrimaryDisplay().work_area().top_right();
  int expected_x = top_right.x() - GlicWidget::GetInitialSize().width() -
                   glic::kDefaultDetachedTopRightDistance;
  int expected_y = top_right.y() + glic::kDefaultDetachedTopRightDistance;
  gfx::Point default_origin(expected_x, expected_y);

  // Check that with no saved position the default location is used.
  gfx::Rect initial_bounds = window_controller().GetInitialBounds(nullptr);
  EXPECT_EQ(initial_bounds.origin(), default_origin);

  // Initial bounds with browser are valid and not default location.
  initial_bounds = window_controller().GetInitialBounds(browser());
  EXPECT_NE(initial_bounds.origin(), default_origin);

  // Use default location if Glic button location results in an invalid widget
  // location. Move browser window so that it is mostly off the screen to the
  // right.
  browser()->window()->SetBounds(
      {{top_right.x() + 500, top_right.y() + 50}, {900, 900}});
  initial_bounds = window_controller().GetInitialBounds(browser());
  EXPECT_EQ(initial_bounds.origin(), default_origin);

  gfx::Rect screen_bounds =
      display::Screen::Get()->GetPrimaryDisplay().bounds();

  struct TestPair {
    gfx::Point test;
    gfx::Point expected;
    std::string msg;
  };

  std::vector<TestPair> test_points = {
      {{10, 20}, {10, 20}, "Valid position on screen"},

      // Valid positions off each corner.
      {{-20, -2}, {-20, -2}, "Valid top-left"},
      {{-20, screen_bounds.height() - 100},
       {-20, screen_bounds.height() - 100},
       "Valid bottom left"},
      {{screen_bounds.width() - initial_bounds.width() + 20,
        screen_bounds.height() - 100},
       {screen_bounds.width() - initial_bounds.width() + 20,
        screen_bounds.height() - 100},
       "Valid bottom right"},
      {{screen_bounds.width() - initial_bounds.width() + 20, -2},
       {screen_bounds.width() - initial_bounds.width() + 20, -2},
       "Valid top right"},

      // Invalid positions off of each edge
      {{10, -5}, default_origin, "Invalid top"},
      {{-400, 10}, default_origin, "Invalid left"},
      {{10, screen_bounds.height() + 600}, default_origin, "Invalid bottom"},
      {{screen_bounds.width() + 400, 10}, default_origin, "Invalid right"},
  };

  for (auto& t : test_points) {
    window_controller().SetPreviousPositionForTesting(t.test);
    initial_bounds = window_controller().GetInitialBounds(nullptr);
    EXPECT_EQ(initial_bounds.origin(), t.expected) << t.msg;
  }
}

// TODO(b/426542319): Fix and enable tests on non-mac platforms.
#if BUILDFLAG(IS_MAC)
class GlicWindowControllerLocationMetricsUiTest
    : public GlicWindowControllerUiTest {
 public:
  GlicWindowControllerLocationMetricsUiTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kGlicPanelResetOnSessionTimeout,
                               features::kGlicPanelResetSizeAndLocationOnOpen});
  }
  ~GlicWindowControllerLocationMetricsUiTest() override = default;

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerLocationMetricsUiTest,
                       TestPositionMetrics) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  if (IsWorkAreaTooSmallForTest()) {
    GTEST_SKIP()
        << "Test's work area bounds are too small for consistent results.";
  }
  // The GlicButton and Tabstrip are not actually shown until a tab is created.
  chrome::AddTabAt(browser(), GURL("about:blank"), 0, true);
  gfx::Rect work_area_bounds =
      display::Screen::Get()->GetPrimaryDisplay().work_area();
  // Work area is split into 9 cells.
  gfx::Size cell_size = {work_area_bounds.width() / 3,
                         work_area_bounds.height() / 3};
  // Set browser bounds to the center cell of the work area bounds.
  gfx::Rect browser_bounds = gfx::Rect(
      gfx::Point(work_area_bounds.width() / 3 + work_area_bounds.x(),
                 work_area_bounds.height() / 3 + work_area_bounds.y()),
      cell_size);
  browser()->window()->SetBounds(browser_bounds);
  browser_bounds = browser()->window()->GetBounds();

  base::HistogramTester tester;

  auto open_and_close = [this,
                         &tester](ChromeRelativePosition expected_position) {
    RunTestSequence(ActivateSurface(kBrowserViewElementId),
                    SimulateGlicHotkey(), WaitForAndInstrumentGlic(kNone),

                    CheckControllerWidgetMode(GlicWindowMode::kDetached),
                    SimulateOsButton(), WaitForHide(test::kGlicHostElementId),
                    CheckControllerHasWidget(false));

    tester.ExpectBucketCount("Glic.PositionOnChrome.OnOpen", expected_position,
                             1);
    tester.ExpectBucketCount("Glic.PositionOnChrome.OnClose", expected_position,
                             1);
  };

  window_controller().SetPreviousPositionForTesting(work_area_bounds.origin());
  open_and_close(ChromeRelativePosition::kAboveLeft);

  window_controller().SetPreviousPositionForTesting(
      {work_area_bounds.origin().x(), browser_bounds.origin().y()});
  open_and_close(ChromeRelativePosition::kCenterLeft);

  window_controller().SetPreviousPositionForTesting(
      {work_area_bounds.origin().x(), browser_bounds.bottom()});
  open_and_close(ChromeRelativePosition::kBelowLeft);

  window_controller().SetPreviousPositionForTesting(
      {browser_bounds.x(), work_area_bounds.origin().y()});
  open_and_close(ChromeRelativePosition::kAboveCenter);

  window_controller().SetPreviousPositionForTesting(browser_bounds.origin());
  open_and_close(ChromeRelativePosition::kOverlap);

  window_controller().SetPreviousPositionForTesting(
      browser_bounds.bottom_left());
  open_and_close(ChromeRelativePosition::kBelowCenter);

  window_controller().SetPreviousPositionForTesting(
      {browser_bounds.right(), work_area_bounds.y()});
  open_and_close(ChromeRelativePosition::kAboveRight);

  window_controller().SetPreviousPositionForTesting(browser_bounds.top_right());
  open_and_close(ChromeRelativePosition::kCenterRight);

  window_controller().SetPreviousPositionForTesting(
      browser_bounds.bottom_right());
  open_and_close(ChromeRelativePosition::kBelowRight);

  RunTestSequence(OpenGlicFloatingWindow());
  browser()->window()->Minimize();
  ASSERT_TRUE(ui_test_utils::WaitForMinimized(browser()));
  EXPECT_FALSE(browser()->window()->IsActive());
  RunTestSequence(CloseGlicWindow());
  tester.ExpectBucketCount("Glic.PositionOnChrome.OnClose",
                           ChromeRelativePosition::kNoVisibleChromeBrowser, 1);

  // ChromeRelativePosition::kChromeOnOtherDisplay isn't being tested since
  // tests involving moving Glic to another display are flaky.
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerLocationMetricsUiTest,
                       TestPercentOverlapMetrics) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  if (IsWorkAreaTooSmallForTest()) {
    GTEST_SKIP()
        << "Test's work area bounds are too small for consistent results.";
  }
  // The GlicButton and Tabstrip are not actually shown until a tab is created.
  chrome::AddTabAt(browser(), GURL("about:blank"), 0, true);
  // Set browser bounds to the center cell of the work area bounds.
  gfx::Rect browser_bounds = gfx::Rect(50, 50, 500, 400);
  browser()->window()->SetBounds(browser_bounds);
  browser_bounds = browser()->window()->GetBounds();
  gfx::Size glic_expected_size = GlicWidget::GetInitialSize();

  base::HistogramTester tester;

  auto open_and_close = [this,
                         &tester](PercentOverlap expected_percent_overlap) {
    RunTestSequence(ActivateSurface(kBrowserViewElementId),
                    SimulateGlicHotkey(), WaitForAndInstrumentGlic(kNone),

                    CheckControllerWidgetMode(GlicWindowMode::kDetached),
                    SimulateOsButton(), WaitForHide(test::kGlicHostElementId),
                    CheckControllerHasWidget(false));

    tester.ExpectBucketCount("Glic.PercentOverlapWithBrowser.OnOpen",
                             expected_percent_overlap, 1);
    tester.ExpectBucketCount("Glic.PercentOverlapWithBrowser.OnClose",
                             expected_percent_overlap, 1);
  };

  gfx::Point test_origin = browser_bounds.top_right();
  window_controller().SetPreviousPositionForTesting(test_origin);
  open_and_close(PercentOverlap::k0);

  test_origin.Offset(-0.5 * glic_expected_size.width(), 0);
  window_controller().SetPreviousPositionForTesting(test_origin);
  open_and_close(PercentOverlap::k50);

  test_origin = browser_bounds.origin();
  window_controller().SetPreviousPositionForTesting(test_origin);
  open_and_close(PercentOverlap::k100);

  RunTestSequence(OpenGlicFloatingWindow());
  browser()->window()->Minimize();
  ASSERT_TRUE(ui_test_utils::WaitForMinimized(browser()));
  EXPECT_FALSE(browser()->window()->IsActive());
  RunTestSequence(CloseGlicWindow());
  tester.ExpectBucketCount("Glic.PositionOnChrome.OnClose",
                           ChromeRelativePosition::kNoVisibleChromeBrowser, 1);
}
#endif  // BUILDFLAG(IS_MAC)

// Note: ChromeOS maintains account auth as a part of OS User session,
// and Profile is coupled with the User. Thus, deletion Profile
// during the use should not happen.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, PermanentlyDeleteProfile) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  Profile& profile1 = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  Browser* const browser1 = CreateBrowser(&profile1);
  GlicKeyedService* const service1 =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser1->profile());
  service1->fre_controller().AcceptFre(nullptr);
  EXPECT_TRUE(service1->enabling().HasConsented());

  // Open glic
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  service1->ToggleUI(nullptr, false, mojom::InvocationSource::kOsHotkey);
  EXPECT_TRUE(service1->IsWindowShowing());

  // Delete the second profile
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      browser1->profile()->GetPath(), base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  ui_test_utils::WaitForBrowserToClose(browser1);

  EXPECT_FALSE(service1->IsWindowShowing());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

class GlicWindowControllerWithPreviousPostionUiTest
    : public GlicWindowControllerUiTest {
 public:
  GlicWindowControllerWithPreviousPostionUiTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kGlicPanelResetOnSessionTimeout,
                               features::kGlicPanelResetSizeAndLocationOnOpen,
                               features::kGlicPanelResetOnStart});
  }
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    // Set initial bounds via pref and check that they are used.
    Profile::FromBrowserContext(context)->GetPrefs()->SetInteger(
        prefs::kGlicPreviousPositionX, 20);
    Profile::FromBrowserContext(context)->GetPrefs()->SetInteger(
        prefs::kGlicPreviousPositionY, 10);
    test::InteractiveGlicTest::SetUpBrowserContextKeyedServices(context);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerWithPreviousPostionUiTest,
                       TestInitialBounds) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  // Check that the saved initial bounds are used.
  gfx::Rect initial_bounds = window_controller().GetInitialBounds(nullptr);
  ASSERT_EQ(initial_bounds.origin(), gfx::Point(20, 10));
}

class GlicWindowControllerUnloadOnCloseTest
    : public GlicWindowControllerUiTest {
 public:
  GlicWindowControllerUnloadOnCloseTest() {
    features_.InitAndEnableFeature(features::kGlicUnloadOnClose);
  }
  ~GlicWindowControllerUnloadOnCloseTest() override = default;

  auto CheckWebUiContentsExist(bool exist) {
    return CheckResult([this]() { return !!GetHost()->webui_contents(); },
                       exist, "CheckWebUiContentsExist");
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUnloadOnCloseTest, UnloadOnClose) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    GTEST_SKIP() << "N/A for kGlicMultiInstance";
  }
  RunTestSequence(OpenGlicFloatingWindow(), CheckWebUiContentsExist(true),
                  CloseGlicWindow(), CheckWebUiContentsExist(false));
}

class GlicWindowControllerWithMemoryPressureUiTest
    : public GlicWindowControllerUiTest {
 public:
  GlicWindowControllerWithMemoryPressureUiTest() {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kGlicWarming,
          {{features::kGlicWarmingDelayMs.name, "0"},
           {features::kGlicWarmingJitterMs.name, "0"}}}},
        /*disabled_features=*/{});
  }
  ~GlicWindowControllerWithMemoryPressureUiTest() override = default;

  void SetUp() override {
    // This will temporarily disable preloading to ensure that we don't load the
    // web client before we've initialized the embedded test server and can set
    // the correct URL.
    GlicProfileManager::ForceMemoryPressureForTesting(
        base::MEMORY_PRESSURE_LEVEL_CRITICAL);
    GlicWindowControllerUiTest::SetUp();
  }

  void TearDown() override {
    GlicWindowControllerUiTest::TearDown();
    GlicProfileManager::ForceMemoryPressureForTesting(std::nullopt);
  }

 protected:
  auto ResetMemoryPressure() {
    return Do([]() {
      GlicProfileManager::ForceMemoryPressureForTesting(
          base::MEMORY_PRESSURE_LEVEL_NONE);
    });
  }

  auto TryPreload() {
    return Do([this]() { glic_service()->TryPreload(); });
  }

  auto CheckWarmed() {
    return Do([this]() {
      if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
        EXPECT_TRUE(GetInstanceCoordinator().HasWarmedInstanceForTesting());
      } else {
        EXPECT_TRUE(GetWindowControllerImpl().IsWarmed());
      }
    });
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerWithMemoryPressureUiTest, Preload) {
  // TODO(crbug.com/411100559): Wait for preload completion rather than assuming
  // that it will finish before the next step in the sequence.
  RunTestSequence(
      ResetMemoryPressure(), TryPreload(), CheckWarmed(),
      PressButton(kGlicButtonElementId),
      InAnyContext(
          WaitForShow(kGlicViewElementId).SetMustRemainVisible(false)));
}

// TODO(crbug.com/454091592): Fix and re-enable.
// These tests for dragging across multiple displays is for mac-only.
// On Windows11, this test times out in calling WaitForDisplaySizes() when
// setting up the virtual displays.
#if !BUILDFLAG(IS_MAC)
#define MAYBE_GlicWindowControllerMultipleDisplaysUiTest \
  DISABLED_GlicWindowControllerMultipleDisplaysUiTest
#else
#define MAYBE_GlicWindowControllerMultipleDisplaysUiTest \
  GlicWindowControllerMultipleDisplaysUiTest
#endif
class MAYBE_GlicWindowControllerMultipleDisplaysUiTest
    : public GlicWindowControllerUiTest {
 public:
  MAYBE_GlicWindowControllerMultipleDisplaysUiTest() = default;
  MAYBE_GlicWindowControllerMultipleDisplaysUiTest(
      const MAYBE_GlicWindowControllerMultipleDisplaysUiTest&) = delete;
  MAYBE_GlicWindowControllerMultipleDisplaysUiTest& operator=(
      const MAYBE_GlicWindowControllerMultipleDisplaysUiTest&) = delete;
  ~MAYBE_GlicWindowControllerMultipleDisplaysUiTest() override = default;

  // Create virtual displays as needed, ensuring 2 displays are available for
  // testing multi-screen functionality.
  bool SetUpVirtualDisplays() {
    if (display::Screen::Get()->GetNumDisplays() > 1) {
      return true;
    }
    if ((virtual_display_util_ = display::test::VirtualDisplayUtil::TryCreate(
             display::Screen::Get()))) {
      virtual_display_util_->AddDisplay(
          display::test::VirtualDisplayUtil::k1024x768);
      return true;
    }
    return false;
  }

  auto CheckDisplaysSetUp(bool is_set_up) {
    return CheckResult([this]() { return SetPrimaryAndSecondaryDisplay(); },
                       is_set_up, "CheckDisplaysSetUp");
  }

  auto CheckWidgetMovedToSecondaryDisplay(bool expect_moved) {
    return CheckResult(
        [this]() {
          return secondary_display_.id() ==
                 window_controller().GetGlicWidget()->GetNearestDisplay()->id();
        },
        expect_moved, "CheckWidgetMovedToSecondaryDisplay");
  }

  bool SetPrimaryAndSecondaryDisplay() {
    display::Display primary_display =
        display::Screen::Get()->GetPrimaryDisplay();
    secondary_display_ =
        ui_test_utils::GetSecondaryDisplay(display::Screen::Get());
    return primary_display.id() && secondary_display_.id();
  }

  auto MoveWidgetToSecondDisplay() {
    return Do([this]() {
      const gfx::Point target(secondary_display_.bounds().CenterPoint());
      // Replace with dragging simulation once supported.
      window_controller().GetGlicWidget()->SetBounds(
          gfx::Rect(target, window_controller()
                                .GetGlicWidget()
                                ->GetWindowBoundsInScreen()
                                .size()));
    });
  }

  auto DetachGlicWindow() {
    return Do([this]() {
      static_cast<GlicWindowControllerImpl&>(window_controller()).Detach();
    });
  }

  void TearDownOnMainThread() override {
    GlicWindowControllerUiTest::TearDownOnMainThread();
    virtual_display_util_.reset();
  }

 private:
  std::unique_ptr<display::test::VirtualDisplayUtil> virtual_display_util_;
  display::Display secondary_display_;
};

// TODO(crbug.com/399703468): Flaky on Mac. Test is targeted for Mac only.
IN_PROC_BROWSER_TEST_F(MAYBE_GlicWindowControllerMultipleDisplaysUiTest,
                       DISABLED_MoveDetachedGlicWindowToSecondDisplay) {
  if (!SetUpVirtualDisplays()) {
    return;
  }

  RunTestSequence(CheckDisplaysSetUp(true), OpenGlicFloatingWindow(),

                  CheckControllerWidgetMode(GlicWindowMode::kDetached),
                  InAnyContext(MoveWidgetToSecondDisplay(),
                               CheckWidgetMovedToSecondaryDisplay(true)),
                  CloseGlicWindow());
}

// TODO(crbug.com/399703468): Flaky on Mac. Test is targeted for Mac only.
IN_PROC_BROWSER_TEST_F(
    MAYBE_GlicWindowControllerMultipleDisplaysUiTest,
    DISABLED_DetachAttachedGlicWindowAndMoveToSecondDisplay) {
  if (!SetUpVirtualDisplays()) {
    return;
  }

  RunTestSequence(CheckDisplaysSetUp(true), OpenGlicFloatingWindow(),

                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  InAnyContext(DetachGlicWindow(), MoveWidgetToSecondDisplay(),
                               CheckWidgetMovedToSecondaryDisplay(true)));
}

}  // namespace glic
