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

class GlicWindowControllerTest : public test::InteractiveGlicTest {
 public:
  GlicWindowControllerTest() = default;
  ~GlicWindowControllerTest() override = default;

  auto CheckControllerHasWidget(bool expect_widget) {
    return CheckResult(
        [this]() { return window_controller().GetGlicWidget() != nullptr; },
        expect_widget, "CheckControllerHasWidget");
  }
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerTest, ShowAndCloseAttachedWidget) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  CheckControllerHasWidget(true), CloseGlicWindow(),
                  CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerTest, ShowAndCloseDetachedWidget) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  CheckControllerHasWidget(true), CloseGlicWindow(),
                  CheckControllerHasWidget(false));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerTest, DoNotCrashOnBrowserClose) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached));
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerTest, DoNotCrashWhenReopening) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached), CloseGlicWindow(),
                  OpenGlicWindow(GlicWindowMode::kAttached));
}

class GlicWindowControllerWithMemoryPressureTest
    : public GlicWindowControllerTest {
 public:
  GlicWindowControllerWithMemoryPressureTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/
        {features::kGlicWarming},
        /*disabled_features=*/{});
  }
  ~GlicWindowControllerWithMemoryPressureTest() override = default;

  void SetUp() override {
    // This will temporarily disable preloading to ensure that we don't load the
    // web client before we've initialized the embedded test server and can set
    // the correct URL.
    GlicProfileManager::ForceMemoryPressureForTesting(&forced_memory_pressure_);
    GlicWindowControllerTest::SetUp();
  }

  void TearDown() override {
    GlicWindowControllerTest::TearDown();
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

IN_PROC_BROWSER_TEST_F(GlicWindowControllerWithMemoryPressureTest, Preload) {
  ResetMemoryPressure();
  glic_service()->TryPreload();
  EXPECT_TRUE(window_controller().IsWarmed());
  RunTestSequence(PressButton(kGlicButtonElementId),
                  InAnyContext(WaitForShow(kGlicViewElementId)));
}

}  // namespace glic
