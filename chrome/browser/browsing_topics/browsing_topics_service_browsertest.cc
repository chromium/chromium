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
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/page_content_annotations_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "components/browsing_topics/browsing_topics_service_impl.h"
#include "components/browsing_topics/epoch_topics.h"
#include "components/browsing_topics/test_util.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/content/browser/test_page_content_annotations_service.h"
#include "components/optimization_guide/content/browser/test_page_content_annotator.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browsing_topics_site_data_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_topics_test_util.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

namespace {

constexpr browsing_topics::HmacKey kTestKey = {1};

constexpr base::Time kTime1 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(1));
constexpr base::Time kTime2 =
    base::Time::FromDeltaSinceWindowsEpoch(base::Days(2));

constexpr size_t kTaxonomySize = 349;
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

constexpr char kExpectedHeaderValueForSiteA[] =
    "1;version=\"chrome.1:1:2\";config_version=\"chrome.1\";model_version="
    "\"2\";taxonomy_version=\"1\", "
    "10;version=\"chrome.1:1:2\";config_version=\"chrome.1\";model_version="
    "\"2\";taxonomy_version=\"1\"";

constexpr char kExpectedHeaderValueForSiteB[] =
    "1;version=\"chrome.1:1:2\";config_version=\"chrome.1\";model_version="
    "\"2\";taxonomy_version=\"1\", "
    "7;version=\"chrome.1:1:2\";config_version=\"chrome.1\";model_version="
    "\"2\";taxonomy_version=\"1\"";

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
                     kPaddedTopTopicsStartIndex, kTaxonomySize,
                     kTaxonomyVersion, kModelVersion, calculation_time);
}

class PortalActivationWaiter : public content::WebContentsObserver {
 public:
  explicit PortalActivationWaiter(content::WebContents* portal_contents)
      : content::WebContentsObserver(portal_contents) {}

  void Wait() {
    if (!web_contents()->IsPortal())
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // content::WebContentsObserver:
  void DidActivatePortal(content::WebContents* predecessor_contents,
                         base::TimeTicks activation_time) override {
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

}  // namespace

// A tester class that allows waiting for the first calculation to finish.
class TesterBrowsingTopicsService : public BrowsingTopicsServiceImpl {
 public:
  TesterBrowsingTopicsService(
      const base::FilePath& profile_path,
      privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      optimization_guide::PageContentAnnotationsService* annotations_service,
      base::OnceClosure calculation_finish_callback)
      : BrowsingTopicsServiceImpl(
            profile_path,
            privacy_sandbox_settings,
            history_service,
            site_data_manager,
            annotations_service,
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

class BrowsingTopicsBrowserTestBase : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
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
                              bool skip_observation = false) {
    return EvalJs(adapter, content::JsReplace(R"(
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
                                              skip_observation))
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

  absl::optional<std::string> GetTopicsHeaderForRequestPath(
      const std::string& request_path) {
    auto it = request_path_topics_map_.find(request_path);
    if (it == request_path_topics_map_.end()) {
      return absl::nullopt;
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

class BrowsingTopicsBrowserTest : public BrowsingTopicsBrowserTestBase {
 public:
  BrowsingTopicsBrowserTest()
      : prerender_helper_(
            base::BindRepeating(&BrowsingTopicsBrowserTest::web_contents,
                                base::Unretained(this))) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kBrowsingTopics, blink::features::kBrowsingTopicsXHR,
         blink::features::kBrowsingTopicsBypassIPIsPubliclyRoutableCheck,
         features::kPrivacySandboxAdsAPIsOverride, blink::features::kPortals},
        /*disabled_features=*/{});
  }

  ~BrowsingTopicsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    prerender_helper_.SetUp(&https_server_);

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

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  std::vector<optimization_guide::WeightedIdentifier> TopicsAndWeight(
      const std::vector<int32_t>& topics,
      double weight) {
    std::vector<optimization_guide::WeightedIdentifier> result;
    for (int32_t topic : topics) {
      result.emplace_back(topic, weight);
    }

    return result;
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    PageContentAnnotationsServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            &BrowsingTopicsBrowserTest::CreatePageContentAnnotationsService,
            base::Unretained(this)));

    browsing_topics::BrowsingTopicsServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindRepeating(
                &BrowsingTopicsBrowserTest::CreateBrowsingTopicsService,
                base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreatePageContentAnnotationsService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);

    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::IMPLICIT_ACCESS);

    DCHECK(!base::Contains(optimization_guide_model_providers_, profile));
    optimization_guide_model_providers_.emplace(
        profile, std::make_unique<
                     optimization_guide::TestOptimizationGuideModelProvider>());

    auto page_content_annotations_service =
        optimization_guide::TestPageContentAnnotationsService::Create(
            optimization_guide_model_providers_.at(profile).get(),
            history_service);

    page_content_annotations_service->OverridePageContentAnnotatorForTesting(
        &test_page_content_annotator_);

    return page_content_annotations_service;
  }

  void InitializePreexistingState(
      history::HistoryService* history_service,
      content::BrowsingTopicsSiteDataManager* site_data_manager,
      const base::FilePath& profile_path) {
    // Configure the (mock) model.
    test_page_content_annotator_.UsePageTopics(
        *optimization_guide::TestModelInfoBuilder().SetVersion(1).Build(),
        {{"foo6.com", TopicsAndWeight({1, 2, 3, 4, 5, 6}, 0.1)},
         {"foo5.com", TopicsAndWeight({2, 3, 4, 5, 6}, 0.1)},
         {"foo4.com", TopicsAndWeight({3, 4, 5, 6}, 0.1)},
         {"foo3.com", TopicsAndWeight({4, 5, 6}, 0.1)},
         {"foo2.com", TopicsAndWeight({5, 6}, 0.1)},
         {"foo1.com", TopicsAndWeight({6}, 0.1)}});

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
        HashMainFrameHostForStorage("foo1.com"), {HashedDomain(1)},
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

    optimization_guide::PageContentAnnotationsService* annotations_service =
        PageContentAnnotationsServiceFactory::GetForProfile(profile);

    InitializePreexistingState(history_service, site_data_manager,
                               profile->GetPath());

    DCHECK(!base::Contains(calculation_finish_waiters_, profile));
    calculation_finish_waiters_.emplace(profile,
                                        std::make_unique<base::RunLoop>());

    if (!ukm_recorder_)
      ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    return std::make_unique<TesterBrowsingTopicsService>(
        profile->GetPath(), privacy_sandbox_settings, history_service,
        site_data_manager, annotations_service,
        calculation_finish_waiters_.at(profile)->QuitClosure());
  }

  content::test::FencedFrameTestHelper fenced_frame_test_helper_;

  content::test::PrerenderTestHelper prerender_helper_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::map<
      Profile*,
      std::unique_ptr<optimization_guide::TestOptimizationGuideModelProvider>>
      optimization_guide_model_providers_;

  std::map<Profile*, std::unique_ptr<base::RunLoop>>
      calculation_finish_waiters_;

  optimization_guide::TestPageContentAnnotator test_page_content_annotator_;

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

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, CalculationResultUkm) {
  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::kEntryName);

  // The number of entries should equal the number of profiles, which could be
  // greater than 1 on some platform.
  EXPECT_EQ(optimization_guide_model_providers_.size(), entries.size());

  for (auto* entry : entries) {
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTopTopic0Name,
        6);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTopTopic1Name,
        5);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTopTopic2Name,
        4);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTopTopic3Name,
        3);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTopTopic4Name,
        2);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kTaxonomyVersionName,
        1);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kModelVersionName,
        1);
    ukm_recorder_->ExpectEntryMetric(
        entry,
        ukm::builders::BrowsingTopics_EpochTopicsCalculationResult::
            kPaddedTopicsStartIndexName,
        5);
  }
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

  // Invoked the API with {skipObservation: true}.
  EXPECT_EQ("[]", InvokeTopicsAPI(content::ChildFrameAt(
                                      web_contents()->GetPrimaryMainFrame(), 0),
                                  /*skip_observation=*/true));

  // Since {skipObservation: true} was specified, the page is not eligible for
  // topics calculation.
  EXPECT_FALSE(
      BrowsingTopicsEligibleForURLVisit(history_service(), main_frame_url));

  // Since {skipObservation: true} was specified, the usage is not tracked. The
  // returned entry was from the pre-existing storage.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);

  // Invoked the API with {skipObservation: false}.
  EXPECT_EQ("[]", InvokeTopicsAPI(content::ChildFrameAt(
                      web_contents()->GetPrimaryMainFrame(), 0)));

  // Since {skipObservation: false} was specified, the page is eligible for
  // topics calculation.
  EXPECT_TRUE(
      BrowsingTopicsEligibleForURLVisit(history_service(), main_frame_url));

  // Since {skipObservation: false} was specified, the usage is tracked.
  api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());

  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo1.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(1));

  EXPECT_EQ(
      api_usage_contexts[1].hashed_main_frame_host,
      HashMainFrameHostForStorage(https_server_.GetURL("a.test", "/").host()));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain,
            GetHashedDomain("b.test"));
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    EmptyPage_PermissionsPolicyBrowsingTopicsNone_TopicsAPI) {
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
            ApiAccessFailureReason::kInvalidRequestingContext);
  EXPECT_FALSE(metrics_entries[0].topic0.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic1.IsValid());
  EXPECT_FALSE(metrics_entries[0].topic2.IsValid());
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

  int host_id = prerender_helper().AddPrerender(prerender_url);

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

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, TopicsAPINotAllowedInPortal) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL portal_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_EQ(true, content::EvalJs(web_contents()->GetPrimaryMainFrame(),
                                  content::JsReplace(R"(
                          new Promise((resolve) => {
                            let portal = document.createElement('portal');
                            portal.src = $1;
                            portal.onload = () => { resolve(true); }
                            document.body.appendChild(portal);
                          });
                          )",
                                                     portal_url)));

  std::vector<content::WebContents*> inner_web_contents =
      web_contents()->GetInnerWebContents();
  EXPECT_EQ(1u, inner_web_contents.size());
  content::WebContents* portal_contents = inner_web_contents[0];

  EXPECT_EQ(
      "document.browsingTopics() is only allowed in the outermost page and "
      "when the page is active.",
      InvokeTopicsAPI(portal_contents));

  // Activate the portal. The API call should succeed.
  PortalActivationWaiter activation_waiter(portal_contents);
  content::ExecuteScriptAsync(web_contents()->GetPrimaryMainFrame(),
                              "document.querySelector('portal').activate();");
  activation_waiter.Wait();

  EXPECT_EQ(portal_contents, web_contents());

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
  EXPECT_TRUE(
      ExecuteScript(web_contents()->GetPrimaryMainFrame(),
                    content::JsReplace("window.open($1)", main_frame_url)));
  popup_observer.Wait();

  GURL new_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_url));
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    FetchSameOrigin_TopicsEligible_SendOneTopic_HasNoObserveResponse) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL fetch_url = https_server_.GetURL(
      "a.test", "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  absl::optional<std::string> topics_header_value =
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

    absl::optional<std::string> topics_header_value =
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

    absl::optional<std::string> topics_header_value =
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
  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL fetch_url = https_server_.GetURL(
      "b.test", "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_TRUE(ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

  absl::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  // Expect an empty header value as "b.test" did not observe the candidate
  // topics.
  EXPECT_TRUE(topics_header_value);
  EXPECT_TRUE(topics_header_value->empty());

  // No observation should have been recorded in addition to the pre-existing
  // one, as the response did not have the `Observe-Browsing-Topics: ?1` header.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    FetchSameOrigin_TopicsEligible_SendNoTopic_HasObserveResponse) {
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

  absl::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  // Expect no topics header as the request was not eligible for topics due to
  // user settings.
  EXPECT_FALSE(topics_header_value);

  // No observation should have been recorded in addition to the pre-existing
  // one even though the response had the `Observe-Browsing-Topics: ?1` header,
  // as the request was not eligible for topics.
  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(browsing_topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    FetchCrossOrigin_TopicsEligible_SendOneTopic_HasObserveResponse) {
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

  absl::optional<std::string> topics_header_value =
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

  absl::optional<std::string> topics_header_value =
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
    GURL fetch_url =
        https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

    EXPECT_TRUE(ExecJs(
        web_contents()->GetPrimaryMainFrame(),
        content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

    absl::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath("/browsing_topics/empty_page.html");

    // No topics header was sent, as the permissions policy denied it.
    EXPECT_FALSE(topics_header_value);
  }

  {
    GURL fetch_url =
        https_server_.GetURL("c.test", "/browsing_topics/empty_page.html");

    EXPECT_TRUE(ExecJs(
        web_contents()->GetPrimaryMainFrame(),
        content::JsReplace("fetch($1, {browsingTopics: true})", fetch_url)));

    absl::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath("/browsing_topics/empty_page.html");

    EXPECT_TRUE(topics_header_value);
  }
}

// On site b.test, test fetch request to a.test that gets redirected to c.test.
// The topics header should be calculated for them individually (i.e. given that
// only a.test has observed the candidate topics for site b.test, the request to
// a.test should have a non-empty topics header, while the redirected request to
// c.test should have an empty topics header.)
IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       FetchCrossOriginWithRedirect) {
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
    absl::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header.html");
    EXPECT_TRUE(topics_header_value);
    EXPECT_EQ(*topics_header_value, kExpectedHeaderValueForSiteB);
  }
  {
    absl::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header2.html");
    EXPECT_TRUE(topics_header_value);

    // An empty topics header value was sent, because "c.test" did not observe
    // the candidate topics.
    EXPECT_TRUE(topics_header_value->empty());
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
    absl::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header.html");

    // No topics header was sent, as the permissions policy denied it.
    EXPECT_FALSE(topics_header_value);
  }
  {
    absl::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header2.html");
    EXPECT_TRUE(topics_header_value);

    // An empty topics header value was sent, as "c.test" did not observe the
    // candidate topics.
    EXPECT_TRUE(topics_header_value->empty());
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

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, XhrWithoutTopicsFlagSet) {
  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL xhr_url = https_server_.GetURL(
      "b.test", "/browsing_topics/page_with_custom_topics_header.html");

  {
    // Send a XHR without the `deprecatedBrowsingTopics` flag. This request
    // isn't eligible for topics.
    EXPECT_EQ("success", EvalJs(web_contents()->GetPrimaryMainFrame(),
                                content::JsReplace(R"(
      const xhr = new XMLHttpRequest();

      xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
          domAutomationController.send('success');
        }
      }

      xhr.open('GET', $1);
      xhr.send();)",
                                                   xhr_url),
                                content::EXECUTE_SCRIPT_USE_MANUAL_REPLY));

    absl::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header.html");

    // Expect no topics header as the request did not set
    // xhr.deprecatedBrowsingTopics.
    EXPECT_FALSE(topics_header_value);
  }

  {
    // Send a XHR with the `deprecatedBrowsingTopics` flag set to false. This
    // request isn't eligible for topics.
    EXPECT_EQ("success", EvalJs(web_contents()->GetPrimaryMainFrame(),
                                content::JsReplace(R"(
      const xhr = new XMLHttpRequest();

      xhr.onreadystatechange = function() {
        if (xhr.readyState == XMLHttpRequest.DONE) {
          domAutomationController.send('success');
        }
      }

      xhr.open('GET', $1);
      xhr.deprecatedBrowsingTopics = false;
      xhr.send();)",
                                                   xhr_url),
                                content::EXECUTE_SCRIPT_USE_MANUAL_REPLY));

    absl::optional<std::string> topics_header_value =
        GetTopicsHeaderForRequestPath(
            "/browsing_topics/page_with_custom_topics_header.html");

    // Expect no topics header as xhr.deprecatedBrowsingTopics was false.
    EXPECT_FALSE(topics_header_value);
  }
}

// On an insecure site (i.e. URL with http scheme), test XHR request that
// attempts to set their `deprecatedBrowsingTopics` to true. Expect that the
// request is not eligible for topics.
IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    XhrCrossOrigin_TopicsNotEligibleDueToInsecureInitiatorContext) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL xhr_url = https_server_.GetURL(
      "b.test", "/browsing_topics/page_with_custom_topics_header.html");

  EXPECT_EQ("success", EvalJs(web_contents()->GetPrimaryMainFrame(),
                              content::JsReplace(R"(
    const xhr = new XMLHttpRequest();

    xhr.onreadystatechange = function() {
      if (xhr.readyState == XMLHttpRequest.DONE) {
        domAutomationController.send('success');
      }
    }

    xhr.open('GET', $1);

    // This will no-op.
    xhr.deprecatedBrowsingTopics = true;

    xhr.send();)",
                                                 xhr_url),
                              content::EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  absl::optional<std::string> topics_header_value =
      GetTopicsHeaderForRequestPath(
          "/browsing_topics/page_with_custom_topics_header.html");

  // Expect no topics header as the request was not eligible for topics due to
  // insecure initiator context.
  EXPECT_FALSE(topics_header_value);
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    XhrCrossOrigin_TopicsEligible_SendOneTopic_HasObserveResponse) {
  GURL main_frame_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  base::StringPairs replacement;
  replacement.emplace_back(std::make_pair("{{STATUS}}", "200 OK"));
  replacement.emplace_back(std::make_pair("{{OBSERVE_BROWSING_TOPICS_HEADER}}",
                                          "Observe-Browsing-Topics: ?1"));
  replacement.emplace_back(std::make_pair("{{REDIRECT_HEADER}}", ""));

  GURL xhr_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "page_with_custom_topics_header.html",
                    replacement));

  EXPECT_EQ("success", EvalJs(web_contents()->GetPrimaryMainFrame(),
                              content::JsReplace(R"(
    const xhr = new XMLHttpRequest();

    xhr.onreadystatechange = function() {
      if (xhr.readyState == XMLHttpRequest.DONE) {
        domAutomationController.send('success');
      }
    }

    xhr.open('GET', $1);
    xhr.deprecatedBrowsingTopics = true;
    xhr.send();)",
                                                 xhr_url),
                              content::EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  absl::optional<std::string> topics_header_value =
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

}  // namespace browsing_topics
