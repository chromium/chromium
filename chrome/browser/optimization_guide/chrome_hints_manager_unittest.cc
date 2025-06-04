// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/chrome_hints_manager.h"

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_tab_url_provider.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/hints_fetcher.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/proto_database_provider_test_base.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_web_contents_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

// A mock class implementation of TabUrlProvider.
class FakeTabUrlProvider : public optimization_guide::TabUrlProvider {
 public:
  const std::vector<GURL> GetUrlsOfActiveTabs(
      const base::TimeDelta& duration_since_last_shown) override {
    num_urls_called_++;
    return urls_;
  }

  void SetUrls(const std::vector<GURL>& urls) { urls_ = urls; }

  int get_num_urls_called() const { return num_urls_called_; }

 private:
  std::vector<GURL> urls_;
  int num_urls_called_ = 0;
};

class ChromeHintsManagerFetchingTest
    : public optimization_guide::ProtoDatabaseProviderTestBase {
 public:
  ChromeHintsManagerFetchingTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{
             optimization_guide::features::kRemoteOptimizationGuideFetching,
             {{"max_concurrent_page_navigation_fetches", "2"},
              {"max_urls_for_optimization_guide_service_hints_fetch", "30"}},
         },
         {optimization_guide::features::kOptimizationHints,
          {{"max_host_keyed_hint_cache_size", "1"}}}},
        {optimization_guide::features::
             kRemoteOptimizationGuideFetchingAnonymousDataConsent});
  }
  ChromeHintsManagerFetchingTest(const ChromeHintsManagerFetchingTest&) =
      delete;
  ChromeHintsManagerFetchingTest& operator=(
      const ChromeHintsManagerFetchingTest&) = delete;
  ~ChromeHintsManagerFetchingTest() override = default;

  void SetUp() override {
    optimization_guide::ProtoDatabaseProviderTestBase::SetUp();
    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    CreateHintsManager();
  }

  void TearDown() override {
    ResetHintsManager();
    optimization_guide::ProtoDatabaseProviderTestBase::TearDown();
  }

  void CreateHintsManager() {
    if (hints_manager_)
      ResetHintsManager();

    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    optimization_guide::prefs::RegisterProfilePrefs(pref_service_->registry());
    unified_consent::UnifiedConsentService::RegisterPrefs(
        pref_service_->registry());

    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    hint_store_ = std::make_unique<optimization_guide::OptimizationGuideStore>(
        db_provider_.get(), temp_dir(),
        task_environment_.GetMainThreadTaskRunner(), /*pref_service=*/nullptr);

    tab_url_provider_ = std::make_unique<FakeTabUrlProvider>();

    hints_manager_ = std::make_unique<ChromeHintsManager>(
        &testing_profile_, pref_service(), hint_store_->AsWeakPtr(),
        /*top_host_provider=*/nullptr, tab_url_provider_.get(),
        url_loader_factory_,
        OptimizationGuideKeyedService::MaybeCreatePushNotificationManager(
            &testing_profile_),
        /*identity_manager=*/nullptr, &optimization_guide_logger_);
    hints_manager_->SetClockForTesting(task_environment_.GetMockClock());

    // Run until hint cache is initialized and the ChromeHintsManager is ready
    // to process hints.
    RunUntilIdle();
  }

  void ResetHintsManager() {
    hints_manager_->Shutdown();
    hints_manager_.reset();
    tab_url_provider_.reset();
    hint_store_.reset();
    pref_service_.reset();
    RunUntilIdle();
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
        std::make_unique<::testing::NiceMock<content::MockNavigationHandle>>(
            web_contents);
    navigation_handle->set_url(url);
    return navigation_handle;
  }

  // Creates a navigation handle WITHOUT the
  // OptimizationGuideWebContentsObserver attached.
  std::unique_ptr<content::MockNavigationHandle> CreateMockNavigationHandle(
      const GURL& url) {
    content::WebContents* web_contents =
        web_contents_factory_->CreateWebContents(&testing_profile_);
    std::unique_ptr<content::MockNavigationHandle> navigation_handle =
        std::make_unique<::testing::NiceMock<content::MockNavigationHandle>>(
            web_contents);
    navigation_handle->set_url(url);
    return navigation_handle;
  }

  content::WebContents* Navigate(GURL url) {
    auto navigation_handle =
        CreateMockNavigationHandleWithOptimizationGuideWebContentsObserver(url);
    return navigation_handle->GetWebContents();
  }

  void FetchHintsUsingWebContentsObserverURLs(
      content::WebContents* web_contents) {
    auto* observer =
        OptimizationGuideWebContentsObserver::FromWebContents(web_contents);
    observer->FetchHintsUsingManager(
        hints_manager(), web_contents->GetPrimaryPage().GetWeakPtr());
  }

  ChromeHintsManager* hints_manager() const { return hints_manager_.get(); }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  PrefService* pref_service() const { return pref_service_.get(); }

  FakeTabUrlProvider* tab_url_provider() const {
    return tab_url_provider_.get();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  TestingProfile testing_profile_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  std::unique_ptr<optimization_guide::OptimizationGuideStore> hint_store_;
  std::unique_ptr<FakeTabUrlProvider> tab_url_provider_;
  std::unique_ptr<ChromeHintsManager> hints_manager_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  OptimizationGuideLogger optimization_guide_logger_;
};

TEST_F(ChromeHintsManagerFetchingTest, HintsFetched_AtSRP_DuplicatesRemoved) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});

  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.emplace_back("https://foo.com/page1.html");
  sorted_predicted_urls.emplace_back("https://foo.com/page2.html");
  sorted_predicted_urls.emplace_back("https://foo.com/page3.html");
  sorted_predicted_urls.emplace_back("https://bar.com/");

  GURL url("https://www.google.com/search?q=a");
  content::WebContents* web_contents = Navigate(url);
  NavigationPredictorKeyedService::Prediction prediction(
      web_contents, url,
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  {
    base::HistogramTester histogram_tester;

    hints_manager()->OnPredictionUpdated(prediction);
    FetchHintsUsingWebContentsObserverURLs(web_contents);

    // Ensure that we only include 2 hosts in the request. These would be
    // foo.com and bar.com.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);
    // Ensure that we include all URLs in the request.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 4, 1);
    RunUntilIdle();
  }

  {
    base::HistogramTester histogram_tester;
    hints_manager()->OnPredictionUpdated(prediction);
    FetchHintsUsingWebContentsObserverURLs(web_contents);

    // Ensure that URLs are not re-fetched.
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0);
  }
}

TEST_F(ChromeHintsManagerFetchingTest,
       HintsFetched_AtSRP_NonHTTPOrHTTPSHostsRemoved) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});

  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.emplace_back("https://foo.com/page1.html");
  sorted_predicted_urls.emplace_back("file://non-web-bar.com/");
  sorted_predicted_urls.emplace_back("http://httppage.com/");

  GURL url("https://www.google.com/search?q=a");
  content::WebContents* web_contents = Navigate(url);
  NavigationPredictorKeyedService::Prediction prediction(
      web_contents, url,
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  FetchHintsUsingWebContentsObserverURLs(web_contents);
  // Ensure that we include both web hosts in the request. These would be
  // foo.com and httppage.com.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 2, 1);
  // Ensure that we only include 2 URLs in the request.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 2, 1);
}

TEST_F(ChromeHintsManagerFetchingTest, HintsFetched_AtSRP) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});

  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.emplace_back("https://foo.com/");
  GURL url("https://www.google.com/search?q=a");
  content::WebContents* web_contents = Navigate(url);
  NavigationPredictorKeyedService::Prediction prediction(
      web_contents, url,
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  FetchHintsUsingWebContentsObserverURLs(web_contents);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 1);
}

TEST_F(ChromeHintsManagerFetchingTest, HintsFetched_AtSRP_GoogleLinksIgnored) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});

  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.emplace_back("https://foo.com/");
  sorted_predicted_urls.emplace_back("https://google.com/bar");
  GURL url("https://www.google.com/search?q=a");
  content::WebContents* web_contents = Navigate(url);
  NavigationPredictorKeyedService::Prediction prediction(
      web_contents, url,
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  FetchHintsUsingWebContentsObserverURLs(web_contents);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 1);
}

TEST_F(ChromeHintsManagerFetchingTest, HintsFetched_AtNonSRP) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});

  base::HistogramTester histogram_tester;
  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.emplace_back("https://foo.com/");
  GURL url("https://www.not-google.com/");
  content::WebContents* web_contents = Navigate(url);
  NavigationPredictorKeyedService::Prediction prediction(
      web_contents, url,
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  hints_manager()->OnPredictionUpdated(prediction);
  FetchHintsUsingWebContentsObserverURLs(web_contents);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.HostCount", 0);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount", 0);
}

class ChromeHintsManagerPushEnabledTest
    : public ChromeHintsManagerFetchingTest {
 public:
  ChromeHintsManagerPushEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(
        optimization_guide::features::kPushNotifications);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeHintsManagerPushEnabledTest, PushManagerSet) {
  EXPECT_TRUE(hints_manager()->push_notification_manager());
}

class ChromeHintsManagerPushDisabledTest
    : public ChromeHintsManagerFetchingTest {
 public:
  ChromeHintsManagerPushDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        optimization_guide::features::kPushNotifications);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromeHintsManagerPushDisabledTest, PushManagerSet) {
  EXPECT_FALSE(hints_manager()->push_notification_manager());
}

TEST_F(ChromeHintsManagerFetchingTest, NoOptimizationGuideWebContentsObserver) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      optimization_guide::switches::kDisableCheckingUserPermissionsForTesting);
  hints_manager()->RegisterOptimizationTypes(
      {optimization_guide::proto::DEFER_ALL_SCRIPT});

  std::vector<GURL> sorted_predicted_urls;
  sorted_predicted_urls.emplace_back("https://foo.com/page1.html");

  GURL url("https://www.google.com/search?q=a");
  auto navigation_handle = CreateMockNavigationHandle(url);
  content::WebContents* web_contents = navigation_handle->GetWebContents();

  NavigationPredictorKeyedService::Prediction prediction(
      web_contents, url,
      NavigationPredictorKeyedService::PredictionSource::
          kAnchorElementsParsedFromWebPage,
      sorted_predicted_urls);

  // Calling `OnPredictionUpdated` without having a valid
  // `OptimizationGuideWebContentsObserver` should not cause a crash.
  hints_manager()->OnPredictionUpdated(prediction);
}
}  // namespace optimization_guide
