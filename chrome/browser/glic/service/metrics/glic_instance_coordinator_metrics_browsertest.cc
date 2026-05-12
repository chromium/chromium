// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_coordinator_metrics.h"

#include "base/test/run_until.h"
#include "build/build_config.h"
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
  instance2->Close(new_tab);

  // Wait for it to close.
  ASSERT_TRUE(base::test::RunUntil([&]() { return !instance2->IsShowing(); }));

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

}  // namespace
}  // namespace glic
