// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/phrase_segmentation/dependency_parser_model_loader.h"

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
#include "chrome/browser/accessibility/phrase_segmentation/dependency_parser_model_loader_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/accessibility/accessibility_features.h"

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

class DependencyParserModelLoaderDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  DependencyParserModelLoaderDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kReadAnythingReadAloudPhraseHighlighting);
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

  ~DependencyParserModelLoaderDisabledBrowserTest() override = default;

  const GURL& english_url() const { return english_url_; }

 private:
  GURL english_url_;
  std::unique_ptr<net::EmbeddedTestServer> origin_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DependencyParserModelLoaderDisabledBrowserTest,
                       DependencyParserModelLoaderDisabled) {
  EXPECT_FALSE(
      DependencyParserModelLoaderFactory::GetForProfile(browser()->profile()));
}

class DependencyParserModelLoaderBrowserTest
    : public DependencyParserModelLoaderDisabledBrowserTest {
 public:
  DependencyParserModelLoaderBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingReadAloud,
         features::kReadAnythingReadAloudAutomaticWordHighlighting,
         features::kReadAnythingReadAloudPhraseHighlighting},
        {});
  }

  void SetUp() override {
    origin_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    origin_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");
    origin_server_->RegisterRequestHandler(base::BindRepeating(
        &DependencyParserModelLoaderBrowserTest::RequestHandler,
        base::Unretained(this)));
    ASSERT_TRUE(origin_server_->Start());
    english_url_ = origin_server_->GetURL("/hello_world.html");
    InProcessBrowserTest::SetUp();
  }

  ~DependencyParserModelLoaderBrowserTest() override = default;

  DependencyParserModelLoader* dependency_parser_model_service() {
    return DependencyParserModelLoaderFactory::GetForProfile(
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
  base::FilePath model_file_path;
  EXPECT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &model_file_path));
  // TODO(b/339037155): Update this to a path that leads to an actual model.
  return model_file_path.AppendASCII(
      "chrome/test/data/accessibility/fixed_size_document.html");
}

IN_PROC_BROWSER_TEST_F(DependencyParserModelLoaderBrowserTest,
                       DependencyParserModelLoaderEnabled) {
  EXPECT_TRUE(dependency_parser_model_service());
}

IN_PROC_BROWSER_TEST_F(DependencyParserModelLoaderBrowserTest,
                       DependencyParserModelLoaderEnabled_OffTheRecord) {
  EXPECT_TRUE(DependencyParserModelLoaderFactory::GetForProfile(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

IN_PROC_BROWSER_TEST_F(DependencyParserModelLoaderBrowserTest,
                       DependencyParserModelReadyOnRequest) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(dependency_parser_model_service());

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_PHRASE_SEGMENTATION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "Accessibility.DependencyParserModelLoader.DependencyParserModel."
      "WasLoaded",
      1);
  histogram_tester.ExpectUniqueSample(
      "Accessibility.DependencyParserModelLoader.DependencyParserModel."
      "WasLoaded",
      true, 1);

  base::File model_file =
      dependency_parser_model_service()->GetDependencyParserModelFile();
  EXPECT_TRUE(model_file.IsValid());
}

IN_PROC_BROWSER_TEST_F(DependencyParserModelLoaderBrowserTest,
                       DependencyParserModelLoadedAfterRequest) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(dependency_parser_model_service());
  EXPECT_FALSE(dependency_parser_model_service()->IsModelAvailable());

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  dependency_parser_model_service()->NotifyOnModelFileAvailable(base::BindOnce(
      [](base::RunLoop* run_loop,
         DependencyParserModelLoader* dependency_parser_model_service,
         bool is_available) {
        EXPECT_TRUE(dependency_parser_model_service->IsModelAvailable());
        EXPECT_TRUE(is_available);
        run_loop->Quit();
      },
      run_loop.get(), dependency_parser_model_service()));

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_PHRASE_SEGMENTATION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "Accessibility.DependencyParserModelLoader.DependencyParserModel."
      "WasLoaded",
      1);
  histogram_tester.ExpectUniqueSample(
      "Accessibility.DependencyParserModelLoader.DependencyParserModel."
      "WasLoaded",
      true, 1);
  run_loop->Run();

  base::File model_file =
      dependency_parser_model_service()->GetDependencyParserModelFile();
  EXPECT_TRUE(model_file.IsValid());
}

IN_PROC_BROWSER_TEST_F(DependencyParserModelLoaderBrowserTest,
                       InvalidModelWhenLoading) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(dependency_parser_model_service());
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_PHRASE_SEGMENTATION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(
                  base::FilePath(optimization_guide::StringToFilePath(
                                     optimization_guide::kTestAbsoluteFilePath)
                                     .value()))
              .Build());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "Accessibility.DependencyParserModelLoader.DependencyParserModel."
      "WasLoaded",
      1);
  histogram_tester.ExpectUniqueSample(
      "Accessibility.DependencyParserModelLoader.DependencyParserModel."
      "WasLoaded",
      false, 1);
}

IN_PROC_BROWSER_TEST_F(DependencyParserModelLoaderBrowserTest,
                       ModelUpdateFromOptimizationGuide) {
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(dependency_parser_model_service());

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_PHRASE_SEGMENTATION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "Accessibility.DependencyParserModelLoader.DependencyParserModel."
      "WasLoaded",
      1);
  histogram_tester.ExpectUniqueSample(
      "Accessibility.DependencyParserModelLoader.DependencyParserModel."
      "WasLoaded",
      true, 1);

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_PHRASE_SEGMENTATION,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path())
              .Build());

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "Accessibility.DependencyParserModelLoader.DependencyParserModel."
      "WasLoaded",
      2);
  histogram_tester.ExpectUniqueSample(
      "Accessibility.DependencyParserModelLoader.DependencyParserModel."
      "WasLoaded",
      true, 2);

  base::File model_file =
      dependency_parser_model_service()->GetDependencyParserModelFile();
  EXPECT_TRUE(model_file.IsValid());
}

}  // namespace
