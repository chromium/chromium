// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_file_value_serializer.h"
#include "base/json/values_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_mixin.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/browsing_topics_service_impl.h"
#include "components/browsing_topics/epoch_topics.h"
#include "components/browsing_topics/test_util.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browsing_topics_site_data_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_topics_test_util.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace browsing_topics {

namespace {

constexpr browsing_topics::HmacKey kTestKey = {1};

constexpr base::Time kTime1 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
constexpr base::Time kTime2 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(2));

constexpr int kConfigVersion = 1;
constexpr int kTaxonomyVersion = 1;
constexpr int64_t kModelVersion = 2;
constexpr size_t kPaddedTopTopicsStartIndex = 5;
constexpr Topic kExpectedTopic1 = Topic(1);
constexpr Topic kExpectedTopic2 = Topic(10);

constexpr char kExpectedApiResult[] =
    "[{\"configVersion\":\"chrome.1\",\"modelVersion\":\"2\","
    "\"taxonomyVersion\":\"1\",\"topic\":1,\"version\":\"chrome.1:1:2\"};{"
    "\"configVersion\":\"chrome.1\",\"modelVersion\":\"2\","
    "\"taxonomyVersion\":\"1\",\"topic\":10,\"version\":\"chrome.1:1:2\"};]";

constexpr char kExpectedHeaderValueForEmptyTopics[] =
    "();p=P0000000000000000000000000000000";

constexpr char kExpectedHeaderValueForSiteA[] =
    "(1 10);v=chrome.1:1:2, ();p=P00000000";

constexpr char kExpectedHeaderValueForSiteB[] =
    "(1 7);v=chrome.1:1:2, ();p=P000000000";

static constexpr char kBrowsingTopicsApiActionTypeHistogramId[] =
    "BrowsingTopics.ApiActionType";

static constexpr char kRedirectCountHistogramId[] =
    "BrowsingTopics.PageLoad.OnTopicsFirstInvoked.RedirectCount";

static constexpr char kRedirectWithTopicsInvokedCountHistogramId[] =
    "BrowsingTopics.PageLoad.OnTopicsFirstInvoked."
    "RedirectWithTopicsInvokedCount";

EpochTopics CreateTestEpochTopics(
    const std::vector<std::pair<Topic, std::set<HashedDomain>>>& topics,
    base::Time calculation_time) {
  DCHECK_EQ(topics.size(), 5u);

  std::vector<TopicAndDomains> top_topics_and_observing_domains;
  for (size_t i = 0; i < 5; ++i) {
    top_topics_and_observing_domains.emplace_back(topics[i].first,
                                                  topics[i].second);
  }

  return EpochTopics(std::move(top_topics_and_observing_domains),
                     kPaddedTopTopicsStartIndex, kConfigVersion,
                     kTaxonomyVersion, kModelVersion, calculation_time,
                     /*from_manually_triggered_calculation=*/false);
}

}  // namespace

// A tester class that allows waiting for the first calculation to finish.
class TesterBrowsingTopicsService : public BrowsingTopicsServiceImpl {
 public:
  TesterBrowsingTopicsService(
      const base::FilePath& profile_path,
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      std::unique_ptr<Annotator> annotator,
      base::OnceClosure calculation_finish_callback)
      : BrowsingTopicsServiceImpl(
            profile_path,
            privacy_sandbox_settings,
            history_service,
            site_data_manager,
            std::move(annotator),
            base::BindRepeating(
                content_settings::PageSpecificContentSettings::TopicAccessed)),
        calculation_finish_callback_(std::move(calculation_finish_callback)) {}

  ~TesterBrowsingTopicsService() override = default;

  TesterBrowsingTopicsService(const TesterBrowsingTopicsService&) = delete;
  TesterBrowsingTopicsService& operator=(const TesterBrowsingTopicsService&) =
      delete;
  TesterBrowsingTopicsService(TesterBrowsingTopicsService&&) = delete;
  TesterBrowsingTopicsService& operator=(TesterBrowsingTopicsService&&) =
      delete;

  const BrowsingTopicsState& browsing_topics_state() override {
    return BrowsingTopicsServiceImpl::browsing_topics_state();
  }

  void OnCalculateBrowsingTopicsCompleted(EpochTopics epoch_topics) override {
    BrowsingTopicsServiceImpl::OnCalculateBrowsingTopicsCompleted(
        std::move(epoch_topics));

    if (calculation_finish_callback_)
      std::move(calculation_finish_callback_).Run();
  }

 private:
  base::OnceClosure calculation_finish_callback_;
};

class BrowsingTopicsBrowserTestBase : public MixinBasedInProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    // Mark all Privacy Sandbox APIs as attested since the test cases are
    // testing behaviors not related to attestations.
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(true);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &BrowsingTopicsBrowserTestBase::MonitorRequestOnNetworkThread,
        base::Unretained(this),
        base::SequencedTaskRunner::GetCurrentDefault()));

    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &BrowsingTopicsBrowserTestBase::MonitorRequestOnNetworkThread,
        base::Unretained(this),
        base::SequencedTaskRunner::GetCurrentDefault()));

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());

    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  ~BrowsingTopicsBrowserTestBase() override = default;

  std::string InvokeTopicsAPI(const content::ToRenderFrameHost& adapter,
                              bool skip_observation = false,
                              content::EvalJsOptions eval_options =
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS) {
    return EvalJs(adapter,
                  content::JsReplace(R"(
      if (!(document.browsingTopics instanceof Function)) {
        'not a function';
      } else {
        document.browsingTopics({skipObservation: $1})
        .then(topics => {
          let result = "[";
          for (const topic of topics) {
            result += JSON.stringify(topic, Object.keys(topic).sort()) + ";"
          }
          result += "]";
          return result;
        })
        .catch(error => error.message);
      }
    )",
                                     skip_observation),
                  eval_options)
        .ExtractString();
  }

  void MonitorRequestOnNetworkThread(
      const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
      const net::test_server::HttpRequest& request) {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BrowsingTopicsBrowserTestBase::MonitorRequestOnMainThread,
            base::Unretained(this), request));
  }

  void MonitorRequestOnMainThread(
      const net::test_server::HttpRequest& request) {
    auto topics_header = request.headers.find("Sec-Browsing-Topics");
    if (topics_header != request.headers.end()) {
      request_path_topics_map_[request.GetURL().path()] = topics_header->second;
    }
  }

  std::optional<std::string> GetTopicsHeaderForRequestPath(
      const std::string& request_path) {
    auto it = request_path_topics_map_.find(request_path);
    if (it == request_path_topics_map_.end()) {
      return std::nullopt;
    }

    return it->second;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  // Mapping of request paths to the topics header they were requested with.
  std::map<std::string, std::string> request_path_topics_map_;
  privacy_sandbox::PrivacySandboxAttestationsMixin
      privacy_sandbox_attestations_mixin_{&mixin_host_};
};

class BrowsingTopicsDisabledBrowserTest : public BrowsingTopicsBrowserTestBase {
 public:
  BrowsingTopicsDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{blink::features::kBrowsingTopics});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowsingTopicsDisabledBrowserTest,
                       NoBrowsingTopicsService) {
  EXPECT_FALSE(
      BrowsingTopicsServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsDisabledBrowserTest, NoTopicsAPI) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ("not a function", InvokeTopicsAPI(web_contents()));
}

// Enables the feature flags for BrowsingTopics but does not override the
// Annotator to a mocked instance.
class BrowsingTopicsAnnotationGoldenDataBrowserTest
    : public BrowsingTopicsBrowserTestBase {
 public:
  BrowsingTopicsAnnotationGoldenDataBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kBrowsingTopics,
         blink::features::kBrowsingTopicsBypassIPIsPubliclyRoutableCheck,
         features::kPrivacySandboxAdsAPIsOverride},
        /*disabled_features=*/{
            optimization_guide::features::kPreventLongRunningPredictionModels});
  }
  ~BrowsingTopicsAnnotationGoldenDataBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Running a TFLite model in a test is expensive so it can only be done in a
// browser test without any page loads.
IN_PROC_BROWSER_TEST_F(BrowsingTopicsAnnotationGoldenDataBrowserTest,
                       GoldenData) {
  // Boilerplate for getting the model to work for a real execution.
  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.PageTopicsModelMetadata");
  optimization_guide::proto::PageTopicsModelMetadata page_topics_model_metadata;
  page_topics_model_metadata.set_version(123);
  if (blink::features::kBrowsingTopicsTaxonomyVersion.Get() >= 2) {
    page_topics_model_metadata.set_taxonomy_version(
        blink::features::kBrowsingTopicsTaxonomyVersion.Get());
  }
  page_topics_model_metadata.add_supported_output(
      optimization_guide::proto::PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES);
  auto* output_params =
      page_topics_model_metadata.mutable_output_postprocessing_params();
  auto* category_params = output_params->mutable_category_params();
  category_params->set_max_categories(5);
  category_params->set_min_none_weight(0.8);
  category_params->set_min_category_weight(0.1);
  category_params->set_min_normalized_weight_within_top_n(0.1);
  page_topics_model_metadata.SerializeToString(any_metadata.mutable_value());
  base::FilePath source_root_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir));
  base::FilePath model_file_path = source_root_dir.AppendASCII("chrome")
                                       .AppendASCII("test")
                                       .AppendASCII("data")
                                       .AppendASCII("browsing_topics")
                                       .AppendASCII("golden_data_model.tflite");

  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->OverrideTargetModelForTesting(
          optimization_guide::proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2,
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path)
              .SetModelMetadata(any_metadata)
              .Build());

  BrowsingTopicsService* service =
      BrowsingTopicsServiceFactory::GetForProfile(browser()->profile());

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  service->GetAnnotator()->BatchAnnotate(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::vector<Annotation>& annotations) {
            ASSERT_EQ(annotations.size(), 1U);
            EXPECT_EQ(annotations[0].input, "foo.bar.com");
            EXPECT_THAT(annotations[0].topics,
                        testing::UnorderedElementsAre(1, 289));
            run_loop->Quit();
          },
          &run_loop),
      {"foo.bar.com"});

  run_loop.Run();

  optimization_guide::RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.ModelExecutor.ExecutionStatus.PageTopicsV2", 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus.PageTopicsV2",
      /*kSuccess=*/1, 1);
}

class BrowsingTopicsBrowserTest : public BrowsingTopicsBrowserTestBase {
 public:
  BrowsingTopicsBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&BrowsingTopicsBrowserTest::web_contents,
                                base::Unretained(this))) {
    // Configure a long epoch_retention_duration to prevent epochs from expiring
    // during tests where expiration is irrelevant.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{blink::features::kBrowsingTopics, {}},
         {blink::features::kBrowsingTopicsParameters,
          {{"epoch_retention_duration", "3650000d"}}},
         {blink::features::kBrowsingTopicsBypassIPIsPubliclyRoutableCheck, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        /*disabled_features=*/{});
  }

  ~BrowsingTopicsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    prerender_helper_.RegisterServerRequestMonitor(&https_server_);

    BrowsingTopicsBrowserTestBase::SetUpOnMainThread();

    for (auto& profile_and_calculation_finish_waiter :
         calculation_finish_waiters_) {
      profile_and_calculation_finish_waiter.second->Run();
    }
  }

  // BrowserTestBase::SetUpInProcessBrowserTestFixture
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &BrowsingTopicsBrowserTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

 protected:
  void CreateIframe(const GURL& url, bool browsing_topics_attribute = false) {
    content::TestNavigationObserver nav_observer(web_contents());

    ExecuteScriptAsync(web_contents(),
                       content::JsReplace(R"(
      {
        const iframe = document.createElement("iframe");
        iframe.browsingTopics = $1;
        iframe.src = $2;
        document.body.appendChild(iframe);
      }
                )",
                                          browsing_topics_attribute, url));

    nav_observer.WaitForNavigationFinished();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  }

  void ExpectResultTopicsEqual(
      const std::vector<TopicAndDomains>& result,
      std::vector<std::pair<Topic, std::set<HashedDomain>>> expected) {
    DCHECK_EQ(expected.size(), 5u);
    EXPECT_EQ(result.size(), 5u);

    for (int i = 0; i < 5; ++i) {
      EXPECT_EQ(result[i].topic(), expected[i].first);
      EXPECT_EQ(result[i].hashed_domains(), expected[i].second);
    }
  }

  HashedDomain GetHashedDomain(const std::string& domain) {
    return HashContextDomainForStorage(kTestKey, domain);
  }

  void CreateBrowsingTopicsStateFile(
      const base::FilePath& profile_path,
      const std::vector<EpochTopics>& epochs,
      base::Time next_scheduled_calculation_time) {
    base::Value::List epochs_list;
    for (const EpochTopics& epoch : epochs) {
      epochs_list.Append(epoch.ToDictValue());
    }

    base::Value::Dict dict;
    dict.Set("epochs", std::move(epochs_list));
    dict.Set("next_scheduled_calculation_time",
             base::TimeToValue(next_scheduled_calculation_time));
    dict.Set("hex_encoded_hmac_key", base::HexEncode(kTestKey));
    dict.Set("config_version", 1);

    JSONFileValueSerializer(
        profile_path.Append(FILE_PATH_LITERAL("BrowsingTopicsState")))
        .Serialize(dict);
  }

  content::BrowsingTopicsSiteDataManager* browsing_topics_site_data_manager() {
    return browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetBrowsingTopicsSiteDataManager();
  }

  history::HistoryService* history_service() {
    return HistoryServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
  }

  TesterBrowsingTopicsService* browsing_topics_service() {
    return static_cast<TesterBrowsingTopicsService*>(
        BrowsingTopicsServiceFactory::GetForProfile(browser()->profile()));
  }

  const BrowsingTopicsState& browsing_topics_state() {
    return browsing_topics_service()->browsing_topics_state();
  }

  privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings() {
    return PrivacySandboxSettingsFactory::GetForProfile(browser()->profile());
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    browsing_topics::BrowsingTopicsServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindRepeating(
                &BrowsingTopicsBrowserTest::CreateBrowsingTopicsService,
                base::Unretained(this)));
  }

  void InitializePreexistingState(
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      const base::FilePath& profile_path,
      TestAnnotator* annotator) {
    // Configure the (mock) model.

    annotator->UseModelInfo(
        *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build());
    annotator->UseAnnotations({
        {"foo6.com", {1, 2, 3, 4, 5, 6}},
        {"foo5.com", {2, 3, 4, 5, 6}},
        {"foo4.com", {3, 4, 5, 6}},
        {"foo3.com", {4, 5, 6}},
        {"foo2.com", {5, 6}},
        {"foo1.com", {6}},
    });

    // Add some initial history.
    history::HistoryAddPageArgs add_page_args;
    add_page_args.time = base::Time::Now();
    add_page_args.context_id = 1;
    add_page_args.nav_entry_id = 1;

    // Note: foo6.com isn't in the initial history.
    for (int i = 1; i <= 5; ++i) {
      add_page_args.url =
          GURL(base::StrCat({"https://foo", base::NumberToString(i), ".com"}));
      history_service->AddPage(add_page_args);
      history_service->SetBrowsingTopicsAllowed(add_page_args.context_id,
                                                add_page_args.nav_entry_id,
                                                add_page_args.url);
    }

    // Add some API usage contexts data.
    site_data_manager->OnBrowsingTopicsApiUsed(
        HashMainFrameHostForStorage("foo1.com"), HashedDomain(1), "foo1.com",
        base::Time::Now());

    // Initialize the `BrowsingTopicsState`.
    std::vector<EpochTopics> preexisting_epochs;
    preexisting_epochs.push_back(
        CreateTestEpochTopics({{Topic(1), {GetHashedDomain("a.test")}},
                               {Topic(2), {GetHashedDomain("a.test")}},
                               {Topic(3), {GetHashedDomain("a.test")}},
                               {Topic(4), {GetHashedDomain("a.test")}},
                               {Topic(5), {GetHashedDomain("a.test")}}},
                              kTime1));
    preexisting_epochs.push_back(
        CreateTestEpochTopics({{Topic(6), {GetHashedDomain("a.test")}},
                               {Topic(7), {GetHashedDomain("a.test")}},
                               {Topic(8), {GetHashedDomain("a.test")}},
                               {Topic(9), {GetHashedDomain("a.test")}},
                               {Topic(10), {GetHashedDomain("a.test")}}},
                              kTime2));

    CreateBrowsingTopicsStateFile(
        profile_path, std::move(preexisting_epochs),
        /*next_scheduled_calculation_time=*/base::Time::Now() - base::Days(1));
  }

  std::unique_ptr<KeyedService> CreateBrowsingTopicsService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);

    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings =
        PrivacySandboxSettingsFactory::GetForProfile(profile);
    privacy_sandbox_settings->SetAllPrivacySandboxAllowedForTesting();

    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::IMPLICIT_ACCESS);

    content::BrowsingTopicsSiteDataManager* site_data_manager =
        context->GetDefaultStoragePartition()
            ->GetBrowsingTopicsSiteDataManager();

    std::unique_ptr<TestAnnotator> annotator =
        std::make_unique<TestAnnotator>();

    InitializePreexistingState(history_service, site_data_manager,
                               profile->GetPath(), annotator.get());

    DCHECK(!base::Contains(calculation_finish_waiters_, profile));
    calculation_finish_waiters_.emplace(profile,
                                        std::make_unique<base::RunLoop>());

    if (!ukm_recorder_)
      ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    return std::make_unique<TesterBrowsingTopicsService>(
        profile->GetPath(), privacy_sandbox_settings, history_service,
        site_data_manager, std::move(annotator),
        calculation_finish_waiters_.at(profile)->QuitClosure());
  }

  content::test::FencedFrameTestHelper fenced_frame_test_helper_;

  content::test::PrerenderTestHelper prerender_helper_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::map<Profile*, std::unique_ptr<base::RunLoop>>
      calculation_finish_waiters_;

  optimization_guide::TestOptimizationGuideModelProvider model_provider_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, HasBrowsingTopicsService) {
  EXPECT_TRUE(browsing_topics_service());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, NoServiceInIncognitoMode) {
  CreateIncognitoBrowser(browser()->profile());

  EXPECT_TRUE(browser()->profile()->HasPrimaryOTRProfile());

  Profile* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/false);
  EXPECT_TRUE(incognito_profile);

  BrowsingTopicsService* incognito_browsing_topics_service =
      BrowsingTopicsServiceFactory::GetForProfile(incognito_profile);
  EXPECT_FALSE(incognito_browsing_topics_service);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, BrowsingTopicsStateOnStart) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::Time now = base::Time::Now();

  EXPECT_EQ(browsing_topics_state().epochs().size(), 3u);
  EXPECT_EQ(browsing_topics_state().epochs()[0].calculation_time(), kTime1);
  EXPECT_EQ(browsing_topics_state().epochs()[1].calculation_time(), kTime2);
  EXPECT_GT(browsing_topics_state().epochs()[2].calculation_time(),
            now - base::Minutes(1));
  EXPECT_LT(browsing_topics_state().epochs()[2].calculation_time(), now);

  ExpectResultTopicsEqual(
      browsing_topics_state().epochs()[0].top_topics_and_observing_domains(),
      {{Topic(1), {GetHashedDomain("a.test")}},
       {Topic(2), {GetHashedDomain("a.test")}},
       {Topic(3), {GetHashedDomain("a.test")}},
       {Topic(4), {GetHashedDomain("a.test")}},
       {Topic(5), {GetHashedDomain("a.test")}}});

  ExpectResultTopicsEqual(
      browsing_topics_state().epochs()[1].top_topics_and_observing_domains(),
      {{Topic(6), {GetHashedDomain("a.test")}},
       {Topic(7), {GetHashedDomain("a.test")}},
       {Topic(8), {GetHashedDomain("a.test")}},
       {Topic(9), {GetHashedDomain("a.test")}},
       {Topic(10), {GetHashedDomain("a.test")}}});

  ExpectResultTopicsEqual(
      browsing_topics_state().epochs()[2].top_topics_and_observing_domains(),
      {{Topic(6), {HashedDomain(1)}},
       {Topic(5), {}},
       {Topic(4), {}},
       {Topic(3), {}},
       {Topic(2), {}}});

  EXPECT_GT(browsing_topics_state().next_scheduled_calculation_time(),
            now + base::Days(7) - base::Minutes(1));
  EXPECT_LT(browsing_topics_state().next_scheduled_calculation_time(),
            now + base::Days(7));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, ApiResultUkm) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  InvokeTopicsAPI(web_contents());

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult2::
          kEntryName);
  EXPECT_EQ(1u, entries.size());

  ukm_recorder_->ExpectEntrySourceHasUrl(entries.back(), main_frame_url);

  std::vector<ApiResultUkmMetrics> metrics_entries =
      ReadApiResultUkmMetrics(*ukm_recorder_);

  EXPECT_EQ(1u, metrics_entries.size());

  EXPECT_FALSE(metrics_entries[0].failure_reason);

  EXPECT_TRUE(metrics_entries[0].topic0.IsValid());
  EXPECT_TRUE(metrics_entries[0].topic0.is_true_topic());
  EXPECT_FALSE(metrics_entries[0].topic0.should_be_filtered());
  EXPECT_EQ(metrics_entries[0].topic0.taxonomy_version(), 1);
  EXPECT_EQ(metrics_entries[0].topic0.model_version(), 2);

  EXPECT_TRUE(metrics_entries[0].topic1.IsValid());
  EXPECT_TRUE(metrics_entries[0].topic1.is_true_topic());
  EXPECT_FALSE(metrics_entries[0].topic1.should_be_filtered());
  EXPECT_EQ(metrics_entries[0].topic1.taxonomy_version(), 1);
  EXPECT_EQ(metrics_entries[0].topic1.model_version(), 2);

  EXPECT_FALSE(metrics_entries[0].topic2.IsValid());

  EXPECT_EQ(metrics_entries[0].topic0.topic(), kExpectedTopic1);
  EXPECT_EQ(metrics_entries[0].topic1.topic(), kExpectedTopic2);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, PageLoadUkm) {
  // The test assumes pages gets deleted after navigation, triggering metrics
  // recording. Disable back/forward cache to ensure that pages don't get
  // preserved in the cache.
  content::DisableBackForwardCacheForTesting(
      web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  InvokeTopicsAPI(web_contents());

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::BrowsingTopics_PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  ukm_recorder_->ExpectEntrySourceHasUrl(entries.back(), main_frame_url);

  ukm_recorder_->ExpectEntryMetric(entries.back(),
                                   ukm::builders::BrowsingTopics_PageLoad::
                                       kTopicsRequestingContextDomainsCountName,
                                   1);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, GetTopTopicsForDisplay) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  std::vector<privacy_sandbox::CanonicalTopic> result =
      browsing_topics_service()->GetTopTopicsForDisplay();

  EXPECT_EQ(result.size(), 15u);
  EXPECT_EQ(result[0].topic_id(), Topic(1));
  EXPECT_EQ(result[1].topic_id(), Topic(2));
  EXPECT_EQ(result[2].topic_id(), Topic(3));
  EXPECT_EQ(result[3].topic_id(), Topic(4));
  EXPECT_EQ(result[4].topic_id(), Topic(5));
  EXPECT_EQ(result[5].topic_id(), Topic(6));
  EXPECT_EQ(result[6].topic_id(), Topic(7));
  EXPECT_EQ(result[7].topic_id(), Topic(8));
  EXPECT_EQ(result[8].topic_id(), Topic(9));
  EXPECT_EQ(result[9].topic_id(), Topic(10));
  EXPECT_EQ(result[10].topic_id(), Topic(6));
  EXPECT_EQ(result[11].topic_id(), Topic(5));
  EXPECT_EQ(result[12].topic_id(), Topic(4));
  EXPECT_EQ(result[13].topic_id(), Topic(3));
  EXPECT_EQ(result[14].topic_id(), Topic(2));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPI_ContextDomainNotFiltered_FromMainFrame) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  std::string result = InvokeTopicsAPI(web_contents());

  EXPECT_EQ(result, kExpectedApiResult);

  // Ensure access has been reported to the Page Specific Content Settings.
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      web_contents()->GetPrimaryPage());
  EXPECT_TRUE(pscs->HasAccessedTopics());
  auto topics = pscs->GetAccessedTopics();
  ASSERT_EQ(2u, topics.size());

  // PSCS::GetAccessedTopics() will return sorted values.
  EXPECT_EQ(topics[0].topic_id(), kExpectedTopic1);
  EXPECT_EQ(topics[1].topic_id(), kExpectedTopic2);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPI_ContextDomainNotFiltered_FromSubframe) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL subframe_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  std::string result = InvokeTopicsAPI(
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0));

  EXPECT_EQ(result, kExpectedApiResult);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPI_ContextDomainFiltered) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL subframe_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  // b.test has yet to call the API so it shouldn't receive a topic.
  EXPECT_EQ("[]", InvokeTopicsAPI(content::ChildFrameAt(
                      web_contents()->GetPrimaryMainFrame(), 0)));
  auto* pscs = content_settings::PageSpecificContentSettings::GetForPage(
      web_contents()->GetPrimaryPage());
  EXPECT_FALSE(pscs->HasAccessedTopics());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, TopicsAPI_ObserveBehavior) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL subframe_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  {
    base::HistogramTester histogram_tester;

    // Invoked the API with {skipObservation: true}.
    EXPECT_EQ("[]",
              InvokeTopicsAPI(content::ChildFrameAt(
                                  web_contents()->GetPrimaryMainFrame(), 0),
                              /*skip_observation=*/true));

    // Since {skipObservation: true} was specified, the page is not eligible for
    // topics calculation.
    EXPECT_FALSE(
        BrowsingTopicsEligibleForURLVisit(history_service(), main_frame_url));

    // Since {skipObservation: true} was specified, the usage is not tracked.
    // The returned entry was from the pre-existing storage.
    std::vector<ApiUsageContext> api_usage_contexts =
        content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
    EXPECT_EQ(api_usage_contexts.size(), 1u);

    histogram_tester.ExpectUniqueSample(kBrowsingTopicsApiActionTypeHistogramId,
                                        0 /*kGetViaDocumentApi*/,
                                        /*expected_bucket_count=*/1);
  }

  {
    base::HistogramTester histogram_tester;

    // Invoked the API with {skipObservation: false}.
    EXPECT_EQ("[]", InvokeTopicsAPI(content::ChildFrameAt(
                        web_contents()->GetPrimaryMainFrame(), 0)));

    // Since {skipObservation: false} was specified, the page is eligible for
    // topics calculation.
    EXPECT_TRUE(
        BrowsingTopicsEligibleForURLVisit(history_service(), main_frame_url));

    // Since {skipObservation: false} was specified, the usage is tracked.
    std::vector<ApiUsageContext> api_usage_contexts =
        content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());

    EXPECT_EQ(api_usage_contexts.size(), 2u);
    EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
              HashMainFrameHostForStorage("foo1.com"));
    EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(1));

    EXPECT_EQ(api_usage_contexts[1].hashed_main_frame_host,
              HashMainFrameHostForStorage(
                  https_server_.GetURL("a.test", "/").host()));
    EXPECT_EQ(api_usage_contexts[1].hashed_context_domain,
              GetHashedDomain("b.test"));

    histogram_tester.ExpectUniqueSample(kBrowsingTopicsApiActionTypeHistogramId,
                                        1 /*kGetAndObserveViaDocumentApi*/,
                                        /*expected_bucket_count=*/1);
  }
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    EmptyPage_PermissionsPolicyBrowsingTopicsNone_TopicsAPI) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/empty_page_browsing_topics_none.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::RunLoop ukm_loop;
  ukm_recorder_->SetOnAddEntryCallback(
      ukm::builders::BrowsingTopics_DocumentBrowsingTopicsApiResult2::
          kEntryName,
      ukm_loop.QuitClosure());

  EXPECT_EQ(
      "The \"browsing-topics\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(web_contents()));

  ukm_loop.Run();

  std::vector<ApiResultUkmMetrics> metrics_entries =
      ReadApiResultUkmMetrics(*ukm_recorder_);

  EXPECT_EQ(1u, metrics_entries.size());

  EXPECT_EQ(metrics_entries[0].failure_reason,
            ApiAccessResult::kInvalidRequestingContext);
  EXPECT_FALSE(metrics_entries[0].topic0.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic1.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic2.IsValid());

  // No BrowsingTopicsApiActionType metrics are recorded, as
  // `BrowsingTopicsServiceImpl` did not get a chance to handle the request due
  // to earlier permissions policy reject.
  histogram_tester.ExpectTotalCount(kBrowsingTopicsApiActionTypeHistogramId,
                                    /*expected_count=*/0);
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    EmptyPage_PermissionsPolicyInterestCohortNone_TopicsAPI) {
  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/empty_page_interest_cohort_none.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ(
      "The \"interest-cohort\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(web_contents()));
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    OneIframePage_SubframePermissionsPolicyBrowsingTopicsNone_TopicsAPI) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL subframe_url = https_server_.GetURL(
      "a.test", "/browsing_topics/empty_page_browsing_topics_none.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  std::string result = InvokeTopicsAPI(web_contents());
  EXPECT_EQ(result, kExpectedApiResult);

  EXPECT_EQ(
      "The \"browsing-topics\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(
          content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       PermissionsPolicyAllowCertainOrigin_TopicsAPI) {
  base::StringPairs allowed_origin_replacement;
  allowed_origin_replacement.emplace_back(
      "{{ALLOWED_ORIGIN}}", https_server_.GetOrigin("c.test").Serialize());

  GURL main_frame_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "one_iframe_page_browsing_topics_allow_certain_origin.html",
                    allowed_origin_replacement));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  std::string result = InvokeTopicsAPI(web_contents());
  EXPECT_EQ(result, kExpectedApiResult);

  GURL subframe_url =
      https_server_.GetURL("c.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));
  EXPECT_EQ("[]", InvokeTopicsAPI(content::ChildFrameAt(
                      web_contents()->GetPrimaryMainFrame(), 0)));

  subframe_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  EXPECT_EQ(
      "The \"browsing-topics\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(
          content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPINotAllowedInInsecureContext) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  // Navigate the iframe to a https site.
  GURL subframe_url = https_server_.GetURL("b.test", "/empty_page.html");
  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"frame", subframe_url);

  // Both the main frame and the subframe are insecure context because the main
  // frame is loaded over HTTP. Expect that the API isn't available in either
  // frame.
  EXPECT_EQ("not a function", InvokeTopicsAPI(web_contents()));
  EXPECT_EQ("not a function", InvokeTopicsAPI(content::ChildFrameAt(
                                  web_contents()->GetPrimaryMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPINotAllowedInDetachedDocument) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ(
      "Failed to execute 'browsingTopics' on 'Document': A browsing "
      "context is required when calling document.browsingTopics().",
      EvalJs(web_contents(), R"(
      const iframe = document.getElementById('frame');
      const childDocument = iframe.contentWindow.document;
      iframe.remove();

      childDocument.browsingTopics()
        .then(topics => "success")
        .catch(error => error.message);
    )"));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPINotAllowedInOpaqueOriginDocument) {
  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/one_sandboxed_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ(
      "document.browsingTopics() is not allowed in an opaque origin context.",
      InvokeTopicsAPI(
          content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPINotAllowedInFencedFrame) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL fenced_frame_url =
      https_server_.GetURL("b.test", "/fenced_frames/title1.html");

  content::RenderFrameHostWrapper fenced_frame_rfh_wrapper(
      fenced_frame_test_helper_.CreateFencedFrame(
          web_contents()->GetPrimaryMainFrame(), fenced_frame_url));

  EXPECT_EQ("document.browsingTopics() is not allowed in a fenced frame.",
            InvokeTopicsAPI(fenced_frame_rfh_wrapper.get()));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPINotAllowedInPrerenderedPage) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL prerender_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  content::FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(prerender_url);

  content::RenderFrameHost* prerender_host =
      prerender_helper().GetPrerenderedMainFrameHost(host_id);
  EXPECT_EQ(
      "document.browsingTopics() is not allowed when the page is being "
      "prerendered.",
      InvokeTopicsAPI(prerender_host));

  // Activate the prerendered page. The API call should succeed.
  content::test::PrerenderHostObserver prerender_observer(*web_contents(),
                                                          prerender_url);
  prerender_helper().NavigatePrimaryPage(prerender_url);
  prerender_observer.WaitForActivation();

  std::string result = InvokeTopicsAPI(web_contents());

  EXPECT_EQ(result, kExpectedApiResult);
}

// Regression test for crbug/1339735.
IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    TopicsAPIInvokedInMainFrameUnloadHandler_NoRendererCrash) {
  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/get_topics_during_unload.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  // A renderer crash won't always be captured if the renderer is also shutting
  // down naturally around the same time. Thus, we create a new page in the same
  // renderer process to keep the renderer process alive when the page navigates
  // away later.
  content::TestNavigationObserver popup_observer(main_frame_url);
  popup_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                     content::JsReplace("window.open($1)", main_frame_url)));
  popup_observer.Wait();

  GURL new_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_url));
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    FetchSameOrigin_TopicsEligible_SendTopics_HasNoObserveResponse) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL fetch_url = https_server_.GetURL(
      "a.test", "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_TRUE(topics_header_value);
  EXPECT_EQ(*topics_header_value, kExpectedHeaderValueForSiteA);

  // No observation should have been recorded in addition to the pre-existing
  // one.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, FetchWithoutTopicsFlagSet) {
  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL fetch_url = https_server_.GetURL(
      "b.test", "/browsing_topics/page_with_custom_topics_header.html");

  {
    // Invoke fetch() without the `browsingTopics` flag. This request isn't
    // eligible for topics.
    EXPECT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       content::JsReplace("fetch($1)", fetch_url)));

    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header.html");

    // Expect no topics header as the request did not specify
    // {browsingTopics: true}.
    EXPECT_FALSE(topics_header_value);
  }

  {
    // Invoke fetch() with the `browsingTopics` flag set to false. This request
    // isn't eligible for topics.
    EXPECT_TRUE(ExecJs(
        web_contents()->GetPrimaryMainFrame(),
        content::JsReplace("fetch($1, {browsingTopics: false})", fetch_url)));

    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header.html");

    // Expect no topics header as the request did not specify
    // {browsingTopics: true}.
    EXPECT_FALSE(topics_header_value);
  }
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    FetchSameOrigin_TopicsEligible_SendNoTopic_HasNoObserveResponse) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL fetch_url = https_server_.GetURL(
      "b.test", "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  // Expect an empty header value as "b.test" did not observe the candidate
  // topics.
  EXPECT_TRUE(topics_header_value);
  EXPECT_EQ(topics_header_value, kExpectedHeaderValueForEmptyTopics);

  // No observation should have been recorded in addition to the pre-existing
  // one, as the response did not have the `Observe-Browsing-Topics: ?1` header.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);

  histogram_tester.ExpectUniqueSample(kBrowsingTopicsApiActionTypeHistogramId,
                                      2 /*kGetViaFetchLikeApi*/,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    FetchSameOrigin_TopicsEligible_SendNoTopic_HasObserveResponse) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL fetch_url = https_server_.GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  // A new observation should have been recorded in addition to the pre-existing
  // one, as the response had the `Observe-Browsing-Topics: ?1` header and the
  // request was eligible for topics.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo1.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(1));
  EXPECT_EQ(
      api_usage_contexts[1].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("b.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain,
            GetHashedDomain("b.test"));

  // Expect a "get" event and an "observe" event respectively.
  histogram_tester.ExpectTotalCount(kBrowsingTopicsApiActionTypeHistogramId,
                                    /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(kBrowsingTopicsApiActionTypeHistogramId,
                                     2 /*kGetViaFetchLikeApi*/,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kBrowsingTopicsApiActionTypeHistogramId,
                                     3 /*kObserveViaFetchLikeApi*/,
                                     /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    FetchSameOrigin_TopicsNotEligibleDueToUserSettings_HasObserveResponse) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL fetch_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(fetch_url, CONTENT_SETTING_BLOCK);

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  // When the request is ineligible for topics due to user settings, an empty
  // list of topics will be sent in the header.
  EXPECT_EQ(topics_header_value, kExpectedHeaderValueForEmptyTopics);

  // No observation should have been recorded in addition to the pre-existing
  // one even though the response had the `Observe-Browsing-Topics: ?1` header,
  // as the request was not eligible for topics.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    FetchCrossOrigin_TopicsEligible_SendTopics_HasObserveResponse) {
  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL fetch_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_TRUE(topics_header_value);
  EXPECT_EQ(*topics_header_value, kExpectedHeaderValueForSiteB);

  // A new observation should have been recorded in addition to the pre-existing
  // one, as the response had the `Observe-Browsing-Topics: ?1` header and the
  // request was eligible for topics.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(
      api_usage_contexts[0].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("b.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("a.test"));
  EXPECT_EQ(api_usage_contexts[1].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo1.com"));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain, HashedDomain(1));
}

// On an insecure site (i.e. URL with http scheme), test fetch request with
// the `browsingTopics` set to true. Expect it to throw an exception.
IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    FetchCrossOrigin_TopicsNotEligibleDueToInsecureInitiatorContext) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL fetch_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  content::EvalJsResult result = EvalJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url));

  EXPECT_THAT(result.error,
              testing::HasSubstr("browsingTopics: Topics operations are only "
                                 "available in secure contexts."));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath("/browsing_topics/empty_page.html");

  // Expect no topics header as the request was not eligible for topics due to
  // insecure initiator context.
  EXPECT_FALSE(topics_header_value);
}

// Only allow topics from origin c.test, and test fetch requests to b.test and
// c.test to verify that only c.test gets them.
IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    FetchCrossOrigin_TopicsNotEligibleDueToPermissionsPolicyAgainstRequestOrigin) {
  base::StringPairs allowed_origin_replacement;
  allowed_origin_replacement.emplace_back(
      "{{ALLOWED_ORIGIN}}", https_server_.GetOrigin("c.test").Serialize());

  GURL main_frame_url = https_server_.GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "one_iframe_page_browsing_topics_allow_certain_origin.html",
                    allowed_origin_replacement));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  {
    base::HistogramTester histogram_tester;

    GURL fetch_url =
        https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

    EXPECT_TRUE(ExecJs(
        web_contents()->GetPrimaryMainFrame(),
        content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath("/browsing_topics/empty_page.html");

    // No topics header was sent, as the permissions policy denied it.
    EXPECT_FALSE(topics_header_value);

    // No BrowsingTopicsApiActionType metrics are recorded, as
    // `BrowsingTopicsServiceImpl` did not get a chance to handle the request
    // due to earlier permissions policy reject.
    histogram_tester.ExpectTotalCount(kBrowsingTopicsApiActionTypeHistogramId,
                                      /*expected_count=*/0);
  }

  {
    base::HistogramTester histogram_tester;

    GURL fetch_url =
        https_server_.GetURL("c.test", "/browsing_topics/empty_page.html");

    EXPECT_TRUE(ExecJs(
        web_contents()->GetPrimaryMainFrame(),
        content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath("/browsing_topics/empty_page.html");

    EXPECT_TRUE(topics_header_value);

    histogram_tester.ExpectUniqueSample(kBrowsingTopicsApiActionTypeHistogramId,
                                        2 /*kGetViaFetchLikeApi*/,
                                        /*expected_bucket_count=*/1);
  }
}

// On site b.test, test fetch request to a.test that gets redirected to c.test.
// The topics header should be calculated for them individually (i.e. given that
// only a.test has observed the candidate topics for site b.test, the request to
// a.test should have a non-empty topics header, while the redirected request to
// c.test should have an empty topics header.)
IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       FetchCrossOriginWithRedirect) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs redirect_replacement;
  redirect_replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  redirect_replacement.emplace_back(std::make_pair(
      "{{OBSERVE_BROWSING_TOPICS_HEADER}}", "Observe-Browsing-Topics: ?1"));
  redirect_replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL redirect_url = https_server_.GetURL(
      "c.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header2.html",
                    redirect_replacement));

  base::StringPairs replacement;
  replacement.emplace_back(
      std::make_pair("{{STATUS}}", "301 Moved Permanently"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}",
                                          "Location: " + redirect_url.spec()));

  GURL fetch_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  {
    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header.html");
    EXPECT_TRUE(topics_header_value);
    EXPECT_EQ(*topics_header_value, kExpectedHeaderValueForSiteB);
  }
  {
    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header2.html");
    EXPECT_TRUE(topics_header_value);

    // An empty topics header value was sent, because "c.test" did not observe
    // the candidate topics.
    EXPECT_EQ(topics_header_value, kExpectedHeaderValueForEmptyTopics);
  }

  // Two new observations should have been recorded in addition to the
  // pre-existing one.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 3u);
  EXPECT_EQ(
      api_usage_contexts[0].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("b.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("c.test"));
  EXPECT_EQ(
      api_usage_contexts[1].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("b.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain,
            GetHashedDomain("a.test"));
  EXPECT_EQ(api_usage_contexts[2].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo1.com"));
  EXPECT_EQ(api_usage_contexts[2].hashed_context_domain, HashedDomain(1));

  // Expect two "get" events and two "observe" events for the initial request
  // and the redirect respectively.
  histogram_tester.ExpectTotalCount(kBrowsingTopicsApiActionTypeHistogramId,
                                    /*expected_count=*/4);
  histogram_tester.ExpectBucketCount(kBrowsingTopicsApiActionTypeHistogramId,
                                     2 /*kGetViaFetchLikeApi*/,
                                     /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(kBrowsingTopicsApiActionTypeHistogramId,
                                     3 /*kObserveViaFetchLikeApi*/,
                                     /*expected_count=*/2);
}

// On site b.test, test fetch request to a.test that gets redirected to c.test.
// The topics header eligibility should be checked for them individually (i.e.
// given that the declared policy on the page only allows origin c.test, the
// request to a.test should not have the topics header, while the redirected
// request to c.test should have the topics header.)
IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    FetchCrossOriginWithRedirect_InitialRequestTopicsNotEligibleDueToPermissionsPolicy) {
  base::StringPairs allowed_origin_replacement;
  allowed_origin_replacement.emplace_back(
      "{{ALLOWED_ORIGIN}}", https_server_.GetOrigin("c.test").Serialize());

  GURL main_frame_url = https_server_.GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "one_iframe_page_browsing_topics_allow_certain_origin.html",
                    allowed_origin_replacement));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs redirect_replacement;
  redirect_replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  redirect_replacement.emplace_back(std::make_pair(
      "{{OBSERVE_BROWSING_TOPICS_HEADER}}", "Observe-Browsing-Topics: ?1"));
  redirect_replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL redirect_url = https_server_.GetURL(
      "c.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header2.html",
                    redirect_replacement));

  base::StringPairs replacement;
  replacement.emplace_back(
      std::make_pair("{{STATUS}}", "301 Moved Permanently"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}",
                                          "Location: " + redirect_url.spec()));

  GURL fetch_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  {
    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header.html");

    // No topics header was sent, as the permissions policy denied it.
    EXPECT_FALSE(topics_header_value);
  }
  {
    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header2.html");
    EXPECT_TRUE(topics_header_value);

    // An empty topics header value was sent, as "c.test" did not observe the
    // candidate topics.
    EXPECT_EQ(topics_header_value, kExpectedHeaderValueForEmptyTopics);
  }

  // A new observation should have been recorded in addition to the pre-existing
  // one.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(
      api_usage_contexts[0].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("b.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("c.test"));
  EXPECT_EQ(api_usage_contexts[1].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo1.com"));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain, HashedDomain(1));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, UseCounter_DocumentApi) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  InvokeTopicsAPI(web_contents());

  // Navigate away to flush use counters.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kTopicsAPI_BrowsingTopics_Method, 1);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, UseCounter_Fetch) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  GURL fetch_url = main_frame_url;

  {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

    // Send a fetch() request with `browsingTopics` set to false. Expect no
    // `kTopicsAPIFetch` use counter.
    EXPECT_TRUE(ExecJs(
        web_contents()->GetPrimaryMainFrame(),
        content::JsReplace("fetch($1, {browsingTopics: false})", fetch_url)));

    // Navigate away to flush use counters.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features", blink::mojom::WebFeature::kTopicsAPIFetch,
        0);
  }

  {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

    // Send a fetch() request with `browsingTopics` set to true. Expect one
    // `kTopicsAPIFetch` use counter.
    EXPECT_TRUE(ExecJs(
        web_contents()->GetPrimaryMainFrame(),
        content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

    // Navigate away to flush use counters.
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

    histogram_tester.ExpectBucketCount(
        "Blink.UseCounter.Features", blink::mojom::WebFeature::kTopicsAPIFetch,
        1);
    histogram_tester.ExpectBucketCount("Blink.UseCounter.Features",
                                    blink::mojom::WebFeature::kTopicsAPIAll,
                                    1);
  }
}

// For a page that contains a static <iframe> with a "browsingtopics"
// attribute, the iframe navigation request should be eligible for topics.
IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       CrossOriginStaticIframeWithTopicsAttribute) {
  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL subframe_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  base::StringPairs topics_attribute_replacement;
  topics_attribute_replacement.emplace_back(
      "{{MAYBE_BROWSING_TOPICS_ATTRIBUTE}}", "browsingtopics");

  topics_attribute_replacement.emplace_back("{{SRC_URL}}", subframe_url.spec());

  GURL main_frame_url = https_server_.GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/page_with_custom_attribute_iframe.html",
                    topics_attribute_replacement));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");
  EXPECT_TRUE(topics_header_value);
  EXPECT_EQ(*topics_header_value, kExpectedHeaderValueForSiteB);

  // A new observation should have been recorded in addition to the pre-existing
  // one.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(
      api_usage_contexts[0].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("b.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("a.test"));
  EXPECT_EQ(api_usage_contexts[1].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo1.com"));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain, HashedDomain(1));
}

// For a page that contains a static <iframe> without a "browsingtopics"
// attribute, the iframe navigation request should not be eligible for topics.
IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       CrossOriginStaticIframeWithoutTopicsAttribute) {
  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL subframe_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  base::StringPairs topics_attribute_replacement;
  topics_attribute_replacement.emplace_back(
      "{{MAYBE_BROWSING_TOPICS_ATTRIBUTE}}", "");

  topics_attribute_replacement.emplace_back("{{SRC_URL}}", subframe_url.spec());

  GURL main_frame_url = https_server_.GetURL(
      "b.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/page_with_custom_attribute_iframe.html",
                    topics_attribute_replacement));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");
  EXPECT_FALSE(topics_header_value);

  // Since the request wasn't eligible for topics, no observation should have
  // been recorded in addition to the pre-existing one, even though the response
  // contains a `Observe-Browsing-Topics: ?1` header.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
}

// For a page with a dynamically appended iframe with iframe.browsingTopics set
// to true, the iframe navigation request should be eligible for topics.
IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       CrossOriginDynamicIframeWithTopicsAttribute) {
  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL subframe_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  CreateIframe(subframe_url, /*browsing_topics_attribute=*/true);

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");
  EXPECT_TRUE(topics_header_value);
  EXPECT_EQ(*topics_header_value, kExpectedHeaderValueForSiteB);

  // A new observation should have been recorded in addition to the pre-existing
  // one.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(
      api_usage_contexts[0].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("b.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("a.test"));
  EXPECT_EQ(api_usage_contexts[1].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo1.com"));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain, HashedDomain(1));
}

// For a page with a dynamically appended iframe with iframe.browsingTopics set
// to true, the iframe navigation request should not be eligible for topics.
IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       CrossOriginDynamicIframeWithoutTopicsAttribute) {
  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL subframe_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  CreateIframe(subframe_url);

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");
  EXPECT_FALSE(topics_header_value);

  // Since the request wasn't eligible for topics, no observation should have
  // been recorded in addition to the pre-existing one, even though the response
  // contains a `Observe-Browsing-Topics: ?1` header.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    CrossOriginDynamicIframe_TopicsNotEligibleDueToUserSettings_HasObserveResponse) {
  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL subframe_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(subframe_url, CONTENT_SETTING_BLOCK);

  CreateIframe(subframe_url, /*browsing_topics_attribute=*/true);

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  // When the request is ineligible for topics due to user settings, an empty
  // list of topics will be sent in the header.
  EXPECT_EQ(topics_header_value, kExpectedHeaderValueForEmptyTopics);

  // No observation should have been recorded in addition to the pre-existing
  // one even though the response had the `Observe-Browsing-Topics: ?1` header,
  // as the request was not eligible for topics.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
}

// Only allow topics from origin c.test, and test <iframe browsingtopics>
// requests to b.test and c.test to verify that only c.test gets the header.
IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    CrossOriginIframe_TopicsNotEligibleDueToPermissionsPolicyAgainstRequestOrigin) {
  base::StringPairs allowed_origin_replacement;
  allowed_origin_replacement.emplace_back(
      "{{ALLOWED_ORIGIN}}", https_server_.GetOrigin("c.test").Serialize());

  GURL main_frame_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "one_iframe_page_browsing_topics_allow_certain_origin.html",
                    allowed_origin_replacement));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  {
    GURL subframe_url =
        https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

    CreateIframe(subframe_url, /*browsing_topics_attribute=*/true);

    // No topics header was sent, as the permissions policy denied it.
    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath("/browsing_topics/empty_page.html");
    EXPECT_FALSE(topics_header_value);
  }

  {
    GURL subframe_url =
        https_server_.GetURL("c.test", "/browsing_topics/empty_page.html");

    CreateIframe(subframe_url, /*browsing_topics_attribute=*/true);

    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath("/browsing_topics/empty_page.html");
    EXPECT_TRUE(topics_header_value);
  }
}

// On site b.test, test <iframe browsingtopics> request to a.test that gets
// redirected to c.test. The topics header should be calculated for them
// individually (i.e. given that only a.test has observed the candidate topics
// for site b.test, the request to a.test should have a non-empty topics header,
// while the redirected request to c.test should have an empty topics header.)
IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       CrossOriginIframeWithRedirect) {
  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs redirect_replacement;
  redirect_replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  redirect_replacement.emplace_back(std::make_pair(
      "{{OBSERVE_BROWSING_TOPICS_HEADER}}", "Observe-Browsing-Topics: ?1"));
  redirect_replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL redirect_url = https_server_.GetURL(
      "c.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header2.html",
                    redirect_replacement));

  base::StringPairs replacement;
  replacement.emplace_back(
      std::make_pair("{{STATUS}}", "301 Moved Permanently"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}",
                                          "Location: " + redirect_url.spec()));

  GURL subframe_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  CreateIframe(subframe_url, /*browsing_topics_attribute=*/true);

  {
    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header.html");
    EXPECT_TRUE(topics_header_value);
    EXPECT_EQ(*topics_header_value, kExpectedHeaderValueForSiteB);
  }
  {
    std::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header2.html");
    EXPECT_TRUE(topics_header_value);

    // An empty topics header value was sent, because "c.test" did not observe
    // the candidate topics.
    EXPECT_EQ(topics_header_value, kExpectedHeaderValueForEmptyTopics);
  }

  // Two new observations should have been recorded in addition to the
  // pre-existing one.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 3u);
  EXPECT_EQ(
      api_usage_contexts[0].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("b.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("c.test"));
  EXPECT_EQ(
      api_usage_contexts[1].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("b.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain,
            GetHashedDomain("a.test"));
  EXPECT_EQ(api_usage_contexts[2].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo1.com"));
  EXPECT_EQ(api_usage_contexts[2].hashed_context_domain, HashedDomain(1));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, RedirectMetrics_NoRedirect) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  // Expect no UMA, as Topics API has not been invoked in the page.
  histogram_tester.ExpectTotalCount(kRedirectCountHistogramId,
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(kRedirectWithTopicsInvokedCountHistogramId,
                                    /*expected_count=*/0);

  InvokeTopicsAPI(web_contents());

  histogram_tester.ExpectUniqueSample(kRedirectCountHistogramId,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      kRedirectWithTopicsInvokedCountHistogramId,
      /*sample=*/0,
      /*expected_bucket_count=*/1);

  // Calling Topics API the second time won't record UMA again.
  InvokeTopicsAPI(web_contents());

  histogram_tester.ExpectUniqueSample(kRedirectCountHistogramId,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      kRedirectWithTopicsInvokedCountHistogramId,
      /*sample=*/0,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       RedirectMetrics_OneRedirectWithoutTopicsInvoked) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url1 =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url1));

  GURL main_frame_url2 =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents(), main_frame_url2));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  histogram_tester.ExpectUniqueSample(kRedirectCountHistogramId,
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      kRedirectWithTopicsInvokedCountHistogramId,
      /*sample=*/0,
      /*expected_bucket_count=*/1);

  // The redirect chain has only one page calling topics. We need at least two
  // for this `TopicsRedirectChainDetected` event, so it's not being logged.
  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::BrowsingTopics_TopicsRedirectChainDetected::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       RedirectMetrics_OneRedirectWithTopicsInvoked) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url1 =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url1));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  GURL main_frame_url2 =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents(), main_frame_url2));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  histogram_tester.ExpectBucketCount(kRedirectCountHistogramId,
                                     /*sample=*/1,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kRedirectWithTopicsInvokedCountHistogramId,
                                     /*sample=*/1,
                                     /*expected_count=*/1);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::BrowsingTopics_TopicsRedirectChainDetected::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder_->ExpectEntrySourceHasUrl(entries.back(), main_frame_url1);
  ukm_recorder_->ExpectEntryMetric(entries.back(), "NumberOfPagesCallingTopics",
                                   2);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       RedirectMetrics_HasGesture_RedirectTrackingReset) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url1 =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url1));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  GURL main_frame_url2 =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  // Trigger a renderer navigation with user activation. The redirect tracking
  // will be reset.
  ASSERT_TRUE(
      content::NavigateToURLFromRenderer(web_contents(), main_frame_url2));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  histogram_tester.ExpectUniqueSample(kRedirectCountHistogramId,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/2);
  histogram_tester.ExpectUniqueSample(
      kRedirectWithTopicsInvokedCountHistogramId,
      /*sample=*/0,
      /*expected_bucket_count=*/2);
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    RedirectMetrics_BrowserInitiatedNavigation_RedirectTrackingReset) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url1 =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url1));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  GURL main_frame_url2 =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  // Trigger a browser-initiated navigation. The redirect tracking will be
  // reset.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url2));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  histogram_tester.ExpectUniqueSample(kRedirectCountHistogramId,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/2);
  histogram_tester.ExpectUniqueSample(
      kRedirectWithTopicsInvokedCountHistogramId,
      /*sample=*/0,
      /*expected_bucket_count=*/2);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       RedirectMetrics_PopUp_RedirectTrackingReset) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url1 =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url1));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  // Enable automated pop-ups.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(web_contents()->GetURL(), GURL(),
                                      ContentSettingsType::POPUPS,
                                      CONTENT_SETTING_ALLOW);

  GURL main_frame_url2 =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  // Trigger an automated pop-up. The redirect tracking will be reset.
  content::WebContentsAddedObserver observer;
  EXPECT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("window.open($1)", main_frame_url2),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  content::WebContents* new_web_contents = observer.GetWebContents();
  content::TestNavigationObserver popup_navigation_observer(new_web_contents);
  popup_navigation_observer.Wait();

  InvokeTopicsAPI(new_web_contents, /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  histogram_tester.ExpectUniqueSample(kRedirectCountHistogramId,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/2);
  histogram_tester.ExpectUniqueSample(
      kRedirectWithTopicsInvokedCountHistogramId,
      /*sample=*/0,
      /*expected_bucket_count=*/2);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       RedirectMetrics_PopUpAndOpenerNavigation) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url1 =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url1));

  content::WebContents* initial_web_contents = web_contents();
  InvokeTopicsAPI(initial_web_contents, /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  GURL main_frame_url2 =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  // Trigger a pop-up (with user gesture).
  content::WebContentsAddedObserver observer;
  EXPECT_TRUE(
      content::ExecJs(initial_web_contents,
                      content::JsReplace("window.open($1)", main_frame_url2)));

  content::WebContents* new_web_contents = observer.GetWebContents();
  content::TestNavigationObserver popup_navigation_observer(new_web_contents);
  popup_navigation_observer.Wait();

  GURL main_frame_url3 =
      https_server_.GetURL("c.test", "/browsing_topics/empty_page.html");

  // Trigger an opener navigation from the pop-up page.
  content::TestNavigationObserver opener_navigation_observer(
      initial_web_contents);
  EXPECT_TRUE(content::ExecJs(
      new_web_contents,
      content::JsReplace("window.opener.location.href = $1", main_frame_url3),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  opener_navigation_observer.Wait();

  InvokeTopicsAPI(initial_web_contents, /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  // Expect that the page resulted from the opener navigation will be
  // initialized with the redirect status derived from the initial page.
  histogram_tester.ExpectBucketCount(kRedirectCountHistogramId,
                                     /*sample=*/1,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kRedirectWithTopicsInvokedCountHistogramId,
                                     /*sample=*/1,
                                     /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    RedirectMetrics_SameDocNavigation_RedirectStateUnaffected) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url1 =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url1));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  GURL main_frame_url2 =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html#123");

  // Trigger a same-doc navigation. The page and its redirect state won't be
  // affected.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents(), main_frame_url2));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  histogram_tester.ExpectUniqueSample(kRedirectCountHistogramId,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      kRedirectWithTopicsInvokedCountHistogramId,
      /*sample=*/0,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       RedirectMetrics_TenRedirectsWithTopicsInvoked) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  for (int i = 0; i < 10; ++i) {
    GURL new_main_frame_url =
        https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

    ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
        web_contents(), new_main_frame_url));

    InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                    content::EXECUTE_SCRIPT_NO_USER_GESTURE);
  }

  // For each bucket from 0 to 4, expect a single sample.
  for (int i = 0; i < 4; ++i) {
    histogram_tester.ExpectBucketCount(kRedirectCountHistogramId,
                                       /*sample=*/i,
                                       /*expected_count=*/1);
    histogram_tester.ExpectBucketCount(
        kRedirectWithTopicsInvokedCountHistogramId,
        /*sample=*/i,
        /*expected_count=*/1);
  }

  // For bucket 5, it should have 6 samples (corresponding to the last 6 page
  // loads), as we cap the number at 5.
  histogram_tester.ExpectBucketCount(kRedirectCountHistogramId,
                                     /*sample=*/5,
                                     /*expected_count=*/6);
  histogram_tester.ExpectBucketCount(kRedirectWithTopicsInvokedCountHistogramId,
                                     /*sample=*/5,
                                     /*expected_count=*/6);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       Download_RedirectStateUnaffected) {
  base::HistogramTester histogram_tester;

  GURL main_frame_url1 =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url1));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  GURL download_url =
      https_server_.GetURL("a.test", "/downloads/a_zip_file.zip");

  // Trigger a renderer-initiated navigation that turns into a download. The
  // page and its redirect state won't be affected.
  std::unique_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          browser()->profile()->GetDownloadManager(), /*wait_count=*/1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  ASSERT_FALSE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents(), download_url));
  observer->WaitForFinished();

  EXPECT_EQ(
      1u, observer->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));

  InvokeTopicsAPI(web_contents(), /*skip_observation=*/false,
                  content::EXECUTE_SCRIPT_NO_USER_GESTURE);

  histogram_tester.ExpectUniqueSample(kRedirectCountHistogramId,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      kRedirectWithTopicsInvokedCountHistogramId,
      /*sample=*/0,
      /*expected_bucket_count=*/1);
}

// Tests that the Topics API abides by the Privacy Sandbox Enrollment framework.
class AttestationBrowsingTopicsBrowserTest : public BrowsingTopicsBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // This test suite tests Privacy Sandbox Attestations related behaviors,
    // turn off the setting that makes all APIs considered attested.
    BrowsingTopicsBrowserTest::SetUpOnMainThread();
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(false);
  }

  ~AttestationBrowsingTopicsBrowserTest() override = default;
};

// Site a.test is attested for Topics, so it should receive a valid response.
IN_PROC_BROWSER_TEST_F(AttestationBrowsingTopicsBrowserTest,
                       AttestedSiteCanGetBrowsingTopicsViaDocumentAPI) {
  privacy_sandbox::PrivacySandboxAttestationsMap map;
  map.insert_or_assign(
      net::SchemefulSite(GURL("https://a.test")),
      privacy_sandbox::PrivacySandboxAttestationsGatedAPISet{
          privacy_sandbox::PrivacySandboxAttestationsGatedAPI::kTopics});
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(map);

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Attestation check for Topics on * failed.");

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  std::string result = InvokeTopicsAPI(web_contents());
  EXPECT_EQ(result, kExpectedApiResult);

  EXPECT_TRUE(console_observer.messages().empty());
}

// Site a.test is not attested for Topics, so it should receive no topics. Note:
// Attestation failure works differently from other failure modes like operating
// in an insecure context. In this case, the API is still exposed, but handling
// will exit before any topics are filled.
IN_PROC_BROWSER_TEST_F(AttestationBrowsingTopicsBrowserTest,
                       UnattestedSiteCannotGetBrowsingTopicsViaDocumentAPI) {
  privacy_sandbox::PrivacySandboxAttestationsMap map;
  map.insert_or_assign(
      net::SchemefulSite(GURL("https://b.test")),
      privacy_sandbox::PrivacySandboxAttestationsGatedAPISet{
          privacy_sandbox::PrivacySandboxAttestationsGatedAPI::kTopics});
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(map);

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Attestation check for Topics on * failed.");

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ("[]", InvokeTopicsAPI(web_contents()));

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_FALSE(console_observer.messages().empty());
}

// Site a.test is attested, but not for Topics, so no topics should be returned.
IN_PROC_BROWSER_TEST_F(
    AttestationBrowsingTopicsBrowserTest,
    AttestedSiteCannotGetBrowsingTopicsViaDocumentAPIWithMismatchedMap) {
  privacy_sandbox::PrivacySandboxAttestationsMap map;
  map.insert_or_assign(net::SchemefulSite(GURL("https://a.test")),
                       privacy_sandbox::PrivacySandboxAttestationsGatedAPISet{
                           privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                               kProtectedAudience});
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(map);

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Attestation check for Topics on * failed.");

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ("[]", InvokeTopicsAPI(web_contents()));

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_FALSE(console_observer.messages().empty());
}

IN_PROC_BROWSER_TEST_F(AttestationBrowsingTopicsBrowserTest,
                       FetchSameOrigin_TopicsEligible_SendTopics_SiteAttested) {
  privacy_sandbox::PrivacySandboxAttestationsMap map;
  map.insert_or_assign(
      net::SchemefulSite(GURL("https://a.test")),
      privacy_sandbox::PrivacySandboxAttestationsGatedAPISet{
          privacy_sandbox::PrivacySandboxAttestationsGatedAPI::kTopics});
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(map);

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Attestation check for Topics on * failed.");

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL fetch_url = https_server_.GetURL(
      "a.test", "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_TRUE(topics_header_value);
  EXPECT_EQ(*topics_header_value, kExpectedHeaderValueForSiteA);

  EXPECT_TRUE(console_observer.messages().empty());
}

IN_PROC_BROWSER_TEST_F(AttestationBrowsingTopicsBrowserTest,
                       FetchSameOrigin_TopicsEligible_SiteNotAttested) {
  privacy_sandbox::PrivacySandboxAttestationsMap map;
  map.insert_or_assign(
      net::SchemefulSite(GURL("https://b.test")),
      privacy_sandbox::PrivacySandboxAttestationsGatedAPISet{
          privacy_sandbox::PrivacySandboxAttestationsGatedAPI::kTopics});
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(map);

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Attestation check for Topics on * failed.");

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL fetch_url = https_server_.GetURL(
      "a.test", "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_EQ(topics_header_value, kExpectedHeaderValueForEmptyTopics);

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_FALSE(console_observer.messages().empty());
}

IN_PROC_BROWSER_TEST_F(
    AttestationBrowsingTopicsBrowserTest,
    FetchSameOrigin_TopicsEligible_SiteAttested_MismatchedMap) {
  privacy_sandbox::PrivacySandboxAttestationsMap map;
  map.insert_or_assign(net::SchemefulSite(GURL("https://a.test")),
                       privacy_sandbox::PrivacySandboxAttestationsGatedAPISet{
                           privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                               kProtectedAudience});
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(map);

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Attestation check for Topics on * failed.");

  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL fetch_url = https_server_.GetURL(
      "a.test", "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_EQ(topics_header_value, kExpectedHeaderValueForEmptyTopics);

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_FALSE(console_observer.messages().empty());
}

// Site a.test is attested, so when an x-origin request is made to it from
// site b.test, a.test should still include a topics header.
IN_PROC_BROWSER_TEST_F(
    AttestationBrowsingTopicsBrowserTest,
    FetchCrossOrigin_TopicsEligible_SendTopics_HasObserveResponse_SiteAttested) {
  privacy_sandbox::PrivacySandboxAttestationsMap map;
  map.insert_or_assign(
      net::SchemefulSite(GURL("https://a.test")),
      privacy_sandbox::PrivacySandboxAttestationsGatedAPISet{
          privacy_sandbox::PrivacySandboxAttestationsGatedAPI::kTopics});
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(map);

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Attestation check for Topics on * failed.");

  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL fetch_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_TRUE(topics_header_value);
  EXPECT_EQ(*topics_header_value, kExpectedHeaderValueForSiteB);

  // A new observation should have been recorded in addition to the pre-existing
  // one, as the response had the `Observe-Browsing-Topics: ?1` header and the
  // request was eligible for topics.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(
      api_usage_contexts[0].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("b.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain,
            GetHashedDomain("a.test"));
  EXPECT_EQ(api_usage_contexts[1].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo1.com"));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain, HashedDomain(1));

  EXPECT_TRUE(console_observer.messages().empty());
}

// Site a.test is not attested, so this should not generate a Topics header in a
// x-origin fetch to site a.test.
IN_PROC_BROWSER_TEST_F(AttestationBrowsingTopicsBrowserTest,
                       FetchCrossOrigin_TopicsEligible_SiteNotAttested) {
  privacy_sandbox::PrivacySandboxAttestationsMap map;
  map.insert_or_assign(
      net::SchemefulSite(GURL("https://b.test")),
      privacy_sandbox::PrivacySandboxAttestationsGatedAPISet{
          privacy_sandbox::PrivacySandboxAttestationsGatedAPI::kTopics});
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(map);

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Attestation check for Topics on * failed.");

  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL fetch_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_EQ(topics_header_value, kExpectedHeaderValueForEmptyTopics);

  // Because a.test is not attested for Topics, we should not have any new
  // observations of API usage.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_FALSE(console_observer.messages().empty());
}

// Site a.test is attested, but not for Topics, so the fetch request to a.test
// should not get a header.
IN_PROC_BROWSER_TEST_F(
    AttestationBrowsingTopicsBrowserTest,
    FetchCrossOrigin_TopicsEligible_SiteNotAttested_MismatchedMap) {
  privacy_sandbox::PrivacySandboxAttestationsMap map;
  map.insert_or_assign(net::SchemefulSite(GURL("https://a.test")),
                       privacy_sandbox::PrivacySandboxAttestationsGatedAPISet{
                           privacy_sandbox::PrivacySandboxAttestationsGatedAPI::
                               kProtectedAudience});
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAttestationsForTesting(map);

  content::WebContentsConsoleObserver console_observer(web_contents());
  console_observer.SetPattern("Attestation check for Topics on * failed.");

  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL fetch_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  std::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_EQ(topics_header_value, kExpectedHeaderValueForEmptyTopics);

  // Because a.test is not attested for Topics, we should not have any new
  // observations of API usage.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);

  ASSERT_TRUE(console_observer.Wait());
  EXPECT_FALSE(console_observer.messages().empty());
}

}  // namespace browsing_topics
