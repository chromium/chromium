// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/swap_pressure_discarding_policy_android.h"

#include <memory>
#include <vector>

#include "base/byte_size.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/process/process_metrics.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/performance_manager/mechanisms/page_discarder.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace performance_manager::policies {

namespace {

class FakePageDiscarder : public mechanism::PageDiscarder {
 public:
  FakePageDiscarder() = default;
  ~FakePageDiscarder() override = default;

  std::optional<base::ByteSize> DiscardPageNode(
      const PageNode* page_node,
      ::mojom::LifecycleUnitDiscardReason discard_reason) override {
    if (failing_pages_.contains(page_node)) {
      return std::nullopt;
    }
    discarded_pages_.push_back(page_node);
    last_reason_ = discard_reason;
    if (on_discard_callback_) {
      on_discard_callback_.Run(page_node);
    }
    if (failing_or_destroyed_pages_.contains(page_node)) {
      return base::MiB(80);
    }
    PageLiveStateDecorator::Data::GetOrCreateForPageNode(page_node)
        ->SetIsDiscardedForTesting(true);
    return base::MiB(80);
  }

  const std::vector<const PageNode*>& discarded_pages() const {
    return discarded_pages_;
  }

  ::mojom::LifecycleUnitDiscardReason last_reason() const {
    return last_reason_;
  }

  void SetFailingPage(const PageNode* page_node) {
    failing_pages_.insert(page_node);
  }

  void SetOnDiscardCallback(
      base::RepeatingCallback<void(const PageNode*)> callback,
      const PageNode* destroyed_node) {
    on_discard_callback_ = std::move(callback);
    failing_or_destroyed_pages_.insert(destroyed_node);
  }

  void Clear() {
    discarded_pages_.clear();
    failing_pages_.clear();
    failing_or_destroyed_pages_.clear();
    on_discard_callback_.Reset();
  }

 private:
  std::vector<const PageNode*> discarded_pages_;
  base::flat_set<const PageNode*> failing_pages_;
  base::flat_set<const PageNode*> failing_or_destroyed_pages_;
  base::RepeatingCallback<void(const PageNode*)> on_discard_callback_;
  ::mojom::LifecycleUnitDiscardReason last_reason_ =
      ::mojom::LifecycleUnitDiscardReason::PROACTIVE;
};

struct TabNodes {
  TestNodeWrapper<ProcessNodeImpl> process_node;
  TestNodeWrapper<PageNodeImpl> page_node;
  TestNodeWrapper<FrameNodeImpl> main_frame_node;
};

// Base class listed before `GraphTestHarness` so `ScopedFeatureList` is
// initialized before `BrowserTaskEnvironment` spawns ThreadPool worker threads
// and destroyed only after `~GraphTestHarness()` joins them.
class SwapPressureFeatureListSetup {
 protected:
  SwapPressureFeatureListSetup() {
    feature_list_.InitAndEnableFeatureWithParameters(
        chrome::android::kAndroidSwapPressureTabDiscard,
        {
            {"swap_free_threshold_pct", "10"},
            {"file_cache_threshold_mb", "800"},
            {"min_tabs_to_keep", "3"},
        });
  }
  ~SwapPressureFeatureListSetup() = default;

  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

class SwapPressureDiscardingPolicyAndroidTest
    : public SwapPressureFeatureListSetup,
      public GraphTestHarness {
 public:
  SwapPressureDiscardingPolicyAndroidTest()
      : GraphTestHarness(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~SwapPressureDiscardingPolicyAndroidTest() override = default;

  void SetUp() override {
    GraphTestHarness::SetUp();

    // Default healthy 8 GB / 4 GB ZRAM state: 50% swap free, 1500 MiB file.
    meminfo_.total = base::MiB(8192);
    meminfo_.swap_total = base::MiB(4096);
    meminfo_.swap_free = base::MiB(2048);
    meminfo_.active_file = base::MiB(900);
    meminfo_.inactive_file = base::MiB(600);

    graph()->PassToGraph(std::make_unique<PageLiveStateDecorator>());

    auto eligibility_policy = std::make_unique<DiscardEligibilityPolicy>();
    eligibility_policy->SetNoDiscardPatternsForProfile(base::UnguessableToken(),
                                                       {});
    graph()->PassToGraph(std::move(eligibility_policy));

    auto discarding_helper = std::make_unique<PageDiscardingHelper>();
    auto fake_discarder = std::make_unique<FakePageDiscarder>();
    fake_discarder_ = fake_discarder.get();
    discarding_helper->SetMockDiscarderForTesting(std::move(fake_discarder));
    graph()->PassToGraph(std::move(discarding_helper));

    auto policy = std::make_unique<SwapPressureDiscardingPolicyAndroid>(
        base::BindRepeating(
            &SwapPressureDiscardingPolicyAndroidTest::GetMemoryInfo,
            base::Unretained(this)));
    policy_ = policy.get();
    graph()->PassToGraph(std::move(policy));
  }

  void TearDown() override {
    fake_discarder_ = nullptr;
    policy_ = nullptr;
    GraphTestHarness::TearDown();
  }

 protected:
  bool GetMemoryInfo(base::SystemMemoryInfo* out) {
    *out = meminfo_;
    return true;
  }

  TabNodes CreateBackgroundTab(const std::string& url_str) {
    auto process_node = CreateNode<ProcessNodeImpl>();
    auto page_node = CreateNode<PageNodeImpl>();
    auto main_frame_node =
        CreateFrameNodeAutoId(process_node.get(), page_node.get());
    page_node->SetType(PageType::kTab);
    page_node->SetIsVisible(false);
    page_node->SetIsAudible(false);
    const GURL url(url_str);
    page_node->OnMainFrameNavigationCommitted(
        false, base::TimeTicks::Now(), 1, url, "text/html",
        blink::mojom::PermissionStatus::ASK);
    main_frame_node->OnNavigationCommitted(
        url, url::Origin::Create(url), /*same_document=*/false,
        /*is_served_from_back_forward_cache=*/false);
    return {std::move(process_node), std::move(page_node),
            std::move(main_frame_node)};
  }

  base::SystemMemoryInfo meminfo_;
  raw_ptr<FakePageDiscarder> fake_discarder_ = nullptr;
  raw_ptr<SwapPressureDiscardingPolicyAndroid> policy_ = nullptr;
};

TEST_F(SwapPressureDiscardingPolicyAndroidTest,
       DiscardsOldestBackgroundTabsUnderSwapAndFileCachePressure) {
  // Create 5 background tabs staggered in time (tab1 is oldest, tab5 newest).
  TabNodes tab1 = CreateBackgroundTab("https://example.com/1");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab2 = CreateBackgroundTab("https://example.com/2");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab3 = CreateBackgroundTab("https://example.com/3");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab4 = CreateBackgroundTab("https://example.com/4");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab5 = CreateBackgroundTab("https://example.com/5");
  // Make tab5 visible (foreground tab).
  tab5.page_node->SetIsVisible(true);
  task_env().FastForwardBy(base::Seconds(15));

  // Healthy state: no discards, and SwapFreePercent / FileCacheMb are not
  // emitted when neither pressure criterion triggers.
  base::HistogramTester histograms;
  policy_->CheckSwapPressureForTesting();
  EXPECT_TRUE(fake_discarder_->discarded_pages().empty());
  histograms.ExpectBucketCount(
      "PerformanceManager.AndroidSwapPressure.CheckOutcome",
      SwapPressureDiscardingPolicyAndroid::CheckOutcome::kHealthy, 1);
  histograms.ExpectTotalCount(
      "PerformanceManager.AndroidSwapPressure.SwapFreePercent", 0);
  histograms.ExpectTotalCount(
      "PerformanceManager.AndroidSwapPressure.FileCacheMb", 0);

  // Trigger swap pressure: SwapFree = 300 MiB / 4096 MiB (7% <= 10%) and
  // FileCache = 550 MiB (<= 800 MiB).
  meminfo_.swap_free = base::MiB(300);
  meminfo_.active_file = base::MiB(300);
  meminfo_.inactive_file = base::MiB(250);

  policy_->CheckSwapPressureForTesting();

  // With 5 live tabs, min_tabs_to_keep=3, and max_discards_per_check=2,
  // the 2 oldest background tabs (tab1 and tab2) should be urgently discarded.
  ASSERT_EQ(2u, fake_discarder_->discarded_pages().size());
  EXPECT_EQ(tab1.page_node.get(), fake_discarder_->discarded_pages()[0]);
  EXPECT_EQ(tab2.page_node.get(), fake_discarder_->discarded_pages()[1]);
  EXPECT_EQ(::mojom::LifecycleUnitDiscardReason::URGENT,
            fake_discarder_->last_reason());
  histograms.ExpectBucketCount(
      "PerformanceManager.AndroidSwapPressure.CheckOutcome",
      SwapPressureDiscardingPolicyAndroid::CheckOutcome::kDiscardedTabs, 1);
  histograms.ExpectUniqueSample(
      "PerformanceManager.AndroidSwapPressure.SwapFreePercent", 7, 1);
  histograms.ExpectUniqueSample(
      "PerformanceManager.AndroidSwapPressure.FileCacheMb", 550, 1);
  histograms.ExpectUniqueSample(
      "PerformanceManager.AndroidSwapPressure.LiveTabCountAtPressure", 5, 1);
  histograms.ExpectUniqueSample(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabCount", 2, 1);
  histograms.ExpectTotalCount(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabTimeInBackground", 2);
  histograms.ExpectTotalCount(
      "PerformanceManager.AndroidSwapPressure.EstimatedReclaimedMb", 1);
  // Eligibility is evaluated once per live tab (5 tabs), not a second time
  // inside ImmediatelyDiscardMultiplePages().
  histograms.ExpectTotalCount("PerformanceManager.Discarding.DecisionResult",
                              5);

  // A subsequent check should not discard any more tabs because only 3 live
  // tabs remain (matching min_tabs_to_keep = 3).
  fake_discarder_->Clear();
  policy_->CheckSwapPressureForTesting();
  EXPECT_TRUE(fake_discarder_->discarded_pages().empty());
  histograms.ExpectBucketCount(
      "PerformanceManager.AndroidSwapPressure.CheckOutcome",
      SwapPressureDiscardingPolicyAndroid::CheckOutcome::kBlockedByMinTabsFloor,
      1);
}

TEST_F(SwapPressureDiscardingPolicyAndroidTest,
       DoesNotDiscardWhenFileCacheIsHighOrSwapIsHealthy) {
  TabNodes tab1 = CreateBackgroundTab("https://example.com/1");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab2 = CreateBackgroundTab("https://example.com/2");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab3 = CreateBackgroundTab("https://example.com/3");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab4 = CreateBackgroundTab("https://example.com/4");
  task_env().FastForwardBy(base::Seconds(15));

  // Case 1: Low swap (5%), but high file cache (1200 MiB > 800 MiB).
  base::HistogramTester histograms;
  meminfo_.swap_free = base::MiB(200);
  meminfo_.active_file = base::MiB(700);
  meminfo_.inactive_file = base::MiB(500);
  policy_->CheckSwapPressureForTesting();
  EXPECT_TRUE(fake_discarder_->discarded_pages().empty());
  histograms.ExpectBucketCount(
      "PerformanceManager.AndroidSwapPressure.CheckOutcome",
      SwapPressureDiscardingPolicyAndroid::CheckOutcome::kSwapPressureOnly, 1);

  // Case 2: Low file cache (400 MiB <= 800 MiB), but healthy swap (25% > 10%).
  meminfo_.swap_free = base::MiB(1024);
  meminfo_.active_file = base::MiB(200);
  meminfo_.inactive_file = base::MiB(200);
  policy_->CheckSwapPressureForTesting();
  EXPECT_TRUE(fake_discarder_->discarded_pages().empty());
  histograms.ExpectBucketCount(
      "PerformanceManager.AndroidSwapPressure.CheckOutcome",
      SwapPressureDiscardingPolicyAndroid::CheckOutcome::kFileCachePressureOnly,
      1);
}

TEST_F(SwapPressureDiscardingPolicyAndroidTest,
       RespectsMinTimeInBackgroundProtection) {
  TabNodes tab1 = CreateBackgroundTab("https://example.com/1");
  task_env().FastForwardBy(base::Seconds(20));
  TabNodes tab2 = CreateBackgroundTab("https://example.com/2");
  TabNodes tab3 = CreateBackgroundTab("https://example.com/3");
  TabNodes tab4 = CreateBackgroundTab("https://example.com/4");
  // Only advance 3 seconds (< min_time_in_background_sec = 10s) for tabs 2..4.
  task_env().FastForwardBy(base::Seconds(3));

  meminfo_.swap_free = base::MiB(200);
  meminfo_.active_file = base::MiB(300);
  meminfo_.inactive_file = base::MiB(200);

  policy_->CheckSwapPressureForTesting();

  // Only tab1 has been in the background for >= 10s, so only tab1 is discarded.
  ASSERT_EQ(1u, fake_discarder_->discarded_pages().size());
  EXPECT_EQ(tab1.page_node.get(), fake_discarder_->discarded_pages()[0]);
}

TEST(SwapPressureDiscardingPolicyAndroidThresholdsTest,
     ScalesThresholdsAcrossFleetRamAndSwapProfiles) {
  base::test::ScopedFeatureList default_params(
      chrome::android::kAndroidSwapPressureTabDiscard);

  // 1. 4 GB RAM device (3608 MiB) with 2048 MiB ZRAM:
  //    - effective_file_cache_threshold = clamp(3608 MiB * 7%, 300 MiB,
  //      600 MiB) = 300 MiB
  //    - effective_min_tabs_to_keep = clamp(4 / 2, 2, 8) = 2
  base::SystemMemoryInfo mem_4gb;
  mem_4gb.total = base::MiB(3608);
  mem_4gb.swap_total = base::MiB(2048);
  mem_4gb.swap_free = base::MiB(180);  // 8% <= [10%, 30%]
  mem_4gb.active_file = base::MiB(140);
  mem_4gb.inactive_file = base::MiB(150);  // 290 MiB <= 300 MiB
  auto t_4gb =
      SwapPressureDiscardingPolicyAndroid::ComputeEffectiveThresholds(mem_4gb);
  EXPECT_EQ(base::MiB(300), t_4gb.effective_file_cache_threshold);
  EXPECT_EQ(2, t_4gb.effective_min_tabs_to_keep);
  EXPECT_TRUE(t_4gb.swap_under_pressure);
  EXPECT_TRUE(t_4gb.file_cache_under_pressure());

  // 2. 6 GB RAM device (Pixel 6a: 5581 MiB RAM, 2048 MiB ZRAM):
  //    - effective_file_cache_threshold = 5581 MiB * 7% = 390.67 MiB
  //    - effective_min_tabs_to_keep = 3
  base::SystemMemoryInfo mem_6gb;
  mem_6gb.total = base::MiB(5581);
  mem_6gb.swap_total = base::MiB(2048);
  mem_6gb.swap_free = base::MiB(180);  // 8% <= [10%, 30%]
  mem_6gb.active_file = base::MiB(180);
  mem_6gb.inactive_file = base::MiB(200);  // 380 MiB <= 390.67 MiB
  auto t_6gb =
      SwapPressureDiscardingPolicyAndroid::ComputeEffectiveThresholds(mem_6gb);
  EXPECT_EQ((base::MiB(5581) * 7) / 100, t_6gb.effective_file_cache_threshold);
  EXPECT_EQ(3, t_6gb.effective_min_tabs_to_keep);
  EXPECT_TRUE(t_6gb.swap_under_pressure);
  EXPECT_TRUE(t_6gb.file_cache_under_pressure());

  // 3. 8 GB RAM device (7386 MiB RAM, 4096 MiB ZRAM):
  //    - effective_file_cache_threshold = 7386 MiB * 7% = 517.02 MiB
  //    - effective_min_tabs_to_keep = 4
  base::SystemMemoryInfo mem_8gb;
  mem_8gb.total = base::MiB(7386);
  mem_8gb.swap_total = base::MiB(4096);
  mem_8gb.swap_free = base::MiB(350);  // 8% <= [10%, 30%]
  mem_8gb.active_file = base::MiB(250);
  mem_8gb.inactive_file = base::MiB(250);  // 500 MiB <= 517.02 MiB
  auto t_8gb =
      SwapPressureDiscardingPolicyAndroid::ComputeEffectiveThresholds(mem_8gb);
  EXPECT_EQ((base::MiB(7386) * 7) / 100, t_8gb.effective_file_cache_threshold);
  EXPECT_EQ(4, t_8gb.effective_min_tabs_to_keep);
  EXPECT_TRUE(t_8gb.swap_under_pressure);
  EXPECT_TRUE(t_8gb.file_cache_under_pressure());

  // 4. 12 GB / 16 GB RAM device (Pixel 9 / Fold: 15460 MiB RAM, 7730 MiB ZRAM):
  //    - effective_file_cache_threshold clamps at 600 MiB
  //    - effective_min_tabs_to_keep clamps at 8
  //    - At 1113 MiB file cache (Fold 80-tab run minimum), file_cache is NOT
  //      under pressure.
  base::SystemMemoryInfo mem_16gb;
  mem_16gb.total = base::MiB(15460);
  mem_16gb.swap_total = base::MiB(7730);
  mem_16gb.swap_free = base::MiB(500);
  mem_16gb.active_file = base::MiB(600);
  mem_16gb.inactive_file = base::MiB(513);  // 1113 MiB > 600 MiB
  auto t_16gb =
      SwapPressureDiscardingPolicyAndroid::ComputeEffectiveThresholds(mem_16gb);
  EXPECT_EQ(base::MiB(600), t_16gb.effective_file_cache_threshold);
  EXPECT_EQ(8, t_16gb.effective_min_tabs_to_keep);
  EXPECT_TRUE(t_16gb.swap_under_pressure);
  EXPECT_FALSE(t_16gb.file_cache_under_pressure());
}

TEST_F(SwapPressureDiscardingPolicyAndroidTest,
       ObserverRateLimitingAndFollowUpTimerStopsAtKeepFloor) {
  TabNodes tab1 = CreateBackgroundTab("https://example.com/1");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab2 = CreateBackgroundTab("https://example.com/2");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab3 = CreateBackgroundTab("https://example.com/3");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab4 = CreateBackgroundTab("https://example.com/4");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab5 = CreateBackgroundTab("https://example.com/5");
  tab5.page_node->SetIsVisible(true);
  task_env().FastForwardBy(base::Seconds(15));

  // Enter swap + file-cache pressure with 6 live tabs (3 above keep floor of 3)
  // by adding tab6. Observer checks are scheduled asynchronously on the task
  // runner (never synchronously on the OnPageNodeAdded / OnFrameNodeAdded
  // stack); running the zero-delay timer discards the 2 oldest (tab1, tab2).
  meminfo_.swap_free = base::MiB(250);
  meminfo_.active_file = base::MiB(250);
  meminfo_.inactive_file = base::MiB(250);

  TabNodes tab6 = CreateBackgroundTab("https://example.com/6");
  EXPECT_TRUE(fake_discarder_->discarded_pages().empty());
  task_env().RunUntilIdle();
  ASSERT_EQ(2u, fake_discarder_->discarded_pages().size());
  EXPECT_EQ(tab1.page_node.get(), fake_discarder_->discarded_pages()[0]);
  EXPECT_EQ(tab2.page_node.get(), fake_discarder_->discarded_pages()[1]);

  // Because 4 live tabs remain (> min_tabs_to_keep = 3) and tab3/tab4 are still
  // eligible, the 3s follow-up timer is armed.
  EXPECT_TRUE(policy_->IsDeferredCheckTimerRunningForTesting());

  // An observer event within min_check_interval_ms (2000ms) is rate-limited and
  // does not synchronously discard tab3.
  fake_discarder_->Clear();
  task_env().FastForwardBy(base::Milliseconds(500));
  tab5.page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  EXPECT_TRUE(fake_discarder_->discarded_pages().empty());

  // Advancing to the 3s follow-up timer fires the next check, discarding tab3
  // (bringing live tabs down to min_tabs_to_keep = 3).
  task_env().FastForwardBy(base::Milliseconds(2500));
  ASSERT_EQ(1u, fake_discarder_->discarded_pages().size());
  EXPECT_EQ(tab3.page_node.get(), fake_discarder_->discarded_pages()[0]);

  // Now live_tab_count == 3 (== min_tabs_to_keep) and no tabs are loading, so
  // the follow-up timer must NOT be re-armed even though swap pressure
  // persists.
  EXPECT_FALSE(policy_->IsDeferredCheckTimerRunningForTesting());

  // Even if one of the remaining 3 tabs (<= min_tabs_to_keep) is actively
  // loading, the follow-up timer must not poll while live_tab_count <=
  // min_tabs_to_keep.
  tab5.page_node->SetLoadingState(PageNode::LoadingState::kLoading);
  task_env().FastForwardBy(base::Seconds(3));
  EXPECT_FALSE(policy_->IsDeferredCheckTimerRunningForTesting());
}

TEST_F(SwapPressureDiscardingPolicyAndroidTest,
       TracksDiscardedTabSessionOutcomeAndFreeTabKills) {
  TabNodes tab1 = CreateBackgroundTab("https://example.com/1");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab2 = CreateBackgroundTab("https://example.com/2");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab3 = CreateBackgroundTab("https://example.com/3");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab4 = CreateBackgroundTab("https://example.com/4");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab5 = CreateBackgroundTab("https://example.com/5");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab6 = CreateBackgroundTab("https://example.com/6");
  tab6.page_node->SetIsVisible(true);
  task_env().FastForwardBy(base::Seconds(15));

  base::HistogramTester histograms;

  // Enter swap + file-cache pressure: first check discards tab1 and tab2,
  // second check discards tab3 (leaving 3 live tabs == min_tabs_to_keep).
  meminfo_.swap_free = base::MiB(250);
  meminfo_.active_file = base::MiB(250);
  meminfo_.inactive_file = base::MiB(250);
  policy_->CheckSwapPressureForTesting();
  policy_->CheckSwapPressureForTesting();
  ASSERT_EQ(3u, fake_discarder_->discarded_pages().size());

  // 1. User switches back to tab1 after 30s in the same foreground session:
  //    logged as kRevisitedInForegroundSession + DiscardedTabTimeToRevisit.
  task_env().FastForwardBy(base::Seconds(30));
  tab1.page_node->SetIsVisible(true);
  histograms.ExpectBucketCount(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabSessionOutcome",
      SwapPressureDiscardingPolicyAndroid::DiscardedTabSessionOutcome::
          kRevisitedInForegroundSession,
      1);
  histograms.ExpectUniqueSample(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabTimeToRevisit",
      base::Seconds(30).InMilliseconds(), 1);

  // 2. User closes tab2 without ever revisiting it ("free tab kill"):
  //    logged as kClosedWithoutRevisit.
  tab2.main_frame_node.reset();
  tab2.page_node.reset();
  tab2.process_node.reset();
  histograms.ExpectBucketCount(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabSessionOutcome",
      SwapPressureDiscardingPolicyAndroid::DiscardedTabSessionOutcome::
          kClosedWithoutRevisit,
      1);

  // 3. User backgrounds Chrome (visible session ends) before ever revisiting
  //    tab3 ("free tab kill" bounded to session):
  //    logged as kSessionEndedWithoutRevisit.
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  task_env().RunUntilIdle();
  histograms.ExpectBucketCount(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabSessionOutcome",
      SwapPressureDiscardingPolicyAndroid::DiscardedTabSessionOutcome::
          kSessionEndedWithoutRevisit,
      1);
  histograms.ExpectTotalCount(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabSessionOutcome", 3);

  // If the user returns in a subsequent session and switches to tab3,
  // neither DiscardedTabSessionOutcome nor DiscardedTabTimeToRevisit is
  // logged again since the session already ended without revisit.
  task_env().FastForwardBy(base::Seconds(60));
  tab3.page_node->SetIsVisible(true);
  histograms.ExpectTotalCount(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabSessionOutcome", 3);
  histograms.ExpectTotalCount(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabTimeToRevisit", 1);
}

TEST_F(SwapPressureDiscardingPolicyAndroidTest,
       TriggersCheckOnSubframeCreationAndAppForegrounding) {
  TabNodes tab1 = CreateBackgroundTab("https://example.com/1");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab2 = CreateBackgroundTab("https://example.com/2");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab3 = CreateBackgroundTab("https://example.com/3");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab4 = CreateBackgroundTab("https://example.com/4");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab5 = CreateBackgroundTab("https://example.com/5");
  tab5.page_node->SetIsVisible(true);
  tab5.page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  task_env().FastForwardBy(base::Seconds(15));
  fake_discarder_->Clear();

  // 1. Simulate swap + file-cache pressure developing while the user scrolls
  //    already-loaded tab5 (kLoadedIdle) and the page lazy-loads a child
  //    subframe (OnFrameNodeAdded).
  meminfo_.swap_free = base::MiB(250);
  meminfo_.active_file = base::MiB(250);
  meminfo_.inactive_file = base::MiB(250);

  auto subframe_process = CreateNode<ProcessNodeImpl>();
  auto child_frame = CreateFrameNodeAutoId(
      subframe_process.get(), tab5.page_node.get(), tab5.main_frame_node.get());
  EXPECT_TRUE(fake_discarder_->discarded_pages().empty());
  task_env().RunUntilIdle();
  ASSERT_EQ(2u, fake_discarder_->discarded_pages().size());
  EXPECT_EQ(tab1.page_node.get(), fake_discarder_->discarded_pages()[0]);
  EXPECT_EQ(tab2.page_node.get(), fake_discarder_->discarded_pages()[1]);

  // 2. Add another background tab while healthy, then simulate external memory
  //    growth while Chrome is in the background followed by Chrome returning to
  //    the foreground (APPLICATION_STATE_HAS_RUNNING_ACTIVITIES).
  fake_discarder_->Clear();
  meminfo_.swap_free = base::MiB(2048);
  meminfo_.active_file = base::MiB(900);
  meminfo_.inactive_file = base::MiB(600);
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab6 = CreateBackgroundTab("https://example.com/6");
  task_env().FastForwardBy(base::Seconds(15));
  EXPECT_TRUE(fake_discarder_->discarded_pages().empty());

  // External app eats swap/cache, then user switches back to Chrome.
  meminfo_.swap_free = base::MiB(200);
  meminfo_.active_file = base::MiB(200);
  meminfo_.inactive_file = base::MiB(200);
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  task_env().RunUntilIdle();

  ASSERT_EQ(1u, fake_discarder_->discarded_pages().size());
  EXPECT_EQ(tab3.page_node.get(), fake_discarder_->discarded_pages()[0]);
}

TEST_F(SwapPressureDiscardingPolicyAndroidTest,
       HandlesPartialDiscardFailureAndSynchronousNodeDestruction) {
  TabNodes tab1 = CreateBackgroundTab("https://example.com/1");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab2 = CreateBackgroundTab("https://example.com/2");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab3 = CreateBackgroundTab("https://example.com/3");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab4 = CreateBackgroundTab("https://example.com/4");
  task_env().FastForwardBy(base::Seconds(5));
  TabNodes tab5 = CreateBackgroundTab("https://example.com/5");
  tab5.page_node->SetIsVisible(true);
  task_env().FastForwardBy(base::Seconds(15));

  meminfo_.swap_free = base::MiB(250);
  meminfo_.active_file = base::MiB(250);
  meminfo_.inactive_file = base::MiB(250);

  // 1. Make tab1 fail to discard while tab2 succeeds: DiscardedTabCount must
  //    record 1 (not 2), and because 4 live tabs remain (> min_tabs_to_keep=3)
  //    with tab3 still eligible, the follow-up timer must be armed.
  fake_discarder_->SetFailingPage(tab1.page_node.get());
  base::HistogramTester histograms;
  policy_->CheckSwapPressureForTesting(/*schedule_followup=*/true);
  histograms.ExpectUniqueSample(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabCount", 1, 1);
  histograms.ExpectTotalCount(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabTimeInBackground", 1);
  EXPECT_TRUE(policy_->IsDeferredCheckTimerRunningForTesting());

  // 2. When tab3 is discarded, simulate synchronous OnLoadingStateChanged()
  //    (as WebContentsImpl::Discard() resets navigations) and synchronous
  //    destruction of tab3's PageNode during ImmediatelyDiscardMultiplePages()
  //    to verify no UAF occurs and no stale timer remains armed once live tabs
  //    reach min_tabs_to_keep.
  const PageNode* tab3_raw = tab3.page_node.get();
  fake_discarder_->SetOnDiscardCallback(
      base::BindRepeating(
          [](TabNodes* tab_to_destroy, const PageNode* discarded) {
            if (tab_to_destroy->page_node.get() == discarded) {
              tab_to_destroy->page_node->SetLoadingState(
                  PageNode::LoadingState::kLoadingNotStarted);
              tab_to_destroy->main_frame_node.reset();
              tab_to_destroy->page_node.reset();
              tab_to_destroy->process_node.reset();
            }
          },
          base::Unretained(&tab3)),
      tab3_raw);
  task_env().FastForwardBy(base::Seconds(3));
  histograms.ExpectBucketCount(
      "PerformanceManager.AndroidSwapPressure.DiscardedTabCount", 1, 2);
  EXPECT_FALSE(policy_->IsDeferredCheckTimerRunningForTesting());
}

}  // namespace performance_manager::policies
