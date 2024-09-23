// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"

#include <memory>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/chrome_hints_manager.h"
#include "chrome/browser/optimization_guide/chrome_model_quality_logs_uploader_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/optimization_guide/core/command_line_top_host_provider.h"
#include "components/optimization_guide/core/feature_registry/mqls_feature_registry.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/core/test_hints_component_creator.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/on_device_model/public/cpp/features.h"

namespace optimization_guide {

using model_execution::prefs::ModelExecutionEnterprisePolicyValue;
using ::testing::ElementsAre;

namespace {

using proto::OptimizationType;

class ScopedSetMetricsConsent {
 public:
  // Enables or disables metrics consent based off of |consent|.
  explicit ScopedSetMetricsConsent(bool consent) : consent_(consent) {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &consent_);
  }

  ScopedSetMetricsConsent(const ScopedSetMetricsConsent&) = delete;
  ScopedSetMetricsConsent& operator=(const ScopedSetMetricsConsent&) = delete;

  ~ScopedSetMetricsConsent() {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

 private:
  const bool consent_;
};

// A WebContentsObserver that asks whether an optimization type can be applied.
class OptimizationGuideConsumerWebContentsObserver
    : public content::WebContentsObserver {
 public:
  OptimizationGuideConsumerWebContentsObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~OptimizationGuideConsumerWebContentsObserver() override = default;

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (callback_) {
      OptimizationGuideKeyedService* service =
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
      service->CanApplyOptimization(navigation_handle->GetURL(),
                                    proto::NOSCRIPT, std::move(callback_));
    }
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    OptimizationGuideKeyedService* service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    last_can_apply_optimization_decision_ = service->CanApplyOptimization(
        navigation_handle->GetURL(), proto::NOSCRIPT,
        /*optimization_metadata=*/nullptr);
  }

  // Returns the last optimization guide decision that was returned by the
  // OptimizationGuideKeyedService's CanApplyOptimization() method.
  OptimizationGuideDecision last_can_apply_optimization_decision() {
    return last_can_apply_optimization_decision_;
  }

  void set_callback(OptimizationGuideDecisionCallback callback) {
    callback_ = std::move(callback);
  }

 private:
  OptimizationGuideDecision last_can_apply_optimization_decision_ =
      OptimizationGuideDecision::kUnknown;
  OptimizationGuideDecisionCallback callback_;
};

// A WebContentsObserver that specifically calls the new API that automatically
// decided whether to use the sync or async api in the background.
class OptimizationGuideNewApiConsumerWebContentsObserver
    : public content::WebContentsObserver {
 public:
  OptimizationGuideNewApiConsumerWebContentsObserver(
      content::WebContents* web_contents,
      OptimizationGuideDecisionCallback callback)
      : content::WebContentsObserver(web_contents),
        callback_(std::move(callback)) {}
  ~OptimizationGuideNewApiConsumerWebContentsObserver() override = default;

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (callback_) {
      OptimizationGuideKeyedService* service =
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
      service->CanApplyOptimization(navigation_handle->GetURL(),
                                    proto::NOSCRIPT, std::move(callback_));
    }
  }

 private:
  OptimizationGuideDecisionCallback callback_;
};

}  // namespace

class OptimizationGuideKeyedServiceDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  OptimizationGuideKeyedServiceDisabledBrowserTest() {
    feature_list_.InitWithFeatures({}, {features::kOptimizationHints});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceDisabledBrowserTest,
                       KeyedServiceEnabledButOptimizationHintsDisabled) {
  EXPECT_EQ(nullptr, OptimizationGuideKeyedServiceFactory::GetForProfile(
                         browser()->profile()));
}

class OptimizationGuideKeyedServiceBrowserTest
    : public OptimizationGuideKeyedServiceDisabledBrowserTest {
 public:
  OptimizationGuideKeyedServiceBrowserTest()
      : network_connection_tracker_(
            network::TestNetworkConnectionTracker::CreateInstance()) {
    // Enable visibility of tab organization feature.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kOptimizationHints, {}},
         {features::kOptimizationGuideModelExecution, {}},
         {features::internal::kComposeSettingsVisibility, {}},
         {features::internal::kWallpaperSearchSettingsVisibility, {}},
         {on_device_model::features::kUseFakeChromeML, {}},
         {features::kLogOnDeviceMetricsOnStartup,
          {
              {"on_device_startup_metric_delay", "0"},
          }},
         {features::internal::kTabOrganizationSettingsVisibility,
          {{"allow_unsigned_user", "true"}}}},
        /*disabled_features=*/
        {features::internal::kWallpaperSearchGraduated,
         features::internal::kComposeGraduated,
         features::internal::kTabOrganizationGraduated});
  }

  OptimizationGuideKeyedServiceBrowserTest(
      const OptimizationGuideKeyedServiceBrowserTest&) = delete;
  OptimizationGuideKeyedServiceBrowserTest& operator=(
      const OptimizationGuideKeyedServiceBrowserTest&) = delete;

  ~OptimizationGuideKeyedServiceBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* cmd) override {
    cmd->AppendSwitch(switches::kPurgeHintsStore);
  }

  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    OptimizationGuideKeyedServiceDisabledBrowserTest::SetUpOnMainThread();

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &OptimizationGuideKeyedServiceBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_->Start());

    url_with_hints_ = https_server_->GetURL("/simple.html");
    url_that_redirects_ =
        https_server_->GetURL("/redirect?" + url_with_hints_.spec());
    url_that_redirects_to_no_hints_ =
        https_server_->GetURL("/redirect?https://nohints.com/");

    SetConnectionType(network::mojom::ConnectionType::CONNECTION_2G);

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&OptimizationGuideKeyedServiceBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  base::CallbackListSubscription create_services_subscription_;
  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_->ShutdownAndWaitUntilComplete());

    OptimizationGuideKeyedServiceDisabledBrowserTest::TearDownOnMainThread();
  }

  void RegisterWithKeyedService() {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->RegisterOptimizationTypes({proto::NOSCRIPT});

    // Set up an OptimizationGuideKeyedService consumer.
    consumer_ = std::make_unique<OptimizationGuideConsumerWebContentsObserver>(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  void CanApplyOptimizationOnDemand(
      const std::vector<GURL>& urls,
      const std::vector<proto::OptimizationType>& optimization_types,
      OnDemandOptimizationGuideDecisionRepeatingCallback callback) {
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->CanApplyOptimizationOnDemand(urls, optimization_types,
                                       proto::CONTEXT_BATCH_UPDATE_ACTIVE_TABS,
                                       callback);
  }

  PredictionManager* prediction_manager() {
    auto* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile());
    return optimization_guide_keyed_service->GetPredictionManager();
  }

  void PushHintsComponentAndWaitForCompletion() {
    RetryForHistogramUntilCountReached(
        histogram_tester(),
        "OptimizationGuide.HintsManager.HintCacheInitialized", 1);

    base::RunLoop run_loop;
    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->GetHintsManager()
        ->ListenForNextUpdateForTesting(run_loop.QuitClosure());

    const HintsComponentInfo& component_info =
        test_hints_component_creator_.CreateHintsComponentInfoWithPageHints(
            proto::NOSCRIPT, {url_with_hints_.host()}, "simple.html");

    OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent(component_info);

    run_loop.Run();
  }

  // Sets the connection type that the Network Connection Tracker will report.
  void SetConnectionType(network::mojom::ConnectionType connection_type) {
    network_connection_tracker_->SetConnectionType(connection_type);
  }

  // Sets the callback on the consumer of the OptimizationGuideKeyedService. If
  // set, this will call the async version of CanApplyOptimization.
  void SetCallbackOnConsumer(OptimizationGuideDecisionCallback callback) {
    ASSERT_TRUE(consumer_);

    consumer_->set_callback(std::move(callback));
  }

  // Returns the last decision from the CanApplyOptimization() method seen by
  // the consumer of the OptimizationGuideKeyedService.
  OptimizationGuideDecision last_can_apply_optimization_decision() {
    return consumer_->last_can_apply_optimization_decision();
  }

  OptimizationGuideKeyedService* service() {
    auto* profile = browser()->profile();
    return OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  }

  ModelExecutionFeaturesController* model_execution_features_controller() {
    return service()->model_execution_features_controller_.get();
  }

  std::unique_ptr<ModelQualityLogEntry> GetModelQualityLogEntryForCompose() {
    std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request(
        new proto::LogAiDataRequest());
    proto::ComposeLoggingData compose_logging_data;
    *(log_ai_data_request->mutable_compose()) = compose_logging_data;

    return std::make_unique<ModelQualityLogEntry>(
        std::move(log_ai_data_request),
        service()->GetModelQualityLogsUploaderService()->GetWeakPtr());
  }

  GURL url_with_hints() { return url_with_hints_; }

  GURL url_that_redirects_to_hints() { return url_that_redirects_; }

  GURL url_that_redirects_to_no_hints() {
    return url_that_redirects_to_no_hints_;
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

  void EnableSignIn() {
    auto account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@gmail.com",
                                          signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
  }

  void SignOut() {
    identity_test_env_adaptor_->identity_test_env()->ClearPrimaryAccount();
  }

  bool IsSettingVisible(UserVisibleFeatureKey feature) {
    return OptimizationGuideKeyedServiceFactory::GetForProfile(
               browser()->profile())
        ->IsSettingVisible(feature);
  }

  void SetMetricsConsent(bool consent) {
    scoped_metrics_consent_.emplace(consent);
  }

  void EnableFeature(UserVisibleFeatureKey feature) {
    // Sign in must be enabled as a prerequisite for enabling any user-visible
    // feature.
    EnableSignIn();

    auto* prefs = browser()->profile()->GetPrefs();
    prefs->SetInteger(prefs::GetSettingEnabledPrefName(feature),
                      static_cast<int>(prefs::FeatureOptInState::kEnabled));
    base::RunLoop().RunUntilIdle();
  }

  void SetEnterprisePolicy(const std::string& key,
                           ModelExecutionEnterprisePolicyValue value) {
    // Enable logging via the enterprise policy.
    policies_.Set(key, policy::POLICY_LEVEL_MANDATORY,
                  policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                  base::Value(static_cast<int>(value)), nullptr);
    policy_provider_.UpdateChromePolicy(policies_);
    base::RunLoop().RunUntilIdle();
  }

  void SetIsDogfoodClient(bool is_dogfood_client) {
    g_browser_process->variations_service()->SetIsLikelyDogfoodClientForTesting(
        is_dogfood_client);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  ::testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("redirect") == std::string::npos) {
      return nullptr;
    }

    GURL request_url = request.GetURL();
    std::string dest =
        base::UnescapeBinaryURLComponent(request_url.query_piece());

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_FOUND);
    http_response->AddCustomHeader("Location", dest);
    return http_response;
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  GURL url_with_hints_;
  GURL url_that_redirects_;
  GURL url_that_redirects_to_no_hints_;

  std::optional<ScopedSetMetricsConsent> scoped_metrics_consent_;

  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;

  // Enterprise policies. Stored as a member variable because each call to
  // `UpdateChromePolicy` clears previous updates; so accumulate the policies
  // in this `PolicyMap` instead.
  policy::PolicyMap policies_;

  testing::TestHintsComponentCreator test_hints_component_creator_;
  std::unique_ptr<OptimizationGuideConsumerWebContentsObserver> consumer_;
  // Histogram tester used specifically to capture metrics that are recorded
  // during browser initialization.
  base::HistogramTester histogram_tester_;

  // Identity test support.
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

// Configures the global VariationsService to treat this client as a likely
// dogfood client, before any keyed services are created.
class DogfoodOptimizationGuideKeyedServiceBrowserTest
    : public OptimizationGuideKeyedServiceBrowserTest {
 public:
  DogfoodOptimizationGuideKeyedServiceBrowserTest() = default;

  DogfoodOptimizationGuideKeyedServiceBrowserTest(
      const OptimizationGuideKeyedServiceBrowserTest&) = delete;
  DogfoodOptimizationGuideKeyedServiceBrowserTest& operator=(
      const OptimizationGuideKeyedServiceBrowserTest&) = delete;

  ~DogfoodOptimizationGuideKeyedServiceBrowserTest() override = default;

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    OptimizationGuideKeyedServiceBrowserTest::
        OnWillCreateBrowserContextServices(context);
    SetIsDogfoodClient(true);
  }
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       RemoteFetchingDisabled) {
  // ChromeOS has multiple profiles and optimization guide currently does not
  // run on non-Android.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.RemoteFetchingEnabled", false, 1);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      "SyntheticOptimizationGuideRemoteFetching", "Disabled"));
#endif
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithAsyncCallbackReturnsAnswerRedirect) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetCallbackOnConsumer(base::BindOnce(
      [](base::RunLoop* run_loop, OptimizationGuideDecision decision,
         const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
        run_loop->Quit();
      },
      run_loop.get()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           url_that_redirects_to_no_hints()));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithAsyncCallbackReturnsAnswer) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetCallbackOnConsumer(base::BindOnce(
      [](base::RunLoop* run_loop, OptimizationGuideDecision decision,
         const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kTrue, decision);
        run_loop->Quit();
      },
      run_loop.get()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_hints()));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithAsyncCallbackReturnsAnswerEventually) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  SetCallbackOnConsumer(base::BindOnce(
      [](base::RunLoop* run_loop, OptimizationGuideDecision decision,
         const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
        run_loop->Quit();
      },
      run_loop.get()));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://nohints.com/")));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithHintsLoadsHint) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_hints()));

  EXPECT_GT(RetryForHistogramUntilCountReached(
                &histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            0);
  // There is a hint that matches this URL, so there should be an attempt to
  // load a hint that succeeds.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
  // We had a hint and it was loaded.
  EXPECT_EQ(OptimizationGuideDecision::kTrue,
            last_can_apply_optimization_decision());

  // Navigate away so metrics get recorded.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_hints()));

  // Expect that UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName));

  const int64_t* entry_metric = ukm_recorder.GetEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName);
  EXPECT_TRUE(*entry_metric & (1 << proto::NOSCRIPT));
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       RecordsMetricsWhenTabHidden) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_hints()));

  EXPECT_GT(RetryForHistogramUntilCountReached(
                &histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            0);
  // There is a hint that matches this URL, so there should be an attempt to
  // load a hint that succeeds.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
  // We had a hint and it was loaded.
  EXPECT_EQ(OptimizationGuideDecision::kTrue,
            last_can_apply_optimization_decision());

  // Make sure metrics get recorded when tab is hidden.
  browser()->tab_strip_model()->GetActiveWebContents()->WasHidden();

  // Expect that the optimization guide UKM is recorded.
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::OptimizationGuide::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  EXPECT_TRUE(ukm_recorder.EntryHasMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName));
  const int64_t* entry_metric = ukm_recorder.GetEntryMetric(
      entry,
      ukm::builders::OptimizationGuide::kRegisteredOptimizationTypesName);
  EXPECT_TRUE(*entry_metric & (1 << proto::NOSCRIPT));
}

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServiceBrowserTest,
    NavigateToPageThatRedirectsToUrlWithHintsShouldAttemptTwoLoads) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), url_that_redirects_to_hints()));

  EXPECT_EQ(RetryForHistogramUntilCountReached(
                &histogram_tester, "OptimizationGuide.LoadedHint.Result", 2),
            2);
  // Should attempt and succeed to load a hint once for the initial navigation
  // and redirect.
  histogram_tester.ExpectBucketCount("OptimizationGuide.LoadedHint.Result",
                                     true, 2);
  // Hint is still applicable so we expect it to be allowed to be applied.
  EXPECT_EQ(OptimizationGuideDecision::kTrue,
            last_can_apply_optimization_decision());
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       NavigateToPageWithoutHint) {
  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://nohints.com/")));

  EXPECT_EQ(RetryForHistogramUntilCountReached(
                &histogram_tester, "OptimizationGuide.LoadedHint.Result", 1),
            1);
  // There were no hints that match this URL, but there should still be an
  // attempt to load a hint but still fail.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      false, 1);
  EXPECT_EQ(OptimizationGuideDecision::kFalse,
            last_can_apply_optimization_decision());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.NoScript",
      static_cast<int>(OptimizationTypeDecision::kNoHintAvailable), 1);
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       CheckForBlocklistFilter) {
  PushHintsComponentAndWaitForCompletion();

  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());

  {
    base::HistogramTester histogram_tester;

    // Register an optimization type with an optimization filter.
    ogks->RegisterOptimizationTypes({proto::FAST_HOST_HINTS});
    // Wait until filter is loaded. This histogram will record twice: once when
    // the config is found and once when the filter is created.
    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.OptimizationFilterStatus.FastHostHints", 2);

    EXPECT_EQ(
        OptimizationGuideDecision::kFalse,
        ogks->CanApplyOptimization(GURL("https://blockedhost.com/whatever"),
                                   proto::FAST_HOST_HINTS, nullptr));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ApplyDecision.FastHostHints",
        static_cast<int>(
            OptimizationTypeDecision::kNotAllowedByOptimizationFilter),
        1);
  }

  // Register another type with optimization filter.
  {
    base::HistogramTester histogram_tester;
    ogks->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});
    // Wait until filter is loaded. This histogram will record twice: once when
    // the config is found and once when the filter is created.
    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 2);

    // The previously loaded filter should still be loaded and give the same
    // result.
    EXPECT_EQ(
        OptimizationGuideDecision::kFalse,
        ogks->CanApplyOptimization(GURL("https://blockedhost.com/whatever"),
                                   proto::FAST_HOST_HINTS, nullptr));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ApplyDecision.FastHostHints",
        static_cast<int>(
            OptimizationTypeDecision::kNotAllowedByOptimizationFilter),
        1);
  }
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       CanApplyOptimizationOnDemand) {
  PushHintsComponentAndWaitForCompletion();
  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
  ogks->RegisterOptimizationTypes({proto::OptimizationType::NOSCRIPT,
                                   proto::OptimizationType::FAST_HOST_HINTS});

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_hints()));
  RetryForHistogramUntilCountReached(&histogram_tester,
                                     "OptimizationGuide.LoadedHint.Result", 1);

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  base::flat_set<GURL> received_callbacks;
  CanApplyOptimizationOnDemand(
      {url_with_hints(), GURL("https://blockedhost.com/whatever")},
      {proto::OptimizationType::NOSCRIPT,
       proto::OptimizationType::FAST_HOST_HINTS},
      base::BindRepeating(
          [](base::RunLoop* run_loop, base::flat_set<GURL>* received_callbacks,
             const GURL& url,
             const base::flat_map<proto::OptimizationType,
                                  OptimizationGuideDecisionWithMetadata>&
                 decisions) {
            received_callbacks->insert(url);

            // Expect one decision per requested type.
            EXPECT_EQ(decisions.size(), 2u);

            if (received_callbacks->size() == 2) {
              run_loop->Quit();
            }
          },
          run_loop.get(), &received_callbacks));
  run_loop->Run();
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       CanApplyOptimizationNewAPI) {
  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
  ogks->RegisterOptimizationTypes({proto::OptimizationType::NOSCRIPT});
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();

  // Before the hints or navigation are initiated, we should get a negative
  // response.
  ogks->CanApplyOptimization(
      url_with_hints(), proto::OptimizationType::NOSCRIPT,
      base::BindOnce(
          [](base::RunLoop* run_loop, OptimizationGuideDecision decision,
             const OptimizationMetadata& metadata) {
            EXPECT_EQ(decision, OptimizationGuideDecision::kFalse);

            run_loop->Quit();
          },
          run_loop.get()));
  run_loop->Run();

  // Now attach a WebContentsObserver to make a request while a navigation is
  // in progress.
  run_loop = std::make_unique<base::RunLoop>();
  OptimizationGuideNewApiConsumerWebContentsObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::BindOnce(
          [](base::RunLoop* run_loop, OptimizationGuideDecision decision,
             const OptimizationMetadata& metadata) {
            EXPECT_EQ(OptimizationGuideDecision::kTrue, decision);
            run_loop->Quit();
          },
          run_loop.get()));

  PushHintsComponentAndWaitForCompletion();
  RegisterWithKeyedService();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_with_hints()));
  run_loop->Run();

  // After the navigation has finished, we should still be able to query and
  // get the correct response.
  run_loop = std::make_unique<base::RunLoop>();
  ogks->CanApplyOptimization(
      url_with_hints(), proto::OptimizationType::NOSCRIPT,
      base::BindOnce(
          [](base::RunLoop* run_loop, OptimizationGuideDecision decision,
             const OptimizationMetadata& metadata) {
            EXPECT_EQ(decision, OptimizationGuideDecision::kTrue);

            run_loop->Quit();
          },
          run_loop.get()));
  run_loop->Run();
}

class TestSettingsEnabledObserver : public SettingsEnabledObserver {
 public:
  explicit TestSettingsEnabledObserver(UserVisibleFeatureKey feature)
      : SettingsEnabledObserver(feature) {}
  void OnChangeInFeatureCurrentlyEnabledState(bool is_now_enabled) override {
    count_feature_enabled_state_changes_++;
    is_currently_enabled_ = is_now_enabled;
  }

  int count_feature_enabled_state_changes_ = 0;
  bool is_currently_enabled_ = false;
};

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       SettingsVisibilitySignedOutVsSignedIn) {
  // User is not signed-in.
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kWallpaperSearch));

  // Visibility of tab organizer is allowed for unsigned users.
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));

  // Visibility of this feature is enabled via finch but the feature is still
  // not visible.
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kCompose));

  // kCompose should now be visible after
  // sign-in.
  EnableSignIn();

  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));

  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kCompose));

#if !BUILDFLAG(IS_CHROMEOS)
  // SignOut not supported on ChromeOS.
  SignOut();
  // Tab Organizer is visible to unsigned users.
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));
  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
#endif
}

// Verifies that Model Execution Features Controller is available for incognito
// profiles and the visibility of settings is correct.
IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       SettingsVisibilityUpdatedCorrectly) {
  EnableSignIn();

  // Visibility of wallpaper search is enabled on ToT.
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kWallpaperSearch));

  // Visibility of tab organizer is enabled via finch.
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));

  // Visibility of compose is enabled via finch.
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kCompose));

  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kWallpaperSearch),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));

  // Restarting the browser should cause wallpaper setting to be visible since
  // the feature is enabled.
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));

  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kCompose));

  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kWallpaperSearch),
      static_cast<int>(prefs::FeatureOptInState::kDisabled));

  // Restarting the browser should cause wallpaper setting to still be visible
  // since the feature is still enabled.
  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));

  EXPECT_TRUE(IsSettingVisible(UserVisibleFeatureKey::kCompose));
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       SettingsOptInRevokedAfterSignOut) {
  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());

  EnableSignIn();

  TestSettingsEnabledObserver wallpaper_search_observer(
      UserVisibleFeatureKey::kWallpaperSearch);
  TestSettingsEnabledObserver compose_observer(UserVisibleFeatureKey::kCompose);

  ogks->AddModelExecutionSettingsEnabledObserver(&wallpaper_search_observer);
  ogks->AddModelExecutionSettingsEnabledObserver(&compose_observer);

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kCompose));

  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kWallpaperSearch),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  EXPECT_EQ(1, wallpaper_search_observer.count_feature_enabled_state_changes_);
  EXPECT_TRUE(wallpaper_search_observer.is_currently_enabled_);
  EXPECT_EQ(0, compose_observer.count_feature_enabled_state_changes_);

  EXPECT_TRUE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kCompose));

#if !BUILDFLAG(IS_CHROMEOS)
  // SignOut not supported on ChromeOS.
  SignOut();

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kCompose));

  EXPECT_EQ(2, wallpaper_search_observer.count_feature_enabled_state_changes_);
  EXPECT_FALSE(wallpaper_search_observer.is_currently_enabled_);
#endif
}

// Verifies that Model Execution Features Controller is available for incognito
// profiles and the setting opt-in toggle and pref is updated correctly.
IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       SettingsOptInUpdatedCorrectly) {
  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());

  EnableSignIn();

  TestSettingsEnabledObserver wallpaper_search_observer(
      UserVisibleFeatureKey::kWallpaperSearch);
  TestSettingsEnabledObserver compose_observer(UserVisibleFeatureKey::kCompose);

  ogks->AddModelExecutionSettingsEnabledObserver(&wallpaper_search_observer);
  ogks->AddModelExecutionSettingsEnabledObserver(&compose_observer);

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kCompose));

  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kWallpaperSearch),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  EXPECT_EQ(1, wallpaper_search_observer.count_feature_enabled_state_changes_);
  EXPECT_TRUE(wallpaper_search_observer.is_currently_enabled_);
  EXPECT_EQ(0, compose_observer.count_feature_enabled_state_changes_);

  EXPECT_TRUE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kCompose));

  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kWallpaperSearch),
      static_cast<int>(prefs::FeatureOptInState::kDisabled));
  EXPECT_EQ(2, wallpaper_search_observer.count_feature_enabled_state_changes_);
  EXPECT_FALSE(wallpaper_search_observer.is_currently_enabled_);
  EXPECT_EQ(0, compose_observer.count_feature_enabled_state_changes_);

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kCompose));
}

// Verifies that Model Execution Features Controller updates feature prefs
// correctly when the main toggle pref changes.
IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       MainToggleUpdatesSettingsCorrectly) {
  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());

  EnableSignIn();

  TestSettingsEnabledObserver wallpaper_search_observer(
      UserVisibleFeatureKey::kWallpaperSearch);
  TestSettingsEnabledObserver compose_observer(UserVisibleFeatureKey::kCompose);
  TestSettingsEnabledObserver tab_observer(
      UserVisibleFeatureKey::kTabOrganization);

  ogks->AddModelExecutionSettingsEnabledObserver(&wallpaper_search_observer);
  ogks->AddModelExecutionSettingsEnabledObserver(&compose_observer);
  ogks->AddModelExecutionSettingsEnabledObserver(&tab_observer);

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kCompose));

  // Enable the main feature toggle. This should enable the compose and tab
  // organizer features on restart.
  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(prefs::kModelExecutionMainToggleSettingState,
                    static_cast<int>(prefs::FeatureOptInState::kEnabled));
  // Visibility of tab organizer feature is enabled via finch. Only tab
  // organizer feature should be enabled.
  EXPECT_EQ(1, wallpaper_search_observer.count_feature_enabled_state_changes_);
  EXPECT_TRUE(wallpaper_search_observer.is_currently_enabled_);
  EXPECT_EQ(1, compose_observer.count_feature_enabled_state_changes_);
  EXPECT_TRUE(compose_observer.is_currently_enabled_);
  EXPECT_EQ(1, tab_observer.count_feature_enabled_state_changes_);
  EXPECT_TRUE(tab_observer.is_currently_enabled_);

  EXPECT_TRUE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_TRUE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));

  EXPECT_TRUE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kCompose));

  // Disable main toggle. The tab organizer feature should be disabled on
  // restart.
  prefs->SetInteger(prefs::kModelExecutionMainToggleSettingState,
                    static_cast<int>(prefs::FeatureOptInState::kDisabled));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, wallpaper_search_observer.count_feature_enabled_state_changes_);
  EXPECT_FALSE(wallpaper_search_observer.is_currently_enabled_);
  EXPECT_EQ(2, compose_observer.count_feature_enabled_state_changes_);
  EXPECT_FALSE(compose_observer.is_currently_enabled_);
  EXPECT_EQ(2, tab_observer.count_feature_enabled_state_changes_);
  EXPECT_FALSE(tab_observer.is_currently_enabled_);

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kTabOrganization));

  EXPECT_FALSE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kCompose));
}

// Verifies that Model Execution Features Controller returns null for incognito
// profiles.
IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       SettingsVisibilityIncognito) {
  EnableSignIn();

  // Set up incognito browser and incognito OptimizationGuideKeyedService
  // consumer.
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());
  EXPECT_TRUE(otr_browser);

  // Instantiate off the record Optimization Guide Service.
  OptimizationGuideKeyedService* otr_ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          browser()->profile()->GetPrimaryOTRProfile(
              /*create_if_needed=*/true));

  auto* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kWallpaperSearch),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));

  EXPECT_FALSE(otr_ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       LogOnDeviceMetricsAfterStart) {
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
  OnDeviceModelComponentStateManager* on_device_component_state_manager =
      OnDeviceModelComponentStateManager::GetInstanceForTesting();
  ASSERT_TRUE(on_device_component_state_manager);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester()
               ->GetAllSamples(
                   "OptimizationGuide.ModelExecution."
                   "OnDeviceModelPerformanceClass")
               .size() > 0;
  }));

  histogram_tester()->ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelPerformanceClass", 1);
  histogram_tester()->ExpectBucketCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelPerformanceClass",
      OnDeviceModelPerformanceClass::kServiceCrash, 0);
}

// Creating multiple profiles isn't supported easily on ash and android.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       LogOnDeviceMetricsSingleTimeForMultipleProfiles) {
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());
  OnDeviceModelComponentStateManager* on_device_component_state_manager =
      OnDeviceModelComponentStateManager::GetInstanceForTesting();
  ASSERT_TRUE(on_device_component_state_manager);

  // Add a second profile which should not log performance class.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath path = profile_manager->GenerateNextProfileDirectoryPath();
  ProfileWaiter profile_waiter;
  profile_manager->CreateProfileAsync(path, {});
  profile_waiter.WaitForProfileAdded();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester()
               ->GetAllSamples(
                   "OptimizationGuide.ModelExecution."
                   "OnDeviceModelPerformanceClass")
               .size() > 0;
  }));

  // Make sure all tasks have finished running.
  content::RunAllTasksUntilIdle();

  histogram_tester()->ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelPerformanceClass", 1);
}
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
// CreateGuestBrowser() is not supported for Android or ChromeOS out of the box.
IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       GuestProfileUniqueKeyedService) {
  Browser* guest_browser = CreateGuestBrowser();
  OptimizationGuideKeyedService* guest_ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          guest_browser->profile());
  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile());

  EXPECT_TRUE(guest_ogks);
  EXPECT_TRUE(ogks);
  EXPECT_NE(guest_ogks, ogks);

  auto* prefs = browser()->profile()->GetPrefs();
  auto* guest_prefs = guest_browser->profile()->GetPrefs();

  EnableSignIn();

  prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kWallpaperSearch),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));
  guest_prefs->SetInteger(
      prefs::GetSettingEnabledPrefName(UserVisibleFeatureKey::kWallpaperSearch),
      static_cast<int>(prefs::FeatureOptInState::kEnabled));

  EXPECT_TRUE(ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));
  EXPECT_FALSE(guest_ogks->ShouldFeatureBeCurrentlyEnabledForUser(
      UserVisibleFeatureKey::kWallpaperSearch));
}
#endif

// Test the visibility of features with `kOptimizationGuideModelExecution`
// enabled or disabled.
class OptimizationGuideKeyedServiceBrowserWithModelExecutionFeatureDisabledTest
    : public ::testing::WithParamInterface<bool>,
      public OptimizationGuideKeyedServiceBrowserTest {
 public:
  OptimizationGuideKeyedServiceBrowserWithModelExecutionFeatureDisabledTest()
      : OptimizationGuideKeyedServiceBrowserTest() {
    // Enable visibility of tab organization feature.
    scoped_feature_list_.Reset();

    if (ShouldFeatureBeEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kOptimizationHints,
           // Enabled.
           features::kOptimizationGuideModelExecution,
           features::internal::kTabOrganizationSettingsVisibility},
          {features::internal::kTabOrganizationGraduated});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {features::kOptimizationHints,
           features::internal::kTabOrganizationSettingsVisibility},
          // Disabled.
          {features::kOptimizationGuideModelExecution,
           features::internal::kTabOrganizationGraduated});
    }
  }

  bool ShouldFeatureBeEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    OptimizationGuideKeyedServiceBrowserWithModelExecutionFeatureDisabledTest,
    ::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    OptimizationGuideKeyedServiceBrowserWithModelExecutionFeatureDisabledTest,
    SettingsNotVisible) {
  EnableSignIn();

  EXPECT_FALSE(IsSettingVisible(UserVisibleFeatureKey::kWallpaperSearch));

  EXPECT_EQ(ShouldFeatureBeEnabled(),
            IsSettingVisible(UserVisibleFeatureKey::kTabOrganization));
}

class OptimizationGuideKeyedServicePermissionsCheckDisabledTest
    : public OptimizationGuideKeyedServiceBrowserTest {
 public:
  OptimizationGuideKeyedServicePermissionsCheckDisabledTest() = default;
  ~OptimizationGuideKeyedServicePermissionsCheckDisabledTest() override =
      default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kRemoteOptimizationGuideFetching);

    OptimizationGuideKeyedServiceBrowserTest::SetUp();
  }

  void TearDown() override {
    OptimizationGuideKeyedServiceBrowserTest::TearDown();

    scoped_feature_list_.Reset();
  }

  void SetUpCommandLine(base::CommandLine* cmd) override {
    OptimizationGuideKeyedServiceBrowserTest::SetUpCommandLine(cmd);

    cmd->AppendSwitch(switches::kDisableCheckingUserPermissionsForTesting);

    // Add switch to avoid racing navigations in the test.
    cmd->AppendSwitch(
        switches::kDisableFetchingHintsAtNavigationStartForTesting);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServicePermissionsCheckDisabledTest,
    RemoteFetchingAllowed) {
  // ChromeOS has multiple profiles and optimization guide currently does not
  // run on non-Android.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  histogram_tester()->ExpectUniqueSample(
      "OptimizationGuide.RemoteFetchingEnabled", true, 1);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      "SyntheticOptimizationGuideRemoteFetching", "Enabled"));
#endif
}

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServicePermissionsCheckDisabledTest,
    IncognitoCanStillReadFromComponentHints) {
  // Wait until initialization logic finishes running and component pushed to
  // both incognito and regular browsers.
  PushHintsComponentAndWaitForCompletion();

  // Set up incognito browser and incognito OptimizationGuideKeyedService
  // consumer.
  Browser* otr_browser = CreateIncognitoBrowser(browser()->profile());

  // Instantiate off the record Optimization Guide Service.
  OptimizationGuideKeyedService* otr_ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          browser()->profile()->GetPrimaryOTRProfile(
              /*create_if_needed=*/true));
  otr_ogks->RegisterOptimizationTypes({proto::NOSCRIPT});

  // Navigate to a URL that has a hint from a component and wait for that hint
  // to have loaded.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(otr_browser, url_with_hints()));
  RetryForHistogramUntilCountReached(&histogram_tester,
                                     "OptimizationGuide.LoadedHint.Result", 1);

  EXPECT_EQ(OptimizationGuideDecision::kTrue,
            otr_ogks->CanApplyOptimization(url_with_hints(), proto::NOSCRIPT,
                                           nullptr));
}

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServicePermissionsCheckDisabledTest,
    IncognitoStillProcessesBloomFilter) {
  PushHintsComponentAndWaitForCompletion();

  CreateIncognitoBrowser(browser()->profile());

  // Instantiate off the record Optimization Guide Service.
  OptimizationGuideKeyedService* otr_ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          browser()->profile()->GetPrimaryOTRProfile(
              /*create_if_needed=*/true));

  base::HistogramTester histogram_tester;

  // Register an optimization type with an optimization filter.
  otr_ogks->RegisterOptimizationTypes({proto::FAST_HOST_HINTS});
  // Wait until filter is loaded. This histogram will record twice: once when
  // the config is found and once when the filter is created.
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.OptimizationFilterStatus.FastHostHints", 2);

  EXPECT_EQ(
      OptimizationGuideDecision::kFalse,
      otr_ogks->CanApplyOptimization(GURL("https://blockedhost.com/whatever"),
                                     proto::FAST_HOST_HINTS, nullptr));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.FastHostHints",
      static_cast<int>(
          OptimizationTypeDecision::kNotAllowedByOptimizationFilter),
      1);
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       CheckUploadWithMetricsConsent) {
  // Enable metrics consent.
  SetMetricsConsent(true);
  ASSERT_TRUE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  // Attempt to upload a new quality log.
  ModelQualityLogEntry::Upload(GetModelQualityLogEntryForCompose());

  // Upload shouldn't be blocked by metrics consent.
  histogram_tester()->ExpectBucketCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      ModelQualityLogsUploadStatus::kMetricsReportingDisabled, 0);

  // Disable metrics consent.
  SetMetricsConsent(false);
  ASSERT_FALSE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  // Attempt to upload a new quality log.
  ModelQualityLogEntry::Upload(GetModelQualityLogEntryForCompose());

  // Upload should be disabled as there is no metrics consent, so total
  // histogram bucket count will be 1.
  histogram_tester()->ExpectBucketCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      ModelQualityLogsUploadStatus::kMetricsReportingDisabled, 1);
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       CheckUploadWithoutMetricsConsent) {
  auto* profile = browser()->profile();
  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);

  // Disable metrics consent.
  SetMetricsConsent(false);
  ASSERT_FALSE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_FALSE(
      ogks->GetModelQualityLogsUploaderService()->CanUploadLogs(metadata));

  // Upload should be disabled as there is no metrics consent, so total
  // histogram bucket count will be 1.
  histogram_tester()->ExpectBucketCount(
      "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus.Compose",
      ModelQualityLogsUploadStatus::kMetricsReportingDisabled, 1);
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       CheckUploadOnDestructionWithoutMetricsConsent) {
  // Disable metrics consent.
  SetMetricsConsent(false);
  ASSERT_FALSE(
      g_browser_process->GetMetricsServicesManager()->IsMetricsConsentGiven());

  // Intercept network requests.
  network::TestURLLoaderFactory url_loader_factory;
  service()
      ->GetModelQualityLogsUploaderService()
      ->SetUrlLoaderFactoryForTesting(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &url_loader_factory));

  // Create a new ModelQualityLogEntry for compose.
  std::unique_ptr<ModelQualityLogEntry> log_entry =
      GetModelQualityLogEntryForCompose();

  // Destruct the log entry, this should trigger uploading the logs.
  log_entry.reset();

  // Upload should be stopped on destruction as there is no metrics consent.
  base::RunLoop().RunUntilIdle();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, url_loader_factory.NumPending());
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       CheckUploadWithEnterprisePolicy) {
  // Enable metrics consent and sign in.
  SetMetricsConsent(true);
  EnableSignIn();

  auto* profile = browser()->profile();
  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  auto compose_feature = UserVisibleFeatureKey::kCompose;
  auto* prefs = profile->GetPrefs();
  prefs->SetInteger(prefs::GetSettingEnabledPrefName(compose_feature),
                    static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();

  policy::PolicyMap policies;

  // Disable logging via the enterprise policy to state kAllowWithoutLogging.
  policies.Set(policy::key::kHelpMeWriteSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   model_execution::prefs::ModelExecutionEnterprisePolicyValue::
                       kAllowWithoutLogging)),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_FALSE(model_execution_features_controller()
                   ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));

  // Attempt to upload a new quality log.
  ModelQualityLogEntry::Upload(GetModelQualityLogEntryForCompose());

  // Disable logging via via the enterprise policy to kDisable state.
  policies.Set(policy::key::kHelpMeWriteSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   model_execution::prefs::ModelExecutionEnterprisePolicyValue::
                       kDisable)),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(model_execution_features_controller()
                   ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));

  // Attempt to upload a new quality log.
  ModelQualityLogEntry::Upload(GetModelQualityLogEntryForCompose());

  // Enable logging via via the enterprise policy to state kAllow this shouldn't
  // stop upload.
  policies.Set(
      policy::key::kHelpMeWriteSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(
          model_execution::prefs::ModelExecutionEnterprisePolicyValue::kAllow)),
      nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  prefs->SetInteger(prefs::GetSettingEnabledPrefName(compose_feature),
                    static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(model_execution_features_controller()
                  ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));

  EXPECT_TRUE(
      ogks->GetModelQualityLogsUploaderService()->CanUploadLogs(metadata));

  // Attempt to upload a new quality log.
  ModelQualityLogEntry::Upload(GetModelQualityLogEntryForCompose());

  // Log uploads should have been recorded as disabled twice because of
  // enterprise policy.
  EXPECT_THAT(
      histogram_tester()->GetAllSamples(
          "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus."
          "Compose"),
      ElementsAre(
          base::Bucket(
              ModelQualityLogsUploadStatus::kDisabledDueToEnterprisePolicy, 1),
          base::Bucket(ModelQualityLogsUploadStatus::kFeatureNotEnabledForUser,
                       1)));
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       CheckCanUploadLogsWithEnterprisePolicy) {
  // Enable metrics consent and sign in.
  SetMetricsConsent(true);
  EnableSignIn();

  auto* profile = browser()->profile();
  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  auto compose_feature = UserVisibleFeatureKey::kCompose;
  auto* prefs = profile->GetPrefs();
  prefs->SetInteger(prefs::GetSettingEnabledPrefName(compose_feature),
                    static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();

  policy::PolicyMap policies;

  // Disable logging via via the enterprise policy to state
  // kAllowWithoutLogging this should return
  // ChromeModelQualityLogsUploaderService::CanUploadLogs to false.
  policies.Set(policy::key::kHelpMeWriteSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   model_execution::prefs::ModelExecutionEnterprisePolicyValue::
                       kAllowWithoutLogging)),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_FALSE(model_execution_features_controller()
                   ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));

  EXPECT_FALSE(
      ogks->GetModelQualityLogsUploaderService()->CanUploadLogs(metadata));

  // Disable logging via the enterprise policy to kDisable state this should
  // return ChromeModelQualityLogsUploaderService::CanUploadLogs to false.
  policies.Set(policy::key::kHelpMeWriteSettings,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(
                   model_execution::prefs::ModelExecutionEnterprisePolicyValue::
                       kDisable)),
               nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(model_execution_features_controller()
                   ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));

  EXPECT_FALSE(
      ogks->GetModelQualityLogsUploaderService()->CanUploadLogs(metadata));

  // Enable logging via the enterprise policy to state kAllow this shouldn't
  // stop upload and should return
  // ChromeModelQualityLogsUploaderService::CanUploadLogs to true.
  policies.Set(
      policy::key::kHelpMeWriteSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(
          model_execution::prefs::ModelExecutionEnterprisePolicyValue::kAllow)),
      nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  prefs->SetInteger(prefs::GetSettingEnabledPrefName(compose_feature),
                    static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(model_execution_features_controller()
                  ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));

  EXPECT_TRUE(
      ogks->GetModelQualityLogsUploaderService()->CanUploadLogs(metadata));

  // Log uploads should have been recorded as disabled twice because of
  // enterprise policy.
  EXPECT_THAT(
      histogram_tester()->GetAllSamples(
          "OptimizationGuide.ModelQualityLogsUploaderService.UploadStatus."
          "Compose"),
      ElementsAre(
          base::Bucket(
              ModelQualityLogsUploadStatus::kDisabledDueToEnterprisePolicy, 1),
          base::Bucket(ModelQualityLogsUploadStatus::kFeatureNotEnabledForUser,
                       1)));
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       LoggingDisabledByEnterprisePolicy_NonDogfood_NoSwitch) {
  auto compose_feature = UserVisibleFeatureKey::kCompose;
  EnableFeature(compose_feature);
  SetEnterprisePolicy(
      policy::key::kHelpMeWriteSettings,
      ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_FALSE(model_execution_features_controller()
                   ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

IN_PROC_BROWSER_TEST_F(
    OptimizationGuideKeyedServiceBrowserTest,
    LoggingDisabledByEnterprisePolicy_NonDogfood_WithSwitch) {
  auto compose_feature = UserVisibleFeatureKey::kCompose;
  EnableFeature(compose_feature);
  SetEnterprisePolicy(
      policy::key::kHelpMeWriteSettings,
      ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableModelQualityDogfoodLogging);

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_FALSE(model_execution_features_controller()
                   ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

IN_PROC_BROWSER_TEST_F(DogfoodOptimizationGuideKeyedServiceBrowserTest,
                       LoggingDisabledByEnterprisePolicy_Dogfood_NoSwitch) {
  auto compose_feature = UserVisibleFeatureKey::kCompose;
  EnableFeature(compose_feature);
  SetEnterprisePolicy(
      policy::key::kHelpMeWriteSettings,
      ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_FALSE(model_execution_features_controller()
                   ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

IN_PROC_BROWSER_TEST_F(DogfoodOptimizationGuideKeyedServiceBrowserTest,
                       LoggingDisabledByEnterprisePolicy_Dogfood_WithSwitch) {
  auto compose_feature = UserVisibleFeatureKey::kCompose;
  EnableFeature(compose_feature);
  SetEnterprisePolicy(
      policy::key::kHelpMeWriteSettings,
      ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableModelQualityDogfoodLogging);

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_TRUE(model_execution_features_controller()
                  ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       FeedbackIsEnabledWhenLoggingIsEnabled) {
  auto compose_feature = UserVisibleFeatureKey::kCompose;
  EnableFeature(compose_feature);
  SetEnterprisePolicy(policy::key::kHelpMeWriteSettings,
                      ModelExecutionEnterprisePolicyValue::kAllow);

  EXPECT_TRUE(service()->ShouldFeatureBeCurrentlyAllowedForFeedback(
      proto::LogAiDataRequest::FeatureCase::kCompose));
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       FeedbackIsDisabledWhenLoggingIsDisabled_NotDogfood) {
  auto compose_feature = UserVisibleFeatureKey::kCompose;
  EnableFeature(compose_feature);
  SetEnterprisePolicy(
      policy::key::kHelpMeWriteSettings,
      ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);
  SetIsDogfoodClient(false);

  EXPECT_FALSE(service()->ShouldFeatureBeCurrentlyAllowedForFeedback(
      proto::LogAiDataRequest::FeatureCase::kCompose));
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       FeedbackIsEnabledWhenLoggingIsDisabled_Dogfood) {
  auto compose_feature = UserVisibleFeatureKey::kCompose;
  EnableFeature(compose_feature);
  SetEnterprisePolicy(
      policy::key::kHelpMeWriteSettings,
      ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);
  SetIsDogfoodClient(true);

  EXPECT_TRUE(service()->ShouldFeatureBeCurrentlyAllowedForFeedback(
      proto::LogAiDataRequest::FeatureCase::kCompose));
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       FeedbackIsDisabledWhenFeatureIsDisabled_Dogfood) {
  // Note: Unlike the tests above, do not enable the feature; leave it in the
  // default state.
  SetEnterprisePolicy(policy::key::kHelpMeWriteSettings,
                      ModelExecutionEnterprisePolicyValue::kDisable);
  SetIsDogfoodClient(true);

  EXPECT_FALSE(service()->ShouldFeatureBeCurrentlyAllowedForFeedback(
      proto::LogAiDataRequest::FeatureCase::kCompose));
}

IN_PROC_BROWSER_TEST_F(OptimizationGuideKeyedServiceBrowserTest,
                       CheckModelQualityLogsUploadOnDestruction) {
  // Enable metrics consent and sign in.
  SetMetricsConsent(true);
  EnableSignIn();

  auto* profile = browser()->profile();
  OptimizationGuideKeyedService* ogks =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  auto compose_feature = UserVisibleFeatureKey::kCompose;
  auto* prefs = profile->GetPrefs();
  policy::PolicyMap policies;

  // Enable logging via via the enterprise policy to state kAllow this shouldn't
  // stop upload and should return
  // ChromeModelQualityLogsUploaderService::CanUploadLogs to true.
  policies.Set(
      policy::key::kHelpMeWriteSettings, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::Value(static_cast<int>(
          model_execution::prefs::ModelExecutionEnterprisePolicyValue::kAllow)),
      nullptr);
  policy_provider_.UpdateChromePolicy(policies);
  prefs->SetInteger(prefs::GetSettingEnabledPrefName(compose_feature),
                    static_cast<int>(prefs::FeatureOptInState::kEnabled));
  base::RunLoop().RunUntilIdle();

  const MqlsFeatureMetadata* metadata =
      MqlsFeatureRegistry::GetInstance().GetFeature(
          proto::LogAiDataRequest::FeatureCase::kCompose);
  EXPECT_TRUE(model_execution_features_controller()
                  ->ShouldFeatureBeCurrentlyAllowedForLogging(metadata));

  EXPECT_TRUE(
      ogks->GetModelQualityLogsUploaderService()->CanUploadLogs(metadata));

  // Intercept network requests.
  network::TestURLLoaderFactory url_loader_factory;
  service()
      ->GetModelQualityLogsUploaderService()
      ->SetUrlLoaderFactoryForTesting(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &url_loader_factory));

  // Create a new ModelQualityLogEntry for compose.
  std::unique_ptr<ModelQualityLogEntry> log_entry =
      GetModelQualityLogEntryForCompose();

  // Destruct the log entry, this should upload the logs.
  log_entry.reset();

  // Logs should be uploaded on destruction.
  base::RunLoop().RunUntilIdle();
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(1, url_loader_factory.NumPending());
}

}  // namespace optimization_guide
