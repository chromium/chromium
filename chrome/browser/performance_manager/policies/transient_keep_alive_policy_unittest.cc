// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/transient_keep_alive_policy.h"

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/browser_thread.h"
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

  // Flushes any pending tasks posted to the UI thread. The policy posts async
  // ref count decrements to the UI thread; this method posts a sentinel task
  // after them and waits for it, guaranteeing the decrements have completed.
  void FlushUIThreadTasks() {
    base::RunLoop run_loop;
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 run_loop.QuitClosure());
    run_loop.Run();
  }

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

  // Ref count decrement is posted asynchronously to the UI thread.
  FlushUIThreadTasks();

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

  // Ref count decrements are posted asynchronously to the UI thread.
  FlushUIThreadTasks();

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

  // Flush to confirm the timer has NOT fired yet — ref count must still be 1.
  FlushUIThreadTasks();
  EXPECT_EQ(1, rph1->GetPendingReuseRefCountForTesting());

  // Fast forward by the remaining time to reach the exact timeout duration.
  task_environment()->FastForwardBy(base::Seconds(1));

  // Ref count decrement is posted asynchronously to the UI thread.
  FlushUIThreadTasks();

  EXPECT_EQ(0, rph1->GetPendingReuseRefCountForTesting());
}

// Test that the oldest render process host kept-alive that is currently tracked
// is evicted once we are over the the limit of tracked items.
// crbug.com/459626659
TEST_F(TransientKeepAlivePolicyTest, EvictsOldestProcessWhenLimitExceeded) {
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

  // Wait for all OnLastMainFrameRemoved tasks to complete, including the
  // eviction logic triggered by the third navigation.
  FlushUIThreadTasks();

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

// Tests that eviction and timeout can happen in the same test run without
// interference: evict the oldest, then let a second keep-alive expire via
// the timer. Both paths post async decrements.
TEST_F(TransientKeepAlivePolicyTest, EvictionFollowedByTimeout) {
  // rph1 will be evicted when we exceed the keep-alive limit.
  NavigateAndCommit(GURL(kTestUrl1));

  NavigateAndCommit(GURL(kTestUrl2));
  content::RenderProcessHost* rph2 = process();

  NavigateAndCommit(GURL(kTestUrl3));
  content::RenderProcessHost* rph3 = process();

  // rph1 and rph2 are now empty and kept alive (limit is 2).
  // Navigating to a 4th URL will evict rph1.
  NavigateAndCommit(GURL(kTestUrl4));

  // Wait for the async eviction decrement to complete.
  FlushUIThreadTasks();

  // rph1 was evicted, rph2 and rph3 are kept alive, rph4 is active.
  histogram_tester_.ExpectBucketCount(
      kKeepAliveResultHistogramName,
      TransientKeepAlivePolicy::TransientKeepAliveResult::kEvicted, 1);
  EXPECT_EQ(1, rph2->GetPendingReuseRefCountForTesting());
  EXPECT_EQ(1, rph3->GetPendingReuseRefCountForTesting());

  // Now let the keep-alive timers expire for rph2 and rph3.
  task_environment()->FastForwardBy(kTimerDelay + base::Seconds(1));

  // Wait for the async timeout decrements to complete.
  FlushUIThreadTasks();

  EXPECT_EQ(0, rph2->GetPendingReuseRefCountForTesting());
  EXPECT_EQ(0, rph3->GetPendingReuseRefCountForTesting());

  // Both eviction and timeout should be recorded. rph2 and rph3 both timed
  // out, so we expect 2 timeout samples.
  histogram_tester_.ExpectBucketCount(
      kKeepAliveResultHistogramName,
      TransientKeepAlivePolicy::TransientKeepAliveResult::kEvicted, 1);
  histogram_tester_.ExpectBucketCount(
      kKeepAliveResultHistogramName,
      TransientKeepAlivePolicy::TransientKeepAliveResult::kTimedOut, 2);
}

// Tests that destroying the web contents while a keep-alive timer is running
// does not crash. The async ref count decrement should safely handle the
// process being gone.
TEST_F(TransientKeepAlivePolicyTest, DestroyContentsWhileKeepAliveActive) {
  NavigateAndCommit(GURL(kTestUrl1));
  content::RenderProcessHost* rph1 = process();
  EXPECT_EQ(1, rph1->GetPendingReuseRefCountForTesting());

  // Navigate away so rph1 becomes empty and gets a keep-alive timer.
  NavigateAndCommit(GURL(kTestUrl2));
  EXPECT_EQ(1, rph1->GetPendingReuseRefCountForTesting());

  // Destroy everything while the keep-alive timer is still running.
  // This triggers OnProcessNodeRemoved, which cleans up the map entry
  // and destroys the KeepAliveInfo (cancelling the timer).
  // No crash should occur.
  DeleteContents();
  FlushUIThreadTasks();

  // The keep-alive was cut short by process removal, not by timeout.
  // No timeout histogram should be recorded.
  histogram_tester_.ExpectTotalCount(kKeepAliveResultHistogramName, 0);
}

}  // namespace performance_manager::policies
