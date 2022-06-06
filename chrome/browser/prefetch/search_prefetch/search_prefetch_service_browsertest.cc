// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_browser_test_base.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
constexpr char kOmniboxSuggestPrefetchQuery[] = "porgs";
constexpr char kOmniboxSuggestPrefetchSecondItemQuery[] = "porgsandwich";
constexpr char16_t kOmniboxSuggestPrefetchSecondItemQuery16[] = u"porgsandwich";
constexpr char kOmniboxSuggestNonPrefetchQuery[] = "puffins";
constexpr char kOmniboxErrorQuery[] = "502_on_prefetch";
constexpr char16_t kOmniboxSuggestNonPrefetchQuery16[] = u"puffins";
constexpr char kLoadInSubframe[] = "/load_in_subframe";
constexpr char kClientHintsURL[] = "/accept_ch.html";
constexpr char kThrottleHeader[] = "porgs-header";
constexpr char kThrottleHeaderValue[] = "porgs-header-value";
constexpr char kServiceWorkerUrl[] = "/navigation_preload.js";

enum class BlockOnHeaders {
  kBlockOnHeaders = 0,
  kDirectBeforeHeaders = 1,
}

const kBlockOnHeadersCases[] = {BlockOnHeaders::kBlockOnHeaders,
                                BlockOnHeaders::kDirectBeforeHeaders};

enum class UseDiskCache {
  kUseDiskCache = 0,
  kUseBrowserMemoryCache = 1,
}

const kUseDiskCacheCases[] = {UseDiskCache::kUseDiskCache,
                              UseDiskCache::kUseBrowserMemoryCache};

}  // namespace

// A delegate to cancel prefetch requests by setting |defer| to true.
class DeferringThrottle : public blink::URLLoaderThrottle {
 public:
  DeferringThrottle() = default;
  ~DeferringThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    *defer = true;
  }
};

class ThrottleAllContentBrowserClient : public ChromeContentBrowserClient {
 public:
  ThrottleAllContentBrowserClient() = default;
  ~ThrottleAllContentBrowserClient() override = default;

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override {
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    throttles.push_back(std::make_unique<DeferringThrottle>());
    return throttles;
  }
};

// A delegate to cancel prefetch requests by calling cancel on |delegate_|.
class CancellingThrottle : public blink::URLLoaderThrottle {
 public:
  CancellingThrottle() = default;
  ~CancellingThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    delegate_->CancelWithError(net::ERR_ABORTED);
  }
};

class CancelAllContentBrowserClient : public ChromeContentBrowserClient {
 public:
  CancelAllContentBrowserClient() = default;
  ~CancelAllContentBrowserClient() override = default;

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override {
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    throttles.push_back(std::make_unique<CancellingThrottle>());
    return throttles;
  }
};

// A delegate to add a custom header to prefetches.
class AddHeaderModifyingThrottle : public blink::URLLoaderThrottle {
 public:
  AddHeaderModifyingThrottle() = default;
  ~AddHeaderModifyingThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    request->headers.SetHeader(kThrottleHeader, kThrottleHeaderValue);
  }
};

class AddHeaderContentBrowserClient : public ChromeContentBrowserClient {
 public:
  AddHeaderContentBrowserClient() = default;
  ~AddHeaderContentBrowserClient() override = default;

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override {
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    throttles.push_back(std::make_unique<AddHeaderModifyingThrottle>());
    return throttles;
  }
};

// A delegate to add a custom header to prefetches.
class AddQueryParamModifyingThrottle : public blink::URLLoaderThrottle {
 public:
  AddQueryParamModifyingThrottle() = default;
  ~AddQueryParamModifyingThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    request->url =
        net::AppendOrReplaceQueryParameter(request->url, "fakeparam", "0");
  }
};

class AddQueryParamContentBrowserClient : public ChromeContentBrowserClient {
 public:
  AddQueryParamContentBrowserClient() = default;
  ~AddQueryParamContentBrowserClient() override = default;

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override {
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    throttles.push_back(std::make_unique<AddQueryParamModifyingThrottle>());
    return throttles;
  }
};

// A delegate to add a custom header to prefetches.
class ChangeQueryModifyingThrottle : public blink::URLLoaderThrottle {
 public:
  ChangeQueryModifyingThrottle() = default;
  ~ChangeQueryModifyingThrottle() override = default;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override {
    request->url =
        net::AppendOrReplaceQueryParameter(request->url, "q", "modifiedsearch");
  }
};

class ChangeQueryContentBrowserClient : public ChromeContentBrowserClient {
 public:
  ChangeQueryContentBrowserClient() = default;
  ~ChangeQueryContentBrowserClient() override = default;

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override {
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    throttles.push_back(std::make_unique<ChangeQueryModifyingThrottle>());
    return throttles;
  }
};

class SearchPrefetchWithoutPrefetchingBrowserTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchWithoutPrefetchingBrowserTest() {
    feature_list_.InitWithFeatures({}, {kSearchPrefetchServicePrefetching});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchWithoutPrefetchingBrowserTest,
                       NoFetchWhenPrefetchDisabled) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));

  EXPECT_FALSE(prefetch_status.has_value());
}

// General test for standard behavior.  The interface bool represents whether
// the response can be served before headers.
class SearchPrefetchServiceEnabledBrowserTest
    : public SearchPrefetchBaseBrowserTest,
      public testing::WithParamInterface<
          std::tuple<BlockOnHeaders, UseDiskCache>> {
 public:
  SearchPrefetchServiceEnabledBrowserTest() {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features = {{kSearchPrefetchServicePrefetching,
                             {{"max_attempts_per_caching_duration", "3"},
                              {"cache_size", "1"},
                              {"device_memory_threshold_MB", "0"}}}};
    std::vector<base::Feature> disabled_features = {};
    if (BlockOnHeadersEnabled()) {
      enabled_features.push_back({kSearchPrefetchBlockBeforeHeaders, {}});
    } else {
      disabled_features.push_back({kSearchPrefetchBlockBeforeHeaders});
    }
    if (UseDiskCacheEnabled()) {
      enabled_features.push_back({kSearchPrefetchUsesNetworkCache, {}});
    } else {
      disabled_features.push_back({kSearchPrefetchUsesNetworkCache});
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  bool BlockOnHeadersEnabled() {
    return std::get<0>(GetParam()) == BlockOnHeaders::kBlockOnHeaders;
  }

  bool UseDiskCacheEnabled() {
    return std::get<1>(GetParam()) == UseDiskCache::kUseDiskCache;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       ServiceNotCreatedWhenIncognito) {
  EXPECT_EQ(nullptr, SearchPrefetchServiceFactory::GetForProfile(
                         browser()->profile()->GetPrimaryOTRProfile(
                             /*create_if_needed=*/true)));
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       BasicPrefetchFunctionality) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kPrefetchStarted, 1);
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.FetchResult.SuggestionPrefetch", true, 1);

  EXPECT_EQ(1u, search_server_requests().size());
  EXPECT_NE(std::string::npos,
            search_server_requests()[0].GetURL().spec().find(search_terms));
  auto headers = search_server_requests()[0].headers;
  ASSERT_TRUE(base::Contains(headers, "Accept"));
  EXPECT_TRUE(base::Contains(headers["Accept"], "text/html"));
  EXPECT_EQ(1u, search_server_request_count());
  EXPECT_EQ(1u, search_server_prefetch_request_count());
  // Make sure we don't get client hints headers by default.
  EXPECT_FALSE(base::Contains(headers, "viewport-width"));
  EXPECT_TRUE(base::Contains(headers, "User-Agent"));
  ASSERT_TRUE(base::Contains(headers, "Upgrade-Insecure-Requests"));
  EXPECT_TRUE(base::Contains(headers["Upgrade-Insecure-Requests"], "1"));

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       PrefetchThrottled) {
  base::HistogramTester histogram_tester;
  ThrottleAllContentBrowserClient browser_client;
  auto* old_client = content::SetBrowserClientForTesting(&browser_client);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kThrottled, 1);
  EXPECT_FALSE(prefetch_status.has_value());
  content::SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       PrefetchCancelledByThrottle) {
  CancelAllContentBrowserClient browser_client;
  base::HistogramTester histogram_tester;
  auto* old_client = content::SetBrowserClientForTesting(&browser_client);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kThrottled, 1);
  EXPECT_FALSE(prefetch_status.has_value());
  content::SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       PrefetchThrottleAddsHeader) {
  AddHeaderContentBrowserClient browser_client;
  auto* old_client = content::SetBrowserClientForTesting(&browser_client);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  auto headers = search_server_requests()[0].headers;
  EXPECT_EQ(1u, search_server_requests().size());
  ASSERT_TRUE(base::Contains(headers, kThrottleHeader));
  EXPECT_TRUE(base::Contains(headers[kThrottleHeader], kThrottleHeaderValue));

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());
  content::SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       QueryParamAddedInThrottle) {
  AddQueryParamContentBrowserClient browser_client;
  auto* old_client = content::SetBrowserClientForTesting(&browser_client);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());
  content::SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       ChangeQueryCancelsPrefetch) {
  ChangeQueryContentBrowserClient browser_client;
  base::HistogramTester histogram_tester;
  auto* old_client = content::SetBrowserClientForTesting(&browser_client);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kThrottled, 1);
  EXPECT_FALSE(prefetch_status.has_value());
  content::SetBrowserClientForTesting(old_client);
}

class HeaderObserverContentBrowserClient : public ChromeContentBrowserClient {
 public:
  HeaderObserverContentBrowserClient() = default;
  ~HeaderObserverContentBrowserClient() override = default;

  // ContentBrowserClient overrides:
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override;

  bool had_raw_request_info() { return had_raw_request_info_; }

  void set_had_raw_request_info(bool had_raw_request_info) {
    had_raw_request_info_ = had_raw_request_info;
  }

 private:
  bool had_raw_request_info_ = false;
};

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
HeaderObserverContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      ChromeContentBrowserClient::CreateURLLoaderThrottles(
          request, browser_context, wc_getter, navigation_ui_data,
          frame_tree_node_id);
  return throttles;
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       HeadersNotReportedFromNetwork) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  auto [prefetch_url, search_url] =
      GetSearchPrefetchAndNonPrefetch(search_terms);
  ;

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  HeaderObserverContentBrowserClient browser_client;
  auto* old_client = content::SetBrowserClientForTesting(&browser_client);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  EXPECT_FALSE(browser_client.had_raw_request_info());
  content::SetBrowserClientForTesting(old_client);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       PrefetchRateLimiting) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_1")));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kPrefetchStarted, 1);
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_2")));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kPrefetchStarted, 2);
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_3")));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kPrefetchStarted, 3);
  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_4")));
  histogram_tester.ExpectBucketCount(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kMaxAttemptsReached, 1);

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(u"prefetch_1");
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(u"prefetch_2");
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(u"prefetch_3");
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(u"prefetch_4");
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       BasicClientHintsFunctionality) {
  // Fetch a response that will set client hints on future requests.
  GURL client_hints = GetSearchServerQueryURLWithNoQuery(kClientHintsURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), client_hints));

  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  EXPECT_EQ(1u, search_server_requests().size());
  EXPECT_NE(std::string::npos,
            search_server_requests()[0].GetURL().spec().find(search_terms));
  auto headers = search_server_requests()[0].headers;

  // Make sure we can get client hints headers.
  EXPECT_TRUE(base::Contains(headers, "viewport-width"));
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       502PrefetchFunctionality) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "502_on_prefetch";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kRequestFailed);

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.FetchResult.SuggestionPrefetch", false, 1);

  EXPECT_EQ(1u, search_server_requests().size());
  EXPECT_NE(std::string::npos,
            search_server_requests()[0].GetURL().spec().find(search_terms));
  EXPECT_EQ(1u, search_server_request_count());
  EXPECT_EQ(1u, search_server_prefetch_request_count());

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kRequestFailed, prefetch_status.value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       FetchSameTermsOnlyOnce) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kPrefetchStarted, 1);

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  histogram_tester.ExpectBucketCount(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kAttemptedQueryRecently, 1);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest, BadURL) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_path = "/bad_path";

  GURL prefetch_url = GetSearchServerQueryURLWithNoQuery(search_path);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kNotDefaultSearchWithTerms, 1);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       PreloadDisabled) {
  base::HistogramTester histogram_tester;
  prefetch::SetPreloadPagesState(browser()->profile()->GetPrefs(),
                                 prefetch::PreloadPagesState::kNoPreloading);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kPrefetchDisabled, 1);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       BasicPrefetchServed) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  auto [prefetch_url, search_url] =
      GetSearchPrefetchAndNonPrefetch(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kPrefetchStarted, 1);
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kServed, 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.ClickToNavigationIntercepted", 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete", 1);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       BackPrefetchServed) {
  // This test prefetches and serves two SRP responses. It then navigates back
  // then forward, the back navigation should not be cached, due to cache limit
  // size of 1, the second navigation should be cached.

  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  // Disable back/forward cache to ensure that it doesn't get preserved in the
  // back/forward cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  std::string search_terms = "prefetch_content";
  auto [prefetch_url, search_url] =
      GetSearchPrefetchAndNonPrefetch(search_terms);
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  // The prefetch should be served, and only 1 request should be issued.
  EXPECT_EQ(1u, search_server_requests().size());
  auto inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.ClickToNavigationIntercepted", 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete", 1);

  search_terms = "prefetch_content_2";
  auto [prefetch_url_2, search_url_2] =
      GetSearchPrefetchAndNonPrefetch(search_terms);
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url_2));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url_2));

  // The prefetch should be served, and only 1 request (now the second total
  // request) should be issued.
  EXPECT_EQ(2u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.ClickToNavigationIntercepted", 2);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete", 2);

  content::TestNavigationObserver back_load_observer(GetWebContents());
  GetWebContents()->GetController().GoBack();
  back_load_observer.Wait();

  // There should not be a cached prefetch request, so there should be a network
  // request.
  EXPECT_EQ(3u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.ClickToNavigationIntercepted", 2);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete", 2);

  content::TestNavigationObserver forward_load_observer(GetWebContents());
  GetWebContents()->GetController().GoForward();
  forward_load_observer.Wait();

  // There should be a cached prefetch request, so there should not be a new
  // network request.
  EXPECT_EQ(3u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.ClickToNavigationIntercepted", 2);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete", 3);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       BackPrefetchServedRefParam) {
  // This test prefetches and serves two SRP responses. It then navigates back
  // then forward, the back navigation should not be cached, due to cache limit
  // size of 1, the second navigation should be cached.

  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  // Disable back/forward cache to ensure that it doesn't get preserved in the
  // back/forward cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  std::string search_terms = "prefetch_content";
  auto [prefetch_url, search_url] =
      GetSearchPrefetchAndNonPrefetch(search_terms);
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  // Add a ref param onto the page before navigating away.
  std::string script = R"(
    const url = new URL(document.URL);
    url.hash = "blah";
    history.replaceState(null, "", url.toString());
  )";

  content::RenderFrameHost* frame = GetWebContents()->GetMainFrame();
  EXPECT_TRUE(content::ExecuteScript(frame, script));

  // The prefetch should be served, and only 1 request should be issued.
  EXPECT_EQ(1u, search_server_requests().size());
  auto inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.ClickToNavigationIntercepted", 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete", 1);

  search_terms = "prefetch_content_2";
  auto [prefetch_url_2, search_url_2] =
      GetSearchPrefetchAndNonPrefetch(search_terms);
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url_2));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url_2));

  // The prefetch should be served, and only 1 request (now the second total
  // request) should be issued.
  EXPECT_EQ(2u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.ClickToNavigationIntercepted", 2);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete", 2);

  content::TestNavigationObserver back_load_observer(GetWebContents());
  GetWebContents()->GetController().GoBack();
  back_load_observer.Wait();

  // There should not be a cached prefetch request, so there should be a network
  // request.
  EXPECT_EQ(3u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.ClickToNavigationIntercepted", 2);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete", 2);

  content::TestNavigationObserver forward_load_observer(GetWebContents());
  GetWebContents()->GetController().GoForward();
  forward_load_observer.Wait();

  // There should be a cached prefetch request, so there should not be a new
  // network request.
  EXPECT_EQ(3u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.ClickToNavigationIntercepted", 2);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete", 3);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       BackPrefetchServedAfterPrefs) {
  // This test prefetches and serves two SRP responses. It then navigates back
  // then forward, the back navigation should not be cached, due to cache limit
  // size of 1, the second navigation should be cached.

  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  // Disable back/forward cache to ensure that it doesn't get preserved in the
  // back/forward cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  std::string search_terms = "prefetch_content";
  GURL prefetch_url = GetSearchServerQueryURL(search_terms + "&pf=cs");
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetSearchServerQueryURL(search_terms)));

  // The prefetch should be served, and only 1 request should be issued.
  EXPECT_EQ(1u, search_server_requests().size());
  auto inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));

  search_terms = "prefetch_content_2";
  prefetch_url = GetSearchServerQueryURL(search_terms + "&pf=cs");
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetSearchServerQueryURL(search_terms)));

  // The prefetch should be served, and only 1 request (now the second total
  // request) should be issued.
  EXPECT_EQ(2u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));

  content::TestNavigationObserver back_load_observer(GetWebContents());
  GetWebContents()->GetController().GoBack();
  back_load_observer.Wait();

  // There should not be a cached prefetch request, so there should be a network
  // request.
  EXPECT_EQ(3u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));

  // Reload the map from prefs.
  EXPECT_FALSE(search_prefetch_service->LoadFromPrefsForTesting());

  content::TestNavigationObserver forward_load_observer(GetWebContents());
  GetWebContents()->GetController().GoForward();
  forward_load_observer.Wait();

  // There should be a cached prefetch request, so there should not be a new
  // network request.
  EXPECT_EQ(3u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       BackPrefetchServedAfterPrefsNoOverflow) {
  // This test prefetches and serves two SRP responses. It then navigates back
  // then forward, the back navigation should not be cached, due to cache limit
  // size of 1, the second navigation should be cached.

  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";
  GURL prefetch_url = GetSearchServerQueryURL(search_terms + "&pf=cs");
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetSearchServerQueryURL(search_terms)));

  // The prefetch should be served, and only 1 request should be issued.
  EXPECT_EQ(1u, search_server_requests().size());
  auto inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));

  search_terms = "prefetch_content_2";
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetSearchServerQueryURL(search_terms)));

  // No prefetch request started, and only 1 request (now the second total
  // request) should be issued.
  EXPECT_EQ(2u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));

  // Reload the map from prefs.
  EXPECT_FALSE(search_prefetch_service->LoadFromPrefsForTesting());

  content::TestNavigationObserver back_load_observer(GetWebContents());
  GetWebContents()->GetController().GoBack();
  back_load_observer.Wait();

  // There should be a cached prefetch request, so there should not be a new
  // network request.
  EXPECT_EQ(2u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       EvictedCacheFallsback) {
  // This test prefetches and serves a SRP responses. It then navigates to a
  // different URL. Then it clears cache as if it was evicted. Then it navigates
  // back to the prefetched SRP. As a result, the back forward cache should
  // attempt to use the prefetch, but fall back to network on the original URL.

  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  // Disable back/forward cache to ensure that it doesn't get preserved in the
  // back/forward cache.
  content::DisableBackForwardCacheForTesting(
      GetWebContents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  std::string search_terms = "prefetch_content";
  GURL prefetch_url = GetSearchServerQueryURL(search_terms + "&pf=cs");
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetSearchServerQueryURL(search_terms)));

  // The prefetch should be served, and only 1 request should be issued.
  ASSERT_EQ(1u, search_server_requests().size());
  EXPECT_TRUE(
      base::Contains(search_server_requests()[0].GetURL().spec(), "pf=cs"));
  auto inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GetSearchServerQueryURL("search")));

  // The prefetch should be served, and only 1 request (now the second total
  // request) should be issued.
  EXPECT_EQ(2u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));

  // Clearing cache should cause the back forward loader to fail over to the
  // regular URL.
  base::RunLoop run_loop;
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ClearHttpCache(base::Time(), base::Time(), nullptr,
                       run_loop.QuitClosure());
  run_loop.Run();

  content::TestNavigationObserver back_load_observer(GetWebContents());
  GetWebContents()->GetController().GoBack();
  back_load_observer.Wait();

  // There should not be a cached prefetch request, so there should be a network
  // request.
  ASSERT_EQ(3u, search_server_requests().size());
  EXPECT_FALSE(
      base::Contains(search_server_requests()[2].GetURL().spec(), "pf=cs"));
  inner_html = GetDocumentInnerHTML();
  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       RegularSearchQueryWhenNoPrefetch) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL search_url = GetSearchServerQueryURL(search_terms);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason",
      SearchPrefetchServingReason::kNoPrefetch, 1);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       NonMatchingPrefetchURL) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";
  std::string search_terms_other = "other";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetSearchServerQueryURL(search_terms_other)));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason",
      SearchPrefetchServingReason::kNoPrefetch, 1);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.ClickToNavigationIntercepted", 0);
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.NavigationInterceptedToForwardingComplete", 0);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       ErrorCausesNoFetch) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "502_on_prefetch";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kRequestFailed);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kRequestFailed, prefetch_status.value());

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("other_query")));
  histogram_tester.ExpectBucketCount(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kErrorBackoff, 1);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       OmniboxEditTriggersPrefetch) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetSearchServerQueryURL(search_terms)));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       OmniboxURLHasPfParam) {
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  ASSERT_TRUE(search_server_requests().size() > 0);
  EXPECT_NE(std::string::npos,
            search_server_requests()[0].GetURL().spec().find("pf=cs"));
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       OmniboxEditDoesNotTriggersPrefetch) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestNonPrefetchQuery;

  // Trigger an omnibox suggest fetch that does not have a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitForDuration(base::Milliseconds(100));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetSearchServerQueryURL(search_terms)));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       OmniboxEditTriggersPrefetchForSecondMatch) {
  // phi being set to one causes the order of prefetch suggest to be different.
  // This should still prefetch a result for the |kOmniboxSuggestPrefetchQuery|.
  AddNewSuggestionRule(
      kOmniboxSuggestPrefetchQuery,
      {kOmniboxSuggestPrefetchQuery, kOmniboxSuggestPrefetchSecondItemQuery},
      /*prefetch_index=*/1, /*prerender_index=*/-1);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(kOmniboxSuggestPrefetchSecondItemQuery16,
                           SearchPrefetchStatus::kComplete);
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          kOmniboxSuggestPrefetchSecondItemQuery16);
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GetSearchServerQueryURL(kOmniboxSuggestPrefetchSecondItemQuery)));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       RemovingMatchCancelsInFlight) {
  set_should_hang_requests(true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           BlockOnHeadersEnabled()
                               ? SearchPrefetchStatus::kCanBeServed
                               : SearchPrefetchStatus::kInFlight);
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ((BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                     : SearchPrefetchStatus::kInFlight),
            prefetch_status.value());

  // Change the autocomplete to remove "porgs" entirely.
  AutocompleteInput other_input(
      kOmniboxSuggestNonPrefetchQuery16, metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  autocomplete_controller->Start(other_input);
  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kRequestCancelled);
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kRequestCancelled, prefetch_status.value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       OmniboxNavigateToMatchingEntryStreaming) {
  set_hang_requests_after_start(true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kCanBeServed);

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kCanBeServed, prefetch_status.value());

  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms), absl::nullopt);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       HungRequestCanBeServed) {
  set_should_hang_requests(true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  if (BlockOnHeadersEnabled()) {
    EXPECT_EQ(SearchPrefetchStatus::kCanBeServed, prefetch_status.value());
  } else {
    EXPECT_EQ(SearchPrefetchStatus::kInFlight, prefetch_status.value());
  }

  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  if (BlockOnHeadersEnabled()) {
    WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms), absl::nullopt);

    prefetch_status =
        search_prefetch_service->GetSearchPrefetchStatusForTesting(
            base::ASCIIToUTF16(search_terms));
    ASSERT_FALSE(prefetch_status.has_value());
  } else {
    WaitForDuration(base::Milliseconds(100));

    prefetch_status =
        search_prefetch_service->GetSearchPrefetchStatusForTesting(
            base::ASCIIToUTF16(search_terms));
    ASSERT_TRUE(prefetch_status.has_value());
    EXPECT_EQ(SearchPrefetchStatus::kRequestCancelled, prefetch_status.value());
  }
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       PrefetchServedBeforeHeaders) {
  if (!BlockOnHeadersEnabled()) {
    return;
  }
  set_delayed_response(true);
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  content::WaitForLoadStop(GetWebContents());

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       PrefetchFallbackFromError) {
  if (!BlockOnHeadersEnabled()) {
    return;
  }

  base::HistogramTester histogram_tester;
  set_delayed_response(true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxErrorQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  content::WaitForLoadStop(GetWebContents());

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("other_query")));
  histogram_tester.ExpectBucketCount(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kErrorBackoff, 1);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       PrefetchSecureSecurityState) {
  if (!BlockOnHeadersEnabled()) {
    return;
  }
  set_delayed_response(true);
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  content::WaitForLoadStop(GetWebContents());

  auto inner_html = GetDocumentInnerHTML();

  // Check we are on the prefetched page, and the security level is correct.
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
  EXPECT_EQ(helper->GetSecurityLevel(), security_state::SECURE);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       PrefetchFallbackSecureSecurityState) {
  if (!BlockOnHeadersEnabled()) {
    return;
  }

  set_delayed_response(true);
  std::string search_terms = kOmniboxErrorQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(GetWebContents());
  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  content::WaitForLoadStop(GetWebContents());

  auto inner_html = GetDocumentInnerHTML();

  // Check we fell back to the regular page, and the security level is correct.
  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
  EXPECT_EQ(helper->GetSecurityLevel(), security_state::SECURE);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       OmniboxNavigateToNonMatchingEntryStreamingCancels) {
  set_hang_requests_after_start(true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kCanBeServed);
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kCanBeServed, prefetch_status.value());
  omnibox->model()->OnUpOrDownKeyPressed(1);
  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kRequestCancelled);
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kRequestCancelled, prefetch_status.value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       ClearCacheRemovesPrefetch) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  ClearBrowsingCacheData(absl::nullopt);
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       ClearCacheSearchRemovesPrefetch) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  ClearBrowsingCacheData(GURL(GetSearchServerQueryURLWithNoQuery("/")));
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       ClearCacheOtherSavesCache) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  ClearBrowsingCacheData(GetSuggestServerURL("/"));
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       ChangeDSESameOriginClearsPrefetches) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  SetDSEWithURL(GetSearchServerQueryURL("blah/q={searchTerms}&extra_stuff"),
                false);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       ChangeDSECrossOriginClearsPrefetches) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  SetDSEWithURL(GetSuggestServerURL("/q={searchTerms}"), false);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       ChangeDSESameDoesntClearPrefetches) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  UpdateButChangeNothingInDSE();

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       NoPrefetchWhenJSDisabled) {
  base::HistogramTester histogram_tester;
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kWebKitJavascriptEnabled,
                                               false);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kJavascriptDisabled, 1);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       NoPrefetchWhenJSDisabledOnDSE) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(prefetch_url, GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kJavascriptDisabled, 1);

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       NoServeWhenJSDisabled) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  auto [prefetch_url, search_url] =
      GetSearchPrefetchAndNonPrefetch(search_terms);
  ;

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kWebKitJavascriptEnabled,
                                               false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason",
      SearchPrefetchServingReason::kJavascriptDisabled, 1);
  // The prefetch request and the new non-prefetched served request.
  EXPECT_EQ(2u, search_server_request_count());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       NoServeWhenJSDisabledOnDSE) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  auto [prefetch_url, search_url] =
      GetSearchPrefetchAndNonPrefetch(search_terms);
  ;

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(prefetch_url, GURL(),
                                      ContentSettingsType::JAVASCRIPT,
                                      CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason",
      SearchPrefetchServingReason::kJavascriptDisabled, 1);
  // The prefetch request and the new non-prefetched served request.
  EXPECT_EQ(2u, search_server_request_count());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       NoServeLinkClick) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  auto [prefetch_url, search_url] =
      GetSearchPrefetchAndNonPrefetch(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());
  EXPECT_EQ(1u, search_server_request_count());

  // Link click.
  NavigateParams params(browser(), search_url, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  EXPECT_EQ(2u, search_server_request_count());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason",
      SearchPrefetchServingReason::kPostReloadOrLink, 1);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest, NoServeReload) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  auto [prefetch_url, search_url] =
      GetSearchPrefetchAndNonPrefetch(search_terms);
  ;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());
  EXPECT_EQ(2u, search_server_request_count());

  // Reload.
  content::TestNavigationObserver load_observer(GetWebContents());
  GetWebContents()->GetController().Reload(content::ReloadType::NORMAL, false);
  load_observer.Wait();

  EXPECT_EQ(3u, search_server_request_count());
  histogram_tester.ExpectBucketCount(
      "Omnibox.SearchPrefetch.PrefetchServingReason",
      SearchPrefetchServingReason::kPostReloadOrLink, 1);
}
IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest, NoServePost) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  auto [prefetch_url, search_url] =
      GetSearchPrefetchAndNonPrefetch(search_terms);
  ;

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());
  EXPECT_EQ(1u, search_server_request_count());

  // Post request.
  ui_test_utils::NavigateToURLWithPost(browser(), search_url);

  EXPECT_EQ(2u, search_server_request_count());
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason",
      SearchPrefetchServingReason::kPostReloadOrLink, 1);
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       OnlyStreamedResponseCanServePartialRequest) {
  set_hang_requests_after_start(true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kCanBeServed);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());

  EXPECT_EQ(SearchPrefetchStatus::kCanBeServed, prefetch_status.value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       DontInterceptSubframes) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";
  GURL prefetch_url = GetSearchServerQueryURL(search_terms);
  GURL navigation_url = GetSearchServerQueryURLWithSubframeLoad(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(BlockOnHeadersEnabled() ? SearchPrefetchStatus::kCanBeServed
                                    : SearchPrefetchStatus::kInFlight,
            prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), navigation_url));

  const auto& requests = search_server_requests();
  EXPECT_EQ(3u, requests.size());
  // This flow should have resulted in a prefetch of the search terms, a main
  // frame navigation to the special subframe loader page, and a navigation to
  // the subframe that matches the prefetch URL.

  // 2 requests should be to the search terms directly, one for the prefetch and
  // one for the subframe (that can't be served from the prefetch cache).
  EXPECT_EQ(2,
            std::count_if(requests.begin(), requests.end(),
                          [search_terms](const auto& request) {
                            return request.relative_url.find(kLoadInSubframe) ==
                                       std::string::npos &&
                                   request.relative_url.find(search_terms) !=
                                       std::string::npos;
                          }));
  // 1 request should specify to load content in a subframe but also contain the
  // search terms.
  EXPECT_EQ(1,
            std::count_if(requests.begin(), requests.end(),
                          [search_terms](const auto& request) {
                            return request.relative_url.find(kLoadInSubframe) !=
                                       std::string::npos &&
                                   request.relative_url.find(search_terms) !=
                                       std::string::npos;
                          }));
}

void RunFirstParam(base::RepeatingClosure closure,
                   blink::ServiceWorkerStatusCode status) {
  ASSERT_EQ(status, blink::ServiceWorkerStatusCode::kOk);
  closure.Run();
}

// crbug.com/1272805
#if BUILDFLAG(IS_MAC)
#define MAYBE_ServiceWorkerServedPrefetchWithPreload \
  DISABLED_ServiceWorkerServedPrefetchWithPreload
#else
#define MAYBE_ServiceWorkerServedPrefetchWithPreload \
  ServiceWorkerServedPrefetchWithPreload
#endif
IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       MAYBE_ServiceWorkerServedPrefetchWithPreload) {
  const GURL worker_url = GetSearchServerQueryURLWithNoQuery(kServiceWorkerUrl);
  const std::string kEnableNavigationPreloadScript = R"(
      self.addEventListener('activate', event => {
          event.waitUntil(self.registration.navigationPreload.enable());
        });
      self.addEventListener('fetch', event => {
          if (event.preloadResponse !== undefined) {
            event.respondWith(async function() {
              const response = await event.preloadResponse;
              if (response) return response;
              return fetch(event.request);
          });
          }
        });)";
  std::string search_terms = "prefetch_content";

  auto [prefetch_url, search_url] =
      GetSearchPrefetchAndNonPrefetch(search_terms);
  ;

  RegisterStaticFile(kServiceWorkerUrl, kEnableNavigationPreloadScript,
                     "text/javascript");

  auto* service_worker_context = browser()
                                     ->profile()
                                     ->GetDefaultStoragePartition()
                                     ->GetServiceWorkerContext();

  base::RunLoop run_loop;
  blink::mojom::ServiceWorkerRegistrationOptions options(
      GetSearchServerQueryURLWithNoQuery("/"),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  blink::StorageKey key(url::Origin::Create(options.scope));
  service_worker_context->RegisterServiceWorker(
      worker_url, key, options,
      base::BindOnce(&RunFirstParam, run_loop.QuitClosure()));
  run_loop.Run();

  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  auto inner_html = GetDocumentInnerHTML();

  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceEnabledBrowserTest,
                       RequestTimingIsNonNegative) {
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  auto [prefetch_url, search_url] =
      GetSearchPrefetchAndNonPrefetch(search_terms);
  ;

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  content::RenderFrameHost* frame = GetWebContents()->GetPrimaryMainFrame();

  // Check the request total time is non-negative.
  int value = -1;
  std::string script =
      "window.domAutomationController.send(window.performance.timing."
      "responseEnd - window.performance.timing.requestStart)";
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(frame, script, &value));
  EXPECT_LE(0, value);

  // Check the response time is non-negative.
  value = -1;
  script =
      "window.domAutomationController.send(window.performance.timing."
      "responseEnd - window.performance.timing.responseStart)";
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(frame, script, &value));
  EXPECT_LE(0, value);

  // Check request start is after (or the same as) navigation start.
  value = -1;
  script =
      "window.domAutomationController.send(window.performance.timing."
      "requestStart - window.performance.timing.navigationStart)";
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(frame, script, &value));
  EXPECT_LE(0, value);

  // Check response end is after (or the same as) navigation start.
  value = -1;
  script =
      "window.domAutomationController.send(window.performance.timing."
      "responseEnd - window.performance.timing.navigationStart)";
  EXPECT_TRUE(content::ExecuteScriptAndExtractInt(frame, script, &value));
  EXPECT_LE(0, value);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SearchPrefetchServiceEnabledBrowserTest,
    testing::Combine(testing::ValuesIn(kBlockOnHeadersCases),
                     testing::ValuesIn(kUseDiskCacheCases)));

class SearchPrefetchServiceHeadStartTooLongTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceHeadStartTooLongTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kSearchPrefetchServicePrefetching,
          {{"max_attempts_per_caching_duration", "3"},
           {"cache_size", "1"},
           {"device_memory_threshold_MB", "0"}}},
         {kSearchPrefetchBlockBeforeHeaders,
          {{"block_head_start_ms", "100000"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceHeadStartTooLongTest,
                       HungRequestNotServedBeforeHeadStart) {
  set_should_hang_requests(true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kInFlight, prefetch_status.value());

  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kRequestCancelled);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kRequestCancelled, prefetch_status.value());
}

class SearchPrefetchServiceHeadStartTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceHeadStartTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kSearchPrefetchServicePrefetching,
          {{"max_attempts_per_caching_duration", "3"},
           {"cache_size", "1"},
           {"device_memory_threshold_MB", "0"}}},
         {kSearchPrefetchBlockBeforeHeaders, {{"block_head_start_ms", "10"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceHeadStartTest,
                       HungRequestServedAfterHeadStart) {
  set_should_hang_requests(true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that has a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kInFlight, prefetch_status.value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kCanBeServed);

  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms), absl::nullopt);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_FALSE(prefetch_status.has_value());
}

class SearchPrefetchServiceBFCacheTest : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceBFCacheTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kSearchPrefetchServicePrefetching, {{"cache_size", "1"}}},
         {{features::kBackForwardCache},
          {{"enable_same_site", "true"},
           {"ignore_outstanding_network_request_for_testing", "true"}}}},
        // Allow BackForwardCache for all devices regardless of their memory.
        {features::kBackForwardCacheMemoryControls});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceBFCacheTest,
                       BackForwardPrefetchServedFromBFCache) {
  // This test prefetches and serves two SRP responses. It then navigates back
  // then forward, the back navigation should not be cached, due to cache limit
  // size of 1, the second navigation should be cached.

  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";
  GURL prefetch_url = GetSearchServerQueryURL(search_terms + "&pf=cs");
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetSearchServerQueryURL(search_terms)));

  // The prefetch should be served, and only 1 request should be issued.
  EXPECT_EQ(1u, search_server_requests().size());
  auto inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));

  search_terms = "prefetch_content_2";
  prefetch_url = GetSearchServerQueryURL(search_terms + "&pf=cs");
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetSearchServerQueryURL(search_terms)));

  // The page should be restored from BF cache.
  EXPECT_EQ(2u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectTotalCount("BackForwardCache.HistoryNavigationOutcome",
                                    0);

  content::TestNavigationObserver back_load_observer(GetWebContents());
  GetWebContents()->GetController().GoBack();
  back_load_observer.Wait();

  // The page should be restored from BF cache.
  EXPECT_EQ(2u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectUniqueSample(
      "BackForwardCache.HistoryNavigationOutcome", 0 /* Restored */, 1);

  content::TestNavigationObserver forward_load_observer(GetWebContents());
  GetWebContents()->GetController().GoForward();
  forward_load_observer.Wait();

  // The page should be restored from BF cache.
  EXPECT_EQ(2u, search_server_requests().size());
  inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
  histogram_tester.ExpectUniqueSample(
      "BackForwardCache.HistoryNavigationOutcome", 0 /* Restored */, 2);
}

class SearchPrefetchServiceZeroCacheTimeBrowserTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceZeroCacheTimeBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kSearchPrefetchServicePrefetching,
          {{"prefetch_caching_limit_ms", "10"},
           {"max_attempts_per_caching_duration", "3"},
           {"device_memory_threshold_MB", "0"}}}},
        {});

    // Hang responses so the status will stay as InFlight until the entry is
    // removed.
    set_should_hang_requests(true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceZeroCacheTimeBrowserTest,
                       ExpireAfterDuration) {
  set_should_hang_requests(true);
  base::HistogramTester histogram_tester;
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  // Make sure a new fetch doesn't happen before expiry.
  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_TRUE(prefetch_status.has_value());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms), absl::nullopt);
  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kInFlight, 1);

  // Prefetch should be gone now.
  EXPECT_FALSE(prefetch_status.has_value());
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceZeroCacheTimeBrowserTest,
                       PrefetchRateLimitingClearsAfterRemoval) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_1")));
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_2")));
  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_3")));
  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_4")));

  WaitUntilStatusChangesTo(u"prefetch_1", absl::nullopt);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("prefetch_4")));
}

class SearchPrefetchServiceZeroErrorTimeBrowserTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceZeroErrorTimeBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kSearchPrefetchServicePrefetching,
          {{"error_backoff_duration_ms", "10"},
           {"device_memory_threshold_MB", "0"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceZeroErrorTimeBrowserTest,
                       ErrorClearedAfterDuration) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "502_on_prefetch";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kRequestFailed);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kRequestFailed, prefetch_status.value());

  WaitForDuration(base::Milliseconds(30));

  EXPECT_TRUE(search_prefetch_service->MaybePrefetchURL(
      GetSearchServerQueryURL("other_query")));
}

class SearchPrefetchServiceLowMemoryDeviceBrowserTest
    : public SearchPrefetchBaseBrowserTest {
 public:
  SearchPrefetchServiceLowMemoryDeviceBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kSearchPrefetchServicePrefetching,
          {{"device_memory_threshold_MB", "2000000000"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceLowMemoryDeviceBrowserTest,
                       NoFetchWhenLowMemoryDevice) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  EXPECT_NE(nullptr, search_prefetch_service);

  std::string search_terms = "prefetch_content";

  GURL prefetch_url = GetSearchServerQueryURL(search_terms);

  EXPECT_FALSE(search_prefetch_service->MaybePrefetchURL(prefetch_url));
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));

  EXPECT_FALSE(prefetch_status.has_value());
}

class GooglePFTest : public InProcessBrowserTest {
 public:
  GooglePFTest() = default;

  void SetUpOnMainThread() override {
    TemplateURLService* model =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());
  }
};

IN_PROC_BROWSER_TEST_F(GooglePFTest, BaseGoogleSearchHasPFForPrefetch) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  auto* default_search = template_url_service->GetDefaultSearchProvider();

  TemplateURLRef::SearchTermsArgs search_terms_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());
  search_terms_args.is_prefetch = true;

  std::string generated_url = default_search->url_ref().ReplaceSearchTerms(
      search_terms_args, template_url_service->search_terms_data(), nullptr);
  EXPECT_TRUE(base::Contains(generated_url, "pf=cs"));
}

IN_PROC_BROWSER_TEST_F(GooglePFTest, BaseGoogleSearchNoPFForNonPrefetch) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  auto* default_search = template_url_service->GetDefaultSearchProvider();

  TemplateURLRef::SearchTermsArgs search_terms_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());
  search_terms_args.is_prefetch = false;

  std::string generated_url = default_search->url_ref().ReplaceSearchTerms(
      search_terms_args, template_url_service->search_terms_data(), nullptr);
  EXPECT_FALSE(base::Contains(generated_url, "pf=cs"));
}

class SearchPrefetchServiceNavigationPrefetchBrowserTest
    : public SearchPrefetchBaseBrowserTest,
      public testing::WithParamInterface<UseDiskCache> {
 public:
  SearchPrefetchServiceNavigationPrefetchBrowserTest() {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features = {{kSearchPrefetchServicePrefetching,
                             {{"max_attempts_per_caching_duration", "3"},
                              {"cache_size", "1"},
                              {"device_memory_threshold_MB", "0"}}},
                            {kSearchNavigationPrefetch, {}}};
    std::vector<base::Feature> disabled_features = {};
    if (UseDiskCacheEnabled()) {
      enabled_features.push_back({kSearchPrefetchUsesNetworkCache, {}});
    } else {
      disabled_features.push_back({kSearchPrefetchUsesNetworkCache});
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  bool UseDiskCacheEnabled() {
    return (GetParam()) == UseDiskCache::kUseDiskCache;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceNavigationPrefetchBrowserTest,
                       DISABLED_NavigationPrefetchIsServed) {
  SetDSEWithURL(
      GetSearchServerQueryURL("{searchTerms}&{google:prefetchSource}"), true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestNonPrefetchQuery;

  // Trigger an omnibox suggest fetch that does not have a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());

  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kCanBeServedAndUserClicked,
            prefetch_status.value());

  content::WaitForLoadStop(GetWebContents());

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());

  auto inner_html = GetDocumentInnerHTML();
  EXPECT_FALSE(base::Contains(inner_html, "regular"));
  EXPECT_TRUE(base::Contains(inner_html, "prefetch"));
}

// TODO(https://crbug.com/1318154): Flaky on Mac bots.
// TODO(https://crbug.com/1317890): Flaky on other bots as well.
IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceNavigationPrefetchBrowserTest,
                       DISABLED_NavigationPrefetchReplacesError) {
  SetDSEWithURL(
      GetSearchServerQueryURL("{searchTerms}&{google:prefetchSource}"), true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxErrorQuery;

  // Trigger an omnibox suggest fetch that does not have a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kRequestFailed);
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kRequestFailed, prefetch_status.value());

  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kCanBeServedAndUserClicked,
            prefetch_status.value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceNavigationPrefetchBrowserTest,
                       NavigationPrefecthDoesntReplaceComplete) {
  SetDSEWithURL(
      GetSearchServerQueryURL("{searchTerms}&{google:prefetchSource}"), true);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestPrefetchQuery;

  // Trigger an omnibox suggest fetch that does not have a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  WaitUntilStatusChangesTo(base::ASCIIToUTF16(search_terms),
                           SearchPrefetchStatus::kComplete);
  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());

  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  ASSERT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(SearchPrefetchStatus::kComplete, prefetch_status.value());
}

IN_PROC_BROWSER_TEST_P(SearchPrefetchServiceNavigationPrefetchBrowserTest,
                       DSEDoesNotAllowPrefetch) {
  SetDSEWithURL(
      GetSearchServerQueryURL("{searchTerms}&{google:prefetchSource}"), false);
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  std::string search_terms = kOmniboxSuggestNonPrefetchQuery;

  // Trigger an omnibox suggest fetch that does not have a prefetch hint.
  AutocompleteInput input(
      base::ASCIIToUTF16(search_terms), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));
  autocomplete_controller->Start(input);

  ui_test_utils::WaitForAutocompleteDone(browser());
  EXPECT_TRUE(autocomplete_controller->done());

  auto prefetch_status =
      search_prefetch_service->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());

  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());

  content::WaitForLoadStop(GetWebContents());

  prefetch_status = search_prefetch_service->GetSearchPrefetchStatusForTesting(
      base::ASCIIToUTF16(search_terms));
  EXPECT_FALSE(prefetch_status.has_value());

  auto inner_html = GetDocumentInnerHTML();
  EXPECT_TRUE(base::Contains(inner_html, "regular"));
  EXPECT_FALSE(base::Contains(inner_html, "prefetch"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SearchPrefetchServiceNavigationPrefetchBrowserTest,
                         testing::ValuesIn(kUseDiskCacheCases));
