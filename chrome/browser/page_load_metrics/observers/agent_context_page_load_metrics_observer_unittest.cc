// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/agent_context_page_load_metrics_observer.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using AgentContext = AgentContextPageLoadMetricsObserver::AgentContext;
using base::Bucket;
using testing::ElementsAre;

// All test pages share an origin: while an actor task controls the tab, a
// navigation to a new origin is cancelled unless it is confirmed, and these
// tests have nothing to confirm it.
constexpr char kUrl[] = "https://www.example.com/";
constexpr char kOtherUrl[] = "https://www.example.com/other";

class AgentContextPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void TearDown() override {
    if (tab_state_) {
      // The lookup points at `tab_state_`, so it has to go first.
      web_contents()->RemoveUserData(
          tabs::TabLookupFromWebContents::UserDataKey());
      tab_state_.reset();
    }
    page_load_metrics::PageLoadMetricsObserverTestHarness::TearDown();
  }

  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<AgentContextPageLoadMetricsObserver>());
  }

  // Puts the test tab under the control of a new actor task.
  void StartActorTask() {
    tab_state_.emplace(web_contents());
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &tab_state_->tab);
    auto* actor_service = actor::ActorKeyedService::Get(browser_context());
    ASSERT_TRUE(actor_service);
    actor::ActorTask* task = actor_service->GetTask(actor_service->CreateTask(
        actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker()));
    task->AddTab(tab_state_->tab.GetHandle(), /*stop_task_on_detach=*/false,
                 base::DoNothing());
  }

  std::vector<Bucket> Samples() {
    return tester()->histogram_tester().GetAllSamples(
        "PageLoad.Clients.AgentContextAtCommit");
  }

 private:
  // The mock tab that the actor task controls. The WebContents holds a pointer
  // to it from StartActorTask() until TearDown().
  std::optional<actor::TestTabState> tab_state_;
};

TEST_F(AgentContextPageLoadMetricsObserverTest, DevToolsClientOnTabTarget) {
  content::TestDevToolsProtocolClient devtools_client;
  devtools_client.AttachToTabTarget(web_contents());

  NavigateAndCommit(GURL(kUrl));

  EXPECT_THAT(Samples(), ElementsAre(Bucket(AgentContext::kDevToolsClient, 1)));
  devtools_client.DetachProtocolClient();
}

TEST_F(AgentContextPageLoadMetricsObserverTest, DevToolsClientOnPageTarget) {
  content::TestDevToolsProtocolClient devtools_client;
  devtools_client.AttachToWebContents(web_contents());

  NavigateAndCommit(GURL(kUrl));

  EXPECT_THAT(Samples(), ElementsAre(Bucket(AgentContext::kDevToolsClient, 1)));
  devtools_client.DetachProtocolClient();
}

// Each page load is recorded once, with the agents present when it commits.
TEST_F(AgentContextPageLoadMetricsObserverTest, RecordsContextAtCommit) {
  NavigateAndCommit(GURL(kUrl));

  // Attaching after the commit does not change what the page recorded.
  content::TestDevToolsProtocolClient devtools_client;
  devtools_client.AttachToWebContents(web_contents());
  EXPECT_THAT(Samples(), ElementsAre(Bucket(AgentContext::kNone, 1)));

  NavigateAndCommit(GURL(kOtherUrl));
  EXPECT_THAT(Samples(), ElementsAre(Bucket(AgentContext::kNone, 1),
                                     Bucket(AgentContext::kDevToolsClient, 1)));

  devtools_client.DetachProtocolClient();
  NavigateAndCommit(GURL(kUrl));
  EXPECT_THAT(Samples(), ElementsAre(Bucket(AgentContext::kNone, 2),
                                     Bucket(AgentContext::kDevToolsClient, 1)));
}

TEST_F(AgentContextPageLoadMetricsObserverTest, ActorTask) {
  NavigateAndCommit(GURL(kUrl));
  StartActorTask();

  NavigateAndCommit(GURL(kOtherUrl));

  EXPECT_THAT(Samples(), ElementsAre(Bucket(AgentContext::kNone, 1),
                                     Bucket(AgentContext::kActorTask, 1)));
}

TEST_F(AgentContextPageLoadMetricsObserverTest, ActorTaskAndDevToolsClient) {
  NavigateAndCommit(GURL(kUrl));
  StartActorTask();
  content::TestDevToolsProtocolClient devtools_client;
  devtools_client.AttachToWebContents(web_contents());

  NavigateAndCommit(GURL(kOtherUrl));

  EXPECT_THAT(
      Samples(),
      ElementsAre(Bucket(AgentContext::kNone, 1),
                  Bucket(AgentContext::kActorTaskAndDevToolsClient, 1)));
  devtools_client.DetachProtocolClient();
}

TEST_F(AgentContextPageLoadMetricsObserverTest, FencedFrameNotRecorded) {
  NavigateAndCommit(GURL(kUrl));
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_rfh())->AppendFencedFrame();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(kOtherUrl), fenced_frame_root);

  EXPECT_THAT(Samples(), ElementsAre(Bucket(AgentContext::kNone, 1)));
}

TEST_F(AgentContextPageLoadMetricsObserverTest,
       PrerenderedPageRecordedOnActivation) {
  content::test::ScopedPrerenderWebContentsDelegate web_contents_delegate(
      *web_contents());
  NavigateAndCommit(GURL(kUrl));
  auto* web_contents_tester = content::WebContentsTester::For(web_contents());

  // The hidden commit of the prerendered page records nothing.
  web_contents_tester->AddPrerenderAndCommitNavigation(GURL(kOtherUrl));
  EXPECT_THAT(Samples(), ElementsAre(Bucket(AgentContext::kNone, 1)));

  // Activation records the agents present at that point.
  content::TestDevToolsProtocolClient devtools_client;
  devtools_client.AttachToWebContents(web_contents());
  web_contents_tester->ActivatePrerenderedPage(GURL(kOtherUrl));
  EXPECT_THAT(Samples(), ElementsAre(Bucket(AgentContext::kNone, 1),
                                     Bucket(AgentContext::kDevToolsClient, 1)));
  devtools_client.DetachProtocolClient();
}

}  // namespace
