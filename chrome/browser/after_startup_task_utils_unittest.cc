// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/after_startup_task_utils.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace {

using performance_manager::GraphTestHarness;
using performance_manager::PageNode;
using performance_manager::PageNodeImpl;
using performance_manager::PageType;

class WrappedTaskRunner : public base::SequencedTaskRunner {
 public:
  explicit WrappedTaskRunner(scoped_refptr<SequencedTaskRunner> real_runner)
      : real_task_runner_(std::move(real_runner)) {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    ++posted_task_count_;
    return real_task_runner_->PostDelayedTask(
        from_here,
        base::BindOnce(&WrappedTaskRunner::RunWrappedTask, this,
                       std::move(task)),
        base::TimeDelta());  // Squash all delays so our tests complete asap.
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    // Not implemented.
    NOTREACHED();
  }

  bool RunsTasksInCurrentSequence() const override {
    return real_task_runner_->RunsTasksInCurrentSequence();
  }

  base::SequencedTaskRunner* real_runner() const {
    return real_task_runner_.get();
  }

  int total_task_count() const { return posted_task_count_ + ran_task_count_; }
  int posted_task_count() const { return posted_task_count_; }
  int ran_task_count() const { return ran_task_count_; }

  void reset_task_counts() {
    posted_task_count_ = 0;
    ran_task_count_ = 0;
  }

 private:
  ~WrappedTaskRunner() override = default;

  void RunWrappedTask(base::OnceClosure task) {
    ++ran_task_count_;
    std::move(task).Run();
  }

  scoped_refptr<base::SequencedTaskRunner> real_task_runner_;
  int posted_task_count_ = 0;
  int ran_task_count_ = 0;
};

class AfterStartupTaskTest : public testing::Test {
 public:
  AfterStartupTaskTest() {
    ui_thread_ = base::MakeRefCounted<WrappedTaskRunner>(
        content::GetUIThreadTaskRunner({}));
    background_sequence_ = base::MakeRefCounted<WrappedTaskRunner>(
        base::ThreadPool::CreateSequencedTaskRunner({}));
    AfterStartupTaskUtils::UnsafeResetForTesting();
  }

  // Hop to the background sequence and call IsBrowserStartupComplete.
  bool GetIsBrowserStartupCompleteFromBackgroundSequence() {
    base::RunLoop run_loop;
    bool is_complete;
    background_sequence_->real_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&AfterStartupTaskUtils::IsBrowserStartupComplete),
        base::BindOnce(&AfterStartupTaskTest::GotIsOnBrowserStartupComplete,
                       &run_loop, &is_complete));
    run_loop.Run();
    return is_complete;
  }

  // Hop to the background sequence and call PostAfterStartupTask.
  void PostAfterStartupTaskFromBackgroundSequence(
      const base::Location& from_here,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::OnceClosure task) {
    base::RunLoop run_loop;
    background_sequence_->real_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&AfterStartupTaskUtils::PostTask, from_here,
                       std::move(task_runner), std::move(task)),
        base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  // Make sure all tasks posted to the background sequence get run.
  void FlushBackgroundSequence() {
    base::RunLoop run_loop;
    background_sequence_->real_runner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  void FlushUIThread() {
    ui_thread_->real_runner()->PostTask(FROM_HERE,
                                        task_environment_.QuitClosure());
    task_environment_.RunUntilQuit();
  }

  static void VerifyExpectedSequence(base::SequencedTaskRunner* task_runner) {
    EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
  }

 protected:
  scoped_refptr<WrappedTaskRunner> ui_thread_;
  scoped_refptr<WrappedTaskRunner> background_sequence_;
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};

 private:
  static void GotIsOnBrowserStartupComplete(base::RunLoop* loop,
                                            bool* out,
                                            bool is_complete) {
    *out = is_complete;
    loop->Quit();
  }
};

#if !BUILDFLAG(IS_ANDROID)

// Arbitrary, since this uses a mock clock, as long as it's less than the
// kStartupDelayFailsafeTimeout feature param.
constexpr base::TimeDelta kVisibleTabTimeout = base::Seconds(5);

enum class StartupObserverFeatureParams {
  // Disable kImprovedStartupBestEffortDelay feature.
  kFeatureDisabled,
  // Enable kImprovedStartupBestEffortDelay feature but don't wait for visible
  // tabs to load.
  kFeatureEnabledIgnoreVisibleTabs,
  // Enable kImprovedStartupBestEffortDelay feature and wait for the first
  // visible tab to reach kLoadedIdle.
  kFeatureEnabledWaitForLoad,
  // Enable kImprovedStartupBestEffortDelay feature and wait for the first
  // visible tab to reach kLoadedIdle or kLoadingTimedOut.
  kFeatureEnabledWaitForLoadOrTimeout,
};

class StartupObserverTest
    : public GraphTestHarness,
      public ::testing::WithParamInterface<StartupObserverFeatureParams> {
 public:
  StartupObserverTest()
      : GraphTestHarness(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    GraphTestHarness::SetUp();
    AfterStartupTaskUtils::UnsafeResetForTesting();

    bool feature_enabled = false;
    base::TimeDelta visible_tab_timeout;
    bool stop_on_loading_timed_out = false;
    switch (GetParam()) {
      case StartupObserverFeatureParams::kFeatureDisabled:
        // Do nothing.
        break;
      case StartupObserverFeatureParams::kFeatureEnabledIgnoreVisibleTabs:
        feature_enabled = true;
        // Leave `visible_tab_timeout` at 0.
        break;
      case StartupObserverFeatureParams::kFeatureEnabledWaitForLoad:
        feature_enabled = true;
        visible_tab_timeout = kVisibleTabTimeout;
        break;
      case StartupObserverFeatureParams::kFeatureEnabledWaitForLoadOrTimeout:
        feature_enabled = true;
        visible_tab_timeout = kVisibleTabTimeout;
        stop_on_loading_timed_out = true;
        break;
    }

    if (feature_enabled) {
      feature_list_.InitAndEnableFeatureWithParameters(
          features::kImprovedStartupBestEffortDelay,
          {
              {"StartupDelayVisibleTabTimeout",
               absl::StrFormat("%dms", visible_tab_timeout.InMilliseconds())},
              // Ensure failsafe timeout is larger than `visible_tab_timeout`.
              // With the mock clock the precise value doesn't matter.
              {"StartupDelayFailsafeTimeout",
               absl::StrFormat("%dms",
                               visible_tab_timeout.InMilliseconds() * 2)},
              {"StartupDelayStopOnLoadingTimedOut",
               stop_on_loading_timed_out ? "true" : "false"},
          });
    } else {
      feature_list_.InitAndDisableFeature(
          features::kImprovedStartupBestEffortDelay);
    }

    ASSERT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  }

  void TearDown() override {
    AfterStartupTaskUtils::UnsafeResetForTesting();
    GraphTestHarness::TearDown();
  }

 protected:
  void FlushTasks() {
    task_env().GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                   task_env().QuitClosure());
    task_env().RunUntilQuit();
  }

  // Call immediately after BeginMonitoringStartupCompletion. Returns true if
  // the observer should consider startup complete immediately, false if it
  // should keep waiting.
  bool ExpectImmediateStartupComplete() {
    FlushTasks();
    if (GetParam() ==
        StartupObserverFeatureParams::kFeatureEnabledIgnoreVisibleTabs) {
      // Should not monitor visible pages.
      EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
      histogram_tester_.ExpectUniqueSample(
          "Startup.BrowserStartupCompleteReason",
          StartupIsCompleteReason::kStartupRegistrationDone, 1);
      return true;
    }
    EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
    return false;
  }

  // Call at the end of the test, if no visible page will be detected.
  void ExpectNoVisiblePage() {
    if (GetParam() == StartupObserverFeatureParams::kFeatureDisabled) {
      // Without the feature, it waits the full failsafe timeout.
      ExpectFailsafeTimeout();
      return;
    }
    task_env().FastForwardBy(kVisibleTabTimeout);
    FlushTasks();
    EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
    histogram_tester_.ExpectUniqueSample(
        "Startup.BrowserStartupCompleteReason",
        StartupIsCompleteReason::kNoVisiblePageFound, 1);
  }

  // Call at the end of the test if the observer is expected to wait for the
  // full failsafe timeout.
  void ExpectFailsafeTimeout() {
    task_env().FastForwardBy(AfterStartupTaskUtils::GetFailsafeTimeout());
    FlushTasks();
    EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
    histogram_tester_.ExpectUniqueSample(
        "Startup.BrowserStartupCompleteReason",
        StartupIsCompleteReason::kFailsafeTimeout, 1);
  }

  // Call as soon as the observer should detect that a visible page is loaded.
  void ExpectVisiblePageLoaded(
      StartupIsCompleteReason expected_reason =
          StartupIsCompleteReason::kVisiblePageLoadingFinished) {
    FlushTasks();
    EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
    histogram_tester_.ExpectUniqueSample("Startup.BrowserStartupCompleteReason",
                                         expected_reason, 1);
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    FeatureParams,
    StartupObserverTest,
    ::testing::Values(
        StartupObserverFeatureParams::kFeatureDisabled,
        StartupObserverFeatureParams::kFeatureEnabledIgnoreVisibleTabs,
        StartupObserverFeatureParams::kFeatureEnabledWaitForLoad,
        StartupObserverFeatureParams::kFeatureEnabledWaitForLoadOrTimeout));

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

TEST_F(AfterStartupTaskTest, IsStartupComplete) {
  // Check IsBrowserStartupComplete on a background sequence first to
  // verify that it does not allocate the underlying flag on that sequence.
  // That allocation sequence correctness part of this test relies on
  // the DCHECK in CancellationFlag::Set().
  EXPECT_FALSE(GetIsBrowserStartupCompleteFromBackgroundSequence());
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  EXPECT_TRUE(GetIsBrowserStartupCompleteFromBackgroundSequence());
}

TEST_F(AfterStartupTaskTest, PostTask) {
  // Nothing should be posted prior to startup completion.
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  AfterStartupTaskUtils::PostTask(
      FROM_HERE, ui_thread_,
      base::BindOnce(&AfterStartupTaskTest::VerifyExpectedSequence,
                     base::RetainedRef(ui_thread_)));
  AfterStartupTaskUtils::PostTask(
      FROM_HERE, background_sequence_,
      base::BindOnce(&AfterStartupTaskTest::VerifyExpectedSequence,
                     base::RetainedRef(background_sequence_)));
  PostAfterStartupTaskFromBackgroundSequence(
      FROM_HERE, ui_thread_,
      base::BindOnce(&AfterStartupTaskTest::VerifyExpectedSequence,
                     base::RetainedRef(ui_thread_)));
  PostAfterStartupTaskFromBackgroundSequence(
      FROM_HERE, background_sequence_,
      base::BindOnce(&AfterStartupTaskTest::VerifyExpectedSequence,
                     base::RetainedRef(background_sequence_)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, background_sequence_->total_task_count() +
                   ui_thread_->total_task_count());

  // Queued tasks should be posted upon setting the flag.
  AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  EXPECT_EQ(2, background_sequence_->posted_task_count());
  EXPECT_EQ(2, ui_thread_->posted_task_count());
  FlushBackgroundSequence();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, background_sequence_->ran_task_count());
  EXPECT_EQ(2, ui_thread_->ran_task_count());

  background_sequence_->reset_task_counts();
  ui_thread_->reset_task_counts();
  EXPECT_EQ(0, background_sequence_->total_task_count() +
                   ui_thread_->total_task_count());

  // Tasks posted after startup should get posted immediately.
  AfterStartupTaskUtils::PostTask(FROM_HERE, ui_thread_, base::DoNothing());
  AfterStartupTaskUtils::PostTask(FROM_HERE, background_sequence_,
                                  base::DoNothing());
  EXPECT_EQ(1, background_sequence_->posted_task_count());
  EXPECT_EQ(1, ui_thread_->posted_task_count());
  PostAfterStartupTaskFromBackgroundSequence(FROM_HERE, ui_thread_,
                                             base::DoNothing());
  PostAfterStartupTaskFromBackgroundSequence(FROM_HERE, background_sequence_,
                                             base::DoNothing());
  EXPECT_EQ(2, background_sequence_->posted_task_count());
  EXPECT_EQ(2, ui_thread_->posted_task_count());
  FlushBackgroundSequence();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, background_sequence_->ran_task_count());
  EXPECT_EQ(2, ui_thread_->ran_task_count());
}

#if !BUILDFLAG(IS_ANDROID)

TEST_F(AfterStartupTaskTest, StartupInProgressRef_NoRefs) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting();
  FlushUIThread();
  EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  histogram_tester.ExpectUniqueSample(
      "Startup.BrowserStartupCompleteReason",
      StartupIsCompleteReason::kStartupRegistrationDone, 1);
}

TEST_F(AfterStartupTaskTest, StartupInProgressRef_MultipleRefs) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  auto ref1 = AfterStartupTaskUtils::RegisterStartupInProgressRef(
      StartupIsCompleteReason::kFirstIdle);
  auto ref2 = AfterStartupTaskUtils::RegisterStartupInProgressRef(
      StartupIsCompleteReason::kSessionRestore);
  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting();
  FlushUIThread();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  ref2.reset();
  FlushUIThread();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  ref1.reset();
  FlushUIThread();
  EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  histogram_tester.ExpectUniqueSample("Startup.BrowserStartupCompleteReason",
                                      StartupIsCompleteReason::kFirstIdle, 1);
}

TEST_F(AfterStartupTaskTest, StartupInProgressRef_FailsafeTimeout) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  auto ref = AfterStartupTaskUtils::RegisterStartupInProgressRef(
      StartupIsCompleteReason::kFirstIdle);

  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting();
  FlushUIThread();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  task_environment_.FastForwardBy(AfterStartupTaskUtils::GetFailsafeTimeout());
  FlushUIThread();
  EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  histogram_tester.ExpectUniqueSample("Startup.BrowserStartupCompleteReason",
                                      StartupIsCompleteReason::kFailsafeTimeout,
                                      1);
}

TEST_F(AfterStartupTaskTest, StartupInProgressRef_ShutdownWithRef) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  auto ref = AfterStartupTaskUtils::RegisterStartupInProgressRef(
      StartupIsCompleteReason::kFirstIdle);
  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting();
  FlushUIThread();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());

  // Simulate browser shutdown: stop pumping tasks, then delete the global
  // BrowserProcess. The test will crash if SetBrowserStartupIsComplete() is
  // called after this point, as it tries to access the browser process.
  task_environment_.ShutdownBrowserTaskExecutor();
  TestingBrowserProcess::DeleteInstance();

  ref.reset();

  // FlushUIThread() will time out because tasks aren't being pumped. Instead
  // advance long enough that the timeout would fire. Anything that calls
  // SetBrowserStartupIsComplete() would execute before this.
  task_environment_.FastForwardBy(AfterStartupTaskUtils::GetFailsafeTimeout());
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  histogram_tester.ExpectTotalCount("Startup.BrowserStartupCompleteReason", 0);
}

TEST_P(StartupObserverTest, NoVisiblePages) {
  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting(graph());
  if (ExpectImmediateStartupComplete()) {
    return;
  }
  ExpectNoVisiblePage();
}

TEST_P(StartupObserverTest, VisibleTabLoads) {
  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting(graph());

  auto page_node = CreateNode<PageNodeImpl>();
  page_node->SetType(PageType::kTab);
  page_node->SetIsVisible(true);

  if (ExpectImmediateStartupComplete()) {
    return;
  }

  // Shouldn't timeout yet since a visible tab exists.
  task_env().FastForwardBy(kVisibleTabTimeout);
  FlushTasks();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());

  page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ExpectVisiblePageLoaded();
}

TEST_P(StartupObserverTest, VisibleNonTabLoads) {
  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting(graph());

  auto page_node = CreateNode<PageNodeImpl>();
  ASSERT_EQ(page_node->GetType(), PageType::kUnknown);
  page_node->SetIsVisible(true);

  if (ExpectImmediateStartupComplete()) {
    return;
  }

  task_env().FastForwardBy(kVisibleTabTimeout);
  FlushTasks();
  if (GetParam() != StartupObserverFeatureParams::kFeatureDisabled) {
    // Should timeout now since there's no visible tab.
    // Without the feature, StartupObserver monitors all pages, not just tabs.
    EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
    histogram_tester_.ExpectUniqueSample(
        "Startup.BrowserStartupCompleteReason",
        StartupIsCompleteReason::kNoVisiblePageFound, 1);
    return;
  }
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());

  page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ExpectVisiblePageLoaded();
}

TEST_P(StartupObserverTest, NonVisibleTabLoads) {
  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting(graph());

  auto page_node = CreateNode<PageNodeImpl>();
  page_node->SetType(PageType::kTab);
  ASSERT_FALSE(page_node->IsVisible());

  if (ExpectImmediateStartupComplete()) {
    return;
  }

  // Ignore the loading state because the page isn't visible.
  page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  FlushTasks();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  ExpectNoVisiblePage();
}

TEST_P(StartupObserverTest, VisibleTabTimesOut) {
  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting(graph());

  auto page_node = CreateNode<PageNodeImpl>();
  page_node->SetType(PageType::kTab);
  page_node->SetIsVisible(true);

  if (ExpectImmediateStartupComplete()) {
    return;
  }

  page_node->SetLoadingState(PageNode::LoadingState::kLoadingTimedOut);
  if (GetParam() == StartupObserverFeatureParams::kFeatureDisabled ||
      GetParam() ==
          StartupObserverFeatureParams::kFeatureEnabledWaitForLoadOrTimeout) {
    // Watching for the LoadingTimedOut state.
    ExpectVisiblePageLoaded(
        StartupIsCompleteReason::kVisiblePageLoadingTimedOut);
    return;
  }

  // Not watching for the kLoadingTimedOut state, so startup is not complete.
  FlushTasks();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  ExpectFailsafeTimeout();
}

TEST_P(StartupObserverTest, VisibleNonTabTimesOut) {
  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting(graph());

  auto page_node = CreateNode<PageNodeImpl>();
  ASSERT_EQ(page_node->GetType(), PageType::kUnknown);
  page_node->SetIsVisible(true);

  if (ExpectImmediateStartupComplete()) {
    return;
  }

  page_node->SetLoadingState(PageNode::LoadingState::kLoadingTimedOut);
  if (GetParam() == StartupObserverFeatureParams::kFeatureDisabled) {
    // Monitoring all pages, and watching for the LoadingTimedOut state.
    ExpectVisiblePageLoaded(
        StartupIsCompleteReason::kVisiblePageLoadingTimedOut);
    return;
  }

  // Only monitoring tabs, so startup isn't complete, even for
  // kFeatureEnabledWaitForLoadOrTimeout.
  FlushTasks();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  ExpectNoVisiblePage();
}

TEST_P(StartupObserverTest, NonVisibleTabTimesOut) {
  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting(graph());

  auto page_node = CreateNode<PageNodeImpl>();
  page_node->SetType(PageType::kTab);
  ASSERT_FALSE(page_node->IsVisible());

  if (ExpectImmediateStartupComplete()) {
    return;
  }

  // Ignore the loading state because the page isn't visible.
  page_node->SetLoadingState(PageNode::LoadingState::kLoadingTimedOut);
  FlushTasks();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());
  ExpectNoVisiblePage();
}

// Tests that make sure StartupObserver detects visible tabs no matter when they
// become visible.

TEST_P(StartupObserverTest, PageBecomesVisibleBeforeStart) {
  auto page_node = CreateNode<PageNodeImpl>();
  page_node->SetType(PageType::kTab);
  page_node->SetIsVisible(true);

  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting(graph());
  if (ExpectImmediateStartupComplete()) {
    return;
  }

  // Shouldn't timeout yet since a visible tab exists.
  task_env().FastForwardBy(kVisibleTabTimeout);
  FlushTasks();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());

  page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ExpectVisiblePageLoaded();
}

TEST_P(StartupObserverTest, PageBecomesVisibleAfterStart) {
  auto page_node = CreateNode<PageNodeImpl>();
  page_node->SetType(PageType::kTab);
  ASSERT_FALSE(page_node->IsVisible());

  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting(graph());
  if (ExpectImmediateStartupComplete()) {
    return;
  }

  page_node->SetIsVisible(true);

  // Shouldn't timeout yet since a visible tab exists.
  task_env().FastForwardBy(kVisibleTabTimeout);
  FlushTasks();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());

  page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ExpectVisiblePageLoaded();
}

TEST_P(StartupObserverTest, PageBecomesTabAfterStart) {
  auto page_node = CreateNode<PageNodeImpl>();
  ASSERT_EQ(page_node->GetType(), PageType::kUnknown);
  page_node->SetIsVisible(true);

  AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting(graph());
  if (ExpectImmediateStartupComplete()) {
    return;
  }

  page_node->SetType(PageType::kTab);

  // Shouldn't timeout yet since a visible tab exists.
  task_env().FastForwardBy(kVisibleTabTimeout);
  FlushTasks();
  EXPECT_FALSE(AfterStartupTaskUtils::IsBrowserStartupComplete());

  page_node->SetLoadingState(PageNode::LoadingState::kLoadedIdle);
  ExpectVisiblePageLoaded();
}

#endif  // !BUILDFLAG(IS_ANDROID)
