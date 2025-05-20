// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector_keyed_service_factory.h"
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

  SearchEnginePreconnector* GetSearchEnginePreconnector() {
    if (PreconnectFromKeyedServiceEnabled()) {
      return SearchEnginePreconnectorKeyedServiceFactory::GetForProfile(
          browser()->profile());
    }

    NavigationPredictorKeyedService* navigation_predictor_keyed_service =
        NavigationPredictorKeyedServiceFactory::GetForProfile(
            browser()->profile());
    EXPECT_TRUE(navigation_predictor_keyed_service);

    return navigation_predictor_keyed_service->search_engine_preconnector();
  }

  void SetUpOnMainThread() override {
    subresource_filter::SubresourceFilterBrowserTest::SetUpOnMainThread();
    host_resolver()->ClearRules();

    // Get notified for Loading predictor's preconnect observer.
    auto* loading_predictor =
        predictors::LoadingPredictorFactory::GetForProfile(
            browser()->profile());
    ASSERT_TRUE(loading_predictor);
    loading_predictor->preconnect_manager()->SetObserverForTesting(this);

    // Also get notified for the SearchEnginePreconnect's preconnect observer
    SearchEnginePreconnector* preconnector = GetSearchEnginePreconnector();
    ASSERT_TRUE(preconnector);
    preconnector->GetPreconnectManager().SetObserverForTesting(this);
  }

  const GURL GetTestURL(const char* file) const {
    return https_server_->GetURL(file);
  }

  void OnPreresolveFinished(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>&
          observer,
      bool success) override {
    // Take the observer so that we can manually send mojo message.
    if (observer.is_valid() && !remote_.is_bound()) {
      remote_.Bind(std::move(observer));
    }

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

  virtual bool PreconnectFromKeyedServiceEnabled() const;

 protected:
  std::map<GURL, int> preresolve_counts_;
  base::test::ScopedFeatureList feature_list_;

  mojo::Remote<network::mojom::ConnectionChangeObserverClient> remote_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::map<GURL, std::unique_ptr<base::RunLoop>> run_loops_;
};

bool SearchEnginePreconnectorBrowserTest::PreconnectFromKeyedServiceEnabled()
    const {
  return SearchEnginePreconnector::ShouldBeEnabledAsKeyedService();
}

// static
constexpr char SearchEnginePreconnectorBrowserTest::kFakeSearch[];
constexpr char SearchEnginePreconnectorBrowserTest::kGoogleSearch[];

class SearchEnginePreconnectorNoDelaysBrowserTest
    : public SearchEnginePreconnectorBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SearchEnginePreconnectorNoDelaysBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features{
        {features::kPreconnectToSearch, {{"startup_delay_ms", "1000000"}}},
        {net::features::kSearchEnginePreconnectInterval,
         {{"preconnect_interval", "0"}}}};

    std::vector<base::test::FeatureRef> disabled_features;

    if (PreconnectFromKeyedServiceEnabled()) {
      enabled_features.push_back(
          {features::kPreconnectFromKeyedService, {{"run_on_otr", "false"}}});
    } else {
      disabled_features.emplace_back(features::kPreconnectFromKeyedService);
    }

    if (SearchEnginePreconnect2Enabled()) {
      enabled_features.push_back({net::features::kSearchEnginePreconnect2, {}});
    } else {
      disabled_features.emplace_back(net::features::kSearchEnginePreconnect2);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  bool PreconnectFromKeyedServiceEnabled() const override {
    return std::get<0>(GetParam());
  }
  bool SearchEnginePreconnect2Enabled() const {
    return std::get<1>(GetParam());
  }

  ~SearchEnginePreconnectorNoDelaysBrowserTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SearchEnginePreconnectorNoDelaysBrowserTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

// Test routinely flakes on the Mac10.11 Tests bot (https://crbug.com/1141028).
IN_PROC_BROWSER_TEST_P(SearchEnginePreconnectorNoDelaysBrowserTest,
                       DISABLED_PreconnectSearch) {
  // Put the fake search URL to be preconnected in foreground.
  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);
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
  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  // After switching search providers, the test URL should now start being
  // preconnected.
  WaitForPreresolveCountForURL(GetTestURL("/"), 1);
  // Preconnect should occur for DSE.
  EXPECT_EQ(1, preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);

  WaitForPreresolveCountForURL(GetTestURL("/"), 2);
  // Preconnect should occur again for DSE.
  EXPECT_EQ(2, preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);
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

  // Reset the count if SearchEnginePreconnect2 is enabled, since the
  // KeyedService may already spawn the preconnector, and will re-attempt
  // preconnect automatically, resulting in flaky counts
  if (SearchEnginePreconnect2Enabled()) {
    preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()] = 0;
  }

  // Put the fake search URL to be preconnected in foreground.
  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);
  const GURL search_url = template_url->GenerateSearchURL({});
  WaitForPreresolveCountForURL(search_url, 1);

  // Preconnect should occur for fake search.
  EXPECT_EQ(1, preresolve_counts_[search_url]);

  // No preconnects should have been issued for the test URL.
  EXPECT_EQ(0, preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);
}

class SearchEnginePreconnectorForegroundBrowserTest
    : public SearchEnginePreconnectorBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
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

      if (PreconnectFromKeyedServiceEnabled()) {
        enabled_features.push_back(
            {features::kPreconnectFromKeyedService, {{"run_on_otr", "false"}}});
      } else {
        disabled_features.emplace_back(features::kPreconnectFromKeyedService);
      }

      if (SearchEnginePreconnect2Enabled()) {
        enabled_features.push_back(
            {net::features::kSearchEnginePreconnect2, {}});
      } else {
        disabled_features.emplace_back(net::features::kSearchEnginePreconnect2);
      }
      feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                  disabled_features);
    }
  }

  bool skip_in_background() const { return std::get<0>(GetParam()); }

  bool load_page() const { return std::get<1>(GetParam()); }

  bool PreconnectFromKeyedServiceEnabled() const override {
    return std::get<2>(GetParam());
  }

  bool SearchEnginePreconnect2Enabled() const {
    return std::get<3>(GetParam());
  }

  ~SearchEnginePreconnectorForegroundBrowserTest() override = default;

  base::SimpleTestTickClock tick_clock_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SearchEnginePreconnectorForegroundBrowserTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

// Test that search engine preconnects are done only if the browser app is
// likely in foreground.
//
// TODO(crbug.com/413293448): Disabled the test for flakiness due to test setup.
IN_PROC_BROWSER_TEST_P(SearchEnginePreconnectorForegroundBrowserTest,
                       DISABLED_PreconnectOnlyInForeground) {
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

  GetSearchEnginePreconnector()->SetTickClockForTesting(&tick_clock_);

  // Reset the count if SearchEnginePreconnect2 is enabled, since the
  // KeyedService may already spawn the preconnector, and will re-attempt
  // preconnect automatically, resulting in flaky counts
  if (SearchEnginePreconnect2Enabled()) {
    preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()] = 0;
  }
  if (load_page()) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                             GetTestURL(kSearchURLWithQuery)));
  }

  // We start recording the histogram here since the KeyedService may already
  // spawn the preconnector and the histogram may already have some entries
  // prior to this `StartPreconnect`. By placing the HistogramTester here, we
  // will can ensure we only record the histogram for this test.
  base::HistogramTester histogram_tester;
  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  if (!skip_in_background() || load_page()) {
    WaitForPreresolveCountForURL(fake_search_url, 1);
  }

  // If preconnects are skipped in background and no web contents is in
  // foreground, then no preconnect should happen.
  EXPECT_EQ(skip_in_background() && !load_page() ? 0 : 1,
            preresolve_counts_[fake_search_url]);
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
         {net::features::kSearchEnginePreconnectInterval,
          {{"preconnect_interval", "60"}}}},
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
  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

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
        {
            {features::kPreconnectToSearch, {{"startup_delay_ms", "0"}}},
            {net::features::kSearchEnginePreconnectInterval,
             {{"preconnect_interval", "0"}}},
        },
        {});
  }

  ~SearchEnginePreconnectorDesktopAutoStartBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SearchEnginePreconnectorDesktopAutoStartBrowserTest,
                       AutoStartDesktop) {
  int preresolve_count =
      SearchEnginePreconnector::SearchEnginePreconnect2Enabled() ? 1 : 2;
  // Verifies that the default search is preconnected.
  WaitForPreresolveCountForURL(GURL(kGoogleSearch), preresolve_count);
}

class SearchEnginePreconnectorEnabledOnlyBrowserTest
    : public SearchEnginePreconnectorBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SearchEnginePreconnectorEnabledOnlyBrowserTest() {
    {
      std::vector<base::test::FeatureRefAndParams> enabled_features{
          {features::kPreconnectToSearch, {{"startup_delay_ms", "1000000"}}},
          {net::features::kSearchEnginePreconnectInterval,
           {{"preconnect_interval", "60"}}}};

      std::vector<base::test::FeatureRef> disabled_features;
      if (PreconnectFromKeyedServiceEnabled()) {
        enabled_features.push_back(
            {features::kPreconnectFromKeyedService, {{"run_on_otr", "false"}}});
      } else {
        disabled_features.emplace_back(features::kPreconnectFromKeyedService);
      }

      if (SearchEnginePreconnect2Enabled()) {
        enabled_features.push_back(
            {net::features::kSearchEnginePreconnect2, {}});
      } else {
        disabled_features.emplace_back(net::features::kSearchEnginePreconnect2);
      }

      feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                  disabled_features);
    }
  }

  bool PreconnectFromKeyedServiceEnabled() const override {
    return std::get<0>(GetParam());
  }
  bool SearchEnginePreconnect2Enabled() const {
    return std::get<1>(GetParam());
  }

  ~SearchEnginePreconnectorEnabledOnlyBrowserTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SearchEnginePreconnectorEnabledOnlyBrowserTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

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

  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  TemplateURLData data_allowed_search;
  data_allowed_search.SetShortName(kShortName);
  data_allowed_search.SetKeyword(data.short_name());
  data_allowed_search.SetURL(kGoogleSearch);
  data_allowed_search.preconnect_to_search_url = true;

  template_url = model->Add(std::make_unique<TemplateURL>(data_allowed_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  const GURL search_url = template_url->GenerateSearchURL({});
  WaitForPreresolveCountForURL(search_url, 1);

  // Preconnect should occur for Google search.
  EXPECT_EQ(1, preresolve_counts_[search_url]);

  // No preconnects should have been issued for the test URL.
  EXPECT_EQ(0, preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);
}

}  // namespace

class SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest
    : public SearchEnginePreconnectorBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features{
        {features::kPreconnectToSearch, {{"startup_delay_ms", "1000000"}}},
        {net::features::kSearchEnginePreconnectInterval,
         {{"preconnect_interval", "0"}}},
        {net::features::kSearchEnginePreconnect2, {}}};

    std::vector<base::test::FeatureRef> disabled_features;

    if (PreconnectFromKeyedServiceEnabled()) {
      enabled_features.push_back(
          {features::kPreconnectFromKeyedService, {{"run_on_otr", "false"}}});
    } else {
      disabled_features.emplace_back(features::kPreconnectFromKeyedService);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  bool PreconnectFromKeyedServiceEnabled() const override { return GetParam(); }

  void OnPreresolveFinished(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>&
          observer,
      bool success) override {
    // Take the observer so that we can manually send mojo message.
    if (observer.is_valid() && !remote_.is_bound()) {
      remote_.Bind(std::move(observer));
    }

    SearchEnginePreconnectorBrowserTest::OnPreresolveFinished(
        url, network_anonymization_key, observer, success);
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
    ::testing::Bool());

IN_PROC_BROWSER_TEST_P(
    SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
    PreconnectSearch) {
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

  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  TemplateURLData data_allowed_search;
  data_allowed_search.SetShortName(kShortName);
  data_allowed_search.SetKeyword(data.short_name());
  data_allowed_search.SetURL(kGoogleSearch);
  data_allowed_search.preconnect_to_search_url = true;

  template_url = model->Add(std::make_unique<TemplateURL>(data_allowed_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  const GURL search_url = template_url->GenerateSearchURL({});
  WaitForPreresolveCountForURL(search_url, 1);

  // Preconnect should occur for Google search.
  EXPECT_LE(1, preresolve_counts_[search_url]);

  // No preconnects should have been issued for the test URL.
  EXPECT_EQ(0, preresolve_counts_[GetTestURL("/").DeprecatedGetOriginAsURL()]);
}

IN_PROC_BROWSER_TEST_P(
    SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
    PreconnectSearchAfterOnClose) {
  constexpr char16_t kShortName[] = u"test";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data_allowed_search;
  data_allowed_search.SetShortName(kShortName);
  data_allowed_search.SetKeyword(data_allowed_search.short_name());
  data_allowed_search.SetURL(kGoogleSearch);
  data_allowed_search.preconnect_to_search_url = true;

  auto* template_url =
      model->Add(std::make_unique<TemplateURL>(data_allowed_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  GetSearchEnginePreconnector()->SetIsShortSessionForTesting(
      /*is_short_session=*/false);

  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  const GURL search_url = template_url->GenerateSearchURL({});
  WaitForPreresolveCountForURL(search_url, 1);

  // Manually trigger a Session Close. This should trigger a reattempt.
  GetSearchEnginePreconnector()->OnSessionClosed();
  WaitForPreresolveCountForURL(search_url, 2);

  // Preconnect should occur for Google search.
  EXPECT_EQ(2, preresolve_counts_[search_url]);

  // Since this is not a short session, we should be resetting the value.
  EXPECT_EQ(0, GetSearchEnginePreconnector()
                   ->GetConsecutiveConnectionFailureForTesting());
}

IN_PROC_BROWSER_TEST_P(
    SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
    PreconnectSearchAfterOnCloseWithShortSession) {
  constexpr char16_t kShortName[] = u"test";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data_allowed_search;
  data_allowed_search.SetShortName(kShortName);
  data_allowed_search.SetKeyword(data_allowed_search.short_name());
  data_allowed_search.SetURL(kGoogleSearch);
  data_allowed_search.preconnect_to_search_url = true;

  auto* template_url =
      model->Add(std::make_unique<TemplateURL>(data_allowed_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  GetSearchEnginePreconnector()->SetIsShortSessionForTesting(
      /*is_short_session=*/true);

  int failure_before_testing =
      GetSearchEnginePreconnector()
          ->GetConsecutiveConnectionFailureForTesting();

  base::HistogramTester histogram_tester;
  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  const GURL search_url = template_url->GenerateSearchURL({});
  WaitForPreresolveCountForURL(search_url, 1);

  // Manually trigger a Session Close. This should trigger a reattempt.
  GetSearchEnginePreconnector()->OnSessionClosed();
  WaitForPreresolveCountForURL(search_url, 2);

  // Preconnect should occur for Google search.
  EXPECT_EQ(2, preresolve_counts_[search_url]);

  // Since this is a short session, we should be resetting the value.
  EXPECT_EQ(1, GetSearchEnginePreconnector()
                       ->GetConsecutiveConnectionFailureForTesting() -
                   failure_before_testing);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.SearchEnginePreconnector."
      "TriggerEvent",
      static_cast<int>(
          SearchEnginePreconnector::PreconnectTriggerEvent::kSessionClosed),
      1);
}

IN_PROC_BROWSER_TEST_P(
    SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
    PreconnectSearchAfterOnFailure) {
  constexpr char16_t kShortName[] = u"test";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data_allowed_search;
  data_allowed_search.SetShortName(kShortName);
  data_allowed_search.SetKeyword(data_allowed_search.short_name());
  data_allowed_search.SetURL(kGoogleSearch);
  data_allowed_search.preconnect_to_search_url = true;

  auto* template_url =
      model->Add(std::make_unique<TemplateURL>(data_allowed_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  int failure_before_testing =
      GetSearchEnginePreconnector()
          ->GetConsecutiveConnectionFailureForTesting();

  base::HistogramTester histogram_tester;
  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  const GURL search_url = template_url->GenerateSearchURL({});
  WaitForPreresolveCountForURL(search_url, 1);

  // Manually trigger a connection failure. This should trigger a reattempt.
  GetSearchEnginePreconnector()->OnConnectionFailed();
  WaitForPreresolveCountForURL(search_url, 2);

  // Preconnect should occur for Google search.
  EXPECT_EQ(2, preresolve_counts_[search_url]);

  // Since this is a short session, we should be resetting the value.
  EXPECT_EQ(1, GetSearchEnginePreconnector()
                       ->GetConsecutiveConnectionFailureForTesting() -
                   failure_before_testing);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.SearchEnginePreconnector."
      "TriggerEvent",
      static_cast<int>(
          SearchEnginePreconnector::PreconnectTriggerEvent::kConnectionFailed),
      1);
}

IN_PROC_BROWSER_TEST_P(
    SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
    PreconnectSearchAfterOnConnect) {
  constexpr char16_t kShortName[] = u"test";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data_allowed_search;
  data_allowed_search.SetShortName(kShortName);
  data_allowed_search.SetKeyword(data_allowed_search.short_name());
  data_allowed_search.SetURL(kGoogleSearch);
  data_allowed_search.preconnect_to_search_url = true;

  auto* template_url =
      model->Add(std::make_unique<TemplateURL>(data_allowed_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  base::HistogramTester histogram_tester;
  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  const GURL search_url = template_url->GenerateSearchURL({});
  WaitForPreresolveCountForURL(search_url, 1);

  // Manually trigger a new connection. This should trigger a reattempt.
  GetSearchEnginePreconnector()->OnNetworkEvent(
      net::NetworkChangeEvent::kConnected);
  WaitForPreresolveCountForURL(search_url, 2);

  // Preconnect should occur for Google search.
  EXPECT_EQ(2, preresolve_counts_[search_url]);

  histogram_tester.ExpectUniqueSample(
      "NavigationPredictor.SearchEnginePreconnector."
      "TriggerEvent",
      static_cast<int>(
          SearchEnginePreconnector::PreconnectTriggerEvent::kNetworkEvent),
      1);
}

IN_PROC_BROWSER_TEST_P(
    SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
    CalculateBackoffMultiplier) {
  GetSearchEnginePreconnector()->StopPreconnecting();

  int failures = 0;
  for (; failures < std::numeric_limits<int32_t>::digits; failures++) {
    GetSearchEnginePreconnector()->SetConsecutiveFailureForTesting(failures);
    ASSERT_EQ(failures, GetSearchEnginePreconnector()
                            ->GetConsecutiveConnectionFailureForTesting());
    ASSERT_EQ(1 << failures,
              GetSearchEnginePreconnector()->CalculateBackoffMultiplier());
  }

  GetSearchEnginePreconnector()->SetConsecutiveFailureForTesting(failures);
  ASSERT_EQ(failures, GetSearchEnginePreconnector()
                          ->GetConsecutiveConnectionFailureForTesting());
  ASSERT_EQ(1 << (std::numeric_limits<int32_t>::digits - 1),
            GetSearchEnginePreconnector()->CalculateBackoffMultiplier());
}

IN_PROC_BROWSER_TEST_P(
    SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
    PreconnectSearchAfterOnClosedViaMojoPipe) {
  constexpr char16_t kShortName[] = u"test";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data_allowed_search;
  data_allowed_search.SetShortName(kShortName);
  data_allowed_search.SetKeyword(data_allowed_search.short_name());
  data_allowed_search.SetURL(kGoogleSearch);
  data_allowed_search.preconnect_to_search_url = true;

  auto* template_url =
      model->Add(std::make_unique<TemplateURL>(data_allowed_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  const GURL search_url = template_url->GenerateSearchURL({});
  WaitForPreresolveCountForURL(search_url, 1);

  // Manually trigger a new connection. This should trigger a reattempt.
  remote_->OnSessionClosed();

  WaitForPreresolveCountForURL(search_url, 2);

  // Preconnect should occur for Google search.
  EXPECT_EQ(2, preresolve_counts_[search_url]);
}

IN_PROC_BROWSER_TEST_P(
    SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
    PreconnectSearchAfterOnFailureViaMojoPipe) {
  constexpr char16_t kShortName[] = u"test";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data_allowed_search;
  data_allowed_search.SetShortName(kShortName);
  data_allowed_search.SetKeyword(data_allowed_search.short_name());
  data_allowed_search.SetURL(kGoogleSearch);
  data_allowed_search.preconnect_to_search_url = true;

  auto* template_url =
      model->Add(std::make_unique<TemplateURL>(data_allowed_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  const GURL search_url = template_url->GenerateSearchURL({});
  WaitForPreresolveCountForURL(search_url, 1);

  // Manually trigger a new connection. This should trigger a reattempt.
  remote_->OnConnectionFailed();

  WaitForPreresolveCountForURL(search_url, 2);

  // Preconnect should occur for Google search.
  EXPECT_EQ(2, preresolve_counts_[search_url]);
}

IN_PROC_BROWSER_TEST_P(
    SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
    PreconnectSearchAfterOnNetworkEventViaMojoPipoe) {
  constexpr char16_t kShortName[] = u"test";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data_allowed_search;
  data_allowed_search.SetShortName(kShortName);
  data_allowed_search.SetKeyword(data_allowed_search.short_name());
  data_allowed_search.SetURL(kGoogleSearch);
  data_allowed_search.preconnect_to_search_url = true;

  auto* template_url =
      model->Add(std::make_unique<TemplateURL>(data_allowed_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  const GURL search_url = template_url->GenerateSearchURL({});
  WaitForPreresolveCountForURL(search_url, 1);

  // Manually trigger a new connection. This should trigger a reattempt.
  remote_->OnNetworkEvent(net::NetworkChangeEvent::kConnected);

  WaitForPreresolveCountForURL(search_url, 2);

  // Preconnect should occur for Google search.
  EXPECT_EQ(2, preresolve_counts_[search_url]);
}

IN_PROC_BROWSER_TEST_P(
    SearchEnginePreconnectorWithPreconnect2FeatureBrowserTest,
    PreconnectSearchOnMojoPipeDisconnect) {
  constexpr char16_t kShortName[] = u"test";
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  TemplateURLData data_allowed_search;
  data_allowed_search.SetShortName(kShortName);
  data_allowed_search.SetKeyword(data_allowed_search.short_name());
  data_allowed_search.SetURL(kGoogleSearch);
  data_allowed_search.preconnect_to_search_url = true;

  auto* template_url =
      model->Add(std::make_unique<TemplateURL>(data_allowed_search));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);

  GetSearchEnginePreconnector()->StartPreconnecting(
      /*with_startup_delay=*/false);

  const GURL search_url = template_url->GenerateSearchURL({});
  WaitForPreresolveCountForURL(search_url, 1);

  // Manually unbind to trigger a new preconnect
  ASSERT_TRUE(remote_.is_bound());
  remote_.Unbind().reset();

  WaitForPreresolveCountForURL(search_url, 2);

  // Preconnect should occur for Google search.
  EXPECT_EQ(2, preresolve_counts_[search_url]);
}
