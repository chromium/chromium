// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/threading_features.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace {

using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Key;

// Tests validating that the HangWatcher is correctly started.
//
// The HangWatcher is started by code under src/content, in
// content_main_runner_impl.cc for un-sandboxed processes, or by
// renderer_main.cc, utility_main.cc or gpu_main.cc for sandboxed processes.
// `HangWatcher::InitializeOnMainThread()` however is only called for chrome in
// ChromeMainDelegate::CommonEarlyInitialization. Thus we can't test whether
// HangWatcher works from a unit test under src/content.
class ChromeMainDelegateHangWatcherTest : public InProcessBrowserTest {
 public:
  ChromeMainDelegateHangWatcherTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{base::kEnableHangWatcher,
          {{base::kHangWatcherMonitoringPeriodParam, "1ms"}}},
         {base::kEnableHangWatcherOnGpuProcess, {}}},
        {});
  }

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Tests that UncoveredStartupTime histograms are recorded for all watched
// sandboxed processes.
// TODO(crbug.com/431802107): Re-enable this test once the failure is fixed.
#if defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
#define MAYBE_RecordsUncoveredTime DISABLED_RecordsUncoveredTime
#else
#define MAYBE_RecordsUncoveredTime RecordsUncoveredTime
#endif
IN_PROC_BROWSER_TEST_F(ChromeMainDelegateHangWatcherTest,
                       MAYBE_RecordsUncoveredTime) {
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  EXPECT_THAT(
      histogram_tester_.GetAllSamplesForPrefix("HangWatcher"),
      IsSupersetOf({Key("HangWatcher.GpuProcess.UncoveredStartupTime"),
                    Key("HangWatcher.RendererProcess.UncoveredStartupTime"),
                    Key("HangWatcher.UtilityProcess.UncoveredStartupTime")}));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Tests that HangWatcher monitors hung threads for all watched process types.
IN_PROC_BROWSER_TEST_F(ChromeMainDelegateHangWatcherTest, MonitorsProcess) {
  absl::flat_hash_set<std::string> histograms_left_to_observe = {
      "HangWatcher.IsThreadHung.BrowserProcess.UIThread.Normal",
      "HangWatcher.IsThreadHung.GpuProcess.MainThread",
      "HangWatcher.IsThreadHung.UtilityProcess.MainThread",
      "HangWatcher.IsThreadHung.RendererProcess.MainThread",
  };

  // Wait for the hang watcher to start monitoring all processes, or abort after
  // a timeout.
  base::ElapsedTimer timer;
  while (!histograms_left_to_observe.empty() &&
         timer.Elapsed() < TestTimeouts::action_max_timeout()) {
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

    absl::erase_if(histograms_left_to_observe,
                   [this](const std::string& histogram) {
                     return !histogram_tester_.GetAllSamples(histogram).empty();
                   });
  }

  EXPECT_THAT(histograms_left_to_observe, IsEmpty());
}

}  // namespace
