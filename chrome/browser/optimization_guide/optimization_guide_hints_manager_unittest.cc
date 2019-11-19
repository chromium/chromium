// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/browser/previews/previews_https_notification_infobar_decider.h"
#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/optimization_guide/bloom_filter.h"
#include "components/optimization_guide/hints_component_util.h"
#include "components/optimization_guide/hints_fetcher.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/proto_database_provider_test_base.h"
#include "components/optimization_guide/top_host_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Retry delay is 16 minutes to allow for kFetchRetryDelaySecs +
// kFetchRandomMaxDelaySecs to pass.
constexpr int kTestFetchRetryDelaySecs = 60 * 16;
constexpr int kUpdateFetchHintsTimeSecs = 24 * 60 * 60;  // 24 hours.

const int kBlackBlacklistBloomFilterNumHashFunctions = 7;
const int kBlackBlacklistBloomFilterNumBits = 511;

void PopulateBlackBlacklistBloomFilter(
    optimization_guide::BloomFilter* bloom_filter) {
  bloom_filter->Add("black.com");
}

void AddBlacklistBloomFilterToConfig(
    optimization_guide::proto::OptimizationType optimization_type,
    const optimization_guide::BloomFilter& blacklist_bloom_filter,
    int num_hash_functions,
    int num_bits,
    optimization_guide::proto::Configuration* config) {
  std::string blacklist_data(
      reinterpret_cast<const char*>(&blacklist_bloom_filter.bytes()[0]),
      blacklist_bloom_filter.bytes().size());
  optimization_guide::proto::OptimizationFilter* blacklist_proto =
      config->add_optimization_blacklists();
  blacklist_proto->set_optimization_type(optimization_type);
  std::unique_ptr<optimization_guide::proto::BloomFilter> bloom_filter_proto =
      std::make_unique<optimization_guide::proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  bloom_filter_proto->set_data(blacklist_data);
  blacklist_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
}

std::unique_ptr<optimization_guide::proto::GetHintsResponse> BuildHintsResponse(
    std::vector<std::string> hosts) {
  std::unique_ptr<optimization_guide::proto::GetHintsResponse>
      get_hints_response =
          std::make_unique<optimization_guide::proto::GetHintsResponse>();

  for (const auto& host : hosts) {
    optimization_guide::proto::Hint* hint = get_hints_response->add_hints();
    hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    hint->set_key(host);
    optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("page pattern");
  }
  return get_hints_response;
}

}  // namespace

class TestOptimizationGuideService
    : public optimization_guide::OptimizationGuideService {
 public:
  explicit TestOptimizationGuideService(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner)
      : OptimizationGuideService(ui_task_runner) {}

  ~TestOptimizationGuideService() override = default;

  void AddObserver(
      optimization_guide::OptimizationGuideServiceObserver* observer) override {
    add_observer_called_ = true;
  }

  void RemoveObserver(
      optimization_guide::OptimizationGuideServiceObserver* observer) override {
    remove_observer_called_ = true;
  }

  bool AddObserverCalled() const { return add_observer_called_; }
  bool RemoveObserverCalled() const { return remove_observer_called_; }

 private:
  bool add_observer_called_ = false;
  bool remove_observer_called_ = false;
};

// A mock class implementation of TopHostProvider.
class FakeTopHostProvider : public optimization_guide::TopHostProvider {
 public:
  explicit FakeTopHostProvider(std::vector<std::string> top_hosts)
      : top_hosts_(top_hosts) {}

  std::vector<std::string> GetTopHosts() override {
    num_top_hosts_called_++;

      return top_hosts_;
  }

  int get_num_top_hosts_called() const { return num_top_hosts_called_; }

 private:
  std::vector<std::string> top_hosts_;
  int num_top_hosts_called_ = 0;
};

enum class HintsFetcherEndState {
  kFetchFailed = 0,
  kFetchSuccessWithHints = 1,
  kFetchSuccessWithNoHints = 2,
};

// A mock class implementation of HintsFetcher.
class TestHintsFetcher : public optimization_guide::HintsFetcher {
  using HintsFetcher::FetchOptimizationGuideServiceHints;

 public:
  TestHintsFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL optimization_guide_service_url,
      HintsFetcherEndState fetch_state,
      PrefService* pref_service)
      : HintsFetcher(url_loader_factory,
                     optimization_guide_service_url,
                     pref_service),
        fetch_state_(fetch_state) {}

  bool FetchOptimizationGuideServiceHints(
      const std::vector<std::string>& hosts,
      optimization_guide::proto::RequestContext request_context,
      optimization_guide::HintsFetchedCallback hints_fetched_callback)
      override {
    switch (fetch_state_) {
      case HintsFetcherEndState::kFetchFailed:
        std::move(hints_fetched_callback)
            .Run(request_context,
                 optimization_guide::HintsFetcherRequestStatus::kResponseError,
                 base::nullopt);
        return false;
      case HintsFetcherEndState::kFetchSuccessWithHints:
        hints_fetched_ = true;
        std::move(hints_fetched_callback)
            .Run(request_context,
                 optimization_guide::HintsFetcherRequestStatus::kSuccess,
                 BuildHintsResponse({"host.com"}));
        return true;
      case HintsFetcherEndState::kFetchSuccessWithNoHints:
        hints_fetched_ = true;
        std::move(hints_fetched_callback)
            .Run(request_context,
                 optimization_guide::HintsFetcherRequestStatus::kSuccess,
                 BuildHintsResponse({}));
        return true;
    }
    return true;
  }

  bool hints_fetched() { return hints_fetched_; }

 private:
  bool hints_fetched_ = false;
  HintsFetcherEndState fetch_state_;
};

class OptimizationGuideHintsManagerTest
    : public optimization_guide::ProtoDatabaseProviderTestBase {
 public:
  OptimizationGuideHintsManagerTest() = default;
  ~OptimizationGuideHintsManagerTest() override = default;

  void SetUp() override {
    optimization_guide::ProtoDatabaseProviderTestBase::SetUp();
    web_contents_factory_.reset(new content::TestWebContentsFactory);
    drp_test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .WithMockConfig()
            .Build();
    CreateServiceAndHintsManager(
        /*optimization_types_at_initialization=*/{},
        /*top_host_provider=*/nullptr);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        data_reduction_proxy::switches::kEnableDataReductionProxy);
  }

  void TearDown() override {
    optimization_guide::ProtoDatabaseProviderTestBase::TearDown();
    drp_test_context_->DestroySettings();
    ResetHintsManager();
  }

  void CreateServiceAndHintsManager(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types_at_initialization,
      optimization_guide::TopHostProvider* top_host_provider) {
    if (hints_manager_) {
      ResetHintsManager();
    }
    optimization_guide_service_ =
        std::make_unique<TestOptimizationGuideService>(
            task_environment_.GetMainThreadTaskRunner());
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    optimization_guide::prefs::RegisterProfilePrefs(pref_service_->registry());

    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    hints_manager_ = std::make_unique<OptimizationGuideHintsManager>(
        optimization_types_at_initialization, optimization_guide_service_.get(),
        &testing_profile_, temp_dir(), pref_service_.get(), db_provider_.get(),
        top_host_provider, url_loader_factory_);
    hints_manager_->SetClockForTesting(task_environment_.GetMockClock());

    // Add observer is called after the HintCache is fully initialized,
    // indicating that the OptimizationGuideHintsManager is ready to process
    // hints.
    while (!optimization_guide_service_->AddObserverCalled()) {
      RunUntilIdle();
    }
  }

  void ResetHintsManager() {
    hints_manager_.reset();
    RunUntilIdle();
  }

  void ProcessInvalidHintsComponentInfo(const std::string& version) {
    optimization_guide::HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("notaconfigfile")));

    base::RunLoop run_loop;
    hints_manager_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager_->OnHintsComponentAvailable(info);
    run_loop.Run();
  }

  void ProcessHints(const optimization_guide::proto::Configuration& config,
                    const std::string& version) {
    optimization_guide::HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("somefile.pb")));
    ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(config, info.path));

    base::RunLoop run_loop;
    hints_manager_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager_->OnHintsComponentAvailable(info);
    run_loop.Run();
  }

  void InitializeWithDefaultConfig(const std::string& version) {
    optimization_guide::proto::Configuration config;
    optimization_guide::proto::Hint* hint1 = config.add_hints();
    hint1->set_key("somedomain.org");
    hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
    hint1->set_version("someversion");
    optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
    page_hint1->set_page_pattern("/news/");
    page_hint1->set_max_ect_trigger(
        optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_3G);
    optimization_guide::proto::Optimization* experimental_opt =
        page_hint1->add_whitelisted_optimizations();
    experimental_opt->set_optimization_type(
        optimization_guide::proto::NOSCRIPT);
    experimental_opt->set_experiment_name("experiment");
    optimization_guide::proto::PreviewsMetadata* experimental_opt_metadata =
        experimental_opt->mutable_previews_metadata();
    experimental_opt_metadata->set_inflation_percent(12345);
    optimization_guide::proto::Optimization* default_opt =
        page_hint1->add_whitelisted_optimizations();
    default_opt->set_optimization_type(optimization_guide::proto::NOSCRIPT);
    optimization_guide::proto::PreviewsMetadata* default_opt_metadata =
        default_opt->mutable_previews_metadata();
    default_opt_metadata->set_inflation_percent(1234);

    ProcessHints(config, version);
  }

  std::unique_ptr<TestHintsFetcher> BuildTestHintsFetcher(
      HintsFetcherEndState end_state) {
    std::unique_ptr<TestHintsFetcher> hints_fetcher =
        std::make_unique<TestHintsFetcher>(url_loader_factory_,
                                           GURL("https://hintsserver.com"),
                                           end_state, pref_service());
    return hints_fetcher;
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    RunUntilIdle();
  }

  void SetUserPermissions(bool data_saver_enabled, bool has_seen_infobar) {
    drp_test_context_->SetDataReductionProxyEnabled(data_saver_enabled);

    // Make sure infobar not shown.
    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(&testing_profile_);
    PreviewsLitePageRedirectDecider* decider =
        previews_service->previews_lite_page_redirect_decider();
    // Initialize settings here so Lite Pages Decider checks for the Data Saver
    // bit.
    decider->OnSettingsInitialized();
    if (has_seen_infobar) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          previews::switches::kDoNotRequireLitePageRedirectInfoBar);
    }
  }

  // Creates a navigation handle with the OptimizationGuideWebContentsObserver
  // attached.
  std::unique_ptr<content::MockNavigationHandle>
  CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
      const GURL& url) {
    content::WebContents* web_contents =
        web_contents_factory_->CreateWebContents(&testing_profile_);
    OptimizationGuideWebContentsObserver::CreateForWebContents(web_contents);
    std::unique_ptr<content::MockNavigationHandle> navigation_handle =
        std::make_unique<content::MockNavigationHandle>(web_contents);
    navigation_handle->set_url(url);
    return navigation_handle;
  }

  OptimizationGuideHintsManager* hints_manager() const {
    return hints_manager_.get();
  }

  TestHintsFetcher* hints_fetcher() const {
    return static_cast<TestHintsFetcher*>(hints_manager()->hints_fetcher());
  }

  GURL url_with_hints() const {
    return GURL("https://somedomain.org/news/whatever");
  }

  GURL url_without_hints() const {
    return GURL("https://url_without_hints.org/");
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  TestingPrefServiceSimple* pref_service() const { return pref_service_.get(); }

 protected:
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 private:
  void WriteConfigToFile(const optimization_guide::proto::Configuration& config,
                         const base::FilePath& filePath) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_EQ(static_cast<int32_t>(serialized_config.size()),
              base::WriteFile(filePath, serialized_config.data(),
                              serialized_config.size()));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile testing_profile_;
  std::unique_ptr<data_reduction_proxy::DataReductionProxyTestContext>
      drp_test_context_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  std::unique_ptr<OptimizationGuideHintsManager> hints_manager_;
  std::unique_ptr<TestOptimizationGuideService> optimization_guide_service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideHintsManagerTest);
};

TEST_F(OptimizationGuideHintsManagerTest,
       OptimizationTypesProvidedAtInitializationAreRegistered) {
  std::vector<optimization_guide::proto::OptimizationType>
      optimization_targets =
          std::vector<optimization_guide::proto::OptimizationType>{
              optimization_guide::proto::DEFER_ALL_SCRIPT};
  CreateServiceAndHintsManager(optimization_targets,
                               /*top_host_provider=*/nullptr);

  EXPECT_TRUE(hints_manager()->HasRegisteredOptimizationTypes());
  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::DEFER_ALL_SCRIPT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::NOSCRIPT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);

  base::HistogramTester histogram_tester;

  ProcessHints(config, "1.0.0.0");

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.DeferAllScript",
      optimization_guide::OptimizationFilterStatus::kFoundServerBlacklistConfig,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.DeferAllScript",
      optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
}

TEST_F(OptimizationGuideHintsManagerTest,
       ProcessHintsWithValidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, encoded_config);
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               /*top_host_provider=*/nullptr);

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult", 0);
  // However, we still expect the local histogram for the hints being updated to
  // be recorded.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.UpdateComponentHints.Result", true, 1);
}

TEST_F(OptimizationGuideHintsManagerTest,
       ProcessHintsWithInvalidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kHintsProtoOverride, "this-is-not-a-proto");
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               /*top_host_provider=*/nullptr);

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult", 0);
  // We also do not expect to update the component hints with bad hints either.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.UpdateComponentHints.Result", 0);
}

TEST_F(OptimizationGuideHintsManagerTest,
       ProcessHintsWithCommandLineOverrideShouldNotBeOverriddenByNewComponent) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  optimization_guide::proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  optimization_guide::proto::Optimization* optimization =
      page_hint->add_whitelisted_optimizations();
  optimization->set_optimization_type(optimization_guide::proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  {
    base::HistogramTester histogram_tester;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        optimization_guide::switches::kHintsProtoOverride, encoded_config);
    CreateServiceAndHintsManager(
        /*optimization_types_at_initialization=*/{},
        /*top_host_provider=*/nullptr);
    // The below histogram should not be recorded since hints weren't coming
    // directly from the component.
    histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult",
                                      0);
    // However, we still expect the local histogram for the hints being updated
    // to be recorded.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.UpdateComponentHints.Result", true, 1);
  }

  // Test that a new component coming in does not update the component hints.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0.0");
    // The below histograms should not be recorded since component hints
    // processing is disabled.
    histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult",
                                      0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.UpdateComponentHints.Result", 0);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ParseTwoConfigVersions) {
  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::RESOURCE_LOADING);
  optimization_guide::proto::ResourceLoadingHint* resource_loading_hint1 =
      optimization1->add_resource_loading_hints();
  resource_loading_hint1->set_loading_optimization_type(
      optimization_guide::proto::LOADING_BLOCK_RESOURCE);
  resource_loading_hint1->set_resource_pattern("news_cruft.js");

  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("1.0.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }

  // Test the second time parsing the config. This should also update the hints.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ParseOlderConfigVersions) {
  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("10.0.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }

  // Test the second time parsing the config. This will be treated by the cache
  // as an older version.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0.0");
    // If we have already parsed a version later than this version, we expect
    // for the hints to not be updated.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kSkippedProcessingHints,
        1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ParseDuplicateConfigVersions) {
  const std::string version = "3.0.0.0";

  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig(version);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }

  // Test the second time parsing the config. This will be treated by the cache
  // as a duplicate version.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig(version);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kSkippedProcessingHints,
        1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ComponentInfoDidNotContainConfig) {
  base::HistogramTester histogram_tester;
  ProcessInvalidHintsComponentInfo("1.0.0.0");
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      optimization_guide::ProcessHintsComponentResult::kFailedReadingFile, 1);
}

TEST_F(OptimizationGuideHintsManagerTest, ProcessHintsWithExistingPref) {
  // Write hints processing pref for version 2.0.0.
  pref_service()->SetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion, "2.0.0");

  // Verify config not processed for same version (2.0.0) and pref not cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kFailedFinishProcessing,
        1);
    EXPECT_FALSE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
  }

  // Now verify config is processed for different version and pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0");
    EXPECT_TRUE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest, ProcessHintsWithInvalidPref) {
  // Create pref file with invalid version.
  pref_service()->SetString(
      optimization_guide::prefs::kPendingHintsProcessingVersion, "bad-2.0.0");

  // Verify config not processed for existing pref with bad value but
  // that the pref is cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    EXPECT_TRUE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::
            kFailedFinishProcessing,
        1);
  }

  // Now verify config is processed with pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    EXPECT_TRUE(
        pref_service()
            ->GetString(
                optimization_guide::prefs::kPendingHintsProcessingVersion)
            .empty());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        optimization_guide::ProcessHintsComponentResult::kSuccess, 1);
  }
}

TEST_F(OptimizationGuideHintsManagerTest,
       LoadHintForNavigationWithHintAfterCommit) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  navigation_handle->set_has_committed(true);

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_before_commit());
  EXPECT_TRUE(navigation_data->has_hint_after_commit().value());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest, LoadHintForNavigationWithHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_TRUE(navigation_data->has_hint_before_commit().value());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_after_commit());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest, LoadHintForNavigationNoHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://notinhints.com"));

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      false, 1);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_FALSE(navigation_data->has_hint_before_commit().value());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_after_commit());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest, LoadHintForNavigationNoHost) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("blargh"));

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectTotalCount("OptimizationGuide.LoadedHint.Result", 0);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_before_commit());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_after_commit());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       OptimizationFiltersAreOnlyLoadedIfTypeIsRegistered) {
  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::NOSCRIPT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);

  {
    base::HistogramTester histogram_tester;

    ProcessHints(config, "1.0.0.0");

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
  }

  // Now register the optimization type and see that it is loaded.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::LITE_PAGE_REDIRECT});
    run_loop.Run();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::
            kFoundServerBlacklistConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist,
        1);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::LITE_PAGE_REDIRECT));
    EXPECT_FALSE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::NOSCRIPT));
  }

  // Re-registering the same optimization type does not re-load the filter.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::LITE_PAGE_REDIRECT});
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
  }

  // Registering a new optimization type without a filter does not trigger a
  // reload of the filter.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::DEFER_ALL_SCRIPT});
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
  }

  // Registering a new optimization type with a filter does trigger a
  // reload of the filters.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {optimization_guide::proto::NOSCRIPT});
    run_loop.Run();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::
            kFoundServerBlacklistConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        optimization_guide::OptimizationFilterStatus::
            kFoundServerBlacklistConfig,
        1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist,
        1);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::LITE_PAGE_REDIRECT));
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationFilter(
        optimization_guide::proto::NOSCRIPT));
  }
}

TEST_F(OptimizationGuideHintsManagerTest,
       OptimizationFiltersOnlyLoadOncePerType) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // We found 2 LPR blacklists: parsed one and duped the other.
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kFoundServerBlacklistConfig,
      2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kCreatedServerBlacklist, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::
          kFailedServerBlacklistDuplicateConfig,
      1);
}

TEST_F(OptimizationGuideHintsManagerTest, InvalidOptimizationFilterNotLoaded) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  int too_many_bits =
      optimization_guide::features::MaxServerBloomFilterByteSize() * 8 + 1;

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions, too_many_bits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(
      optimization_guide::proto::LITE_PAGE_REDIRECT, blacklist_bloom_filter,
      kBlackBlacklistBloomFilterNumHashFunctions, too_many_bits, &config);
  ProcessHints(config, "1.0.0.0");

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::kFoundServerBlacklistConfig,
      1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      optimization_guide::OptimizationFilterStatus::
          kFailedServerBlacklistTooBig,
      1);
  EXPECT_FALSE(hints_manager()->HasLoadedOptimizationFilter(
      optimization_guide::proto::LITE_PAGE_REDIRECT));
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetchNotAllowedIfFeatureIsEnabledButTopHostProviderIsNotProvided) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHintsFetching}, {});

  SetUserPermissions(/*data_saver_enabled=*/true, /*has_seen_infobar=*/true);
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               /*top_host_provider=*/nullptr);
  hints_manager()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(hints_fetcher()->hints_fetched());
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetchNotAllowedIfFeatureIsEnabledButDataSaverIsNotEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHintsFetching}, {});

  SetUserPermissions(/*data_saver_enabled=*/false, /*has_seen_infobar=*/true);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());
  hints_manager()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(0, top_host_provider->get_num_top_hosts_called());
  EXPECT_FALSE(hints_fetcher()->hints_fetched());
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetchNotAllowedIfFeatureIsNotEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {optimization_guide::features::kOptimizationHintsFetching});

  SetUserPermissions(/*data_saver_enabled=*/true, /*has_seen_infobar=*/true);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(0, top_host_provider->get_num_top_hosts_called());
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetchAllowedIfFeatureIsEnabledAndDataSaverUserHasNotSeenInfobar) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHintsFetching}, {});

  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  SetUserPermissions(/*data_saver_enabled=*/true, /*has_seen_infobar=*/false);
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());
  hints_manager()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(0, top_host_provider->get_num_top_hosts_called());
  EXPECT_FALSE(hints_fetcher()->hints_fetched());
}

TEST_F(
    OptimizationGuideHintsManagerTest,
    HintsFetchAllowedIfFeatureIsEnabledAndUserMeetsAllDataSaverUserCriteria) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {optimization_guide::features::kOptimizationHintsFetching}, {});

  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  SetUserPermissions(/*data_saver_enabled=*/true, /*has_seen_infobar=*/true);
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());
  hints_manager()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(1, top_host_provider->get_num_top_hosts_called());
  EXPECT_TRUE(hints_fetcher()->hints_fetched());
}

TEST_F(OptimizationGuideHintsManagerTest, HintsFetcherEnabledNoHostsToFetch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);

  SetUserPermissions(/*data_saver_enabled=*/true, /*has_seen_infobar=*/true);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(std::vector<std::string>({}));
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());
  hints_manager()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(1, top_host_provider->get_num_top_hosts_called());
  EXPECT_FALSE(hints_fetcher()->hints_fetched());
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetcherEnabledWithHostsNoHintsInResponse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);

  SetUserPermissions(/*data_saver_enabled=*/true, /*has_seen_infobar=*/true);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());
  hints_manager()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithNoHints));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(1, top_host_provider->get_num_top_hosts_called());
  EXPECT_TRUE(hints_fetcher()->hints_fetched());

  // Check that hints should not be fetched again after the delay for a failed
  // hints fetch attempt.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  // This should be called exactly once, confirming that hints are not fetched
  // again after |kTestFetchRetryDelaySecs|.
  EXPECT_EQ(1, top_host_provider->get_num_top_hosts_called());
}

TEST_F(OptimizationGuideHintsManagerTest, HintsFetcherTimerRetryDelay) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);

  SetUserPermissions(/*data_saver_enabled=*/true, /*has_seen_infobar=*/true);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());
  hints_manager()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchFailed));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch - first time.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(1, top_host_provider->get_num_top_hosts_called());
  EXPECT_FALSE(hints_fetcher()->hints_fetched());

  // Force speculative timer to expire after fetch fails first time, update
  // hints fetcher so it succeeds this time.
  hints_manager()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_EQ(2, top_host_provider->get_num_top_hosts_called());
  EXPECT_TRUE(hints_fetcher()->hints_fetched());
}

TEST_F(OptimizationGuideHintsManagerTest, HintsFetcherTimerFetchSucceeds) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);

  SetUserPermissions(/*data_saver_enabled=*/true, /*has_seen_infobar=*/true);
  std::unique_ptr<FakeTopHostProvider> top_host_provider =
      std::make_unique<FakeTopHostProvider>(
          std::vector<std::string>({"example1.com", "example2.com"}));

  // Force hints fetch scheduling.
  CreateServiceAndHintsManager(/*optimization_types_at_initialization=*/{},
                               top_host_provider.get());
  hints_manager()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));
  InitializeWithDefaultConfig("1.0.0");

  // Force timer to expire and schedule a hints fetch that succeeds.
  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_TRUE(hints_fetcher()->hints_fetched());

  // TODO(mcrouse): Make sure timer is triggered by metadata entry,
  // |hint_cache| control needed.
  hints_manager()->SetHintsFetcherForTesting(
      BuildTestHintsFetcher(HintsFetcherEndState::kFetchSuccessWithHints));

  MoveClockForwardBy(base::TimeDelta::FromSeconds(kTestFetchRetryDelaySecs));
  EXPECT_FALSE(hints_fetcher()->hints_fetched());

  MoveClockForwardBy(base::TimeDelta::FromSeconds(kUpdateFetchHintsTimeSecs));
  EXPECT_TRUE(hints_fetcher()->hints_fetched());
}

TEST_F(OptimizationGuideHintsManagerTest, CanApplyOptimizationUrlWithNoHost) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // Set ECT estimate to be "painful".
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("urlwithnohost"));

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::LITE_PAGE_REDIRECT,
      &optimization_target_decision, &optimization_type_decision,
      /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch,
      optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_before_commit());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_after_commit());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationHasFilterForTypeButNotLoadedYet) {
  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // Set ECT estimate to be "painful".
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://whatever.com/123"));

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::LITE_PAGE_REDIRECT,
      &optimization_target_decision, &optimization_type_decision,
      /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kHadOptimizationFilterButNotLoadedInTime,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_before_commit());
  EXPECT_FALSE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlInBlacklistFilter) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // Set ECT estimate to be "painful".
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://m.black.com/123"));

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::LITE_PAGE_REDIRECT,
      &optimization_target_decision, &optimization_type_decision,
      /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kNotAllowedByOptimizationFilter,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_before_commit());
  EXPECT_FALSE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlNotInBlacklistFilter) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // Set ECT estimate to be "painful".
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://whatever.com/123"));

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::LITE_PAGE_REDIRECT,
      &optimization_target_decision, &optimization_type_decision,
      /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kAllowedByOptimizationFilter,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_before_commit());
  EXPECT_FALSE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest, CanApplyOptimizationNoECTEstimate) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // Explicitly set ECT estimate to be unknown.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://whatever.com/123"));

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::LITE_PAGE_REDIRECT,
      &optimization_target_decision, &optimization_type_decision,
      /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch,
      optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kAllowedByOptimizationFilter,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_before_commit());
  EXPECT_FALSE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationNoHintToTriggerHigherThan2G) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  // Explicitly set ECT estimate to be unknown.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://whatever.com/123"));

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::LITE_PAGE_REDIRECT,
      &optimization_target_decision, &optimization_type_decision,
      /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch,
      optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kAllowedByOptimizationFilter,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_before_commit());
  EXPECT_FALSE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationAndPopulatesMetadataWithFirstOptThatMatchesWithExp) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeatureWithParameters(
      optimization_guide::features::kOptimizationHintsExperiments,
      {{"experiment_name", "experiment"}});

  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  optimization_guide::OptimizationMetadata optimization_metadata;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::NOSCRIPT, &optimization_target_decision,
      &optimization_type_decision, &optimization_metadata);
  EXPECT_EQ(12345, optimization_metadata.previews_metadata.inflation_percent());

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_TRUE(navigation_data->has_hint_before_commit().value());
  EXPECT_TRUE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ("someversion", navigation_data->serialized_hint_version_string());
  ASSERT_TRUE(navigation_data->page_hint());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationAndPopulatesMetadataWithFirstOptThatMatchesNoExp) {
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  optimization_guide::OptimizationMetadata optimization_metadata;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::NOSCRIPT, &optimization_target_decision,
      &optimization_type_decision, &optimization_metadata);
  EXPECT_EQ(1234, optimization_metadata.previews_metadata.inflation_percent());

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_TRUE(navigation_data->has_hint_before_commit().value());
  EXPECT_TRUE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ("someversion", navigation_data->serialized_hint_version_string());
  ASSERT_TRUE(navigation_data->page_hint());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationHasHintButNotSlowEnough) {
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  optimization_guide::OptimizationMetadata optimization_metadata;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::NOSCRIPT, &optimization_target_decision,
      &optimization_type_decision, &optimization_metadata);
  EXPECT_EQ(1234, optimization_metadata.previews_metadata.inflation_percent());

  // Make sure decisions are logged correctly.
  EXPECT_EQ(
      optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch,
      optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_TRUE(navigation_data->has_hint_before_commit().value());
  EXPECT_TRUE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ("someversion", navigation_data->serialized_hint_version_string());
  ASSERT_TRUE(navigation_data->page_hint());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationWithNonPainfulPageLoadTarget) {
  InitializeWithDefaultConfig("1.0.0.0");

  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  optimization_guide::OptimizationMetadata optimization_metadata;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN,
      optimization_guide::proto::NOSCRIPT, &optimization_target_decision,
      &optimization_type_decision, &optimization_metadata);
  // Make sure metadata is populated.
  EXPECT_EQ(1234, optimization_metadata.previews_metadata.inflation_percent());

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::
                kModelNotAvailableOnClient,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_TRUE(navigation_data->has_hint_before_commit().value());
  EXPECT_TRUE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ("someversion", navigation_data->serialized_hint_version_string());
  EXPECT_TRUE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationHasPageHintButNoMatchingOptType) {
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  optimization_guide::OptimizationMetadata optimization_metadata;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::DEFER_ALL_SCRIPT,
      &optimization_target_decision, &optimization_type_decision,
      /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_TRUE(navigation_data->has_hint_before_commit().value());
  EXPECT_TRUE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ("someversion", navigation_data->serialized_hint_version_string());
  ASSERT_TRUE(navigation_data->page_hint());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationUsesCachedPageHintFromNavigationData) {
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  // Purposely set the page hint to be null to show that we override the page
  // hint information from the navigation handle.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  navigation_data->set_page_hint(nullptr);

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  optimization_guide::OptimizationMetadata optimization_metadata;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::DEFER_ALL_SCRIPT,
      &optimization_target_decision, &optimization_type_decision,
      /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNoMatchingPageHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  EXPECT_TRUE(navigation_data->has_hint_before_commit().value());
  EXPECT_TRUE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ("someversion", navigation_data->serialized_hint_version_string());
  EXPECT_EQ(nullptr, navigation_data->page_hint());
}

TEST_F(OptimizationGuideHintsManagerTest, IsGoogleURL) {
  const struct {
    const char* url;
    bool expect_is_google_url;
  } tests[] = {
      {"https://www.google.com/"
       "search?q=cats&oq=cq&aqs=foo&ie=UTF-8",
       true},
      {"https://www.google.com/", true},
      {"https://www.google.com/:99", true},

      // Try localized search pages.
      {"https://www.google.co.in/"
       "search?q=cats&oq=cq&aqs=foo&ie=UTF-8",
       true},
      {"https://www.google.co.in/", true},
      {"https://www.google.co.in/:99", true},

      // Try Google domain pages that are not web search related.
      {"https://www.not-google.com/", false},
      {"https://www.youtube.com/", false},
      {"https://domain.google.com/", false},
      {"https://images.google.com/", false},
  };

  for (const auto& test : tests) {
    GURL url(test.url);
    EXPECT_TRUE(url.is_valid());
    EXPECT_EQ(test.expect_is_google_url, hints_manager()->IsGoogleURL(url));
  }
}

TEST_F(OptimizationGuideHintsManagerTest, HintsFetched_AtSRP_ECT_SLOW_2G) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/"));
  NavigationPredictorKeyedService::Prediction prediction(
      nullptr, GURL("https://www.google.com/"), sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1, 1);
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetched_AtSRP_ECT_SLOW_2G_DuplicatesRemoved) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/page1.html"));
  sorted_predicted_urls.push_back(GURL("https://foo.com/page2.html"));
  sorted_predicted_urls.push_back(GURL("https://foo.com/page3.html"));
  sorted_predicted_urls.push_back(GURL("https://bar.com/"));

  NavigationPredictorKeyedService::Prediction prediction(
      nullptr, GURL("https://www.google.com/"), sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  // Ensure that we only include 2 hosts in the request. These would be foo.com
  // and bar.com.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetched_AtSRP_ECT_SLOW_2G_InsecureHostsRemoved) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/page1.html"));
  sorted_predicted_urls.push_back(GURL("http://insecure-bar.com/"));

  NavigationPredictorKeyedService::Prediction prediction(
      nullptr, GURL("https://www.google.com/"), sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  // Ensure that we only include 1 secure host in the request. These would be
  // foo.com.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1, 1);
}

TEST_F(OptimizationGuideHintsManagerTest, HintsFetched_AtSRP_ECT_4G) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/"));
  NavigationPredictorKeyedService::Prediction prediction(
      nullptr, GURL("https://www.google.com/"), sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
}

TEST_F(OptimizationGuideHintsManagerTest, HintsFetched_AtNonSRP_ECT_SLOW_2G) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.push_back(GURL("https://foo.com/"));
  NavigationPredictorKeyedService::Prediction prediction(
      nullptr, GURL("https://www.not-google.com/"), sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetchedAtNavigationTime_ECT_SLOW_2G) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1, 1);
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsFetchedAtNavigationTime_HasComponentHintButNotFetched) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());
  base::RunLoop run_loop;
  base::HistogramTester histogram_tester;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
}

TEST_F(OptimizationGuideHintsManagerTest,
       HintsNotFetchedAtNavigationTime_ECT_4G) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
}

TEST_F(OptimizationGuideHintsManagerTest, CanApplyOptimizationCalledMidFetch) {
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHintsFetching);
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint will attempt to be fetched.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_without_hints());
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               base::DoNothing());
  optimization_guide::OptimizationTargetDecision unused_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::DEFER_ALL_SCRIPT, &unused_target_decision,
      &optimization_type_decision, /*optimization_metadata=*/nullptr);

  EXPECT_EQ(optimization_type_decision,
            optimization_guide::OptimizationTypeDecision::
                kHintFetchStartedButNotAvailableInTime);
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationNoMatchingPageHint) {
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://somedomain.org/nomatch"));
  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  optimization_guide::OptimizationMetadata optimization_metadata;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::NOSCRIPT, &optimization_target_decision,
      &optimization_type_decision,
      /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNoMatchingPageHint,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_TRUE(navigation_data->has_hint_before_commit().value());
  EXPECT_TRUE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ("someversion", navigation_data->serialized_hint_version_string());
  EXPECT_EQ(nullptr, navigation_data->page_hint());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationNoHintForNavigationMetadataClearedAnyway) {
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://nohint.com"));

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  optimization_guide::OptimizationMetadata optimization_metadata;
  optimization_metadata.previews_metadata.set_inflation_percent(12345);
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::NOSCRIPT, &optimization_target_decision,
      &optimization_type_decision, &optimization_metadata);
  EXPECT_EQ(0, optimization_metadata.previews_metadata.inflation_percent());

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_before_commit());
  EXPECT_FALSE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationHasHintInCacheButNotLoaded) {
  InitializeWithDefaultConfig("1.0.0.0");

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          url_with_hints());

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  optimization_guide::OptimizationMetadata optimization_metadata;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::NOSCRIPT, &optimization_target_decision,
      &optimization_type_decision, &optimization_metadata);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(
      optimization_guide::OptimizationTypeDecision::kHadHintButNotLoadedInTime,
      optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_EQ(base::nullopt, navigation_data->has_hint_before_commit());
  EXPECT_TRUE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ(base::nullopt, navigation_data->serialized_hint_version_string());
  EXPECT_FALSE(navigation_data->has_page_hint_value());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationFilterTakesPrecedence) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://m.black.com/urlinfilterandhints"));

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("black.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("https://m.black.com");
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  // Set ECT estimate so hint is activated.
  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::LITE_PAGE_REDIRECT,
      &optimization_target_decision, &optimization_type_decision,
      /*optimization_metadata=*/nullptr);

  // Make sure decision points logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kNotAllowedByOptimizationFilter,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_TRUE(navigation_data->has_hint_before_commit().value());
  EXPECT_TRUE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ("someversion", navigation_data->serialized_hint_version_string());
  ASSERT_TRUE(navigation_data->page_hint());
}

TEST_F(OptimizationGuideHintsManagerTest,
       CanApplyOptimizationFilterTakesPrecedenceWithECTComingFromHint) {
  std::unique_ptr<content::MockNavigationHandle> navigation_handle =
      CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(
          GURL("https://notfiltered.com/whatever"));

  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::LITE_PAGE_REDIRECT});

  optimization_guide::proto::Configuration config;
  optimization_guide::proto::Hint* hint1 = config.add_hints();
  hint1->set_key("notfiltered.com");
  hint1->set_key_representation(optimization_guide::proto::HOST_SUFFIX);
  hint1->set_version("someversion");
  optimization_guide::proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("https://notfiltered.com");
  page_hint1->set_max_ect_trigger(
      optimization_guide::proto::EFFECTIVE_CONNECTION_TYPE_3G);
  optimization_guide::proto::Optimization* optimization1 =
      page_hint1->add_whitelisted_optimizations();
  optimization1->set_optimization_type(
      optimization_guide::proto::LITE_PAGE_REDIRECT);
  optimization_guide::BloomFilter blacklist_bloom_filter(
      kBlackBlacklistBloomFilterNumHashFunctions,
      kBlackBlacklistBloomFilterNumBits);
  PopulateBlackBlacklistBloomFilter(&blacklist_bloom_filter);
  AddBlacklistBloomFilterToConfig(optimization_guide::proto::LITE_PAGE_REDIRECT,
                                  blacklist_bloom_filter,
                                  kBlackBlacklistBloomFilterNumHashFunctions,
                                  kBlackBlacklistBloomFilterNumBits, &config);
  ProcessHints(config, "1.0.0.0");

  base::RunLoop run_loop;
  hints_manager()->OnNavigationStartOrRedirect(navigation_handle.get(),
                                               run_loop.QuitClosure());
  run_loop.Run();

  hints_manager()->OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_3G);

  optimization_guide::OptimizationTargetDecision optimization_target_decision;
  optimization_guide::OptimizationTypeDecision optimization_type_decision;
  hints_manager()->CanApplyOptimization(
      navigation_handle.get(),
      optimization_guide::proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD,
      optimization_guide::proto::LITE_PAGE_REDIRECT,
      &optimization_target_decision, &optimization_type_decision,
      /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(optimization_guide::OptimizationTargetDecision::kPageLoadMatches,
            optimization_target_decision);
  EXPECT_EQ(optimization_guide::OptimizationTypeDecision::
                kAllowedByOptimizationFilter,
            optimization_type_decision);
  // Make sure navigation data is populated correctly.
  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle.get());
  EXPECT_TRUE(navigation_data->has_hint_before_commit().value());
  EXPECT_TRUE(navigation_data->has_hint_after_commit().value());
  EXPECT_EQ("someversion", navigation_data->serialized_hint_version_string());
  ASSERT_TRUE(navigation_data->page_hint());
}
