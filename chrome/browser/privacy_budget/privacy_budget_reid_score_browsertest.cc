// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
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

class PrivacyBudgetReidScoreBrowserTest : public PlatformBrowserTest {
 public:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  PrivacyBudgetReidScoreBrowserTest() {
    test::ScopedPrivacyBudgetConfig::Parameters parameters;

    parameters.reid_blocks = {
        {blink::IdentifiableSurface::FromTypeAndToken(
             blink::IdentifiableSurface::Type::kWebFeature,
             blink::mojom::WebFeature::kNavigatorUserAgent),
         blink::IdentifiableSurface::FromTypeAndToken(
             blink::IdentifiableSurface::Type::kWebFeature,
             blink::mojom::WebFeature::kNavigatorLanguage)}};

    parameters.reid_salts_ranges = {1};

    scoped_config_.Apply(parameters);
  }

  void SetUpOnMainThread() override {
    // Do an initial empty navigation then create the recorder to make sure we
    // start on a clean slate. This clears the platform differences in between
    // Android and Desktop.
    content::NavigateToURLBlockUntilNavigationsComplete(web_contents(),
                                                        GURL("about:blank"), 1);

    // Ensure that the actively sampled surfaces reported at browser startup go
    // through before we set up the test recorder.
    content::RunAllTasksUntilIdle();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  ukm::TestUkmRecorder& recorder() { return *ukm_recorder_; }

 private:
  test::ScopedPrivacyBudgetConfig scoped_config_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PrivacyBudgetReidScoreBrowserTest, LoadsAGroup) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));

  const auto* settings = blink::IdentifiabilityStudySettings::Get();
  ASSERT_TRUE(settings->IsActive());
}

IN_PROC_BROWSER_TEST_F(PrivacyBudgetReidScoreBrowserTest, ReidHashIsReported) {
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
  content::RunAllTasksUntilIdle();
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
  std::vector<blink::IdentifiableToken> tokens{surface_1.GetInputHash(),
                                               surface_2.GetInputHash()};
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
