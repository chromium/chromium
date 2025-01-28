// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/memory_pressure_monitor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/glic/interactive_glic_test.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
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
#include "ui/views/interaction/element_tracker_views.h"
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

  auto CheckControllerWidgetMode(GlicWindowMode mode) {
    bool check_mode = mode == GlicWindowMode::kAttached;
    return CheckResult([this]() { return window_controller().IsAttached(); },
                       check_mode, "CheckControllerWidgetMode");
  }
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, ShowAndCloseAttachedWidget) {
  RunTestSequence(ToggleGlicWindowAndWaitForShow(GlicWindowMode::kAttached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, ShowAndCloseDetachedWidget) {
  RunTestSequence(ToggleGlicWindowAndWaitForShow(GlicWindowMode::kDetached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kDetached),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, DoNotCrashOnBrowserClose) {
  RunTestSequence(ToggleGlicWindowAndWaitForShow(GlicWindowMode::kAttached));
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest, DoNotCrashWhenReopening) {
  RunTestSequence(ToggleGlicWindowAndWaitForShow(GlicWindowMode::kAttached),
                  CloseGlicWindow(),
                  ToggleGlicWindowAndWaitForShow(GlicWindowMode::kAttached));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenAttachedThenOpenAttachedToSameBrowserCloses) {
  RunTestSequence(ToggleGlicWindowAndWaitForShow(GlicWindowMode::kAttached),
                  CheckControllerHasWidget(true),

                  ToggleGlicWindowAndWaitForHide(GlicWindowMode::kAttached),
                  CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenAttachedThenOpenAttachedToDifferentBrowser) {
  const BrowserList* active_browser_list = BrowserList::GetInstance();

  // Open a second browser window.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  ASSERT_EQ(2u, active_browser_list->size());
  Browser* new_browser = active_browser_list->get(1);

  RunTestSequence(
      ToggleGlicWindowAndWaitForShow(GlicWindowMode::kAttached),
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kAttached),

      Do([this, new_browser] { window_controller().Toggle(new_browser); }),
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kAttached),

      CheckResult(
          [this] { return window_controller().GetAttachedBrowserForTesting(); },
          new_browser, "attached to the other browser"));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyWhenAttachedToActiveBrowserCloses) {
  RunTestSequence(ToggleGlicWindowAndWaitForShow(GlicWindowMode::kAttached),
                  CheckControllerHasWidget(true), Do([this] {
                    window_controller()
                        .GetAttachedBrowserForTesting()
                        ->GetBrowserView()
                        .Activate();
                  }),
                  // Glic should close.
                  ToggleGlicWindowAndWaitForHide(GlicWindowMode::kDetached),
                  CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       OpenDetachedAndThenOpenAttached) {
  RunTestSequence(ToggleGlicWindowAndWaitForShow(GlicWindowMode::kDetached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kDetached),
                  PressButton(kGlicButtonElementId),
                  WaitForEvent(kGlicButtonElementId, kGlicWidgetAttached),
                  CheckControllerHasWidget(true),
                  CheckControllerWidgetMode(GlicWindowMode::kAttached),
                  CloseGlicWindow(), CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       HotkeyWhenDetachedActiveCloses) {
  RunTestSequence(
      ToggleGlicWindowAndWaitForShow(GlicWindowMode::kDetached),
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kDetached),
      Do([this] { window_controller().GetGlicWidget()->Activate(); }),

      // Glic should close.
      ToggleGlicWindowAndWaitForHide(GlicWindowMode::kDetached),
      CheckControllerHasWidget(false));
}

// TODO(392649231): Fix and enable.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_HotkeyWhenOpenDetachedInactiveActivates \
  DISABLED_HotkeyWhenOpenDetachedInactiveActivates
#else
#define MAYBE_HotkeyWhenOpenDetachedInactiveActivates \
  HotkeyWhenOpenDetachedInactiveActivates
#endif
IN_PROC_BROWSER_TEST_F(GlicWindowControllerUiTest,
                       MAYBE_HotkeyWhenOpenDetachedInactiveActivates) {
  RunTestSequence(
      ToggleGlicWindowAndWaitForShow(GlicWindowMode::kDetached),
      CheckControllerHasWidget(true),
      CheckControllerWidgetMode(GlicWindowMode::kDetached),
      Do([this] { window_controller().GetGlicWidget()->Deactivate(); }),
      ToggleGlicWindow(GlicWindowMode::kDetached),

      // Glic should activate
      CheckControllerHasWidget(true),
      CheckResult([this] { return window_controller().IsActive(); }, true,
                  "Glic is active"));
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
