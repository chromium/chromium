// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/url_constants.h"

namespace {

class SearchEnginePreconnectorBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public predictors::PreconnectManager::Observer {
 public:
  static constexpr char kFakeSearch[] = "https://www.fakesearch.com/";
  static constexpr char kGoogleSearch[] = "https://www.google.com/";

  SearchEnginePreconnectorBrowserTest() = default;
  ~SearchEnginePreconnectorBrowserTest() override = default;

  void SetUp() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->ServeFilesFromSourceDirectory(
        "chrome/test/data/navigation_predictor");
    ASSERT_TRUE(https_server_->Start());

    preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()] = 0;
    preresolve_counts_[GURL(kGoogleSearch)] = 0;
    preresolve_counts_[GURL(kFakeSearch)] = 0;

    subresource_filter::SubresourceFilterBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();

    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(
            browser()->profile());
    ASSERT_TRUE(loading_predictor);
    loading_predictor->preconnect_manager()->SetObserverForTesting(this);
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  void OnPreresolveFinished(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      bool success) override {
    const GURL origin = url.DeprecatedGetOriginAsURL();
    if (!base::Contains(preresolve_counts_, origin)) {
      return;
    }

    // Only the test URL should successfully preconnect.
    EXPECT_EQ(origin == GetTestURL("/").DeprecatedGetOriginAsURL(), success);

    ++preresolve_counts_[origin];
    if (run_loops_[origin])
      run_loops_[origin]->Quit();
  }

  void WaitForPreresolveCountForURL(const GURL& url, int expected_count) {
    const GURL origin = url.DeprecatedGetOriginAsURL();
    EXPECT_TRUE(base::Contains(preresolve_counts_, origin));
    while (preresolve_counts_[origin] < expected_count) {
      run_loops_[origin] = std::make_unique<base::RunLoop>();
      run_loops_[origin]->Run();
      run_loops_[origin].reset();
    }
  }

  void WaitForDelay(base::TimeDelta delay) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delay);
    run_loop.Run();
  }

 protected:
  std::map<GURL, int> preresolve_counts_;
  base::test::ScopedFeatureList feature_list_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::map<GURL, std::unique_ptr<base::RunLoop>> run_loops_;
};

// static
constexpr char SearchEnginePreconnectorBrowserTest::kFakeSearch[];
constexpr char SearchEnginePreconnectorBrowserTest::kGoogleSearch[];

class SearchEnginePreconnectorNoDelaysBrowserTest
    : public SearchEnginePreconnectorBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  SearchEnginePreconnectorNoDelaysBrowserTest() {
    if (PreconnectWithPrivacyModeEnabled()) {
      feature_list_.InitWithFeaturesAndParameters(
          {{features::kPreconnectToSearch, {{"startup_delay_ms", "1000000"}}},
           {net::features::kNetUnusedIdleSocketTimeout,
            {{"unused_idle_socket_timeout_seconds", "0"}}},
           {features::kPreconnectToSearchWithPrivacyModeEnabled, {}}},
          {});
    } else {
      feature_list_.InitWithFeaturesAndParameters(
          {{features::kPreconnectToSearch, {{"startup_delay_ms", "1000000"}}},
           {net::features::kNetUnusedIdleSocketTimeout,
            {{"unused_idle_socket_timeout_seconds", "0"}}}},
          {{features::kPreconnectToSearchWithPrivacyModeEnabled}});
    }
  }

  bool PreconnectWithPrivacyModeEnabled() const { return GetParam(); }

  ~SearchEnginePreconnectorNoDelaysBrowserTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SearchEnginePreconnectorNoDelaysBrowserTest,
                         testing::Bool());

// Test routinely flakes on the Mac10.11 Tests bot (https://crbug.com/1141028).
IN_PROC_BROWSER_TEST_P(SearchEnginePreconnectorNoDelaysBrowserTest,
                       DISABLED_PreconnectSearch) {
  // Put the fake search URL to be preconnected in foreground.
  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);
  // Verifies that the default search is preconnected.
  constexpr char16_t kShortName[] = u"test";
  constexpr char kSearchURL[] = "/anchors_different_area.html?q={searchTerms}";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  // Check default URL is being preconnected and test URL is not.
  const GURL kDefaultUrl(kGoogleSearch);
  WaitForPreresolveCountForURL(kDefaultUrl, 2);
  EXPECT_EQ(2, preresolve_counts_[kDefaultUrl]);
  EXPECT_EQ(0, preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);

  TemplateURLData data;
  data.SetShortName(kShortName);
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());
  data.preconnect_to_search_url = true;

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  // Put the fake search URL to be preconnected in foreground.
  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);

  // After switching search providers, the test URL should now start being
  // preconnected.
  if (PreconnectWithPrivacyModeEnabled()) {
    WaitForPreresolveCountForURL(GetTestURL("/"), 2);
    // Preconnect should occur for DSE.
    EXPECT_EQ(2,
              preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);

    WaitForPreresolveCountForURL(GetTestURL("/"), 4);
    // Preconnect should occur again for DSE.
    EXPECT_EQ(4,
              preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);
  } else {
    WaitForPreresolveCountForURL(GetTestURL("/"), 1);
    // Preconnect should occur for DSE.
    EXPECT_EQ(1,
              preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);

    WaitForPreresolveCountForURL(GetTestURL("/"), 2);
    // Preconnect should occur again for DSE.
    EXPECT_EQ(2,
              preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);
  }
}

IN_PROC_BROWSER_TEST_P(SearchEnginePreconnectorNoDelaysBrowserTest,
                       PreconnectOnlyInForeground) {
  constexpr char16_t kShortName[] = u"test";
  constexpr char kSearchURL[] = "/anchors_different_area.html?q={searchTerms}";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(kShortName);
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());
  data.preconnect_to_search_url = true;

  // Set the DSE to the test URL.
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  // Ensure that we wait long enough to trigger preconnects.
  WaitForDelay(base::Milliseconds(200));

  TemplateURLData data_fake_search;
  data_fake_search.SetShortName(kShortName);
  data_fake_search.SetKeyword(data.short_name());
  data_fake_search.SetURL(kFakeSearch);
  data_fake_search.preconnect_to_search_url = true;

  template_url = model->Add(std::make_unique<TemplateURL>(data_fake_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  // Put the fake search URL to be preconnected in foreground.
  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);
  const GURL search_url = template_url->GenerateSearchURL({});
  if (PreconnectWithPrivacyModeEnabled()) {
    WaitForPreresolveCountForURL(search_url, 2);

    // Preconnect should occur for fake search (2 since there are 2 NAKs).
    EXPECT_EQ(2, preresolve_counts_[search_url]);
  } else {
    WaitForPreresolveCountForURL(search_url, 1);

    // Preconnect should occur for fake search.
    EXPECT_EQ(1, preresolve_counts_[search_url]);
  }

  // No preconnects should have been issued for the test URL.
  EXPECT_EQ(0, preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);
}

class SearchEnginePreconnectorForegroundBrowserTest
    : public SearchEnginePreconnectorBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  SearchEnginePreconnectorForegroundBrowserTest() {
    {
      std::vector<base::test::FeatureRefAndParams> enabled_features;
      std::vector<base::test::FeatureRef> disabled_features;
      if (skip_in_background()) {
        enabled_features.push_back({features::kPreconnectToSearch,
                                    {{"startup_delay_ms", "1000000"},
                                     {"skip_in_background", "true"}}});
      } else {
        enabled_features.push_back({features::kPreconnectToSearch,
                                    {{"startup_delay_ms", "1000000"},
                                     {"skip_in_background", "false"}}});
      }
      if (preconnect_to_search_with_privacy_mode_enabled()) {
        enabled_features.push_back(
            {features::kPreconnectToSearchWithPrivacyModeEnabled, {}});
      } else {
        disabled_features.emplace_back(
            features::kPreconnectToSearchWithPrivacyModeEnabled);
      }
      feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                  disabled_features);
    }
  }

  bool skip_in_background() const { return std::get<0>(GetParam()); }

  bool load_page() const { return std::get<1>(GetParam()); }

  bool preconnect_to_search_with_privacy_mode_enabled() const {
    return std::get<2>(GetParam());
  }

  ~SearchEnginePreconnectorForegroundBrowserTest() override = default;

  base::SimpleTestTickClock tick_clock_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SearchEnginePreconnectorForegroundBrowserTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Test that search engine preconnects are done only if the browser app is
// likely in foreground.
IN_PROC_BROWSER_TEST_P(SearchEnginePreconnectorForegroundBrowserTest,
                       PreconnectOnlyInForeground) {
  base::HistogramTester histogram_tester;
  static const char16_t kShortName[] = u"test";
  static const char kSearchURL[] =
      "/anchors_different_area.html?q={searchTerms}";
  static const char kSearchURLWithQuery[] =
      "/anchors_different_area.html?q=porgs";

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(kShortName);
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());
  data.preconnect_to_search_url = true;

  // Set the DSE to the test URL.
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  // Ensure that we wait long enough to trigger preconnects.
  WaitForDelay(base::Milliseconds(200));

  TemplateURLData data_fake_search;
  data_fake_search.SetShortName(kShortName);
  data_fake_search.SetKeyword(data.short_name());
  const GURL fake_search_url(kFakeSearch);
  data_fake_search.SetURL(kFakeSearch);
  data_fake_search.preconnect_to_search_url = true;

  template_url = model->Add(std::make_unique<TemplateURL>(data_fake_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  tick_clock_.SetNowTicks(base::TimeTicks::Now());
  tick_clock_.Advance(base::Seconds(10000));

  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->SetTickClockForTesting(&tick_clock_);

  if (load_page()) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                             GetTestURL(kSearchURLWithQuery)));
  }

  // Put the fake search URL to be preconnected in foreground.
  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);

  if (preconnect_to_search_with_privacy_mode_enabled()) {
    if (!skip_in_background() || load_page()) {
      WaitForPreresolveCountForURL(fake_search_url, 2);
    }

    // If preconnects are skipped in background and no web contents is in
    // foreground, then no preconnect should happen.
    EXPECT_EQ(skip_in_background() && !load_page() ? 0 : 2,
              preresolve_counts_[fake_search_url]);
  } else {
    if (!skip_in_background() || load_page()) {
      WaitForPreresolveCountForURL(fake_search_url, 1);
    }

    // If preconnects are skipped in background and no web contents is in
    // foreground, then no preconnect should happen.
    EXPECT_EQ(skip_in_background() && !load_page() ? 0 : 1,
              preresolve_counts_[fake_search_url]);
  }
  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.SearchEnginePreconnector."
      "IsBrowserAppLikelyInForeground",
      !!load_page(), 1);

  EXPECT_EQ(load_page() ? 1 : 0,
            preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);
}

class SearchEnginePreconnectorKeepSocketBrowserTest
    : public SearchEnginePreconnectorBrowserTest {
 public:
  SearchEnginePreconnectorKeepSocketBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPreconnectToSearch, {{"startup_delay_ms", "1000000"}}},
         {net::features::kNetUnusedIdleSocketTimeout,
          {{"unused_idle_socket_timeout_seconds", "60"}}}},
        {});
  }

  ~SearchEnginePreconnectorKeepSocketBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SearchEnginePreconnectorKeepSocketBrowserTest,
                       SocketWarmForSearch) {
  // Verifies that a navigation to search will use a warm socket.
  constexpr char16_t kShortName[] = u"test";
  constexpr char kSearchURL[] = "/anchors_different_area.html?q={searchTerms}";
  constexpr char kSearchURLWithQuery[] = "/anchors_different_area.html?q=porgs";

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(kShortName);
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());
  data.preconnect_to_search_url = true;

  // Set the DSE to the test URL.
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  // Put the fake search URL to be preconnected in foreground.
  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);

  WaitForPreresolveCountForURL(GetTestURL(kSearchURL), 1);

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetTestURL(kSearchURLWithQuery)));

  auto ukm_recorder = std::make_unique<ukm::TestAutoSetUkmRecorder>();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  const auto& entries =
      ukm_recorder->GetMergedEntriesByName(ukm::builders::PageLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());

  for (const auto& kv : entries) {
    EXPECT_TRUE(ukm_recorder->EntryHasMetric(
        kv.second.get(),
        ukm::builders::PageLoad::kMainFrameResource_SocketReusedName));
  }
}

class SearchEnginePreconnectorDesktopAutoStartBrowserTest
    : public SearchEnginePreconnectorBrowserTest {
 public:
  SearchEnginePreconnectorDesktopAutoStartBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPreconnectToSearch, {{"startup_delay_ms", "0"}}},
         {net::features::kNetUnusedIdleSocketTimeout,
          {{"unused_idle_socket_timeout_seconds", "0"}}}},
        {});
  }

  ~SearchEnginePreconnectorDesktopAutoStartBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SearchEnginePreconnectorDesktopAutoStartBrowserTest,
                       AutoStartDesktop) {
  // Verifies that the default search is preconnected.
  WaitForPreresolveCountForURL(GURL(kGoogleSearch), 2);
}

class SearchEnginePreconnectorEnabledOnlyBrowserTest
    : public SearchEnginePreconnectorBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  SearchEnginePreconnectorEnabledOnlyBrowserTest() {
    {
      if (PreconnectWithPrivacyModeEnabled()) {
        feature_list_.InitWithFeaturesAndParameters(
            {{features::kPreconnectToSearch, {{"startup_delay_ms", "1000000"}}},
             {net::features::kNetUnusedIdleSocketTimeout,
              {{"unused_idle_socket_timeout_seconds", "60"}}},
             {features::kPreconnectToSearchWithPrivacyModeEnabled, {}}},
            {});
      } else {
        feature_list_.InitWithFeaturesAndParameters(
            {{features::kPreconnectToSearch, {{"startup_delay_ms", "1000000"}}},
             {net::features::kNetUnusedIdleSocketTimeout,
              {{"unused_idle_socket_timeout_seconds", "60"}}}},
            {{features::kPreconnectToSearchWithPrivacyModeEnabled}});
      }
    }
  }

  bool PreconnectWithPrivacyModeEnabled() const { return GetParam(); }

  ~SearchEnginePreconnectorEnabledOnlyBrowserTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SearchEnginePreconnectorEnabledOnlyBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(SearchEnginePreconnectorEnabledOnlyBrowserTest,
                       AllowedSearch) {
  constexpr char16_t kShortName[] = u"test";
  constexpr char kSearchURL[] = "/anchors_different_area.html?q={searchTerms}";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data;
  data.SetShortName(kShortName);
  data.SetKeyword(data.short_name());
  data.SetURL(GetTestURL(kSearchURL).spec());
  data.preconnect_to_search_url = false;

  // Set the DSE to the test URL.
  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);

  TemplateURLData data_allowed_search;
  data_allowed_search.SetShortName(kShortName);
  data_allowed_search.SetKeyword(data.short_name());
  data_allowed_search.SetURL(kGoogleSearch);
  data_allowed_search.preconnect_to_search_url = true;

  template_url = model->Add(std::make_unique<TemplateURL>(data_allowed_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  NavigationPredictorKeyedServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser()->profile()))
      ->search_engine_preconnector()
      ->StartPreconnecting(/*with_startup_delay=*/false);

  const GURL search_url = template_url->GenerateSearchURL({});
  if (PreconnectWithPrivacyModeEnabled()) {
    WaitForPreresolveCountForURL(search_url, 2);

    // Preconnect should occur for Google search (2 since there are 2 NAKs).
    EXPECT_EQ(2, preresolve_counts_[search_url]);
  } else {
    WaitForPreresolveCountForURL(search_url, 1);

    // Preconnect should occur for Google search.
    EXPECT_EQ(1, preresolve_counts_[search_url]);
  }

  // No preconnects should have been issued for the test URL.
  EXPECT_EQ(0, preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);
}

}  // namespace
