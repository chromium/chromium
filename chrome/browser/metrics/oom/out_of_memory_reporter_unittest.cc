// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/oom/out_of_memory_reporter.h"

#include <memory>
#include <set>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/common/chrome_descriptors.h"
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/child_process_crash_observer_android.h"
#endif

#if defined(OS_ANDROID)
// This class listens for notifications that crash dumps have been processed.
// Notifications will come from all crashes, even if an associated crash dump
// was not created.
class CrashDumpWaiter : public crash_reporter::CrashMetricsReporter::Observer {
 public:
  CrashDumpWaiter() {
    crash_reporter::CrashMetricsReporter::GetInstance()->AddObserver(this);
  }
  ~CrashDumpWaiter() {
    crash_reporter::CrashMetricsReporter::GetInstance()->RemoveObserver(this);
  }

  // Waits for the crash dump notification and returns whether the crash was
  // considered a foreground oom.
  const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet& Wait() {
    waiter_.Run();
    return reported_counts_;
  }

 private:
  // CrashDumpManager::Observer:
  void OnCrashDumpProcessed(
      int rph_id,
      const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet&
          reported_counts) override {
    reported_counts_ = reported_counts;
    waiter_.Quit();
  }

  base::RunLoop waiter_;
  crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet reported_counts_;
  DISALLOW_COPY_AND_ASSIGN(CrashDumpWaiter);
};
#endif  // defined(OS_ANDROID)

class OutOfMemoryReporterTest : public ChromeRenderViewHostTestHarness,
                                public OutOfMemoryReporter::Observer {
 public:
  OutOfMemoryReporterTest() {}
  ~OutOfMemoryReporterTest() override {}

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
#if defined(OS_ANDROID)
    crash_reporter::ChildExitObserver::Create();
    crash_reporter::ChildExitObserver::GetInstance()->RegisterClient(
        std::make_unique<crash_reporter::ChildProcessCrashObserver>());
#endif

    ChromeRenderViewHostTestHarness::SetUp();
    EXPECT_NE(content::ChildProcessHost::kInvalidUniqueID, process()->GetID());

    OutOfMemoryReporter::CreateForWebContents(web_contents());
    OutOfMemoryReporter* reporter =
        OutOfMemoryReporter::FromWebContents(web_contents());
    reporter->AddObserver(this);
    auto tick_clock = std::make_unique<base::SimpleTestTickClock>();
    test_tick_clock_ = tick_clock.get();
    reporter->SetTickClockForTest(std::move(tick_clock));
    // Ensure clock is set to something that's not 0 to begin.
    test_tick_clock_->Advance(base::TimeDelta::FromSeconds(1));

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  }

  // OutOfMemoryReporter::Observer:
  void OnForegroundOOMDetected(const GURL& url,
                               ukm::SourceId source_id) override {
    last_oom_url_ = url;
  }

  void SimulateRendererCreated() {
#if defined(OS_ANDROID)
    content::RenderProcessHostCreationObserver* creation_observer =
        crash_reporter::ChildExitObserver::GetInstance();
    creation_observer->OnRenderProcessHostCreated(process());
#endif
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_RENDERER_PROCESS_CREATED,
        content::Source<content::RenderProcessHost>(process()),
        content::NotificationService::NoDetails());
  }

  void SimulateOOM() {
    test_tick_clock_->Advance(base::TimeDelta::FromSeconds(3));
    SimulateRendererCreated();
#if defined(OS_ANDROID)
    process()->SimulateRenderProcessExit(base::TERMINATION_STATUS_OOM_PROTECTED,
                                         0);
#elif defined(OS_CHROMEOS)
    process()->SimulateRenderProcessExit(
        base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM, 0);
#else
    process()->SimulateRenderProcessExit(base::TERMINATION_STATUS_OOM, 0);
#endif
  }

  // Runs a closure which should simulate some sort of crash, and waits until
  // the OutOfMemoryReporter *should* have received a notification for it.
  void RunCrashClosureAndWait(base::OnceClosure crash_closure,
                              bool oom_expected) {
#if defined(OS_ANDROID)
    CrashDumpWaiter crash_waiter;
    std::move(crash_closure).Run();
    const auto& reported_counts = crash_waiter.Wait();
    EXPECT_EQ(oom_expected ? 1u : 0u,
              reported_counts.count(
                  crash_reporter::CrashMetricsReporter::ProcessedCrashCounts::
                      kRendererForegroundVisibleOom));

    // Since the observer list is not ordered, it isn't guaranteed that the
    // OutOfMemoryReporter will be notified at this point. Flush the current
    // message loop and task scheduler of tasks.
    content::RunAllTasksUntilIdle();
#else
    // No need to wait on non-android platforms. The message will be
    // synchronous.
    std::move(crash_closure).Run();
#endif
  }

  void SimulateOOMAndWait() {
    RunCrashClosureAndWait(base::BindOnce(&OutOfMemoryReporterTest::SimulateOOM,
                                          base::Unretained(this)),
                           true);
  }

  void CheckUkmMetricRecorded(const GURL& url, int64_t time_delta) {
    const auto& entries = test_ukm_recorder_->GetEntriesByName(
        ukm::builders::Tab_RendererOOM::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto* entry : entries) {
      test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, url);
      test_ukm_recorder_->ExpectEntryMetric(
          entry, ukm::builders::Tab_RendererOOM::kTimeSinceLastNavigationName,
          time_delta);
    }
  }

 protected:
  base::ShadowingAtExitManager at_exit_;

  base::Optional<GURL> last_oom_url_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;

 private:
  base::SimpleTestTickClock* test_tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(OutOfMemoryReporterTest);
};

TEST_F(OutOfMemoryReporterTest, SimpleOOM) {
  const GURL url("https://example.test/");
  NavigateAndCommit(url);

  SimulateOOMAndWait();
  EXPECT_TRUE(last_oom_url_.has_value());
  EXPECT_EQ(url, last_oom_url_.value());
  CheckUkmMetricRecorded(url, 3000);
}

TEST_F(OutOfMemoryReporterTest, NormalCrash_NoOOM) {
  const GURL url("https://example.test/");
  NavigateAndCommit(url);
  SimulateRendererCreated();
#if defined(OS_ANDROID)
  crash_reporter::ChildExitObserver::GetInstance()->ChildReceivedCrashSignal(
      process()->GetProcess().Handle(), SIGSEGV);
#endif
  RunCrashClosureAndWait(
      base::BindOnce(&content::MockRenderProcessHost::SimulateRenderProcessExit,
                     base::Unretained(process()),
                     base::TERMINATION_STATUS_PROCESS_WAS_KILLED, 0),
      false);
  EXPECT_FALSE(last_oom_url_.has_value());
  const auto& entries = test_ukm_recorder_->GetEntriesByName(
      ukm::builders::Tab_RendererOOM::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

TEST_F(OutOfMemoryReporterTest, SubframeNavigation_IsNotLogged) {
  const GURL url("https://example.test/");
  NavigateAndCommit(url);

  // Navigate a subframe, make sure it isn't the navigation that is logged.
  const GURL subframe_url("https://subframe.test/");
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      subframe_url, subframe);
  EXPECT_TRUE(subframe);

  SimulateOOMAndWait();
  EXPECT_TRUE(last_oom_url_.has_value());
  EXPECT_EQ(url, last_oom_url_.value());
  CheckUkmMetricRecorded(url, 3000);
}

TEST_F(OutOfMemoryReporterTest, OOMOnPreviousPage) {
  const GURL url1("https://example.test1/");
  const GURL url2("https://example.test2/");
  const GURL url3("https://example.test2/");
  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  // Should not commit.
  content::NavigationSimulator::NavigateAndFailFromBrowser(web_contents(), url3,
                                                           net::ERR_ABORTED);
  SimulateOOMAndWait();
  EXPECT_TRUE(last_oom_url_.has_value());
  EXPECT_EQ(url2, last_oom_url_.value());
  CheckUkmMetricRecorded(url2, 3000);

  last_oom_url_.reset();
  NavigateAndCommit(url1);

  // Should navigate to an error page.
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), url3, net::ERR_CONNECTION_RESET);
  // Don't report OOMs on error pages.
  SimulateOOMAndWait();
  EXPECT_FALSE(last_oom_url_.has_value());
  // Only the first OOM is recorded.
  CheckUkmMetricRecorded(url2, 3000);
}
