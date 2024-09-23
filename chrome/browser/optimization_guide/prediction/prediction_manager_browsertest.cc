// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/component_updater/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/prediction_manager.h"
#include "components/optimization_guide/core/prediction_model_download_manager.h"
#include "components/optimization_guide/core/store_update_data.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/variations/hashing.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/base/ip_address.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

namespace {

constexpr int kSuccessfulModelVersion = 123;

// Timeout to allow the model file to be downloaded, unzipped and sent to the
// model file observers.
constexpr base::TimeDelta kModelFileDownloadTimeout = base::Seconds(60);

enum class PredictionModelsFetcherRemoteResponseType {
  kSuccessfulWithValidModelFile = 0,
  kSuccessfulWithInvalidModelFile = 1,
  kSuccessfulWithValidModelFileAndInvalidAdditionalFiles = 2,
  kSuccessfulWithValidModelFileAndValidAdditionalFiles = 3,
  kSuccessfulWithNoModelUpdate = 4,
  kSuccessfulWithNullModel = 5,
  kUnsuccessful = 6,
};

}  // namespace

namespace optimization_guide {

namespace {

// Sets up the |model_file_observer| to receive valid ModelInfo.
void SetUpValidModelInfoReceival(ModelFileObserver* model_file_observer,
                                 base::RunLoop* run_loop,
                                 const std::set<base::FilePath::StringType>&
                                     expected_additional_files = {}) {
  model_file_observer->set_model_file_received_callback(base::BindOnce(
      [](base::RunLoop* run_loop,
         const std::set<base::FilePath::StringType>& expected_additional_files,
         proto::OptimizationTarget optimization_target,
         base::optional_ref<const ModelInfo> model_info) {
        base::ScopedAllowBlockingForTesting scoped_allow_blocking;

        EXPECT_EQ(optimization_target,
                  proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
        EXPECT_TRUE(model_info.has_value());

        EXPECT_EQ(123, model_info->GetVersion());
        EXPECT_TRUE(model_info->GetModelFilePath().IsAbsolute());
        EXPECT_TRUE(base::PathExists(model_info->GetModelFilePath()));

        EXPECT_EQ(expected_additional_files.size(),
                  model_info->GetAdditionalFiles().size());
        for (const base::FilePath& add_file :
             model_info->GetAdditionalFiles()) {
          EXPECT_TRUE(add_file.IsAbsolute());
          EXPECT_TRUE(base::PathExists(add_file));
          EXPECT_TRUE(base::Contains(expected_additional_files,
                                     add_file.BaseName().value()));
        }
        run_loop->Quit();
      },
      run_loop, expected_additional_files));
}

// Sets up the |model_file_observer| to not receive any model.
void SetUpNoModelInfoReceival(ModelFileObserver* model_file_observer) {
  model_file_observer->set_model_file_received_callback(
      base::BindOnce([](proto::OptimizationTarget optimization_target,
                        base::optional_ref<const ModelInfo> model_info) {
        FAIL() << "Should not be called";
      }));
}
}  // namespace

// Abstract base class for browser testing Prediction Manager.
// Actual class fixtures should implement InitializeFeatureList to set up
// features used in tests.
class PredictionManagerBrowserTestBase : public InProcessBrowserTest {
 public:
  PredictionManagerBrowserTestBase() = default;
  ~PredictionManagerBrowserTestBase() override = default;

  PredictionManagerBrowserTestBase(const PredictionManagerBrowserTestBase&) =
      delete;
  PredictionManagerBrowserTestBase& operator=(
      const PredictionManagerBrowserTestBase&) = delete;

  void SetUp() override {
    InitializeFeatureList();

    models_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    net::EmbeddedTestServer::ServerCertificateConfig models_server_cert_config;
    models_server_cert_config.dns_names = {
        GURL(kOptimizationGuideServiceGetModelsDefaultURL).host()};
    models_server_cert_config.ip_addresses = {net::IPAddress::IPv4Localhost()};
    models_server_->SetSSLConfig(models_server_cert_config);
    models_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");
    models_server_->RegisterRequestHandler(base::BindRepeating(
        &PredictionManagerBrowserTestBase::HandleGetModelsRequest,
        base::Unretained(this)));

    ASSERT_TRUE(models_server_->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());
    https_url_with_content_ = https_server_->GetURL("/english_page.html");
    https_url_without_content_ = https_server_->GetURL("/empty.html");
    model_file_url_ = models_server_->GetURL("/signed_valid_model.crx3");
    model_file_with_good_additional_file_url_ =
        models_server_->GetURL("/additional_file_exists.crx3");
    model_file_with_nonexistent_additional_file_url_ =
        models_server_->GetURL("/additional_file_doesnt_exist.crx3");

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(models_server_->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(switches::kGoogleApiKeyConfigurationCheckOverride);
    cmd->AppendSwitchASCII(
        switches::kOptimizationGuideServiceGetModelsURL,
        models_server_
            ->GetURL(GURL(kOptimizationGuideServiceGetModelsDefaultURL).host(),
                     "/")
            .spec());
    cmd->AppendSwitchASCII("force-variation-ids", "4");
  }

  void SetResponseType(
      PredictionModelsFetcherRemoteResponseType response_type) {
    response_type_ = response_type;
  }

  void RegisterWithKeyedService(ModelFileObserver* model_file_observer) {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->AddObserverForOptimizationTargetModel(
            optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            std::nullopt, model_file_observer);
  }

  PredictionManager* GetPredictionManager() {
    OptimizationGuideKeyedService* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    return optimization_guide_keyed_service->GetPredictionManager();
  }

 protected:
  // Virtualize for testing different feature configurations.
  virtual void InitializeFeatureList() = 0;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleGetModelsRequest(
      const net::test_server::HttpRequest& request) {
    // Returning nullptr will cause the test server to fallback to serving the
    // file from the test data directory.
    if (request.GetURL() == model_file_url_)
      return nullptr;
    if (request.GetURL() == model_file_with_good_additional_file_url_)
      return nullptr;
    if (request.GetURL() == model_file_with_nonexistent_additional_file_url_)
      return nullptr;

    std::unique_ptr<net::test_server::BasicHttpResponse> response;

    response = std::make_unique<net::test_server::BasicHttpResponse>();
    // The request to the remote Optimization Guide Service should always be a
    // POST.
    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    EXPECT_NE(request.headers.end(), request.headers.find("X-Client-Data"));
    optimization_guide::proto::GetModelsRequest models_request;
    EXPECT_TRUE(models_request.ParseFromString(request.content));

    response->set_code(net::HTTP_OK);
    std::unique_ptr<optimization_guide::proto::GetModelsResponse>
        get_models_response = BuildGetModelsResponse();
    if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                              kSuccessfulWithInvalidModelFile) {
      get_models_response->mutable_models(0)->mutable_model()->set_download_url(
          https_url_with_content_.spec());
    } else if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                                     kSuccessfulWithValidModelFile) {
      get_models_response->mutable_models(0)->mutable_model()->set_download_url(
          model_file_url_.spec());
    } else if (response_type_ ==
               PredictionModelsFetcherRemoteResponseType::
                   kSuccessfulWithValidModelFileAndInvalidAdditionalFiles) {
      get_models_response->mutable_models(0)->mutable_model()->set_download_url(
          model_file_with_nonexistent_additional_file_url_.spec());
    } else if (response_type_ ==
               PredictionModelsFetcherRemoteResponseType::
                   kSuccessfulWithValidModelFileAndValidAdditionalFiles) {
      get_models_response->mutable_models(0)->mutable_model()->set_download_url(
          model_file_with_good_additional_file_url_.spec());
    } else if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                                     kSuccessfulWithNoModelUpdate) {
      // This simulates the server sending PredictionModel with no Model in it,
      // which indicates the client is up-to-date. No model download should
      // happen in that case.
      get_models_response->mutable_models(0)->clear_model();
    } else if (response_type_ == PredictionModelsFetcherRemoteResponseType::
                                     kSuccessfulWithNullModel) {
      // This simulates the server sending no PredictionModel in the response,
      // which indicates the server had stopped serving the opt target. In that
      // case model should be deleted from the store, and the observers notified
      // of null model.
      get_models_response->clear_models();
    } else if (response_type_ ==
               PredictionModelsFetcherRemoteResponseType::kUnsuccessful) {
      response->set_code(net::HTTP_NOT_FOUND);
    }

    std::string serialized_response;
    get_models_response->SerializeToString(&serialized_response);
    response->set_content(serialized_response);
    return std::move(response);
  }

  GURL model_file_url_;
  GURL model_file_with_good_additional_file_url_;
  GURL model_file_with_nonexistent_additional_file_url_;
  GURL https_url_with_content_, https_url_without_content_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> models_server_;
  PredictionModelsFetcherRemoteResponseType response_type_ =
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile;
};

class PredictionManagerBrowserTest : public PredictionManagerBrowserTestBase {
 public:
  PredictionManagerBrowserTest() = default;
  ~PredictionManagerBrowserTest() override = default;

  PredictionManagerBrowserTest(const PredictionManagerBrowserTest&) = delete;
  PredictionManagerBrowserTest& operator=(const PredictionManagerBrowserTest&) =
      delete;

 private:
  void InitializeFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {optimization_guide::features::kOptimizationHints, {}},
        {optimization_guide::features::kRemoteOptimizationGuideFetching, {}},
        {optimization_guide::features::kOptimizationTargetPrediction,
         {{"fetch_startup_delay_ms", "8000"}}},
    };
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }
};

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest,
                       ComponentUpdatesPrefDisabled) {
  ModelFileObserver model_file_observer;
  SetResponseType(PredictionModelsFetcherRemoteResponseType::kUnsuccessful);
  g_browser_process->local_state()->SetBoolean(
      ::prefs::kComponentUpdatesEnabled, false);
  base::HistogramTester histogram_tester;

  RegisterWithKeyedService(&model_file_observer);

  base::RunLoop().RunUntilIdle();

  // Should not have made fetch request.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status."
      "PainfulPageLoad",
      0);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest,
                       ModelsAndFeaturesStoreInitialized) {
  ModelFileObserver model_file_observer;
  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile);
  base::HistogramTester histogram_tester;

  RegisterWithKeyedService(&model_file_observer);
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerBrowserTest,
                       PredictionModelFetchFailed) {
  ModelFileObserver model_file_observer;
  SetResponseType(PredictionModelsFetcherRemoteResponseType::kUnsuccessful);
  base::HistogramTester histogram_tester;

  RegisterWithKeyedService(&model_file_observer);

  // Wait until histograms have been updated before performing checks for
  // correct behavior based on the response.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status",
      net::HTTP_NOT_FOUND, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status."
      "PainfulPageLoad",
      net::HTTP_NOT_FOUND, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
}

class PredictionManagerModelDownloadingBrowserTest
    : public PredictionManagerBrowserTest {
 public:
  PredictionManagerModelDownloadingBrowserTest() = default;
  ~PredictionManagerModelDownloadingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    model_file_observer_ = std::make_unique<ModelFileObserver>();

    PredictionManagerBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PredictionManagerBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
#endif
    // TODO(crbug.com/40285326): This fails with the field trial testing config.
    command_line->AppendSwitch("disable-field-trial-config");
  }

  void TearDownOnMainThread() override {
    PredictionManagerBrowserTest::TearDownOnMainThread();
  }

  ModelFileObserver* model_file_observer() {
    return model_file_observer_.get();
  }

  void RegisterModelFileObserverWithKeyedService(Profile* profile = nullptr) {
    OptimizationGuideKeyedServiceFactory::GetForProfile(
        profile ? profile : browser()->profile())
        ->AddObserverForOptimizationTargetModel(
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            /*model_metadata=*/std::nullopt, model_file_observer_.get());
  }

 private:
  void InitializeFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kOptimizationHints, {}},
        {features::kRemoteOptimizationGuideFetching, {}},
        {features::kOptimizationTargetPrediction, {}},
        {features::kOptimizationGuideModelDownloading,
         {{"unrestricted_model_downloading", "true"}}},
    };
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  std::unique_ptr<ModelFileObserver> model_file_observer_;
};

// Flaky on various bots. See https://crbug.com/1266318
IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       DISABLED_TestIncognitoUsesModelFromRegularProfile) {
  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile);

  // Set up model download with regular profile.
  {
    base::HistogramTester histogram_tester;

    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    SetUpValidModelInfoReceival(model_file_observer(), run_loop.get());
    RegisterModelFileObserverWithKeyedService();

    // Wait until the observer receives the file. We increase the timeout to 60
    // seconds here since the file is on the larger side.
    {
      base::test::ScopedRunLoopTimeout file_download_timeout(
          FROM_HERE, kModelFileDownloadTimeout);
      run_loop->Run();
    }

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
        PredictionModelDownloadStatus::kSuccess, 1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
        kSuccessfulModelVersion, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad",
        kSuccessfulModelVersion, 1);
  }

  // Now set up model download with incognito profile. Download should not
  // happen, but the OnModelUpdated callback should be triggered.
  {
    base::HistogramTester otr_histogram_tester;
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    SetUpValidModelInfoReceival(model_file_observer(), run_loop.get());
    Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
    RegisterModelFileObserverWithKeyedService(otr_browser->profile());

    run_loop->Run();

    otr_histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 0);
    otr_histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  }
}

// TODO(crbug.com/336399137): Flaky on Linux Chromium OS ASan LSan Tests.
#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(ADDRESS_SANITIZER)
#define MAYBE_TestIncognitoDoesntFetchModels \
  DISABLED_TestIncognitoDoesntFetchModels
#else
#define MAYBE_TestIncognitoDoesntFetchModels TestIncognitoDoesntFetchModels
#endif
IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       MAYBE_TestIncognitoDoesntFetchModels) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithInvalidModelFile);

  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());

  // Registering should not initiate the fetch and the model updated callback
  // should not be triggered too.
  RegisterModelFileObserverWithKeyedService(otr_browser->profile());

  SetUpNoModelInfoReceival(model_file_observer());
  RetryForHistogramUntilCountReached(
      &histogram_tester, "OptimizationGuide.PredictionManager.StoreInitialized",
      1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       TestDownloadUrlAcceptedByDownloadServiceButInvalid) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithInvalidModelFile);

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded.
  RegisterModelFileObserverWithKeyedService();

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.State.PainfulPageLoad",
      2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionModelDownloadManager.State.PainfulPageLoad",
      PredictionModelDownloadManager::PredictionModelDownloadState::kRequested,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PredictionModelDownloadManager.State.PainfulPageLoad",
      PredictionModelDownloadManager::PredictionModelDownloadState::kStarted,
      1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStartLatency."
      "PainfulPageLoad",
      1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kFailedCrxVerification, 1);
  // An unverified file should not notify us that it's ready.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       TestSuccessfulModelFileFlow) {
  base::HistogramTester histogram_tester;

  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  model_file_observer()->set_model_file_received_callback(base::BindOnce(
      [](base::RunLoop* run_loop, proto::OptimizationTarget optimization_target,
         base::optional_ref<const ModelInfo> model_info) {
        EXPECT_EQ(optimization_target,
                  proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
        EXPECT_TRUE(model_info.has_value());
        run_loop->Quit();
      },
      run_loop.get()));

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded.
  RegisterModelFileObserverWithKeyedService();

  // Wait until the observer receives the file. We increase the timeout to 60
  // seconds here since the file is on the larger side.
  {
    base::test::ScopedRunLoopTimeout file_download_timeout(
        FROM_HERE, kModelFileDownloadTimeout);
    run_loop->Run();
  }

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);

  // No error when moving the file so there will be no record.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.ReplaceFileError", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("OptimizationGuide.PredictionManager."
                                     "ModelDeliveryEvents.PainfulPageLoad"),
      testing::UnorderedElementsAre(
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ModelDeliveryEvent::kGetModelsRequest),
                       1),
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ModelDeliveryEvent::kDownloadServiceRequest),
                       1),
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ModelDeliveryEvent::kModelDownloadStarted),
                       1),
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ModelDeliveryEvent::kModelDownloaded),
                       1),
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ModelDeliveryEvent::kModelDelivered),
                       1)));
}

IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       TestSuccessfulModelFileFlowWithAdditionalFile) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithValidModelFileAndValidAdditionalFiles);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetUpValidModelInfoReceival(model_file_observer(), run_loop.get(),
                              {FILE_PATH_LITERAL("good_additional_file.txt")});

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded.
  RegisterModelFileObserverWithKeyedService();

  // Wait until the observer receives the file. We increase the timeout to 60
  // seconds here since the file is on the larger side.
  {
    base::test::ScopedRunLoopTimeout file_download_timeout(
        FROM_HERE, kModelFileDownloadTimeout);
    run_loop->Run();
  }

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);

  // No error when moving the file so there will be no record.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.ReplaceFileError", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("OptimizationGuide.PredictionManager."
                                     "ModelDeliveryEvents.PainfulPageLoad"),
      testing::UnorderedElementsAre(
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ModelDeliveryEvent::kGetModelsRequest),
                       1),
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ModelDeliveryEvent::kDownloadServiceRequest),
                       1),
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ModelDeliveryEvent::kModelDownloadStarted),
                       1),
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ModelDeliveryEvent::kModelDownloaded),
                       1),
          base::Bucket(static_cast<base::HistogramBase::Sample>(
                           ModelDeliveryEvent::kModelDelivered),
                       1)));
}

IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       TestSuccessfulModelFileFlowWithInvalidAdditionalFile) {
  base::HistogramTester histogram_tester;

  SetResponseType(PredictionModelsFetcherRemoteResponseType::
                      kSuccessfulWithValidModelFileAndInvalidAdditionalFiles);

  // Since the model's additional file is invalid, model observer callback
  // should never be run.
  SetUpNoModelInfoReceival(model_file_observer());

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded.
  RegisterModelFileObserverWithKeyedService();

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 1);
  base::RunLoop().RunUntilIdle();

  // The additional file does not exist.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kFailedInvalidAdditionalFile, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 0);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       TestModelHasNoUpdateFlow) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile);
  SetUpValidModelInfoReceival(model_file_observer(), run_loop.get());

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded.
  RegisterModelFileObserverWithKeyedService();

  // Wait until the observer receives the file. We increase the timeout to 60
  // seconds here since the file is on the larger side.
  {
    base::test::ScopedRunLoopTimeout file_download_timeout(
        FROM_HERE, kModelFileDownloadTimeout);
    run_loop->Run();
  }

  // Model will be downloaded and loaded.
  RetryForHistogramUntilCountReached(
      histogram_tester.get(),
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);

  // Set up the next periodic model fetch to not send any model updates.
  histogram_tester = std::make_unique<base::HistogramTester>();
  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithNoModelUpdate);
  // Since the model was already downloaded and present in the store, model
  // observer callback should never be run.
  SetUpNoModelInfoReceival(model_file_observer());

  // Trigger the periodic fetch timer.
  auto* prediction_model_fetch_timer =
      GetPredictionManager()->GetPredictionModelFetchTimerForTesting();
  EXPECT_EQ(
      PredictionModelFetchTimer::PredictionModelFetchTimerState::kPeriodicFetch,
      prediction_model_fetch_timer->GetStateForTesting());
  prediction_model_fetch_timer->ScheduleImmediateFetchForTesting();
  base::RunLoop().RunUntilIdle();

  // The model fetch will happen, but no new model will be downloaded.
  RetryForHistogramUntilCountReached(
      histogram_tester.get(),
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status", 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status."
      "PainfulPageLoad",
      net::HTTP_OK, 1);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 0);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.PredictionModelRemoved.PainfulPageLoad", 0);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason."
      "PainfulPageLoad",
      0);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       TestEmptyModelRemovedFlow) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile);
  SetUpValidModelInfoReceival(model_file_observer(), run_loop.get());

  // Registering should initiate the fetch and receive a response with a model
  // containing a download URL and then subsequently downloaded.
  RegisterModelFileObserverWithKeyedService();

  // Wait until the observer receives the file. We increase the timeout to 60
  // seconds here since the file is on the larger side.
  {
    base::test::ScopedRunLoopTimeout file_download_timeout(
        FROM_HERE, kModelFileDownloadTimeout);
    run_loop->Run();
  }

  // Model will be downloaded and loaded.
  RetryForHistogramUntilCountReached(
      histogram_tester.get(),
      "OptimizationGuide.PredictionManager.PredictionModelsStored", 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true, 1);

  // Set up the next periodic model fetch to send null model, which will trigger
  // the model removal from the store, and the observer notified of null model.
  histogram_tester = std::make_unique<base::HistogramTester>();
  run_loop = std::make_unique<base::RunLoop>();
  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithNullModel);
  model_file_observer()->set_model_file_received_callback(base::BindOnce(
      [](base::RunLoop* run_loop, proto::OptimizationTarget optimization_target,
         base::optional_ref<const ModelInfo> model_info) {
        EXPECT_EQ(optimization_target,
                  proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
        EXPECT_FALSE(model_info.has_value());
        run_loop->Quit();
      },
      run_loop.get()));

  // Trigger the periodic fetch timer.
  auto* prediction_model_fetch_timer =
      GetPredictionManager()->GetPredictionModelFetchTimerForTesting();
  EXPECT_EQ(
      PredictionModelFetchTimer::PredictionModelFetchTimerState::kPeriodicFetch,
      prediction_model_fetch_timer->GetStateForTesting());
  prediction_model_fetch_timer->ScheduleImmediateFetchForTesting();
  run_loop->Run();

  // The model fetch will happen, and the model will be removed from the store,
  // and the observers notified of null model.
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.PredictionModelFetcher.GetModelsResponse.Status."
      "PainfulPageLoad",
      net::HTTP_OK, 1);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 0);
  histogram_tester->ExpectTotalCount(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad", 0);
  histogram_tester->ExpectUniqueSample(
      "OptimizationGuide.PredictionModelStore.ModelRemovalReason."
      "PainfulPageLoad",
      PredictionModelStoreModelRemovalReason::kNoModelInGetModelsResponse, 1);
}

IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       TestSwitchProfileDoesntCrash) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath other_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  // Create an additional profile.
  Profile& profile =
      profiles::testing::CreateProfileSync(profile_manager, other_path);
  CreateBrowser(&profile);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
// CreateGuestBrowser() is not supported for Android or ChromeOS out of the box.
IN_PROC_BROWSER_TEST_F(PredictionManagerModelDownloadingBrowserTest,
                       GuestProfileReceivesModel) {
  SetResponseType(
      PredictionModelsFetcherRemoteResponseType::kSuccessfulWithValidModelFile);

  {
    base::HistogramTester histogram_tester;
    // Register in the primary profile and ensure the model returns.
    RegisterModelFileObserverWithKeyedService(browser()->profile());

    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    SetUpValidModelInfoReceival(model_file_observer(), run_loop.get());
    run_loop->Run();
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
        PredictionModelDownloadStatus::kSuccess, 1);
  }

  {
    base::HistogramTester histogram_tester;
    // Now hook everything up in the guest profile and we should still get the
    // model back but no additional fetches should be made.
    Browser* guest_browser = CreateGuestBrowser();

    // To prevent any race, ensure the store has be initialized.
    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PredictionManager.StoreInitialized", 1);
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    ModelFileObserver model_file_observer;
    SetUpValidModelInfoReceival(&model_file_observer, run_loop.get());
    OptimizationGuideKeyedServiceFactory::GetForProfile(
        guest_browser->profile())
        ->AddObserverForOptimizationTargetModel(
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            /*model_metadata=*/std::nullopt, &model_file_observer);
    // Wait until the opt guide is up and the model is loaded as its shared
    // between profiles.
    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad", 1);

    run_loop->Run();
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 0);
  }
}
#endif

class PredictionManagerModelPackageOverrideTest : public InProcessBrowserTest {
 public:
  PredictionManagerModelPackageOverrideTest() = default;
  ~PredictionManagerModelPackageOverrideTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd_line) override {
    InProcessBrowserTest::SetUpCommandLine(cmd_line);

    base::FilePath src_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &src_dir);

    cmd_line->AppendSwitchASCII(
        switches::kModelOverride,
        base::StrCat({
            "OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD",
            ModelOverrideSeparator(),
            FilePathToString(src_dir.AppendASCII("optimization_guide")
                                 .AppendASCII("additional_file_exists.crx3")),
        }));
  }
};

IN_PROC_BROWSER_TEST_F(PredictionManagerModelPackageOverrideTest, TestE2E) {
  base::RunLoop run_loop;
  ModelFileObserver model_file_observer;

  SetUpValidModelInfoReceival(&model_file_observer, &run_loop,
                              {FILE_PATH_LITERAL("good_additional_file.txt")});

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddObserverForOptimizationTargetModel(
          proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
          /*model_metadata=*/std::nullopt, &model_file_observer);

  run_loop.Run();
}

}  // namespace optimization_guide
