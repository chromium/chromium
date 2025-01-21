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
namespace {

class GlicWindowControllerTest : public InteractiveBrowserTest {
 public:
  GlicWindowControllerTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/
        {features::kGlic, features::kTabstripComboButton,
         features::kGlicWarming},
        /*disabled_features=*/{});
  }
  ~GlicWindowControllerTest() override = default;

  void SetUp() override {
    // This will temporarily disable preloading to ensure that we don't load the
    // web client before we've initialized the embedded test server and can set
    // the correct URL.
    GlicProfileManager::ForceMemoryPressureForTesting(&forced_memory_pressure_);
    InteractiveBrowserTest::SetUp();
  }

  void TearDown() override {
    InteractiveBrowserTest::TearDown();
    GlicProfileManager::ForceMemoryPressureForTesting(nullptr);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));

    ASSERT_TRUE(embedded_test_server()->Start());

    // Need to set this here rather than in SetUpCommandLine because we need to
    // use the embedded test server to get the right URL and it's not started
    // at that time.
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        ::switches::kGlicGuestURL,
        embedded_test_server()->GetURL("/glic/test.html").spec());
    command_line->AppendSwitchASCII(::switches::kCSPOverride, "");
  }

 protected:
  glic::GlicKeyedService* glic_service() {
    return glic::GlicKeyedServiceFactory::GetGlicKeyedService(
        browser()->GetProfile());
  }

  glic::GlicWindowController& window_controller() {
    return glic_service()->window_controller();
  }

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

IN_PROC_BROWSER_TEST_F(GlicWindowControllerTest, ShowAndCloseAttachedWidget) {
  RunTestSequence(PressButton(kGlicButtonElementId),
                  InAnyContext(WaitForShow(kGlicViewElementId)),
                  InSameContext(Steps(
                      Check([this]() {
                        return window_controller().GetGlicWidgetForTesting() !=
                               nullptr;
                      }),
                      Do([this]() { window_controller().Close(); }),
                      WaitForHide(kGlicViewElementId))));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerTest, ShowAndCloseDetachedWidget) {
  RunTestSequence(Do([this]() {
                    // Simulate showing the glic widget detached (without
                    // glic_button_view).
                    window_controller().Show(nullptr);
                  }),
                  InAnyContext(WaitForShow(kGlicViewElementId)),
                  InSameContext(Steps(
                      Check([this]() {
                        return window_controller().GetGlicWidgetForTesting() !=
                               nullptr;
                      }),
                      Do([this]() { window_controller().Close(); }),
                      WaitForHide(kGlicViewElementId))));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerTest, Preload) {
  ResetMemoryPressure();
  glic_service()->TryPreload();
  EXPECT_TRUE(window_controller().IsWarmed());
  RunTestSequence(PressButton(kGlicButtonElementId),
                  InAnyContext(WaitForShow(kGlicViewElementId)));
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerTest, DoNotCrashOnBrowserClose) {
  RunTestSequence(PressButton(kGlicButtonElementId),
                  InAnyContext(WaitForShow(kGlicViewElementId)));
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
}

IN_PROC_BROWSER_TEST_F(GlicWindowControllerTest, DoNotCrashWhenReopening) {
  RunTestSequence(
      PressButton(kGlicButtonElementId),
      // TODO(crbug.com/389729273): observe web client initialization directly.
      InAnyContext(WaitForShow(kGlicViewElementId)),
      InSameContext(Steps(Do([this]() { window_controller().Close(); }),
                          WaitForHide(kGlicViewElementId))),
      PressButton(kGlicButtonElementId),
      InAnyContext(WaitForShow(kGlicViewElementId)));
}

}  // namespace
}  // namespace glic
