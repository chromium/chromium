// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/chrome_prediction_model_store.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/model_store_metadata_entry.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/prediction_manager.h"
#include "components/optimization_guide/core/prediction_model_fetch_timer.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
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
constexpr char kTestLocaleBar[] = "fr-FR";

// Timeout to allow the model file to be downloaded, unzipped and sent to the
// model file observers.
constexpr base::TimeDelta kModelFileDownloadTimeout = base::Seconds(60);

Profile* CreateProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return &profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
}

proto::ModelCacheKey CreateModelCacheKey(const std::string& locale) {
  proto::ModelCacheKey model_cache_key;
  model_cache_key.set_locale(locale);
  return model_cache_key;
}

}  // namespace

class PredictionModelStoreBrowserTestBase : public InProcessBrowserTest {
 public:
  PredictionModelStoreBrowserTestBase() = default;
  ~PredictionModelStoreBrowserTestBase() override = default;

  PredictionModelStoreBrowserTestBase(
      const PredictionModelStoreBrowserTestBase&) = delete;
  PredictionModelStoreBrowserTestBase& operator=(
      const PredictionModelStoreBrowserTestBase&) = delete;

  void SetUp() override {
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
        &PredictionModelStoreBrowserTestBase::HandleGetModelsRequest,
        base::Unretained(this)));

    ASSERT_TRUE(models_server_->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    model_file_url_ = models_server_->GetURL("/signed_valid_model.crx3");

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
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
            /*model_metadata=*/std::nullopt, model_file_observer);
  }

  // Registers |model_file_observer| for model updates from the optimization
  // guide service in |profile|. Default profile is used, when |profile| is
  // null.
  void RegisterAndWaitForModelUpdate(ModelFileObserver* model_file_observer,
                                     Profile* profile = nullptr) {
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    model_file_observer->set_model_file_received_callback(base::BindOnce(
        [](base::RunLoop* run_loop,
           proto::OptimizationTarget optimization_target,
           base::optional_ref<const ModelInfo> model_info) {
          EXPECT_TRUE(model_info.has_value());
          run_loop->Quit();
        },
        run_loop.get()));

    RegisterModelFileObserverWithKeyedService(
        model_file_observer, profile ? profile : browser()->profile());
    base::test::ScopedRunLoopTimeout model_file_download_timeout(
        FROM_HERE, kModelFileDownloadTimeout);
    run_loop->Run();
  }

  PredictionManager* GetPredictionManager(Profile* profile) {
    return OptimizationGuideKeyedServiceFactory::GetForProfile(profile)
        ->GetPredictionManager();
  }

  void SetModelCacheKey(Profile* profile,
                        const proto::ModelCacheKey& model_cache_key) {
    GetPredictionManager(profile)->SetModelCacheKeyForTesting(model_cache_key);
  }

  void set_server_model_cache_key(
      std::optional<proto::ModelCacheKey> server_model_cache_key) {
    server_model_cache_key_ = server_model_cache_key;
  }

  base::FilePath GetModelStoreBaseDir() {
    return ChromePredictionModelStore::GetInstance()
        ->GetBaseStoreDirForTesting();
  }

  size_t ComputeModelsInStore() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    size_t models_count = 0;
    base::FileEnumerator model_file_enumerator(GetModelStoreBaseDir(),
                                               /*recursive=*/true,
                                               base::FileEnumerator::FILES);
    for (base::FilePath model_file = model_file_enumerator.Next();
         !model_file.empty(); model_file = model_file_enumerator.Next()) {
      if (model_file.BaseName() == GetBaseFileNameForModels()) {
        models_count++;
      }
    }
    return models_count;
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
    get_models_response->mutable_models(0)->mutable_model_info()->set_version(
        kSuccessfulModelVersion);
    if (server_model_cache_key_) {
      *get_models_response->mutable_models(0)
           ->mutable_model_info()
           ->mutable_model_cache_key() = *server_model_cache_key_;
    }
    std::string serialized_response;
    get_models_response->SerializeToString(&serialized_response);
    response->set_content(serialized_response);
    return std::move(response);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  GURL model_file_url_;
  std::unique_ptr<net::EmbeddedTestServer> models_server_;
  base::HistogramTester histogram_tester_;

  // Server returned ModelCacheKey in the GetModels response.
  std::optional<proto::ModelCacheKey> server_model_cache_key_;
};

class PredictionModelStoreBrowserTest
    : public PredictionModelStoreBrowserTestBase {
 public:
  PredictionModelStoreBrowserTest() = default;
  ~PredictionModelStoreBrowserTest() override = default;

  PredictionModelStoreBrowserTest(const PredictionModelStoreBrowserTest&) =
      delete;
  PredictionModelStoreBrowserTest& operator=(
      const PredictionModelStoreBrowserTest&) = delete;
};

// TODO(crbug.com/329617221): Test is flaky on Win, Linux, and Mac ASan bots.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || \
    (BUILDFLAG(IS_MAC) && defined(ADDRESS_SANITIZER))
#define MAYBE_TestRegularProfile DISABLED_TestRegularProfile
#else
#define MAYBE_TestRegularProfile TestRegularProfile
#endif
IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest,
                       MAYBE_TestRegularProfile) {
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

// TODO(crbug.com/41490438): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS) && defined(ADDRESS_SANITIZER)
#define MAYBE_TestIncognitoProfile DISABLED_TestIncognitoProfile
#else
#define MAYBE_TestIncognitoProfile TestIncognitoProfile
#endif
IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest,
                       MAYBE_TestIncognitoProfile) {
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

// Tests that two dissimilar profiles do not share the model, and the model will
// be redownloaded.
IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest,
                       TestDissimilarProfilesNotShareModel) {
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
    SetModelCacheKey(profile_foo, CreateModelCacheKey(kTestLocaleFoo));

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

// Tests that two similar profiles share the model, and the model is not
// redownloaded, based on server returned model cache key.
IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest,
                       TestSimilarProfilesShareModelWithServerModelCacheKey) {
  ModelFileObserver model_file_observer_foo, model_file_observer_bar;
  set_server_model_cache_key(CreateModelCacheKey(kTestLocaleFoo));
  {
    base::HistogramTester histogram_tester_foo;
    Profile* profile_foo = CreateProfile();
    SetModelCacheKey(profile_foo, CreateModelCacheKey(kTestLocaleFoo));
    RegisterAndWaitForModelUpdate(&model_file_observer_foo, profile_foo);

    histogram_tester_foo.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
        PredictionModelDownloadStatus::kSuccess, 1);
    EXPECT_EQ(model_file_observer_foo.optimization_target(),
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    EXPECT_TRUE(
        model_file_observer_foo.model_info()->GetModelFilePath().IsAbsolute());
  }
  {
    base::HistogramTester histogram_tester_bar;
    Profile* profile_bar = CreateProfile();
    SetModelCacheKey(profile_bar, CreateModelCacheKey(kTestLocaleBar));
    RegisterAndWaitForModelUpdate(&model_file_observer_bar, profile_bar);

    // No more downloads should happen.
    histogram_tester_bar.ExpectTotalCount(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 0);
    EXPECT_EQ(model_file_observer_bar.optimization_target(),
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    EXPECT_EQ(model_file_observer_foo.model_info()->GetModelFilePath(),
              model_file_observer_bar.model_info()->GetModelFilePath());
  }
}

// Tests that two dissimilar profiles do not share the model, and the model will
// be redownloaded, based on server returned model cache key.
IN_PROC_BROWSER_TEST_F(
    PredictionModelStoreBrowserTest,
    TestDissimilarProfilesNotShareModelWithServerModelCacheKey) {
  ModelFileObserver model_file_observer_foo, model_file_observer_bar;
  {
    set_server_model_cache_key(CreateModelCacheKey(kTestLocaleFoo));
    base::HistogramTester histogram_tester_foo;
    Profile* profile_foo = CreateProfile();
    SetModelCacheKey(profile_foo, CreateModelCacheKey(kTestLocaleFoo));
    RegisterAndWaitForModelUpdate(&model_file_observer_foo, profile_foo);

    histogram_tester_foo.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
        PredictionModelDownloadStatus::kSuccess, 1);
    EXPECT_EQ(model_file_observer_foo.optimization_target(),
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    EXPECT_TRUE(
        model_file_observer_foo.model_info()->GetModelFilePath().IsAbsolute());
  }
  {
    set_server_model_cache_key(CreateModelCacheKey(kTestLocaleBar));
    base::HistogramTester histogram_tester_bar;
    Profile* profile_bar = CreateProfile();
    SetModelCacheKey(profile_bar, CreateModelCacheKey(kTestLocaleBar));
    RegisterAndWaitForModelUpdate(&model_file_observer_bar, profile_bar);

    // Model will be downloaded since the server returned different model cache
    // key.
    histogram_tester_bar.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
        PredictionModelDownloadStatus::kSuccess, 1);
    EXPECT_EQ(model_file_observer_bar.optimization_target(),
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    EXPECT_NE(model_file_observer_foo.model_info()->GetModelFilePath(),
              model_file_observer_bar.model_info()->GetModelFilePath());
    EXPECT_TRUE(base::ContentsEqual(
        model_file_observer_foo.model_info()->GetModelFilePath(),
        model_file_observer_bar.model_info()->GetModelFilePath()));
  }
}

// Tests that when a second similar profile is loaded, model is downloaded when
// the model version has been updated. The old model should not be used.
IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest,
                       TestSimilarProfilesOnModelVersionUpdate) {
  ModelFileObserver model_file_observer_foo, model_file_observer_bar;
  set_server_model_cache_key(CreateModelCacheKey(kTestLocaleFoo));
  {
    base::HistogramTester histogram_tester_foo;
    Profile* profile_foo = CreateProfile();
    SetModelCacheKey(profile_foo, CreateModelCacheKey(kTestLocaleFoo));
    RegisterAndWaitForModelUpdate(&model_file_observer_foo, profile_foo);

    histogram_tester_foo.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
        PredictionModelDownloadStatus::kSuccess, 1);
    EXPECT_EQ(model_file_observer_foo.optimization_target(),
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    EXPECT_TRUE(
        model_file_observer_foo.model_info()->GetModelFilePath().IsAbsolute());
  }
  {
    // Mark the downloaded model as old version, to simulate model version
    // update.
    ModelStoreMetadataEntryUpdater updater(
        g_browser_process->local_state(),
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
        CreateModelCacheKey(kTestLocaleFoo));
    updater.SetVersion(kSuccessfulModelVersion - 1);
  }
  {
    base::HistogramTester histogram_tester_bar;
    Profile* profile_bar = CreateProfile();
    SetModelCacheKey(profile_bar, CreateModelCacheKey(kTestLocaleBar));
    RegisterAndWaitForModelUpdate(&model_file_observer_bar, profile_bar);

    // Model will be downloaded since the model version got updated.
    histogram_tester_bar.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
        PredictionModelDownloadStatus::kSuccess, 1);
    EXPECT_EQ(model_file_observer_bar.optimization_target(),
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    EXPECT_NE(model_file_observer_foo.model_info()->GetModelFilePath(),
              model_file_observer_bar.model_info()->GetModelFilePath());
    EXPECT_TRUE(base::ContentsEqual(
        model_file_observer_foo.model_info()->GetModelFilePath(),
        model_file_observer_bar.model_info()->GetModelFilePath()));
    histogram_tester_bar.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelUpdateVersion.PainfulPageLoad",
        kSuccessfulModelVersion, 1);
    histogram_tester_bar.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
        PredictionModelStoreModelRemovalReason::kNewModelUpdate, 1);
  }
}

IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest,
                       PRE_TestOldModelRemovedOnModelUpdate) {
  auto model_cache_key =
      CreateModelCacheKey(g_browser_process->GetApplicationLocale());
  base::FilePath old_model_dir, new_model_dir;
  ModelFileObserver model_file_observer;
  {
    base::HistogramTester histogram_tester;
    RegisterAndWaitForModelUpdate(&model_file_observer);
    EXPECT_EQ(model_file_observer.optimization_target(),
              proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
    EXPECT_TRUE(
        model_file_observer.model_info()->GetModelFilePath().IsAbsolute());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
        PredictionModelDownloadStatus::kSuccess, 1);
  }
  {
    // Mark the downloaded model as old version, to simulate model version
    // update.
    ModelStoreMetadataEntryUpdater updater(
        g_browser_process->local_state(),
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_cache_key);
    updater.SetVersion(kSuccessfulModelVersion - 1);
    old_model_dir = GetModelStoreBaseDir().Append(*updater.GetModelBaseDir());
  }
  {
    // Trigger the periodic fetch timer.
    base::HistogramTester histogram_tester;
    auto* prediction_model_fetch_timer =
        GetPredictionManager(browser()->profile())
            ->GetPredictionModelFetchTimerForTesting();
    EXPECT_EQ(PredictionModelFetchTimer::PredictionModelFetchTimerState::
                  kPeriodicFetch,
              prediction_model_fetch_timer->GetStateForTesting());
    prediction_model_fetch_timer->ScheduleImmediateFetchForTesting();

    // Updated model is downloaded, causing the old model to be scheduled for
    // deletion.
    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus", 1);
    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PredictionModelStore.ModelRemovalReason", 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
        PredictionModelDownloadStatus::kSuccess, 1);
    GetPredictionManager(browser()->profile())
        ->RemoveObserverForOptimizationTargetModel(
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, &model_file_observer);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PredictionModelStore.ModelRemovalReason",
        PredictionModelStoreModelRemovalReason::kNewModelUpdate, 1);
  }

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto entry = ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
        g_browser_process->local_state(),
        proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD, model_cache_key);
    new_model_dir = GetModelStoreBaseDir().Append(*entry->GetModelBaseDir());
    EXPECT_TRUE(base::DirectoryExists(old_model_dir));
    EXPECT_TRUE(base::DirectoryExists(new_model_dir));
  }
  DCHECK_NE(old_model_dir, new_model_dir);
  EXPECT_EQ(2U, ComputeModelsInStore());
}

IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest,
                       TestOldModelRemovedOnModelUpdate) {
  ModelFileObserver model_file_observer;
  RegisterAndWaitForModelUpdate(&model_file_observer);
  EXPECT_EQ(model_file_observer.optimization_target(),
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  // The old model should be removed.
  EXPECT_EQ(1U, ComputeModelsInStore());
}

// Tests the case when local state is inconsistent with the model directory,
// i.e., when model file does not exist but the local state entry is populated,
// it will lead to redownloading of the model.
IN_PROC_BROWSER_TEST_F(PredictionModelStoreBrowserTest,
                       TestInconsistentLocalState) {
  ModelFileObserver model_file_observer;
  RegisterAndWaitForModelUpdate(&model_file_observer);

  histogram_tester_.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);
  EXPECT_EQ(model_file_observer.optimization_target(),
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_TRUE(
      model_file_observer.model_info()->GetModelFilePath().IsAbsolute());

  // Remove the model file so that model directory is inconsistent with local
  // state.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::DeleteFile(model_file_observer.model_info()->GetModelFilePath());
  }

  base::HistogramTester histogram_tester_foo;
  ModelFileObserver model_file_observer_foo;
  Profile* profile_foo = CreateProfile();
  RegisterAndWaitForModelUpdate(&model_file_observer_foo, profile_foo);

  // The model will be redownloaded.
  histogram_tester_foo.ExpectUniqueSample(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadStatus",
      PredictionModelDownloadStatus::kSuccess, 1);
  EXPECT_EQ(model_file_observer_foo.optimization_target(),
            proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD);
  EXPECT_TRUE(
      model_file_observer_foo.model_info()->GetModelFilePath().IsAbsolute());
}

}  // namespace optimization_guide
