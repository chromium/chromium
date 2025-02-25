// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/language_detection/language_detection_model_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
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

namespace language_detection {
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
  for (const auto& bucket : buckets) {
    total += bucket.count;
  }

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
    if (total >= count) {
      return total;
    }

    content::FetchHistogramsFromChildProcesses();
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }
}

class LanguageDetectionModelServiceDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  LanguageDetectionModelServiceDisabledBrowserTest() {
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

  ~LanguageDetectionModelServiceDisabledBrowserTest() override = default;

  const GURL& english_url() const { return english_url_; }

 private:
  GURL english_url_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceDisabledBrowserTest,
                       LanguageDetectionModelServiceDisabled) {
  EXPECT_FALSE(LanguageDetectionModelServiceFactory::GetForProfile(
      browser()->profile()));
}

class LanguageDetectionModelServiceWithoutOptimizationGuideBrowserTest
    : public LanguageDetectionModelServiceDisabledBrowserTest {
 public:
  LanguageDetectionModelServiceWithoutOptimizationGuideBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {translate::kTFLiteLanguageDetectionEnabled},
        {optimization_guide::features::kOptimizationHints});
  }

  ~LanguageDetectionModelServiceWithoutOptimizationGuideBrowserTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This test confirms the translate model service is not available if
// the optimization guide does not exist.
IN_PROC_BROWSER_TEST_F(
    LanguageDetectionModelServiceWithoutOptimizationGuideBrowserTest,
    LanguageDetectionModelServiceEnabled) {
  EXPECT_FALSE(LanguageDetectionModelServiceFactory::GetForProfile(
      browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceDisabledBrowserTest,
                       LanguageDetectionModelNotCreated) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), english_url()));
  RetryForHistogramUntilCountReached(
      &histogram_tester, "Translate.CLD3.TopLanguageEvaluationDuration", 1);
  histogram_tester.ExpectTotalCount(
      "LanguageDetection.TFLiteModel.WasModelAvailableForDetection", 0);
}

// Makes requesting and waiting for the model file easy. This can only be used
// once.
class ModelFileGetter {
 public:
  explicit ModelFileGetter(
      LanguageDetectionModelService& language_detection_model_service)
      : language_detection_model_service_(language_detection_model_service) {}

  // Queues a request to get the model file. Do not call this again.
  void RequestModelFile() {
    CHECK_EQ(state_, State::kUnused);
    language_detection_model_service_->GetLanguageDetectionModelFile(
        base::BindOnce(
            [](ModelFileGetter* getter, base::File model_file) {
              getter->waiter_.OnEvent();
              getter->model_file_ = std::move(model_file);
            },
            base::Unretained(this)));
    state_ = State::kWaiting;
  }

  // Waits for the file and returns it if the request is satisfied. Returns
  // `nullopt` if the waiting is interrupted before the request is satisfied.
  // `RequestModelFile` must be called first.
  [[nodiscard]] std::optional<base::File> WaitForModelFile() {
    CHECK_EQ(state_, State::kWaiting);
    if (waiter_.Wait()) {
      state_ = State::kUsed;
      return std::move(model_file_);
    } else {
      // This should really only happen if the test times out.
      return std::nullopt;
    }
  }

  // Wraps requesting and waiting.
  [[nodiscard]] std::optional<base::File> RequestAndWaitForModelFile() {
    RequestModelFile();
    return WaitForModelFile();
  }

  // Returns whether a file has been received.
  bool HasFileBeenReceived() { return model_file_.has_value(); }

 private:
  raw_ref<LanguageDetectionModelService> language_detection_model_service_;
  content::WaiterHelper waiter_;
  enum class State {
    kUnused = 0,
    kWaiting = 1,
    kUsed = 2,
  };
  State state_ = State::kUnused;
  std::optional<base::File> model_file_;
};

class LanguageDetectionModelServiceBrowserTest
    : public LanguageDetectionModelServiceDisabledBrowserTest {
 public:
  LanguageDetectionModelServiceBrowserTest() {
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
    origin_server_->RegisterRequestHandler(base::BindRepeating(
        &LanguageDetectionModelServiceBrowserTest::RequestHandler,
        base::Unretained(this)));
    ASSERT_TRUE(origin_server_->Start());
    english_url_ = origin_server_->GetURL("/hello_world.html");
    InProcessBrowserTest::SetUp();
  }

  // Waits for the model file to be resolved. `nullopt` will be returned if the
  // waiting is interrupted, e.g. by test timeout.
  [[nodiscard]] std::optional<base::File> RequestAndWaitForModelFile() {
    ModelFileGetter getter(*language_detection_model_service());
    return getter.RequestAndWaitForModelFile();
  }

  ~LanguageDetectionModelServiceBrowserTest() override = default;

  LanguageDetectionModelService* language_detection_model_service() {
    return LanguageDetectionModelServiceFactory::GetForProfile(
        browser()->profile());
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
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  return source_root_dir.AppendASCII("components")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("translate")
      .AppendASCII("valid_model.tflite");
}

IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceBrowserTest,
                       LanguageDetectionModelServiceEnabled) {
  EXPECT_TRUE(language_detection_model_service());
}

IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceBrowserTest,
                       LanguageDetectionModelServiceEnabled_OffTheRecord) {
  EXPECT_TRUE(LanguageDetectionModelServiceFactory::GetForProfile(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceBrowserTest,
                       LanguageDetectionModelReadyOnRequest) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(language_detection_model_service());

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

  ASSERT_TRUE(RequestAndWaitForModelFile()->IsValid());
}

IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceBrowserTest,
                       LanguageDetectionModelLoadedAfterRequest) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(language_detection_model_service());

  ModelFileGetter getter(*language_detection_model_service());
  getter.RequestModelFile();
  ASSERT_FALSE(getter.HasFileBeenReceived());

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

  auto model_file = getter.WaitForModelFile();
  ASSERT_TRUE(model_file.has_value());
  EXPECT_TRUE(model_file->IsValid());
}

IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceBrowserTest,
                       InvalidModelWhenLoading) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(language_detection_model_service());
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

// TODO(crbug.com/40836720): Re-enable this test
IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceBrowserTest,
                       DISABLED_LanguageDetectionModelAvailableForDetection) {
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
// TODO(crbug.com/40904444): Re-enable this test
IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceBrowserTest,
                       DISABLED_LanguageDetectionWithBackgroundTab) {
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
      ui_test_utils::BROWSER_TEST_NO_WAIT);

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

IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceBrowserTest,
                       ModelUpdateFromOptimizationGuide) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(language_detection_model_service());

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

  ASSERT_TRUE(RequestAndWaitForModelFile()->IsValid());
}

// Test that the service correctly handles being notified that there is no
// longer a valid model available and also that it then handles a valid model
// becoming available.
IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceBrowserTest,
                       ModelUpdateFromOptimizationGuideMissingModelInfo) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(language_detection_model_service());

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

  ASSERT_TRUE(RequestAndWaitForModelFile()->IsValid());

  // Tell the service that there is no longer a model available.
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          nullptr);
  histogram_tester.ExpectUniqueSample(
      "TranslateModelService.LanguageDetectionModel.WasLoaded", true, 1);

  ASSERT_FALSE(RequestAndWaitForModelFile()->IsValid());

  // Tell the service that a model is available again.
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

  ASSERT_TRUE(RequestAndWaitForModelFile()->IsValid());
}

// Tests that we immediately reject requests if we exceed the allowed number of
// pending requests.
IN_PROC_BROWSER_TEST_F(LanguageDetectionModelServiceBrowserTest,
                       LimitPendingRequests) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(language_detection_model_service());

  ASSERT_GE(kMaxPendingRequestsAllowed, 1);

  // The intention is to queue `kMaxPendingRequestsAllowed` pending requests and
  // then one more that should fail immediately. However sometimes a request is
  // already queued due to the renderer process, so we queue 1-less than max,
  // expecting them all to remain pending and then be successfully fulfilled and
  // then one more which may or may not remain pending.
  // TODO(https://crbug.com/364504537): Make this a unittest to avoid this race
  // condition.
  std::vector<std::unique_ptr<ModelFileGetter>> getters;
  for (int i = 0; i < kMaxPendingRequestsAllowed - 1; i++) {
    getters.emplace_back(
        std::make_unique<ModelFileGetter>(*language_detection_model_service()));
    getters.back()->RequestModelFile();
  }

  // This one might exceed the max depending on whether we received a request
  // from the renderer, so we never check its status.
  ModelFileGetter maybe_getter(*language_detection_model_service());
  maybe_getter.RequestModelFile();

  // Requesting one more should definitely give an invalid file immediately.
  ASSERT_FALSE(RequestAndWaitForModelFile()->IsValid());
  // The first `kMaxPendingRequestsAllowed - 1` pending ones should still be
  // pending.
  for (auto& getter_good : getters) {
    ASSERT_FALSE(getter_good->HasFileBeenReceived());
  }

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_LANGUAGE_DETECTION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());

  // The first `kMaxPendingRequestsAllowed` should get a valid file now.
  for (auto& getter_good : getters) {
    ASSERT_TRUE(getter_good->WaitForModelFile()->IsValid());
  }

  // Requesting one more now should give a valid file because the queue has been
  // emptied.
  ASSERT_TRUE(RequestAndWaitForModelFile()->IsValid());
}

}  // namespace
}  // namespace language_detection
