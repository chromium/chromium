// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/glic/glic_view.h"
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

namespace {

class GlicWindowControllerTest : public InteractiveBrowserTest {
 public:
  GlicWindowControllerTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/
        {features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }
  ~GlicWindowControllerTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL empty_url = embedded_test_server()->GetURL("/glic/blank.html");

    // Need to set this here rather than in SetUpCommandLine because we need to
    // use the embedded test server to get the right URL and it's not started
    // at that time.
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL,
                                    empty_url.spec());
  }

 protected:
  glic::GlicView* glic_view() {
    auto* const service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
        browser()->GetProfile());
    return service->window_controller().GetGlicView();
  }

  void WaitForGlicGuestToFinishLoading() {
    auto* const web_contents = glic_view()->web_view()->GetWebContents();
    ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicWindowControllerTest, DoNotCrashOnBrowserClose) {
  RunTestSequence(PressButton(kGlicButtonElementId));
  RunTestSequence(
      InContext(views::ElementTrackerViews::GetContextForView(glic_view()),
                MoveMouseTo(kGlicViewElementId)),
      InContext(views::ElementTrackerViews::GetContextForView(glic_view()),
                ActivateSurface(kGlicViewElementId)));
  WaitForGlicGuestToFinishLoading();
  chrome::CloseAllBrowsers();
  ui_test_utils::WaitForBrowserToClose();
}

}  // namespace
