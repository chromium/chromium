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
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
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
#include "ui/gfx/native_widget_types.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/test/button_test_api.h"

namespace glic {

namespace {

const InteractiveBrowserTestApi::DeepQuery
    kMockGlicClientStart3sUnresponsiveButton = {"#busyWork3s"};
const InteractiveBrowserTestApi::DeepQuery
    kMockGlicClientStart8sUnresponsiveButton = {"#busyWork8s"};

}  // anonymous namespace

class GlicWindowControllerUiTest : public test::InteractiveGlicTest {
 public:
  GlicWindowControllerUiTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kGlicHostLogging);
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

  auto ForceInvalidateAccount() {
    return Do([this]() { InvalidateAccount(window_controller().profile()); });
  }

  auto ForceReauthAccount() {
    return Do([this]() { ReauthAccount(window_controller().profile()); });
  }

 private:
  std::unique_ptr<GlicController> glic_controller_ =
      std::make_unique<GlicController>();
};

// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true.
// TODO(394945970): Check top right corner position.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       DISABLED_ShowAndCloseAttachedWidget) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  // Verify glic is open in attached mode.
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, ShowAndCloseDetachedWidget) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kDetached),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, DoNotCrashOnBrowserClose) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached));
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, DoNotCrashWhenReopening) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached), CloseGlicWindow(),
                  OpenGlicWindow(GlicWindowMode::kAttached));
}

// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true.
IN_PROC_BROWSER_TEST_F(
    GlicWindowControllerUiTest,
    DISABLED_OpenAttachedThenOpenAttachedToSameBrowserCloses) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  ToggleGlicWindow(GlicWindowMode::kAttached),
                  InAnyContext(WaitForHide(kGlicViewElementId)),
                  CheckControllerHasWidget(false));
}

// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true.
IN_PROC_BROWSER_TEST_F(
    GlicWindowControllerUiTest,
    DISABLED_OpenAttachedThenOpenAttachedToDifferentBrowser) {
  Browser* const new_browser = CreateBrowser(browser()->profile());

  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  InContext(new_browser->window()->GetElementContext(),
                            PressButton(kGlicButtonElementId)),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  CheckIfAttachedToBrowser(new_browser));
}

// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true.
#if !BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(
    GlicWindowControllerUiTest,
    DISABLED_OpenAttachedThenOpenAttachedToDifferentBrowserWithHotkey) {
  Browser* const new_browser = CreateBrowser(browser()->profile());

  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  Do([&]() { new_browser->window()->Activate(); }),
                  SimulateGlicHotkey(), CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  CheckIfAttachedToBrowser(new_browser));
}
#endif

// Disabled due to flakes Mac; see https://crbug.com/394350688.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       DISABLED_OpenDetachedAndThenOpenAttached) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  PressButton(kGlicButtonElementId),
                  WaitForEvent(kGlicButtonElementId, kGlicWidgetAttached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

constexpr char kActivateSurfaceIncompatibilityNotice[] =
    "Programmatic window activation does not work on the Weston reference "
    "implementation of Wayland used on Linux testbots. It also doesn't work "
    "reliably on Linux in general. For this reason, some of these tests which "
    "use ActivateSurface() may be skipped on machine configurations which do "
    "not reliably support them.";

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ButtonWhenAttachedToActiveBrowserCloses) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId),
      // Glic should close.
      PressButton(kGlicButtonElementId),
      InAnyContext(WaitForHide(kGlicViewElementId)),
      CheckControllerHasWidget(false));
}

// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       DISABLED_HotkeyWhenAttachedToActiveBrowserCloses) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      // Glic should close.
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId), SimulateGlicHotkey(),
      InAnyContext(WaitForHide(kGlicViewElementId)),
      CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyWhenDetachedActiveCloses) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateGlicHotkey(), InAnyContext(WaitForHide(kGlicViewElementId)),
      CheckControllerHasWidget(false));
}

// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       DISABLED_HotkeyAttachesToActiveBrowser) {
  RunTestSequence(
      // Glic should open attached to active browser.
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId), SimulateGlicHotkey(),
      InAnyContext(WaitForShow(kGlicViewElementId).SetMustRemainVisible(false)),
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kAttached));
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
  RunTestSequence(
      SimulateGlicHotkey(),
      InAnyContext(WaitForShow(kGlicViewElementId).SetMustRemainVisible(false)),
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kDetached));
}

#if !BUILDFLAG(IS_LINUX)
// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true. Widget activation doesn't work on
// Linux; see InteractionTestUtilSimulatorViews::ActivateWidget.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       DISABLED_CanFocusGlicWindowWithFocusDialogHotkey) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached),
      ActivateSurface(kBrowserViewElementId),
      // Activating the browser actually focuses the omnibox.
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, true),
      // Trigger the popup focusing code.
      Do([&]() {
        browser()->GetBrowserView().FocusInactivePopupForAccessibility();
      }),
      // That should have moved the focus back to the Glic web view.
      CheckViewProperty(kOmniboxElementId, &views::View::HasFocus, false),
      InAnyContext(CheckViewProperty(GlicView::kWebViewElementIdForTesting,
                                     &views::View::HasFocus, true)));
}
#endif  // !BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyOpensDetachedWithNonActiveBrowser) {
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
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kDetached));
}
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ESCWhenDetachedActiveCloses) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateAcceleratorPress(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)),
      InAnyContext(WaitForHide(kGlicViewElementId)),
      CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ESCWhenAttachedActiveCloses) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateAcceleratorPress(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)),
      InAnyContext(WaitForHide(kGlicViewElementId)),
      CheckControllerHasWidget(false));
}

// TODO(388102775): When Mac app focus issues are resolved, add a test to verify
// that invoking the hotkey while open detached always closes glic regardless of
// activation.

// TODO (crbug.com/406528268): Delete or fix tests that are disabled because
// kGlicAlwaysDetached is now default true.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, DISABLED_ApiDetach) {
  base::HistogramTester tester;
  RunTestSequence(
      // Open attached.
      OpenGlicWindow(GlicWindowMode::kAttached), CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kAttached),

      // Detach. State will temporarily be kDetaching but will again be kOpen
      // after the animation runs.
      // TODO(393203136): Observe state() without polling, then we can verify
      // that we go to kDetaching and then kOpen.
      ObserveState(test::internal::kGlicWindowControllerState,
                   std::ref(window_controller())),
      Do([this] { window_controller().Detach(); }),
      WaitForState(test::internal::kGlicWindowControllerState,
                   GlicWindowController::State::kOpen),
      StopObservingState(test::internal::kGlicWindowControllerState),

      CheckControllerWidgetMode(GlicWindowMode::kDetached));

  tester.ExpectTotalCount("Glic.AttachedToBrowser", 1);
  tester.ExpectBucketCount("Glic.AttachedToBrowser", AttachChangeReason::kInit,
                           1);
  tester.ExpectTotalCount("Glic.DetachedFromBrowser", 1);
  tester.ExpectBucketCount("Glic.DetachedFromBrowser",
                           AttachChangeReason::kMenu, 1);
}

// TODO: Re-nable this test when there is a glic state for post-resize.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       DISABLED_CloseWithContextMenu) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  CheckControllerHasWidget(true));
  auto center =
      window_controller().GetGlicView()->GetBoundsInScreen().CenterPoint();
  RunTestSequence(
      MoveMouseTo(center), ClickMouse(ui_controls::RIGHT),
      InAnyContext(SelectMenuItem(RenderViewContextMenu::kGlicCloseMenuItem)),
      CheckControllerHasWidget(false));
}

// Flaky on macOS: https://crbug.com/401158115
#if BUILDFLAG(IS_MAC)
#define MAYBE_OpenMenuItemShows DISABLED_OpenMenuItemShows
#else
#define MAYBE_OpenMenuItemShows OpenMenuItemShows
#endif
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, MAYBE_OpenMenuItemShows) {
  RunTestSequence(SimulateOpenMenuItem(),
                  WaitForAndInstrumentGlic(kHostAndContents),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kDetached),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenMenuItemWhenAttachedToActiveBrowserDoesNotClose) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      // Glic should close.
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kBrowserViewElementId), SimulateOpenMenuItem(),
      CheckControllerShowing(true));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenMenuItemWhenDetachedActiveDoesNotClose) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached),
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              kActivateSurfaceIncompatibilityNotice),
      InAnyContext(ActivateSurface(test::kGlicHostElementId)),
      SimulateOpenMenuItem(), CheckControllerShowing(true));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpeningProfilePickerClosesPanel) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached),
      CheckControllerWidgetMode(GlicWindowMode::kDetached), Do([&]() {
        glic::GlicProfileManager::GetInstance()->ShowProfilePicker();
      }),
      CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ClientUnresponsiveThenResumeResponsive) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicClientStart3sUnresponsiveButton, true),
      ObserveState(test::internal::kGlicAppState, &window_controller()),
      WaitForState(test::internal::kGlicAppState,
                   mojom::WebUiState::kUnresponsive),
      // Client should resume responsive if unresponsive less than 5s.
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ClientUnresponsiveThenError) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      ClickMockGlicElement(kMockGlicClientStart8sUnresponsiveButton, true),
      ObserveState(test::internal::kGlicAppState, &window_controller()),
      WaitForState(test::internal::kGlicAppState,
                   mojom::WebUiState::kUnresponsive),
      // Client should show error after showing the unresponsive UI for 5s.
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kError));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       InvalidatedAccountSignInOnGlicOpenFlow) {
  RunTestSequence(
      ObserveState(test::internal::kGlicAppState, &window_controller()),
      ForceInvalidateAccount(), SimulateGlicHotkey(),
      CheckControllerHasWidget(true), WaitForAndInstrumentGlic(kHostOnly),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kSignIn),
      InAnyContext(ClickElement(test::kGlicHostElementId, {"#signInButton"},
                                ui_controls::LEFT, ui_controls::kNoAccelerator,
                                ExecuteJsMode::kFireAndForget)),
      WaitForHide(test::kGlicHostElementId),
      // Without a pause here, we will 'sign-in' before the callback is
      // registered to listen for it. This isn't a bug because it takes real
      // users finite time to actually sign-in.
      Wait(base::Milliseconds(500)), ForceReauthAccount(),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kReady));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       AccountInvalidatedWhileGlicOpen) {
  RunTestSequence(
      SimulateGlicHotkey(), CheckControllerHasWidget(true),
      ObserveState(test::internal::kGlicAppState, &window_controller()),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kReady),
      ForceInvalidateAccount(),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kSignIn));
}

// Open glic with an invalidated account, then sign in without clicking the
// sign-in button. The web client should loaded and shown.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenGlicWithInvalidatedAccountAndThenSignIn) {
  RunTestSequence(
      ForceInvalidateAccount(), SimulateGlicHotkey(),
      CheckControllerHasWidget(true),
      ObserveState(test::internal::kGlicAppState, &window_controller()),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kSignIn),
      ForceReauthAccount(),
      WaitForState(test::internal::kGlicAppState, mojom::WebUiState::kReady));
}

class GlicWindowControllerWithMemoryPressureUiTest
    : public GlicWindowControllerUiTest {
 public:
  GlicWindowControllerWithMemoryPressureUiTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/
        {features::kGlicWarming},
        /*disabled_features=*/{});
  }
  ~GlicWindowControllerWithMemoryPressureUiTest() override = default;

  void SetUp() override {
    // This will temporarily disable preloading to ensure that we don't load the
    // web client before we've initialized the embedded test server and can set
    // the correct URL.
    GlicProfileManager::ForceMemoryPressureForTesting(&forced_memory_pressure_);
    GlicWindowControllerUiTest::SetUp();
  }

  void TearDown() override {
    GlicWindowControllerUiTest::TearDown();
    GlicProfileManager::ForceMemoryPressureForTesting(nullptr);
  }

 protected:
  void ResetMemoryPressure() {
    forced_memory_pressure_ = base::MemoryPressureMonitor::MemoryPressureLevel::
        MEMORY_PRESSURE_LEVEL_NONE;
  }

 private:
  base::MemoryPressureMonitor::MemoryPressureLevel forced_memory_pressure_ =
      base::MemoryPressureMonitor::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_CRITICAL;

  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerWithMemoryPressureUiTest, Preload) {
  ResetMemoryPressure();
  glic_service()->TryPreload();
  EXPECT_TRUE(window_controller().IsWarmed());
  RunTestSequence(
      PressButton(kGlicButtonElementId),
      InAnyContext(
          WaitForShow(kGlicViewElementId).SetMustRemainVisible(false)));
}

// These tests for dragging across multiple displays is for mac-only.
// On Windows11, this test times out in calling WaitForDisplaySizes() when
// setting up the virtual displays.
#if BUILDFLAG(IS_MAC)
class GlicWindowControllerMultipleDisplaysUiTest
    : public GlicWindowControllerUiTest {
 public:
  GlicWindowControllerMultipleDisplaysUiTest() = default;
  GlicWindowControllerMultipleDisplaysUiTest(
      const GlicWindowControllerMultipleDisplaysUiTest&) = delete;
  GlicWindowControllerMultipleDisplaysUiTest& operator=(
      const GlicWindowControllerMultipleDisplaysUiTest&) = delete;
  ~GlicWindowControllerMultipleDisplaysUiTest() override = default;

  // Create virtual displays as needed, ensuring 2 displays are available for
  // testing multi-screen functionality.
  bool SetUpVirtualDisplays() {
    if (display::Screen::GetScreen()->GetNumDisplays() > 1) {
      return true;
    }
    if ((virtual_display_util_ = display::test::VirtualDisplayUtil::TryCreate(
             display::Screen::GetScreen()))) {
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
        display::Screen::GetScreen()->GetPrimaryDisplay();
    secondary_display_ =
        ui_test_utils::GetSecondaryDisplay(display::Screen::GetScreen());
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
    return Do([this]() { window_controller().Detach(); });
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
IN_PROC_BROWSER_TEST_F(GlicWindowControllerMultipleDisplaysUiTest,
                       DISABLED_MoveDetachedGlicWindowToSecondDisplay) {
  if (!SetUpVirtualDisplays()) {
    return;
  }

  RunTestSequence(CheckDisplaysSetUp(true),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kDetached),
                  InAnyContext(MoveWidgetToSecondDisplay(),
                               CheckWidgetMovedToSecondaryDisplay(true)),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

// TODO(crbug.com/399703468): Flaky on Mac. Test is targeted for Mac only.
IN_PROC_BROWSER_TEST_F(
    GlicWindowControllerMultipleDisplaysUiTest,
    DISABLED_DetachAttachedGlicWindowAndMoveToSecondDisplay) {
  if (!SetUpVirtualDisplays()) {
    return;
  }

  RunTestSequence(CheckDisplaysSetUp(true),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  InAnyContext(DetachGlicWindow(), MoveWidgetToSecondDisplay(),
                               CheckWidgetMovedToSecondaryDisplay(true)));
}
#endif

}  // namespace glic
