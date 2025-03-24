// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_coordinator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tab_collections/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

using base::test::ScopedFeatureList;
using base::test::TestFuture;
using content::WebContents;
using optimization_guide::proto::BrowserAction;
using optimization_guide::proto::ClickAction;
using tabs::TabInterface;

namespace actor {

namespace {

class ActorToolsTest : public InProcessBrowserTest {
 public:
  ActorToolsTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicActor},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ActorToolsTest(const ActorToolsTest&) = delete;
  ActorToolsTest& operator=(const ActorToolsTest&) = delete;

  ~ActorToolsTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    actor_coordinator_ = std::make_unique<actor::ActorCoordinator>();
  }

  WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  TabInterface* active_tab() { return browser()->GetActiveTabInterface(); }

  ActorCoordinator& actor_coordinator() { return *actor_coordinator_; }

 private:
  std::unique_ptr<ActorCoordinator> actor_coordinator_;

  ScopedFeatureList scoped_feature_list_;
};

// Exercises the basic API to ensure nothing CHECKs or crashes.
IN_PROC_BROWSER_TEST_F(ActorToolsTest, BasicSmokeTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  BrowserAction action;
  ClickAction* click = action.add_action_information()->mutable_click();
  click->mutable_target()->set_content_node_id(123);
  click->set_click_type(ClickAction::LEFT);
  click->set_click_count(ClickAction::SINGLE);

  TabInterface& tab = *active_tab();

  TestFuture<bool> result_success;
  actor_coordinator().Act(tab, action, result_success.GetCallback());
  EXPECT_TRUE(result_success.Get());
}

}  // namespace

}  // namespace actor
