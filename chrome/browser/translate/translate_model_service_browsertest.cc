// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_model_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/translate/core/common/translate_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run (such as chromeos) there
// might be two profiles created, and this will return the total sample count
// across profiles.
int GetTotalHistogramSamples(const base::HistogramTester* histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester->GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count) {
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    base::RunLoop().RunUntilIdle();

    int total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return total;

    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

}  // namespace

using TranslateModelServiceDisabledBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(TranslateModelServiceDisabledBrowserTest,
                       TranslateModelServiceDisabled) {
  EXPECT_FALSE(
      TranslateModelServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceDisabledBrowserTest,
                       LanguageDetectionModelNotCreated) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), GURL("https://test.com"));
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Translate.CLD3.TopLanguageEvaluationDuration", 1);
  histogram_tester.ExpectTotalCount(
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", 0);
}

class TranslateModelServiceBrowserTest
    : public TranslateModelServiceDisabledBrowserTest {
 public:
  TranslateModelServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        translate::kTFLiteLanguageDetectionEnabled);
  }

  ~TranslateModelServiceBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       TranslateModelServiceEnabled) {
  EXPECT_TRUE(
      TranslateModelServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       TranslateModelServiceEnabled_OffTheRecord) {
  EXPECT_TRUE(TranslateModelServiceFactory::GetForProfile(
      browser()->profile()->GetPrimaryOTRProfile()));
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       LanguageDetectionModelCreated) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), GURL("https://test.com"));
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", 1);
  histogram_tester.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", false, 1);
}
