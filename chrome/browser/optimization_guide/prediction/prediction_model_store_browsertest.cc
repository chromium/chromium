// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/prediction_manager.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace optimization_guide {

namespace {

constexpr int kSuccessfulModelVersion = 123;

// Test locales.
constexpr char kTestLocaleFoo[] = "en-CA";

// Timeout to allow the model file to be downloaded, unzipped and sent to the
// model file observers.
constexpr base::TimeDelta kModelFileDownloadTimeout = base::Seconds(60);

Profile* CreateProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
}

proto::ModelCacheKey GetModelCacheKey(const std::string& locale) {
  proto::ModelCacheKey model_cache_key;
  model_cache_key.set_locale(locale);
  return model_cache_key;
}

}  // namespace

class PredictionModelStoreBrowserTest : public InProcessBrowserTest {
 public:
  PredictionModelStoreBrowserTest() = default;
  ~PredictionModelStoreBrowserTest() override = default;

  PredictionModelStoreBrowserTest(const PredictionModelStoreBrowserTest&) =
      delete;
  PredictionModelStoreBrowserTest& operator=(
      const PredictionModelStoreBrowserTest&) = delete;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {{features::kOptimizationGuideInstallWideModelStore}}, {});
    models_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    models_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");
    models_server_->RegisterRequestHandler(base::BindRepeating(
        &PredictionModelStoreBrowserTest::HandleGetModelsRequest,
        base::Unretained(this)));

    ASSERT_TRUE(models_server_->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    download_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    download_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(download_server_->Start());
    model_file_url_ = models_server_->GetURL("/signed_valid_model.crx3");

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(download_server_->ShutdownAndWaitUntilComplete());
    EXPECT_TRUE(models_server_->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(switches::kDisableCheckingUserPermissionsForTesting);
    cmd->AppendSwitchASCII(
        switches::kOptimizationGuideServiceGetModelsURL,
        models_server_
            ->GetURL(GURL(kOptimizationGuideServiceGetModelsDefaultURL).host(),
                     "/")
            .spec());
    cmd->AppendSwitchASCII("host-rules", "MAP * 127.0.0.1");
    cmd->AppendSwitchASCII("force-variation-ids", "4");
#if BUILDFLAG(IS_CHROMEOS_ASH)
    cmd->AppendSwitch(ash::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  void RegisterModelFileObserverWithKeyedService(
      ModelFileObserver* model_file_observer,
      Profile* profile) {
    OptimizationGuideKeyedServiceFactory::GetForProfile(profile)
        ->AddObserverForOptimizationTargetModel(
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
            /*model_metadata=*/absl::nullopt, model_file_observer);
  }

  // Registers |model_file_observer| for model updates from the optimization
  // guide service in |profile|. Default profile is used, when |profile| is
  // null.
  void RegisterAndWaitForModelUpdate(ModelFileObserver* model_file_observer,
                                     Profile* profile = nullptr) {
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    model_file_observer->set_model_file_received_callback(
        base::BindOnce([](base::RunLoop* run_loop,
                          proto::OptimizationTarget optimization_target,
                          const ModelInfo& model_info) { run_loop->Quit(); },
                       run_loop.get()));

    RegisterModelFileObserverWithKeyedService(
        model_file_observer, profile ? profile : browser()->profile());
    base::test::ScopedRunLoopTimeout model_file_download_timeout(
        FROM_HERE, kModelFileDownloadTimeout);
    run_loop->Run();
  }

  void SetModelCacheKey(Profile* profile,
                        const proto::ModelCacheKey& model_cache_key) {
    OptimizationGuideKeyedServiceFactory::GetForProfile(profile)
        ->GetPredictionManager()
        ->SetModelCacheKeyForTesting(model_cache_key);
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleGetModelsRequest(
      const net::test_server::HttpRequest& request) {
    // Returning nullptr will cause the test server to fallback to serving the
    // file from the test data directory.
    if (request.GetURL() == model_file_url_) {
      return nullptr;
    }
    optimization_guide::proto::GetModelsRequest get_models_request;
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();

    EXPECT_EQ(request.method, net::test_server::METHOD_POST);
    EXPECT_TRUE(get_models_request.ParseFromString(request.content));
    response->set_code(net::HTTP_OK);
    if (!base::ranges::any_of(
            get_models_request.requested_models(),
            [](const proto::ModelInfo& model_info) {
              return model_info.optimization_target() ==
                     proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD;
            })) {
      // Return empty response since this request is from the default profile
      // not setup by the tests.
      return std::move(response);
    }
    auto get_models_response = BuildGetModelsResponse();
    get_models_response->mutable_models(0)->mutable_model()->set_download_url(
        model_file_url_.spec());
    std::string serialized_response;
    get_models_response->SerializeToString(&serialized_response);
    response->set_content(serialized_response);
    return std::move(response);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  GURL model_file_url_;
  std::unique_ptr<net::EmbeddedTestServer> download_server_;
  std::unique_ptr<net::EmbeddedTestServer> models_server_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest, TestRegularProfile) {
  ModelFileObserver model_file_observer;
  RegisterAndWaitForModelUpdate(&model_file_observer);
  EXPECT_EQ(model_file_observer.optimization_target(),
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_TRUE(
      model_file_observer.model_info()->GetModelFilePath().IsAbsolute());

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelLoadedVersion.PainfulPageLoad",
      kSuccessfulModelVersion, 1);
}

IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest, TestIncognitoProfile) {
  ModelFileObserver model_file_observer;
  RegisterAndWaitForModelUpdate(&model_file_observer);
  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);
  EXPECT_EQ(model_file_observer.optimization_target(),
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_TRUE(
      model_file_observer.model_info()->GetModelFilePath().IsAbsolute());

  base::HistogramTester histogram_tester_otr;
  ModelFileObserver model_file_observer_otr;
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  RegisterAndWaitForModelUpdate(&model_file_observer_otr,
                                otr_browser->profile());

  // No more downloads should happen.
  histogram_tester_otr.ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 0);
  EXPECT_EQ(model_file_observer_otr.optimization_target(),
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_EQ(model_file_observer.model_info()->GetModelFilePath(),
            model_file_observer_otr.model_info()->GetModelFilePath());
}

// Tests that two similar profiles share the model, and the model is not
// redownloaded.
IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest,
                       TestSimilarProfilesShareModel) {
  ModelFileObserver model_file_observer;
  RegisterAndWaitForModelUpdate(&model_file_observer);

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);
  EXPECT_EQ(model_file_observer.optimization_target(),
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_TRUE(
      model_file_observer.model_info()->GetModelFilePath().IsAbsolute());

  base::HistogramTester histogram_tester_foo;
  ModelFileObserver model_file_observer_foo;
  Profile* profile_foo = CreateProfile();
  RegisterAndWaitForModelUpdate(&model_file_observer_foo, profile_foo);

  // No more downloads should happen.
  histogram_tester_foo.ExpectTotalCount(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 0);
  EXPECT_EQ(model_file_observer_foo.optimization_target(),
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_EQ(model_file_observer.model_info()->GetModelFilePath(),
            model_file_observer_foo.model_info()->GetModelFilePath());
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TestDissimilarProfilesNotShareModel \
  DISABLED_TestDissimilarProfilesNotShareModel
#else
#define MAYBE_TestDissimilarProfilesNotShareModel \
  TestDissimilarProfilesNotShareModel
#endif

// Tests that two dissimilar profiles do not share the model, and the model will
// be redownloaded.
IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest,
                       MAYBE_TestDissimilarProfilesNotShareModel) {
  ModelFileObserver model_file_observer;
  RegisterAndWaitForModelUpdate(&model_file_observer);

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);
  EXPECT_EQ(model_file_observer.optimization_target(),
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_TRUE(
      model_file_observer.model_info()->GetModelFilePath().IsAbsolute());

  {
    base::HistogramTester histogram_tester_foo;
    ModelFileObserver model_file_observer_foo;
    Profile* profile_foo = CreateProfile();
    SetModelCacheKey(profile_foo, GetModelCacheKey(kTestLocaleFoo));

    RegisterAndWaitForModelUpdate(&model_file_observer_foo, profile_foo);
    // Same model will be redownloaded.
    histogram_tester_foo.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
        PredictionModelDownloadStatus::kSuccess, 1);
    EXPECT_EQ(model_file_observer_foo.optimization_target(),
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    EXPECT_NE(model_file_observer.model_info()->GetModelFilePath(),
              model_file_observer_foo.model_info()->GetModelFilePath());
    EXPECT_TRUE(base::ContentsEqual(
        model_file_observer.model_info()->GetModelFilePath(),
        model_file_observer_foo.model_info()->GetModelFilePath()));
  }
}

}  // namespace optimization_guide
