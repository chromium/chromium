// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/discards/tab_discards_dump_android.h"

#include <memory>
#include <optional>
#include <string>

#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/graph_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/browsing_instance_id.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace discards {

namespace {

using ::performance_manager::FrameNodeImpl;
using ::performance_manager::GraphImpl;
using ::performance_manager::PageLiveStateDecorator;
using ::performance_manager::PageNode;
using ::performance_manager::PageNodeImpl;
using ::performance_manager::PageType;
using ::performance_manager::PerformanceManager;
using ::performance_manager::PerformanceManagerTestHarnessHelper;
using ::performance_manager::TestNodeWrapper;
using ::performance_manager::TestProcessNodeImpl;
using ::performance_manager::policies::DiscardEligibilityPolicy;
using ::testing::HasSubstr;
using ::testing::Not;

struct MockPageGraph {
  TestNodeWrapper<TestProcessNodeImpl> process;
  TestNodeWrapper<PageNodeImpl> page;
  TestNodeWrapper<FrameNodeImpl> frame;
};

}  // namespace

class TabDiscardsDumpAndroidTest : public ChromeRenderViewHostTestHarness {
 public:
  TabDiscardsDumpAndroidTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TabDiscardsDumpAndroidTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {chrome::android::kProtectRecentlyVisibleTab,
             {{"duration_in_seconds", "0"}}},
        },
        /*disabled_features=*/{});
    pm_harness_.SetGraphImplCallback(
        base::BindLambdaForTesting([this](GraphImpl* graph) {
          graph_ = graph;
          graph_->PassToGraph(std::make_unique<DiscardEligibilityPolicy>());
          DiscardEligibilityPolicy::GetFromGraph(graph_)
              ->SetNoDiscardPatternsForProfile(
                  GetBrowserContext()->UniqueToken(), {});
        }));
    pm_harness_.SetUp();
  }

  void TearDown() override {
    graph_ = nullptr;
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
    scoped_feature_list_.Reset();
  }

  MockPageGraph CreateDefaultPage() {
    auto process = TestNodeWrapper<TestProcessNodeImpl>::Create(graph_);
    process->SetProcessWithPid(100);
    auto page = TestNodeWrapper<PageNodeImpl>::Create(
        graph_, web_contents()->GetWeakPtr(),
        GetBrowserContext()->UniqueToken());
    page->SetType(PageType::kTab);
    auto frame = TestNodeWrapper<FrameNodeImpl>::Create(
        graph_, process.get(), page.get(),
        /*parent_frame_node=*/nullptr,
        /*outer_document_for_inner_frame_root=*/nullptr,
        /*render_frame_id=*/1, blink::LocalFrameToken(),
        content::FrameTreeNodeId(1),
        content::BrowsingInstanceId::FromUnsafeValue(1));
    return {std::move(process), std::move(page), std::move(frame)};
  }

  void DefaultNavigation(PageNodeImpl* page) {
    page->OnMainFrameNavigationCommitted(
        false, base::TimeTicks::Now(), 123, kDefaultUrl, "text/html",
        /*notification_permission_status=*/std::nullopt);
    page->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  }

  const GURL kDefaultUrl{"https://secret.example.test/private"};
  PerformanceManagerTestHarnessHelper pm_harness_;
  raw_ptr<GraphImpl> graph_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TabDiscardsDumpAndroidTest, NullAndEmptyGraph) {
  EXPECT_EQ("PerformanceManager graph not available.\n",
            DumpTabDiscardsInfo(nullptr));
  EXPECT_EQ("Tab Discards Info (0 tabs):\n", DumpTabDiscardsInfo(graph_));
}

TEST_F(TabDiscardsDumpAndroidTest, DiscardableBackgroundTab) {
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  auto subframe_process = TestNodeWrapper<TestProcessNodeImpl>::Create(graph_);
  subframe_process->SetProcessWithPid(200);
  auto subframe = TestNodeWrapper<FrameNodeImpl>::Create(
      graph_, subframe_process.get(), page_graph.page.get(),
      /*parent_frame_node=*/page_graph.frame.get(),
      /*outer_document_for_inner_frame_root=*/nullptr,
      /*render_frame_id=*/2, blink::LocalFrameToken(),
      content::FrameTreeNodeId(2),
      content::BrowsingInstanceId::FromUnsafeValue(1));

  page_graph.page->SetIsVisible(true);
  page_graph.page->SetIsVisible(false);
  page_graph.page->SetIsFocused(false);
  task_environment()->FastForwardBy(base::Seconds(42));

  std::string dump = DumpTabDiscardsInfo(graph_);

  EXPECT_THAT(dump, HasSubstr("Tab Discards Info (1 tabs):\n"));
  EXPECT_THAT(
      dump,
      HasSubstr("visibility=hidden loading_state=loaded lifecycle_state=hidden "
                "can_discard=true cannot_discard_reasons=[] "
                "last_active=42s main_frame_pid=100 subframe_pids=[200]"));
  // Verify no URLs or hostnames are leaked in the dump.
  EXPECT_THAT(dump, Not(HasSubstr("secret.example.test")));
}

TEST_F(TabDiscardsDumpAndroidTest, ProtectedTabReasons) {
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page->SetIsVisible(true);
  page_graph.page->SetIsFocused(true);
  page_graph.page->SetIsAudible(true);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsActiveTabForTesting(true);

  std::string dump = DumpTabDiscardsInfo(graph_);

  EXPECT_THAT(dump, HasSubstr("visibility=visible loading_state=loaded "
                              "lifecycle_state=active can_discard=false"));
  EXPECT_THAT(dump, HasSubstr("Tab is currently visible"));
  EXPECT_THAT(dump, HasSubstr("Tab is playing audio"));
  EXPECT_THAT(dump, HasSubstr("Tab is currently active"));
  EXPECT_THAT(dump,
              HasSubstr("last_active=0s main_frame_pid=100 subframe_pids=[]"));
  EXPECT_THAT(dump, Not(HasSubstr("secret.example.test")));
}

}  // namespace discards
