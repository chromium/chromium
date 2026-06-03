// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/process_rank_policy_android.h"

#include <optional>

#include "base/android/android_info.h"
#include "base/android/device_info.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/browser/android/child_process_importance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "chrome/common/webui_url_constants.h"
#include "components/guest_view/browser/test_guest_view_manager.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/guest_view/chrome_guest_view_manager_delegate.h"  // nogncheck
#include "extensions/browser/guest_view/web_view/web_view_guest.h"  // nogncheck
#else  // !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/android/guest_view/chrome_guest_view_manager_delegate.h"
#include "components/guest_view/browser/slim_web_view/slim_web_view_guest.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

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
        ->SetNoDiscardPatternsForProfile(GetBrowserContext()->UniqueToken(),
                                         {});
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
        GetBrowserContext()->UniqueToken());
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

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  std::unique_ptr<content::WebContents> CreateTestGuestWebContents() {
    auto* context = GetBrowserContext();
    return content::WebContentsTester::CreateTestWebContents(
        context, content::SiteInstance::CreateForGuest(
                     context, content::StoragePartitionConfig::Create(
                                  context, "foo", "", true)));
  }

#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

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

#if BUILDFLAG(IS_DESKTOP_ANDROID)
#define MAYBE_FocusedNotVisiblePage DISABLED_FocusedNotVisiblePage
#else
#define MAYBE_FocusedNotVisiblePage FocusedNotVisiblePage
#endif
TEST_F(ProcessRankPolicyAndroidTest, MAYBE_FocusedNotVisiblePage) {
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
  if (base::android::device_info::is_desktop()) {
    GTEST_SKIP()
        << "ChangeUnfocusedPriority feature is always enabled on desktop";
  }
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
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
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
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

#if BUILDFLAG(IS_DESKTOP_ANDROID)
#define MAYBE_RecentlyVisiblePage DISABLED_RecentlyVisiblePage
#else
#define MAYBE_RecentlyVisiblePage RecentlyVisiblePage
#endif
TEST_F(ProcessRankPolicyAndroidTest, MAYBE_RecentlyVisiblePage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

#if BUILDFLAG(IS_DESKTOP_ANDROID)
#define MAYBE_RecentlyAudiblePage DISABLED_RecentlyAudiblePage
#else
#define MAYBE_RecentlyAudiblePage RecentlyAudiblePage
#endif
TEST_F(ProcessRankPolicyAndroidTest, MAYBE_RecentlyAudiblePage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, PdfPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, InvalidURLPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, OptedOutURLPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
  }
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kProtectedTabsAndroid);
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();

  // TODO(crbug.com/410444953): Once observer for
  // `DiscardEligibilityPolicy::profiles_no_discard_patterns_` changes was
  // introduced, we can to move this after the navigation.
  DiscardEligibilityPolicy::GetFromGraph(graph_.get())
      ->SetNoDiscardPatternsForProfile(GetBrowserContext()->UniqueToken(),
                                       {kDefaultUrl.spec()});

  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, NotificationGrantedPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, NotAutoDiscardablePage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, CapturingVideoPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, CapturingAudioPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, BeingMirroredPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, CapturingWindowPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, CapturingDisplayPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, ConnectedToBluetoothDevicePage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, ConnectedToUSBDevicePage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, PinnedTabPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, DevToolsOpenPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, UpdatedTitleOrFaviconInBackgroundPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, HadFormInteractionPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest, HadUserEditsPage) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
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
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

#if BUILDFLAG(IS_DESKTOP_ANDROID)
#define MAYBE_NonVisiblePage DISABLED_NonVisiblePage
#else
#define MAYBE_NonVisiblePage NonVisiblePage
#endif
TEST_F(ProcessRankPolicyAndroidTest, MAYBE_NonVisiblePage) {
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
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
  }
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(true);
  page_graph.page.get()->SetIsVisible(true);

  EXPECT_EQ(web_contents()->GetPrimaryPageSubframeImportanceForTesting(),
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest,
       SubframeImportanceForImportantWithoutPerceptibleSupport) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
  }
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{chrome::android::kProtectedTabsAndroid});
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
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
  }
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{chrome::android::kProtectedTabsAndroid,
        {{"fallback_to_moderate", "true"}}}},
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
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
  }
  scoped_feature_list_
      .InitWithFeatures(/*enabled_features=*/
                        {chrome::android::kProtectedTabsAndroid},
                        /*disabled_features=*/{});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);
  page_graph.page.get()->SetIsVisible(false);
  page_graph.page.get()->SetIsAudible(true);

  EXPECT_EQ(web_contents()->GetPrimaryPageSubframeImportanceForTesting(),
            content::ChildProcessImportance::NOT_PERCEPTIBLE);
}

TEST_F(ProcessRankPolicyAndroidTest,
       SubframeImportanceForProtectedTabWithoutPerceptibleSupport) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
  }
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{chrome::android::kProtectedTabsAndroid,
        {{"fallback_to_moderate", "false"}}}},
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
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
  }
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{chrome::android::kProtectedTabsAndroid,
        {{"fallback_to_moderate", "true"}}}},
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

TEST_F(ProcessRankPolicyAndroidTest, ProtectRecentlyVisibleTab) {
  if (!content::IsNotPerceptibleImportanceSupported()) {
    GTEST_SKIP() << "NOT_PERCEPTIBLE importance is not supported.";
  }
  const base::TimeDelta kDuration = base::Seconds(10);
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{chrome::android::kProtectedTabsAndroid, {}},
       {chrome::android::kProtectRecentlyVisibleTab,
        {{"duration_in_seconds", base::ToString(kDuration.InSeconds())}}}},
      /*disabled_features=*/{});
  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>(true));
  MockPageGraph page_graph = CreateDefaultPage();
  DefaultNavigation(page_graph.page.get());

  page_graph.page.get()->SetIsFocused(false);

  // Make the page visible then invisible.
  page_graph.page.get()->SetIsVisible(true);
  page_graph.page.get()->SetIsVisible(false);

  // The page should be protected because it was recently visible.
  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::NOT_PERCEPTIBLE);

  // Advance time by the protection duration.
  task_environment()->FastForwardBy(kDuration);
  base::RunLoop().QuitWhenIdle();

  // The page should no longer be protected.
  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::NORMAL);
}

#if BUILDFLAG(ENABLE_GUEST_VIEW)
TEST_F(ProcessRankPolicyAndroidTest,
       WebViewPageInWebUiInheritsVisibilityFromEmbedder) {
  guest_view::TestGuestViewManagerFactory factory;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  factory.GetOrCreateTestGuestViewManager(
      GetBrowserContext(),
      std::make_unique<extensions::ChromeGuestViewManagerDelegate>());
#else
  factory.GetOrCreateTestGuestViewManager(
      GetBrowserContext(),
      std::make_unique<android::ChromeGuestViewManagerDelegate>());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

  graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>());

  // Create Owner Page Node (the default one in the harness).
  MockPageGraph owner_page_graph = CreateDefaultPage();
  // Fake that the owner page is a WebUI
  owner_page_graph.page->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), 123, GURL(chrome::kChromeUIGlicURL),
      "text/html",
      /* notification_permission_status= */ std::nullopt);
  owner_page_graph.page.get()->SetIsFocused(true);
  owner_page_graph.page.get()->SetIsVisible(true);

  // Create Guest WebContents and its PageNode.
  std::unique_ptr<content::WebContents> guest_contents =
      CreateTestGuestWebContents();
  auto guest_process = TestNodeWrapper<ProcessNodeImpl>::Create(graph_.get());
  auto guest_page = TestNodeWrapper<PageNodeImpl>::Create(
      graph_.get(), guest_contents->GetWeakPtr(),
      GetBrowserContext()->UniqueToken());

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  std::unique_ptr<guest_view::GuestViewBase> webview_guest =
      extensions::WebViewGuest::Create(main_rfh());
#else   // !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  std::unique_ptr<guest_view::GuestViewBase> webview_guest =
      guest_view::SlimWebViewGuest::Create(main_rfh());
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  webview_guest->InitWithWebContents(base::DictValue(), guest_contents.get());
  // In these tests, there is no PerformanceManagerTabHelper, which would
  // normally associate the contents with the page. Due to this absence, the
  // test needs to manually notify the ProcessRankPolicyAndroid that the
  // webcontents are now associated with the GuestView.
  ProcessRankPolicyAndroid::GetFromGraph(guest_page->GetGraph())
      ->OnGuestViewAssociated(guest_page.get());

  // Link Guest Page Node to Owner Frame Node.
  guest_page->SetEmbedderFrameNode(owner_page_graph.frame.get());

  // Make the guest page invisible.
  guest_page->SetIsVisible(false);

  // Even though the guest page is not visible, the parent is visible and
  // focused, so the guest should be IMPORTANT.
  EXPECT_EQ(guest_contents->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::IMPORTANT);

  // Toggling the visibility of the parent should affect the guest.
  owner_page_graph.page->SetIsVisible(false);

  EXPECT_NE(guest_contents->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::IMPORTANT);
}
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

}  // namespace performance_manager::policies
