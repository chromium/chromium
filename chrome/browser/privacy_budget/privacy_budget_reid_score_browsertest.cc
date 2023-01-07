// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "chrome/browser/privacy_budget/privacy_budget_browsertest_util.h"
#include "chrome/browser/privacy_budget/privacy_budget_ukm_entry_filter.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace {

using testing::IsSupersetOf;
using testing::Key;

class EnableReidEstimation {
 public:
  EnableReidEstimation() {
    test::ScopedPrivacyBudgetConfig::Parameters parameters;

    parameters.reid_blocks = {
        {blink::IdentifiableSurface::FromTypeAndToken(
             blink::IdentifiableSurface::Type::kWebFeature,
             blink::mojom::WebFeature::kNavigatorUserAgent),
         blink::IdentifiableSurface::FromTypeAndToken(
             blink::IdentifiableSurface::Type::kWebFeature,
             blink::mojom::WebFeature::kNavigatorLanguage)}};

    parameters.reid_salts_ranges = {1};
    parameters.reid_bits = {1};
    parameters.reid_noise = {0.01};

    scoped_config_.Apply(parameters);
  }

 private:
  test::ScopedPrivacyBudgetConfig scoped_config_;
};

class PrivacyBudgetReidScoreBrowserTest : private EnableReidEstimation,
                                          public PlatformBrowserTest {};

class PrivacyBudgetReidScoreBrowserTestWithTestRecorder
    : private EnableReidEstimation,
      public PrivacyBudgetBrowserTestBaseWithTestRecorder {};

}  // namespace

IN_PROC_BROWSER_TEST_F(PrivacyBudgetReidScoreBrowserTest, LoadsAGroup) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));

  const auto* settings = blink::IdentifiabilityStudySettings::Get();
  ASSERT_TRUE(settings->IsActive());
}

IN_PROC_BROWSER_TEST_F(PrivacyBudgetReidScoreBrowserTestWithTestRecorder,
                       ReidHashIsReported) {
  blink::IdentifiabilityStudySettings::ResetStateForTesting();
  auto study_state = std::make_unique<IdentifiabilityStudyState>(
      g_browser_process->local_state());
  auto filter =
      std::make_unique<PrivacyBudgetUkmEntryFilter>(study_state.get());
  recorder().SetEntryFilter(std::move(filter));
  ASSERT_TRUE(embedded_test_server()->Start());
  content::DOMMessageQueue messages(web_contents());
  base::RunLoop run_loop;

  recorder().SetOnAddEntryCallback(ukm::builders::Identifiability::kEntryName,
                                   BarrierClosure(4u, run_loop.QuitClosure()));

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL("/privacy_budget/calls_user_agent.html")));

  // The document calls the user agent and language apis and replies with done.
  // Receipt of the message indicates that the script successfully completed.
  std::string reply;
  ASSERT_TRUE(messages.WaitForMessage(&reply));
  // Navigating away from the test page causes the document to be unloaded. That
  // will cause any buffered metrics to be flushed.
  content::NavigateToURLBlockUntilNavigationsComplete(web_contents(),
                                                      GURL("about:blank"), 1);
  // Wait for the metrics to come down the pipe.
  run_loop.Run();

  auto merged_entries = recorder().GetMergedEntriesByName(
      ukm::builders::Identifiability::kEntryName);

  // Calculate the reid surface key manually.
  constexpr auto kReidScoreType =
      blink::IdentifiableSurface::Type::kReidScoreEstimator;
  auto surface_1 = blink::IdentifiableSurface::FromTypeAndToken(
      blink::IdentifiableSurface::Type::kWebFeature,
      blink::mojom::WebFeature::kNavigatorUserAgent);
  auto surface_2 = blink::IdentifiableSurface::FromTypeAndToken(
      blink::IdentifiableSurface::Type::kWebFeature,
      blink::mojom::WebFeature::kNavigatorLanguage);
  std::vector<uint64_t> tokens{surface_1.ToUkmMetricHash(),
                               surface_2.ToUkmMetricHash()};
  auto expected_surface = blink::IdentifiableSurface::FromTypeAndToken(
      kReidScoreType, base::make_span(tokens));

  // Merge all entries for comparison in the next step.
  base::flat_map<uint64_t, int64_t> metrics;

  for (auto& it : merged_entries) {
    metrics.insert(it.second->metrics.begin(), it.second->metrics.end());
  }

  EXPECT_THAT(metrics, IsSupersetOf({
                           Key(expected_surface.ToUkmMetricHash()),
                       }));
}

namespace {

// Test class that allows to enable UKM recording.
class PrivacyBudgetReidScoreBrowserTestWithUkmRecording
    : private EnableReidEstimation,
      public PrivacyBudgetBrowserTestBaseWithUkmRecording {};

}  // namespace

IN_PROC_BROWSER_TEST_F(PrivacyBudgetReidScoreBrowserTestWithUkmRecording,
                       ReidIsReported) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));
  ASSERT_TRUE(EnableUkmRecording());

  constexpr blink::IdentifiableToken kDummyToken = 1;
  constexpr blink::IdentifiableSurface kUserAgentSurface =
      blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kWebFeature,
          blink::mojom::WebFeature::kNavigatorUserAgent);
  constexpr blink::IdentifiableSurface kLanguageSurface =
      blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kWebFeature,
          blink::mojom::WebFeature::kNavigatorLanguage);

  auto* ukm_recorder = ukm::UkmRecorder::Get();

  blink::IdentifiabilityMetricBuilder(ukm::UkmRecorder::GetNewSourceID())
      .Add(kUserAgentSurface, kDummyToken)
      .Add(kLanguageSurface, kDummyToken)
      .Record(ukm_recorder);

  blink::IdentifiabilitySampleCollector::Get()->Flush(ukm_recorder);

  // Wait for the metrics to come down the pipe.
  content::RunAllTasksUntilIdle();

  ukm::UkmTestHelper ukm_test_helper(ukm_service());
  ukm_test_helper.BuildAndStoreLog();
  std::unique_ptr<ukm::Report> ukm_report = ukm_test_helper.GetUkmReport();
  ASSERT_TRUE(ukm_test_helper.HasUnsentLogs());
  ASSERT_TRUE(ukm_report);
  ASSERT_NE(ukm_report->entries_size(), 0);

  std::map<uint64_t, int64_t> seen_metrics;
  for (const auto& entry : ukm_report->entries()) {
    ASSERT_TRUE(entry.has_event_hash());
    if (entry.event_hash() != ukm::builders::Identifiability::kEntryNameHash) {
      continue;
    }
    for (const auto& metric : entry.metrics()) {
      ASSERT_TRUE(metric.has_metric_hash());
      ASSERT_TRUE(metric.has_value());
      seen_metrics.insert({metric.metric_hash(), metric.value()});
    }
  }

  // Calculate the reid surface key manually.
  constexpr auto kReidScoreType =
      blink::IdentifiableSurface::Type::kReidScoreEstimator;
  std::vector<uint64_t> tokens{kUserAgentSurface.ToUkmMetricHash(),
                               kLanguageSurface.ToUkmMetricHash()};
  auto expected_surface = blink::IdentifiableSurface::FromTypeAndToken(
      kReidScoreType, base::make_span(tokens));

  EXPECT_THAT(seen_metrics, IsSupersetOf({
                                Key(expected_surface.ToUkmMetricHash()),
                            }));
}
