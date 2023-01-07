// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/translate/translate_model_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/translate/core/common/translate_util.h"
#include "components/translate/core/language_detection/language_detection_model.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {
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

class TranslateModelServiceDisabledBrowserTest : public InProcessBrowserTest {
 public:
  TranslateModelServiceDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        translate::kTFLiteLanguageDetectionEnabled);
  }

  void SetUp() override {
    origin_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    origin_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");

    ASSERT_TRUE(origin_server_->Start());
    english_url_ = origin_server_->GetURL("/hello_world.html");
    InProcessBrowserTest::SetUp();
  }

  ~TranslateModelServiceDisabledBrowserTest() override = default;

  const GURL& english_url() const { return english_url_; }

 private:
  GURL english_url_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TranslateModelServiceDisabledBrowserTest,
                       TranslateModelServiceDisabled) {
  EXPECT_FALSE(
      TranslateModelServiceFactory::GetForProfile(browser()->profile()));
}

class TranslateModelServiceWithoutOptimizationGuideBrowserTest
    : public TranslateModelServiceDisabledBrowserTest {
 public:
  TranslateModelServiceWithoutOptimizationGuideBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {translate::kTFLiteLanguageDetectionEnabled},
        {optimization_guide::features::kOptimizationHints});
  }

  ~TranslateModelServiceWithoutOptimizationGuideBrowserTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test confirms the translate model service is not available if
// the optimization guide does not exist.
IN_PROC_BROWSER_TEST_F(TranslateModelServiceWithoutOptimizationGuideBrowserTest,
                       TranslateModelServiceEnabled) {
  EXPECT_FALSE(
      TranslateModelServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceDisabledBrowserTest,
                       LanguageDetectionModelNotCreated) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), english_url()));
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Translate.CLD3.TopLanguageEvaluationDuration", 1);
  histogram_tester.ExpectTotalCount(
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", 0);
}

class TranslateModelServiceBrowserTest
    : public TranslateModelServiceDisabledBrowserTest {
 public:
  TranslateModelServiceBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {translate::kTFLiteLanguageDetectionEnabled,
         optimization_guide::features::kOptimizationHints,
         optimization_guide::features::kRemoteOptimizationGuideFetching},
        {});
  }

  void SetUp() override {
    origin_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    origin_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");
    origin_server_->RegisterRequestHandler(
        base::BindRepeating(&TranslateModelServiceBrowserTest::RequestHandler,
                            base::Unretained(this)));
    ASSERT_TRUE(origin_server_->Start());
    english_url_ = origin_server_->GetURL("/hello_world.html");
    InProcessBrowserTest::SetUp();
  }

  ~TranslateModelServiceBrowserTest() override = default;

  translate::TranslateModelService* translate_model_service() {
    return TranslateModelServiceFactory::GetForProfile(browser()->profile());
  }

  const GURL& english_url() const { return english_url_; }

 private:
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    std::string path_value;

    // This script is render blocking in the HTML, but is intentionally slow.
    // This provides important time between commit and first layout for model
    // requests to make it to the renderer, reducing flakes.
    if (request.GetURL().path() == "/slow-first-layout.js") {
      std::unique_ptr<net::test_server::DelayedHttpResponse> resp =
          std::make_unique<net::test_server::DelayedHttpResponse>(
              base::Milliseconds(500));
      resp->set_code(net::HTTP_OK);
      resp->set_content_type("application/javascript");
      resp->set_content(std::string());
      return resp;
    }

    return nullptr;
  }
  base::test::ScopedFeatureList scoped_feature_list_;
  GURL english_url_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
};

base::FilePath model_file_path() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("translate")
      .AppendASCII("valid_model.tflite");
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       TranslateModelServiceEnabled) {
  EXPECT_TRUE(translate_model_service());
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       TranslateModelServiceEnabled_OffTheRecord) {
  EXPECT_TRUE(TranslateModelServiceFactory::GetForProfile(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       LanguageDetectionModelReadyOnRequest) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(translate_model_service());

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "TranslateModelService.LanguageDetectionModel.WasLoaded", 1);
  histogram_tester.ExpectUniqueSample(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", true, 1);

  base::File model_file =
      translate_model_service()->GetLanguageDetectionModelFile();
  EXPECT_TRUE(model_file.IsValid());
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       LanguageDetectionModelLoadedAfterRequest) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(translate_model_service());
  EXPECT_FALSE(translate_model_service()->IsModelAvailable());

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  translate_model_service()->NotifyOnModelFileAvailable(base::BindOnce(
      [](base::RunLoop* run_loop,
         TranslateModelService* translate_model_service, bool is_available) {
        EXPECT_TRUE(translate_model_service->IsModelAvailable());
        EXPECT_TRUE(is_available);
        run_loop->Quit();
      },
      run_loop.get(), translate_model_service()));

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "TranslateModelService.LanguageDetectionModel.WasLoaded", 1);
  histogram_tester.ExpectUniqueSample(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", true, 1);
  run_loop->Run();

  base::File model_file =
      translate_model_service()->GetLanguageDetectionModelFile();
  EXPECT_TRUE(model_file.IsValid());
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       InvalidModelWhenLoading) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(translate_model_service());
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(
                  base::FilePath(optimization_guide::StringToFilePath(
                                     optimization_guide::kTestAbsoluteFilePath)
                                     .value()))
              .Build());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "TranslateModelService.LanguageDetectionModel.WasLoaded", 1);
  histogram_tester.ExpectUniqueSample(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", false, 1);
}

// TODO(crbug.com/1320359): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_LanguageDetectionModelAvailableForDetection \
  DISABLED_LanguageDetectionModelAvailableForDetection
#else
#define MAYBE_LanguageDetectionModelAvailableForDetection \
  LanguageDetectionModelAvailableForDetection
#endif
IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       MAYBE_LanguageDetectionModelAvailableForDetection) {
  base::HistogramTester histogram_tester;
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "TranslateModelService.LanguageDetectionModel.WasLoaded", 1);
  histogram_tester.ExpectUniqueSample(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", true, 1);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), english_url(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", 1);
  histogram_tester.ExpectUniqueSample(
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", true, 1);
}

// Disabled on linux+ASAN, macOS+ASAN, chromeOS+ASAN and windows due to high
// failure rate: crbug.com/1199854 crbug.com/1297485.
#if ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)) && \
     defined(ADDRESS_SANITIZER)) ||                                          \
    BUILDFLAG(IS_WIN)
#define MAYBE_LanguageDetectionWithBackgroundTab \
  DISABLED_LanguageDetectionWithBackgroundTab
#else
#define MAYBE_LanguageDetectionWithBackgroundTab \
  LanguageDetectionWithBackgroundTab
#endif
IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       MAYBE_LanguageDetectionWithBackgroundTab) {
  base::HistogramTester histogram_tester;
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "TranslateModelService.LanguageDetectionModel.WasLoaded", 1);
  histogram_tester.ExpectUniqueSample(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", true, 1);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), english_url(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // Opening the browser causes the first model deferral event. The second
  // is due to the background tab.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "LanguageDetection.TFLiteModel.WasModelRequestDeferred", 2);
  histogram_tester.ExpectBucketCount(
      "LanguageDetection.TFLiteModel.WasModelRequestDeferred", true, 2);

  // Make the background tab the active tab.
  browser()->tab_strip_model()->SelectNextTab();

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", 1);
  histogram_tester.ExpectBucketCount(
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", true, 1);
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       ModelUpdateFromOptimizationGuide) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(translate_model_service());

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "TranslateModelService.LanguageDetectionModel.WasLoaded", 1);
  histogram_tester.ExpectUniqueSample(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", true, 1);

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "TranslateModelService.LanguageDetectionModel.WasLoaded", 2);
  histogram_tester.ExpectUniqueSample(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", true, 2);

  base::File model_file =
      translate_model_service()->GetLanguageDetectionModelFile();
  EXPECT_TRUE(model_file.IsValid());
}

}  // namespace
}  // namespace translate
