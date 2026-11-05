// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/metrics_provider_common.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/performance_manager/test_support/fake_frame_throttling_delegate.h"
#include "chrome/browser/performance_manager/test_support/test_user_performance_tuning_manager_environment.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include <vector>

#include "base/byte_size.h"
#include "base/process/process_metrics.h"
#endif

class PerformanceManagerMetricsProviderCommonTest : public testing::Test {
 protected:
  performance_manager::MetricsProviderCommon* provider() {
    return provider_.get();
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  void SetUp() override {
    provider_ = std::make_unique<performance_manager::MetricsProviderCommon>();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<performance_manager::MetricsProviderCommon> provider_;
};

TEST_F(PerformanceManagerMetricsProviderCommonTest, A11yModeOff) {
  base::HistogramTester tester;
  provider()->ProvideCurrentSessionData(nullptr);
  tester.ExpectUniqueSample(
      "PerformanceManager.Experimental.HasAccessibilityModeFlag", false, 1);
}

#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
// TODO(crbug.com/534240517): Fix this test on Win ARM64.
#define MAYBE_A11yModeOn DISABLED_A11yModeOn
#else
#define MAYBE_A11yModeOn A11yModeOn
#endif
TEST_F(PerformanceManagerMetricsProviderCommonTest, MAYBE_A11yModeOn) {
  content::ScopedAccessibilityModeOverride scoped_setter(
      ui::AXMode::kWebContents);

  base::HistogramTester tester;
  provider()->ProvideCurrentSessionData(nullptr);
  tester.ExpectUniqueSample(
      "PerformanceManager.Experimental.HasAccessibilityModeFlag", true, 1);
  tester.ExpectUniqueSample(
      "PerformanceManager.Experimental.AccessibilityModeFlag",
      ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS, 1);
}

TEST_F(PerformanceManagerMetricsProviderCommonTest, MultipleA11yModeFlags) {
  content::ScopedAccessibilityModeOverride scoped_setter(
      ui::AXMode::kWebContents | ui::AXMode::kHTML);

  base::HistogramTester tester;
  provider()->ProvideCurrentSessionData(nullptr);
  tester.ExpectUniqueSample(
      "PerformanceManager.Experimental.HasAccessibilityModeFlag", true, 1);
  // Each mode flag gets recorded in its own bucket
  tester.ExpectBucketCount(
      "PerformanceManager.Experimental.AccessibilityModeFlag",
      ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_WEB_CONTENTS, 1);
  tester.ExpectBucketCount(
      "PerformanceManager.Experimental.AccessibilityModeFlag",
      ui::AXMode::ModeFlagHistogramValue::UMA_AX_MODE_HTML, 1);
}

#if BUILDFLAG(IS_MAC)
TEST_F(PerformanceManagerMetricsProviderCommonTest, MacSystemMemoryMetrics) {
  base::HistogramTester tester;
  // The provider's constructor starts a 2 minute repeating timer that records
  // the memory histograms; advance mock time to fire it once.
  task_environment().FastForwardBy(base::Minutes(2));

  // Values are live machine readings, so only check that each histogram was
  // emitted once and that percentages stay within range.
  static constexpr const char* kSizeHistograms[] = {
      "Memory.Experimental.MacFileBackedMemoryMB2",
      "Memory.Experimental.MacFreeMemoryMB",
      "Memory.Experimental.MacSpeculativeMemoryMB",
      "Memory.Experimental.MacPurgeableMemoryMB",
      "Memory.Experimental.MacWiredMemoryMB",
      "Memory.Experimental.MacUnwiredMemoryMB",
      "Memory.Experimental.MacCompressorMemoryMB",
      "Memory.Experimental.MacCompressorUncompressedMemoryMB",
      "Memory.Experimental.MacCompressorSwappedMemoryMB",
  };
  for (const char* name : kSizeHistograms) {
    tester.ExpectTotalCount(name, 1);
  }

  static constexpr const char* kPercentHistograms[] = {
      "Memory.Experimental.MacFileBackedMemoryPercent",
      "Memory.Experimental.MacFreeMemoryPercent",
      "Memory.Experimental.MacWiredMemoryPercent",
      "Memory.Experimental.MacCompressorPercentOfTotal",
      "Memory.Experimental.MacCompressorPercentOfUnwired",
  };
  for (const char* name : kPercentHistograms) {
    const auto samples = tester.GetAllSamples(name);
    ASSERT_EQ(samples.size(), 1u) << name;
    EXPECT_GE(samples[0].min, 0) << name;
    EXPECT_LE(samples[0].min, 100) << name;
  }

  static constexpr const char* kChurnRateHistograms[] = {
      "Memory.Experimental.MacCompressionRateKBPerSecond",
      "Memory.Experimental.MacDecompressionRateKBPerSecond",
      "Memory.Experimental.MacPageInRateKBPerSecond",
      "Memory.Experimental.MacPageOutRateKBPerSecond",
      "Memory.Experimental.MacPurgeRateKBPerSecond",
      "Memory.Experimental.MacReactivationRateKBPerSecond",
      "Memory.Experimental.MacSwapInRateKBPerSecond",
      "Memory.Experimental.MacSwapOutRateKBPerSecond",
  };
  // The first tick only captures the baseline counters, so churn rates are not
  // emitted until the second tick.
  for (const char* name : kChurnRateHistograms) {
    tester.ExpectTotalCount(name, 0);
  }

  task_environment().FastForwardBy(base::Minutes(2));
  for (const char* name : kSizeHistograms) {
    tester.ExpectTotalCount(name, 2);
  }
  for (const char* name : kChurnRateHistograms) {
    const std::vector<base::Bucket> samples = tester.GetAllSamples(name);
    ASSERT_EQ(samples.size(), 1u) << name;
    EXPECT_GE(samples[0].min, 0) << name;
  }

  // MacSwap{Used,Total}MB are not asserted on: they depend on the vm.swapusage
  // sysctl succeeding.
}

TEST_F(PerformanceManagerMetricsProviderCommonTest,
       MacSystemMemoryChurnRateCalculation) {
  base::SystemMemoryInfo prev;
  prev.reactivations = base::KiB(100);
  prev.pageins = base::KiB(100);
  prev.pageouts = base::KiB(100);
  prev.purges = base::KiB(100);
  prev.compressions = base::KiB(100);
  prev.decompressions = base::KiB(100);
  prev.swapins = base::KiB(100);
  prev.swapouts = base::KiB(100);

  base::SystemMemoryInfo cur = prev;
  cur.reactivations += base::KiB(100);    // 10 KiB/s over 10s
  cur.pageins += base::KiB(200);          // 20 KiB/s
  cur.pageouts += base::KiB(400);         // 40 KiB/s
  cur.purges += base::KiB(800);           // 80 KiB/s
  cur.compressions += base::KiB(1600);    // 160 KiB/s
  cur.decompressions += base::KiB(3200);  // 320 KiB/s
  cur.swapins += base::KiB(6400);         // 640 KiB/s
  cur.swapouts += base::KiB(12800);       // 1280 KiB/s

  base::HistogramTester tester;
  // Non-positive elapsed time emits nothing.
  performance_manager::RecordMacSystemMemoryChurnMetrics(prev, cur,
                                                         base::TimeDelta());
  tester.ExpectTotalCount("Memory.Experimental.MacReactivationRateKBPerSecond",
                          0);

  performance_manager::RecordMacSystemMemoryChurnMetrics(prev, cur,
                                                         base::Seconds(10));
  // Reversed snapshots (cur < prev) emit no additional samples.
  performance_manager::RecordMacSystemMemoryChurnMetrics(cur, prev,
                                                         base::Seconds(10));

  tester.ExpectUniqueSample(
      "Memory.Experimental.MacReactivationRateKBPerSecond", 10, 1);
  tester.ExpectUniqueSample("Memory.Experimental.MacPageInRateKBPerSecond", 20,
                            1);
  tester.ExpectUniqueSample("Memory.Experimental.MacPageOutRateKBPerSecond", 40,
                            1);
  tester.ExpectUniqueSample("Memory.Experimental.MacPurgeRateKBPerSecond", 80,
                            1);
  tester.ExpectUniqueSample("Memory.Experimental.MacCompressionRateKBPerSecond",
                            160, 1);
  tester.ExpectUniqueSample(
      "Memory.Experimental.MacDecompressionRateKBPerSecond", 320, 1);
  tester.ExpectUniqueSample("Memory.Experimental.MacSwapInRateKBPerSecond", 640,
                            1);
  tester.ExpectUniqueSample("Memory.Experimental.MacSwapOutRateKBPerSecond",
                            1280, 1);
}
#endif  // BUILDFLAG(IS_MAC)
