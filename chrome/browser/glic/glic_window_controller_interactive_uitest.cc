// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/memory_pressure_monitor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/glic/interactive_glic_test.h"
#include "chrome/browser/glic/launcher/glic_controller.h"
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
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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

  auto CheckControllerHasWidget(bool expect_widget) {
    return CheckResult(
        [this]() { return window_controller().GetGlicWidget() != nullptr; },
        expect_widget, "CheckControllerHasWidget");
  }

  auto CheckControllerShowing(bool expect_showing) {
    return CheckResult([this]() { return window_controller().IsShowing(); },
                       expect_showing, "CheckControllerShowing");
  }

  auto CheckControllerWidgetMode(GlicWindowMode mode) {
    return CheckResult(
        [this]() {
          return window_controller().IsAttached() ? GlicWindowMode::kAttached
                                                  : GlicWindowMode::kDetached;
        },
        mode, "CheckControllerWidgetMode");
  }

  auto SimulateGlicHotkey() {
    // TODO: Actually implement the hotkey when we know what it is.
    return Do([this]() {
      window_controller().Toggle(nullptr, /*prevent_close=*/false,
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

// TODO(394945970): Re-enable on Linux; failing the top right corner position
// check.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ShowAndCloseAttachedWidget DISABLED_ShowAndCloseAttachedWidget
#else
#define MAYBE_ShowAndCloseAttachedWidget ShowAndCloseAttachedWidget
#endif
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       MAYBE_ShowAndCloseAttachedWidget) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      // Verify glic is open in attached mode.
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kAttached),
      // Top right corner should match the glic button's inset top right corner.
      CheckResult(
          [this] {
            return window_controller()
                .GetGlicWidget()
                ->GetWindowBoundsInScreen()
                .top_right();
          },
          browser()
              ->window()
              ->AsBrowserView()
              ->tab_strip_region_view()
              ->GetGlicButton()
              ->GetBoundsWithInset()
              .top_right(),
          "glic widget top right corner position"

          ),

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
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached), CloseGlicWindow(),
                  OpenGlicWindow(GlicWindowMode::kAttached), CloseGlicWindow(),
                  CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenAttachedThenOpenAttachedToDifferentBrowser) {
  Browser* const new_browser = CreateBrowser(browser()->profile());

  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),
      CheckControllerWidgetMode(GlicWindowMode::kAttached),
      InContext(new_browser->window()->GetElementContext(),
                PressButton(kGlicButtonElementId)),
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kAttached),
      CheckResult([this] { return window_controller().attached_browser(); },
                  new_browser, "attached to the other browser"));
}

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
  RunTestSequence(PressButton(kGlicButtonElementId),
                  InAnyContext(WaitForShow(kGlicViewElementId)));
}

}  // namespace glic
