// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_coordinator_metrics.h"

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_histogram_tester.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace glic {
namespace {

class GlicInstanceCoordinatorMetricsBrowserTest : public GlicBrowserTest {
 public:
  GlicInstanceCoordinatorMetricsBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorMetricsBrowserTest,
                       InitialState) {
  EXPECT_EQ(coordinator().GetVisibleInstanceCount(), 0);
}

// Detach is not supported on Android, and multiple windows/tabs don't work
// yet in Android browsertests, preventing concurrent visibility testing.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ConcurrentVisibility DISABLED_ConcurrentVisibility
#else
#define MAYBE_ConcurrentVisibility ConcurrentVisibility
#endif
IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorMetricsBrowserTest,
                       MAYBE_ConcurrentVisibility) {
  GlicHistogramTester histogram_tester;

  // 1. Open Glic for active tab and detach it (making it floating).
  ASSERT_OK(OpenGlicForActiveTabAndDetach());

  // 2. Create a new tab to allow opening another side panel instance.
  auto* new_tab = CreateAndActivateTab(GetSimpleTestUrl());

  // 3. Open Glic for the new active tab (side panel).
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance2, OpenGlicForActiveTab());

  // 4. Close one instance to end concurrent visibility.
  PreventDeletionOnClose(instance2);
  ASSERT_OK(CloseGlicForTabAndWait(new_tab));

  // Verify histograms.
  histogram_tester.ExpectUniqueSample("Glic.ConcurrentVisibility.PeakCount", 2,
                                      1);
  histogram_tester.ExpectTotalCount("Glic.ConcurrentVisibility.Duration", 1);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorMetricsBrowserTest,
                       MemoryPressure) {
  GlicHistogramTester histogram_tester;

  // Open at least one instance so we have processes to measure.
  ASSERT_OK(OpenGlicForActiveTab());

  // Simulate memory pressure.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Verify that memory histograms are recorded.
  // We use ExpectTotalCount because the actual values depend on the
  // environment.
  histogram_tester.ExpectTotalCount(
      "Glic.Instance.AvgWebUIPrivateMemoryFootprint.CriticalPressure", 1);
  histogram_tester.ExpectTotalCount(
      "Glic.Instance.AvgClientPrivateMemoryFootprint.CriticalPressure", 1);
  histogram_tester.ExpectTotalCount(
      "Glic.Instance.AvgTotalPrivateMemoryFootprint.CriticalPressure", 1);
  histogram_tester.ExpectTotalCount(
      "Glic.Instance.TotalPrivateMemoryFootprint.CriticalPressure", 1);
}

class GlicInstanceCoordinatorMetricsPeriodicTest : public GlicBrowserTest {
 public:
  GlicInstanceCoordinatorMetricsPeriodicTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicRecordMemoryFootprintMetrics, {{"period", "1s"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorMetricsPeriodicTest,
                       PeriodicMemoryMetrics) {
  GlicHistogramTester histogram_tester;

  // Open at least one instance so we have processes to measure.
  ASSERT_OK(OpenGlicForActiveTab());

  // Wait for periodic recording to happen at least twice.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetTotalCount(
               "Glic.Instance.AvgWebUIPrivateMemoryFootprint.Periodic") >= 2;
  }));

  // Verify that memory histograms are recorded.
  EXPECT_GE(histogram_tester.GetTotalCount(
                "Glic.Instance.AvgWebUIPrivateMemoryFootprint.Periodic"),
            2);
  EXPECT_GE(histogram_tester.GetTotalCount(
                "Glic.Instance.AvgClientPrivateMemoryFootprint.Periodic"),
            2);
  EXPECT_GE(histogram_tester.GetTotalCount(
                "Glic.Instance.AvgTotalPrivateMemoryFootprint.Periodic"),
            2);
  EXPECT_GE(histogram_tester.GetTotalCount(
                "Glic.Instance.TotalPrivateMemoryFootprint.Periodic"),
            2);
}

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorMetricsBrowserTest,
                       SwitchConversation) {
  GlicHistogramTester histogram_tester;

  ASSERT_OK(OpenGlicForActiveTab());
  GlicInstanceImpl* instance = GetOnlyGlicInstance();

  auto info = mojom::ConversationInfo::New();
  info->conversation_id = "test_conversation";
  info->conversation_title = "Test Conversation";

  auto* tab = GetTabListInterface()->GetActiveTab();
  coordinator().SwitchConversation(*instance,
                                   ShowOptions{SidePanelShowOptions{*tab}},
                                   std::move(info), base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "Glic.Interaction.SwitchConversationTarget",
      GlicSwitchConversationTarget::kSwitchedToNewInstance, 1);
}

class GlicInstanceCoordinatorMetricsWarmingTest : public GlicBrowserTest {
 public:
  GlicInstanceCoordinatorMetricsWarmingTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicWebContentsWarming,
        {{features::kGlicWebContentsWarmingDelay.name, "0ms"}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicInstanceCoordinatorMetricsWarmingTest,
                       MemoryPressureWithOnlyWarmedInstance) {
  GlicHistogramTester histogram_tester;

  // 1. Ensure preload (warmed instance exists) and wait for it to load.
  coordinator().GetWebContentsWarmingPoolForTesting().EnsurePreload();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return coordinator()
               .GetWebContentsWarmingPoolForTesting()
               .GetWarmedContainerForTesting() != nullptr;
  }));

  content::WebContents* web_contents =
      coordinator()
          .GetWebContentsWarmingPoolForTesting()
          .GetWarmedWebContents();
  ASSERT_TRUE(web_contents);
  content::WaitForLoadStop(web_contents);

  // 2. Simulate memory pressure.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // 3. Verify that memory histograms are recorded and values are > 0 where
  // expected. Client memory might be 0 if the guest hasn't loaded yet for the
  // warmed instance.

  histogram_tester.ExpectSampleValueGreaterThan(
      "Glic.Instance.AvgWebUIPrivateMemoryFootprint.CriticalPressure", 0);

  histogram_tester.ExpectTotalCount(
      "Glic.Instance.AvgClientPrivateMemoryFootprint.CriticalPressure", 1);

  histogram_tester.ExpectSampleValueGreaterThan(
      "Glic.Instance.AvgTotalPrivateMemoryFootprint.CriticalPressure", 0);

  histogram_tester.ExpectSampleValueGreaterThan(
      "Glic.Instance.TotalPrivateMemoryFootprint.CriticalPressure", 0);
}

}  // namespace
}  // namespace glic
