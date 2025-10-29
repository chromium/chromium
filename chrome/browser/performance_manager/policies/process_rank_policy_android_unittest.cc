// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/process_rank_policy_android.h"

#include <optional>

#include "base/android/android_info.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/browser/android/child_process_importance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager::policies {

namespace {

struct MockPageGraph {
  TestNodeWrapper<ProcessNodeImpl> process;
  TestNodeWrapper<PageNodeImpl> page;
  TestNodeWrapper<FrameNodeImpl> frame;
};

}  // namespace

class ProcessRankPolicyAndroidTest : public ChromeRenderViewHostTestHarness {
 public:
  ProcessRankPolicyAndroidTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        graph_(std::make_unique<TestGraphImpl>()) {}
  ~ProcessRankPolicyAndroidTest() override = default;
  ProcessRankPolicyAndroidTest(const ProcessRankPolicyAndroidTest& other) =
      delete;
  ProcessRankPolicyAndroidTest& operator=(const ProcessRankPolicyAndroidTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    graph_->SetUp();

    graph_->PassToGraph(std::make_unique<DiscardEligibilityPolicy>());
    DiscardEligibilityPolicy::GetFromGraph(graph_.get())
        ->SetNoDiscardPatternsForProfile(GetBrowserContext()->UniqueId(), {});
  }

  void TearDown() override {
    graph_->TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
    scoped_feature_list_.Reset();
  }

  MockPageGraph CreateDefaultPage() {
    auto process = TestNodeWrapper<ProcessNodeImpl>::Create(graph_.get());
    auto page = TestNodeWrapper<PageNodeImpl>::Create(
        graph_.get(), web_contents()->GetWeakPtr(),
        GetBrowserContext()->UniqueId());
    page->SetType(PageType::kTab);
    auto frame = graph_->CreateFrameNodeAutoId(
        process.get(), page.get(),
        /*parent_frame_node=*/nullptr,
        content::BrowsingInstanceId::FromUnsafeValue(1));
    return {std::move(process), std::move(page), std::move(frame)};
  }

  void DefaultNavigation(PageNodeImpl* page) {
    page->OnMainFrameNavigationCommitted(
        false, base::TimeTicks::Now(), 123, kDefaultUrl, "text/html",
        /* notification_permission_status= */ std::nullopt);
  }

  const GURL kDefaultUrl{"http://example.test/"};
  std::unique_ptr<TestGraphImpl> graph_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProcessRankPolicyAndroidTest, FocusedPage) {
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>());
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(true);
  page_graph.page.get()->SetIsVisible(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::IMPORTANT);
}

TEST_F(ProcessRankPolicyAndroidTest, FocusedNotVisiblePage) {
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>());
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(true);
  page_graph.page.get()->SetIsVisible(false);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::NORMAL);
}

TEST_F(ProcessRankPolicyAndroidTest,
       NonFocusedVisiblePageWithChangeUnfocusedPriority) {
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kChangeUnfocusedPriority);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>());
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::MODERATE);
}

TEST_F(ProcessRankPolicyAndroidTest, NonFocusedVisiblePage) {
  scoped_feature_list_.InitAndDisableFeature(
      chrome::android::kChangeUnfocusedPriority);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>());
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::IMPORTANT);
}

TEST_F(ProcessRankPolicyAndroidTest,
       NonVisibleActivePageProtectedTabsAndroidDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>());
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsActiveTabForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::MODERATE);
}

TEST_F(ProcessRankPolicyAndroidTest, NonVisibleActivePage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsActiveTabForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest,
       NonVisibleActivePageWithoutPerceptibleImportanceSupport) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chrome::android::kProtectedTabsAndroid,
      {{"fallback_to_moderate", "false"}});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(false));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsActiveTabForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::MODERATE);
}

TEST_F(ProcessRankPolicyAndroidTest,
       NonVisibleActivePageWithoutPerceptibleImportanceSupportWithFallback) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chrome::android::kProtectedTabsAndroid,
      {{"fallback_to_moderate", "true"}});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(false));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsActiveTabForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::MODERATE);
}

TEST_F(ProcessRankPolicyAndroidTest,
       ProtectedPageWithoutPerceptibleImportanceSupport) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chrome::android::kProtectedTabsAndroid,
      {{"fallback_to_moderate", "false"}});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(false));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->SetIsAudible(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::NORMAL);
}

TEST_F(ProcessRankPolicyAndroidTest,
       ProtectedPageWithoutPerceptibleImportanceSupportWithFallback) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chrome::android::kProtectedTabsAndroid,
      {{"fallback_to_moderate", "true"}});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(false));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->SetIsAudible(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::MODERATE);
}

TEST_F(ProcessRankPolicyAndroidTest,
       ProtectedPageWithPerceptibleImportanceSupportWithFallback) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      chrome::android::kProtectedTabsAndroid,
      {{"fallback_to_moderate", "true"}});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->SetIsAudible(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, RecentlyVisiblePage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(true);
  page_graph.page.get()->SetIsVisible(false);
  task_environment()->FastForwardBy(base::Seconds(1));

  // On Android recently visible page is not taken as protected.
  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::NORMAL);
}

TEST_F(ProcessRankPolicyAndroidTest, AudiblePage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->SetIsAudible(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, RecentlyAudiblePage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->SetIsAudible(true);
  page_graph.page.get()->SetIsAudible(false);
  task_environment()->FastForwardBy(base::Seconds(1));

  // On Android recently audible page is not taken as protected.
  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::NORMAL);
}

TEST_F(ProcessRankPolicyAndroidTest, PictureInPicturePage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->SetHasPictureInPicture(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, PdfPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), 123, GURL("https://foo.com/doc.pdf"),
      "application/pdf",
      /* notification_permission_status= */ std::nullopt);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, InvalidURLPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->SetMainFrameRestoredState(
      GURL("invalid url"),
      /* notification_permission_status= */ blink::mojom::PermissionStatus::
          ASK);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, OptedOutURLPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();

  // TODO(crbug.com/410444953): Once observer for
  // `DiscardEligibilityPolicy::profiles_no_discard_patterns_` changes was
  // introduced, we can to move this after the navigation.
  DiscardEligibilityPolicy::GetFromGraph(graph_.get())
      ->SetNoDiscardPatternsForProfile(GetBrowserContext()->UniqueId(),
                                       {kDefaultUrl.spec()});

  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, NotificationGrantedPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->OnNotificationPermissionStatusChange(
      blink::mojom::PermissionStatus::GRANTED);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, NotAutoDiscardablePage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsAutoDiscardableForTesting(false);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, CapturingVideoPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsCapturingVideoForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, CapturingAudioPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsCapturingAudioForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, BeingMirroredPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsBeingMirroredForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, CapturingWindowPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsCapturingWindowForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, CapturingDisplayPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsCapturingDisplayForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, ConnectedToBluetoothDevicePage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsConnectedToBluetoothDeviceForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, ConnectedToUSBDevicePage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsConnectedToUSBDeviceForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, PinnedTabPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsPinnedTabForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, DevToolsOpenPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetIsDevToolsOpenForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, UpdatedTitleOrFaviconInBackgroundPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_graph.page.get())
      ->SetUpdatedTitleOrFaviconInBackgroundForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, HadFormInteractionPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page->SetHadFormInteractionForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, HadUserEditsPage) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page->SetHadUserEditsForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, NonVisiblePage) {
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>());
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::NORMAL);
}

TEST_F(ProcessRankPolicyAndroidTest, SubframeImportanceForImportant) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_
      .InitWithFeatures(/*enabled_features=*/
                        {::features::kSubframeImportance,
                         ::features::kSubframePriorityContribution},
                        /*disabled_features=*/{});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(true);
  page_graph.page.get()->SetIsVisible(true);

  EXPECT_EQ(web_contents()->GetPrimaryPageSubframeImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest,
       SubframeImportanceForImportantWithoutPerceptibleSupport) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_
      .InitWithFeatures(/*enabled_features=*/
                        {::features::kSubframeImportance,
                         ::features::kSubframePriorityContribution},
                        /*disabled_features=*/{
                            chrome::android::kProtectedTabsAndroid});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(false));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(true);
  page_graph.page.get()->SetIsVisible(true);

  EXPECT_EQ(web_contents()->GetPrimaryPageSubframeImportanceForTesting(),
            content::ChildProcessImportance::NORMAL);
}

TEST_F(ProcessRankPolicyAndroidTest,
       SubframeImportanceForImportantFallbackToModerate) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{chrome::android::kProtectedTabsAndroid,
        {{"fallback_to_moderate", "true"}}},
       {::features::kSubframeImportance, {}},
       {::features::kSubframePriorityContribution, {}}},
      /*disabled_features=*/{});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(false));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(true);
  page_graph.page.get()->SetIsVisible(true);

  EXPECT_EQ(web_contents()->GetPrimaryPageSubframeImportanceForTesting(),
            content::ChildProcessImportance::MODERATE);
}

TEST_F(ProcessRankPolicyAndroidTest, SubframeImportanceForProtectedTab) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_
      .InitWithFeatures(/*enabled_features=*/
                        {chrome::android::kProtectedTabsAndroid,
                         ::features::kSubframeImportance,
                         ::features::kSubframePriorityContribution},
                        /*disabled_features=*/{});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->SetIsAudible(true);

  EXPECT_EQ(web_contents()->GetPrimaryPageSubframeImportanceForTesting(),
            content::ChildProcessImportance::PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest,
       SubframeImportanceForProtectedTabWithoutPerceptibleSupport) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{chrome::android::kProtectedTabsAndroid,
        {{"fallback_to_moderate", "false"}}},
       {::features::kSubframeImportance, {}},
       {::features::kSubframePriorityContribution, {}}},
      /*disabled_features=*/{});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(false));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->SetIsAudible(true);

  EXPECT_EQ(web_contents()->GetPrimaryPageSubframeImportanceForTesting(),
            content::ChildProcessImportance::NORMAL);
}

TEST_F(ProcessRankPolicyAndroidTest,
       SubframeImportanceForProtectedTabFallbackToModerate) {
  if (!content::IsPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "Perceptible importance is not supported.";
  }
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{chrome::android::kProtectedTabsAndroid,
        {{"fallback_to_moderate", "true"}}},
       {::features::kSubframeImportance, {}},
       {::features::kSubframePriorityContribution, {}}},
      /*disabled_features=*/{});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(false));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->SetIsAudible(true);

  EXPECT_EQ(web_contents()->GetPrimaryPageSubframeImportanceForTesting(),
            content::ChildProcessImportance::MODERATE);
}

}  // namespace performance_manager::policies
