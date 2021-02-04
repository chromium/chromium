// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tflite_experiment/tflite_experiment_keyed_service.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tflite_experiment/tflite_experiment_keyed_service_factory.h"
#include "chrome/browser/tflite_experiment/tflite_experiment_switches.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kNavigationURL[] = "https://google.com";

namespace {
// Fetch and calculate the total number of samples from all the bins for
// |histogram_name|. Note: from some browertests run, there might be two
// profiles created, and this will return the total sample count across
// profiles.
int GetTotalHistogramSamples(const base::HistogramTester& histogram_tester,
                             const std::string& histogram_name) {
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples(histogram_name);
  int total = 0;
  for (const auto& bucket : buckets)
    total += bucket.count;

  return total;
}

// Retries fetching |histogram_name| until it contains at least |count| samples.
int RetryForHistogramUntilCountReached(
    const base::HistogramTester& histogram_tester,
    const std::string& histogram_name,
    int count) {
  int total = 0;
  while (true) {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    total = GetTotalHistogramSamples(histogram_tester, histogram_name);
    if (total >= count)
      return total;
    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}
}  // namespace

class TFLiteExperimentKeyedServiceDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  TFLiteExperimentKeyedServiceDisabledBrowserTest() = default;
  ~TFLiteExperimentKeyedServiceDisabledBrowserTest() override = default;

  void WaitForTFLiteObserverToCallNullTFLitePredictor() {
    EXPECT_GT(RetryForHistogramUntilCountReached(
                  histogram_tester_,
                  "TFLiteExperiment.Observer.TFLitePredictor.Null", 1),
              0);
  }

  const base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(TFLiteExperimentKeyedServiceDisabledBrowserTest,
                       TFLiteExperimentEnabledButTFLitePredictorDisabled) {
  auto* tflite_experiment_keyed_service =
      TFLiteExperimentKeyedServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(tflite_experiment_keyed_service);
  EXPECT_FALSE(tflite_experiment_keyed_service->tflite_predictor());
}

class TFLiteExperimentKeyedServiceBrowserTest : public InProcessBrowserTest {
 public:
  TFLiteExperimentKeyedServiceBrowserTest() = default;
  ~TFLiteExperimentKeyedServiceBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    base::FilePath model_file_path;

    EXPECT_TRUE(
        base::PathService::Get(base::DIR_SOURCE_ROOT, &model_file_path));

    model_file_path = model_file_path.Append(FILE_PATH_LITERAL("components"))
                          .Append(FILE_PATH_LITERAL("test"))
                          .Append(FILE_PATH_LITERAL("data"))
                          .Append(FILE_PATH_LITERAL("optimization_guide"))
                          .Append(FILE_PATH_LITERAL("simple_test.tflite"));
    cmd->AppendSwitchASCII(tflite_experiment::switches::kTFLiteModelPath,
                           model_file_path.MaybeAsASCII());

    // Set TFLite experiment log path.
    cmd->AppendSwitchASCII(
        tflite_experiment::switches::kTFLiteExperimentLogPath,
        GetTFLiteExperimentLogPath().MaybeAsASCII());
  }

  base::FilePath GetTFLiteExperimentLogPath() {
    base::FilePath g_test_data_directory;
    base::PathService::Get(chrome::DIR_TEST_DATA, &g_test_data_directory);
    g_test_data_directory = g_test_data_directory.Append(
        FILE_PATH_LITERAL("tflite_experiment.log"));
    return g_test_data_directory;
  }

  void WaitForObserverToFinish() {
    EXPECT_GT(RetryForHistogramUntilCountReached(
                  histogram_tester_, "TFLiteExperiment.Observer.Finish", 1),
              0);
  }

  void WaitForTFLitePredictorToBeReEvaluated() {
    EXPECT_GT(
        RetryForHistogramUntilCountReached(
            histogram_tester_,
            "TFLiteExperiment.Observer.TFLitePredictor.EvaluationRequested", 1),
        0);
  }

  const base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(TFLiteExperimentKeyedServiceBrowserTest,
                       TFLiteExperimentEnabledWithKeyedService) {
  EXPECT_TRUE(
      TFLiteExperimentKeyedServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(TFLiteExperimentKeyedServiceBrowserTest,
                       TFLiteExperimentPredictorCreated) {
  auto* tflite_experiment_keyed_service =
      TFLiteExperimentKeyedServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(tflite_experiment_keyed_service);
  EXPECT_TRUE(tflite_experiment_keyed_service->tflite_predictor());
}

IN_PROC_BROWSER_TEST_F(TFLiteExperimentKeyedServiceBrowserTest,
                       TFLiteExperimentPredictorEvaluated) {
  GURL navigation_url(kNavigationURL);
  ui_test_utils::NavigateToURL(browser(), navigation_url);
  WaitForTFLitePredictorToBeReEvaluated();
}

IN_PROC_BROWSER_TEST_F(TFLiteExperimentKeyedServiceBrowserTest,
                       TFLiteExperimentLog) {
  GURL navigation_url(kNavigationURL);
  ui_test_utils::NavigateToURL(browser(), navigation_url);
  WaitForObserverToFinish();

  std::string data;
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ReadFileToString(GetTFLiteExperimentLogPath(), &data);
  base::Optional<base::Value> root = base::JSONReader::Read(data);
  EXPECT_TRUE(root);
  EXPECT_TRUE(root->FindIntKey("input_set_time_ms"));
  EXPECT_TRUE(root->FindIntKey("evaluation_time_ms"));
}

IN_PROC_BROWSER_TEST_F(TFLiteExperimentKeyedServiceBrowserTest,
                       TFLiteExperimentHistogram) {
  GURL navigation_url(kNavigationURL);
  ui_test_utils::NavigateToURL(browser(), navigation_url);
  WaitForObserverToFinish();

  histogram_tester()->ExpectTotalCount(
      "TFLiteExperiment.Observer.TFLitePredictor.InputSetTime", 1);
  histogram_tester()->ExpectTotalCount(
      "TFLiteExperiment.Observer.TFLitePredictor.EvaluationTime", 1);
  histogram_tester()->ExpectUniqueSample("TFLiteExperiment.Observer.Finish",
                                         true, 1);
}
