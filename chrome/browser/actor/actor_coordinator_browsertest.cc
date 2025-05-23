// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_coordinator.h"

#include <optional>
#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

using ::base::test::TestFuture;
using ::optimization_guide::proto::BrowserAction;

namespace actor {

namespace {

class ActorCoordinatorBrowserTest : public InProcessBrowserTest {
 public:
  ActorCoordinatorBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ActorCoordinatorBrowserTest(const ActorCoordinatorBrowserTest&) = delete;
  ActorCoordinatorBrowserTest& operator=(const ActorCoordinatorBrowserTest&) =
      delete;

  ~ActorCoordinatorBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // TODO(crbug.com/409564704): Mock the delay so that tests can run at
    // reasonable speed. Remove once there is a more permanent approach.
    OverrideActionObservationDelay(base::Milliseconds(10));

    actor_coordinator().StartTaskForTesting(browser()->GetActiveTabInterface());
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  ActorCoordinator& actor_coordinator() {
    Profile* profile = chrome_test_utils::GetProfile(this);
    auto* glic_service = glic::GlicKeyedService::Get(profile);
    return glic_service->GetActorCoordinatorForTesting();
  }

  void ClickTarget(std::string_view query_selector) {
    std::optional<int> dom_node_id =
        content::GetDOMNodeId(*main_frame(), query_selector);
    ASSERT_TRUE(dom_node_id);
    BrowserAction action = MakeClick(*main_frame(), dom_node_id.value());
    TestFuture<mojom::ActionResultPtr> result;
    actor_coordinator().Act(action, result.GetCallback());
    ExpectOkResult(result);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The coordinator does not yet handle multi-tab cases. For now,
// while acting on a tab, we override attempts by the page to create new
// tabs, and instead navigate the existing tab.
IN_PROC_BROWSER_TEST_F(ActorCoordinatorBrowserTest, ForceSameTabNavigation) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/target_blank_links.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check specifically that it's the existing frame that navigates.
  content::TestFrameNavigationObserver frame_nav_observer(main_frame());
  ClickTarget("#anchorTarget");
  frame_nav_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(ActorCoordinatorBrowserTest,
                       ForceSameTabNavigationByScript) {
  const GURL url =
      embedded_test_server()->GetURL("/actor/target_blank_links.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Check specifically that it's the existing frame that navigates.
  content::TestFrameNavigationObserver frame_nav_observer(main_frame());
  ClickTarget("#scriptOpen");
  frame_nav_observer.Wait();
}

}  // namespace

}  // namespace actor
