// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/transient_keep_alive_policy.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::policies {

namespace {
const char* kTestUrl1 = "https://foo.com/page1";
const char* kTestUrl2 = "https://bar.com/page2";
const char* kTestUrl3 = "https://bax.com/page3";
const char* kTestUrl4 = "https://dax.com/page4";

constexpr base::TimeDelta kTimerDelay = base::Seconds(5);

}  // namespace

class TransientKeepAlivePolicyTest : public ChromeRenderViewHostTestHarness {
 public:
  // Enable mock time to control the keep-alive timer.
  TransientKeepAlivePolicyTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TransientKeepAlivePolicyTest() override = default;

  void SetUp() override {
    // Enable the feature with a short duration for testing.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kTransientKeepAlivePolicy,
        {{"duration", "5s"}, {"count", "2"}});

    ChromeRenderViewHostTestHarness::SetUp();
    pm_harness_.SetUp();
    SetContents(CreateTestWebContents());

    policy_ = PerformanceManager::GetGraph()->PassToGraph(
        std::make_unique<TransientKeepAlivePolicy>());

    content::DisableBackForwardCacheForTesting(
        web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
  }

  void TearDown() override {
    policy_ = nullptr;
    DeleteContents();
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TransientKeepAlivePolicy* policy() { return policy_; }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  PerformanceManagerTestHarnessHelper pm_harness_;

  raw_ptr<TransientKeepAlivePolicy> policy_;
};

// Tests that the keep-alive expires after the configured duration.
TEST_F(TransientKeepAlivePolicyTest, KeepAliveReleasedAfterTimeout) {
  NavigateAndCommit(GURL(kTestUrl1));
  content::RenderProcessHost* initial_rph = process();
  EXPECT_EQ(1, initial_rph->GetPendingReuseRefCountForTesting());

  NavigateAndCommit(GURL(kTestUrl2));
  EXPECT_EQ(1, initial_rph->GetPendingReuseRefCountForTesting());

  // Fast forward to just after the timeout.
  task_environment()->FastForwardBy(kTimerDelay + base::Seconds(1));

  EXPECT_EQ(0, initial_rph->GetPendingReuseRefCountForTesting());

  histogram_tester_.ExpectUniqueSample(
      kKeepAliveResultHistogramName,
      TransientKeepAlivePolicy::TransientKeepAliveResult::kTimedOut, 1);
}

// Tests that multiple navigations in quick succession are handled correctly.
TEST_F(TransientKeepAlivePolicyTest, MultipleQuickNavigations) {
  NavigateAndCommit(GURL(kTestUrl1));
  content::RenderProcessHost* rph1 = process();

  NavigateAndCommit(GURL(kTestUrl2));
  content::RenderProcessHost* rph2 = process();

  EXPECT_EQ(1, rph1->GetPendingReuseRefCountForTesting());
  EXPECT_EQ(1, rph2->GetPendingReuseRefCountForTesting());

  NavigateAndCommit(GURL(kTestUrl3));
  content::RenderProcessHost* rph3 = process();

  EXPECT_EQ(1, rph1->GetPendingReuseRefCountForTesting());
  EXPECT_EQ(1, rph2->GetPendingReuseRefCountForTesting());
  EXPECT_EQ(1, rph3->GetPendingReuseRefCountForTesting());

  // Fast forward to just after the timeout.
  task_environment()->FastForwardBy(kTimerDelay + base::Seconds(1));

  EXPECT_EQ(0, rph1->GetPendingReuseRefCountForTesting());
  EXPECT_EQ(0, rph2->GetPendingReuseRefCountForTesting());
  // This should remain 1 since 'rph3' still has a main frame.
  EXPECT_EQ(1, rph3->GetPendingReuseRefCountForTesting());
}

// Test that keep-alive duration shorter than expiration still works.
TEST_F(TransientKeepAlivePolicyTest, KeepAliveNotYetExpired) {
  NavigateAndCommit(GURL(kTestUrl1));
  content::RenderProcessHost* rph1 = process();

  NavigateAndCommit(GURL(kTestUrl2));
  EXPECT_EQ(1, rph1->GetPendingReuseRefCountForTesting());

  // Fast forward to just before the timeout.
  task_environment()->FastForwardBy(kTimerDelay - base::Seconds(1));

  EXPECT_EQ(1, rph1->GetPendingReuseRefCountForTesting());

  // Fast forward by the remaining time to reach the exact timeout duration.
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_EQ(0, rph1->GetPendingReuseRefCountForTesting());
}

// Test that the oldest render process host kept-alive that is currently tracked
// is evicted once we are over the the limit of tracked items.
// crbug.com/459626659
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif
TEST_F(TransientKeepAlivePolicyTest,
       MAYBE(EvictsOldestProcessWhenLimitExceeded)) {
  NavigateAndCommit(GURL(kTestUrl1));
  content::RenderProcessHost* rph1 = process();

  EXPECT_EQ(1, rph1->GetPendingReuseRefCountForTesting());  // Active process

  NavigateAndCommit(GURL(kTestUrl2));
  content::RenderProcessHost* rph2 = process();

  EXPECT_EQ(1, rph1->GetPendingReuseRefCountForTesting());  // Now kept alive
  EXPECT_EQ(1, rph2->GetPendingReuseRefCountForTesting());  // Active process

  NavigateAndCommit(GURL(kTestUrl3));
  content::RenderProcessHost* rph3 = process();

  // Verify that `rph1` & `rph2` are being kept alive.
  EXPECT_EQ(1, rph1->GetPendingReuseRefCountForTesting());  // Still kept alive
  EXPECT_EQ(1, rph2->GetPendingReuseRefCountForTesting());  // Now kept alive
  EXPECT_EQ(1, rph3->GetPendingReuseRefCountForTesting());  // Active process

  NavigateAndCommit(GURL(kTestUrl4));
  content::RenderProcessHost* rph4 = process();

  // This ensures all OnLastMainFrameRemoved tasks have completed, including the
  // eviction logic triggered by the third navigation.
  task_environment()->RunUntilIdle();

  // Verify final ref count for each renderer process host.
  EXPECT_EQ(0, rph1->GetPendingReuseRefCountForTesting());  // Evicted
  EXPECT_EQ(1, rph2->GetPendingReuseRefCountForTesting());  // Still kept alive
  EXPECT_EQ(1, rph3->GetPendingReuseRefCountForTesting());  // Now kept alive
  EXPECT_EQ(1, rph4->GetPendingReuseRefCountForTesting());  // Active process

  // Verify the eviction was logged correctly.
  histogram_tester_.ExpectUniqueSample(
      kKeepAliveResultHistogramName,
      TransientKeepAlivePolicy::TransientKeepAliveResult::kEvicted, 1);
}

// Test that makes sure the reporting does not get recorded during shutdown.
TEST_F(TransientKeepAlivePolicyTest, NoHistogramOnActiveProcessShutdown) {
  base::HistogramTester histogram_tester_;

  NavigateAndCommit(GURL(kTestUrl1));
  content::RenderProcessHost* rph1 = process();
  EXPECT_EQ(1, rph1->GetPendingReuseRefCountForTesting());

  // rph1 is active (timer is not running). Deleting it should not
  // log anything.
  DeleteContents();

  // Verify no logs.
  histogram_tester_.ExpectTotalCount(kKeepAliveResultHistogramName, 0);
}

}  // namespace performance_manager::policies
