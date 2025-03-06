// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/memory_pressure_monitor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/background/glic/glic_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/glic/interactive_glic_test.h"
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
#include "glic_profile_manager.h"
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

class GlicWindowControllerUiTest : public test::InteractiveGlicTest {
 public:
  GlicWindowControllerUiTest() = default;
  ~GlicWindowControllerUiTest() override = default;

  auto SimulateGlicHotkey() {
    // TODO: Actually implement the hotkey when we know what it is.
    return Do([this]() {
      glic_service()->ToggleUI(nullptr, /*prevent_close=*/false,
                               InvocationSource::kOsHotkey);
    });
  }

  auto SimulateOpenMenuItem() {
    return Do(
        [this]() { glic_controller_->Show(InvocationSource::kOsButtonMenu); });
  }

 private:
  std::unique_ptr<GlicController> glic_controller_ =
      std::make_unique<GlicController>();
};

// TODO(394945970): Check top right corner position.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       ShowAndCloseAttachedWidget) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
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

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenAttachedThenOpenAttachedToSameBrowserCloses) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  ToggleGlicWindow(GlicWindowMode::kAttached),
                  InAnyContext(WaitForHide(kGlicViewElementId)),
                  CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenAttachedThenOpenAttachedToDifferentBrowser) {
  Browser* const new_browser = CreateBrowser(browser()->profile());

  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  InContext(new_browser->window()->GetElementContext(),
                            PressButton(kGlicButtonElementId)),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  CheckIfAttachedToBrowser(new_browser));
}

#if !BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(
    GlicWindowControllerUiTest,
    OpenAttachedThenOpenAttachedToDifferentBrowserWithHotkey) {
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

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyWhenAttachedToActiveBrowserCloses) {
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

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyAttachesToActiveBrowser) {
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
// Widget activation doesn't work on Linux; see
// InteractionTestUtilSimulatorViews::ActivateWidget.
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       CanFocusGlicWindowWithFocusDialogHotkey) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
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

// TODO(388102775): When Mac app focus issues are resolved, add a test to verify
// that invoking the hotkey while open detached always closes glic regardless of
// activation.

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, ApiDetach) {
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

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, OpenMenuItemShows) {
  RunTestSequence(SimulateOpenMenuItem(),
                  WaitForAndInstrumentGlic(kHostAndContents),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
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
