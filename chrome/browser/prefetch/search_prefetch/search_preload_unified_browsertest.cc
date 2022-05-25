// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/policy/policy_constants.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

// Sets up testing context for the search preloading features: search prefetch
// and search prerender.
// These features are able to coordinate with the other: A prefetched result
// might be upgraded to prerender when needed (usually when service suggests
// clients to do so), and they share the prefetched response and other
// resources, so it is a unified test designed to test the interaction between
// these two features.
class SearchPreloadUnifiedBrowserTest : public PlatformBrowserTest {
 public:
  SearchPreloadUnifiedBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SearchPreloadUnifiedBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kSupportSearchSuggestionForPrerender2, {}},
         {kSearchPrefetchServicePrefetching,
          {{"max_attempts_per_caching_duration", "3"},
           {"cache_size", "1"},
           {"device_memory_threshold_MB", "0"}}}},
        /*disabled_features=*/{kSearchPrefetchBlockBeforeHeaders});
  }

  void SetUp() override {
    prerender_helper().SetUp(&search_engine_server_);
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up a generic server.
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Set up server for search engine.
    search_engine_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    search_engine_server_.RegisterRequestHandler(base::BindRepeating(
        &SearchPreloadUnifiedBrowserTest::HandleSearchRequest,
        base::Unretained(this)));
    ASSERT_TRUE(search_engine_server_.Start());

    TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());
    TemplateURLData data;
    data.SetShortName(kSearchDomain16);
    data.SetKeyword(data.short_name());
    data.SetURL(
        search_engine_server_
            .GetURL(kSearchDomain,
                    "/search_page.html?q={searchTerms}&{google:prefetchSource}"
                    "type=test")
            .spec());
    data.suggestions_url =
        search_engine_server_.GetURL(kSearchDomain, "/?q={searchTerms}").spec();
    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleSearchRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("favicon") != std::string::npos)
      return nullptr;

    std::unique_ptr<net::test_server::BasicHttpResponse> resp =
        std::make_unique<net::test_server::BasicHttpResponse>();
    resp->set_code(net::HTTP_OK);
    resp->set_content_type("text/html");
    std::string content = R"(
      <html><body>
      PRERENDER: HI PREFETCH! \o/
      </body></html>
    )";
    resp->set_content(content);
    return resp;
  }

  GURL GetSearchUrl(const std::string& search_terms,
                    bool attach_prefetch_flag) {
    // $1: the search terms that will be retrieved.
    // $2: parameter for prefetch request. Should be &pf=cs if the url is
    // expected to declare itself as a prefetch request. Otherwise it should be
    // an empty string.
    std::string url_template = "/search_page.html?q=$1$2&type=test";
    return search_engine_server_.GetURL(
        kSearchDomain,
        base::ReplaceStringPlaceholders(
            url_template, {search_terms, attach_prefetch_flag ? "&pf=cs" : ""},
            nullptr));
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(search_engine_server_.ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void SetUpContext() {
    // Have SearchPrefetchService and PrerenderManager prepared.
    PrerenderManager::CreateForWebContents(GetActiveWebContents());
    prerender_manager_ =
        PrerenderManager::FromWebContents(GetActiveWebContents());
    ASSERT_TRUE(prerender_manager_);
    search_prefetch_service_ = SearchPrefetchServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
    ASSERT_NE(nullptr, search_prefetch_service_);
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

  AutocompleteMatch CreateSearchSuggestionMatch(
      const std::string& original_query,
      const std::string& search_terms,
      bool is_prerender_hint,
      bool is_prefetch_hint) {
    AutocompleteMatch match;
    match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
        base::UTF8ToUTF16(search_terms));
    match.search_terms_args->original_query = base::UTF8ToUTF16(original_query);
    match.destination_url =
        GetSearchUrl(search_terms, /*attach_prefetch_flag=*/false);
    match.keyword = base::UTF8ToUTF16(original_query);
    if (is_prerender_hint)
      match.RecordAdditionalInfo("should_prerender", "true");
    if (is_prefetch_hint)
      match.RecordAdditionalInfo("should_prefetch", "true");
    return match;
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  PrerenderManager* prerender_manager() { return prerender_manager_; }

  SearchPrefetchService* search_prefetch_service() {
    return search_prefetch_service_;
  }

 private:
  constexpr static char kSearchDomain[] = "a.test";
  constexpr static char16_t kSearchDomain16[] = u"a.test";
  raw_ptr<PrerenderManager> prerender_manager_ = nullptr;
  raw_ptr<SearchPrefetchService> search_prefetch_service_ = nullptr;
  net::test_server::EmbeddedTestServer search_engine_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the SearchSuggestionService can trigger prerendering when it
// receives prerender hints.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest, PrerenderBeTriggered) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  std::string search_query = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, /*attach_prefetch_flag=*/true);

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  AutocompleteInput input(
      base::ASCIIToUTF16(search_query), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(chrome_test_utils::GetProfile(this)));
  AutocompleteMatch autocomplete_match = CreateSearchSuggestionMatch(
      search_query, prerender_query, /*is_prerender_hint=*/true,
      /*is_prefetch_hint=*/true);
  AutocompleteResult autocomplete_result;
  autocomplete_result.AppendMatches({autocomplete_match});
  search_prefetch_service()->OnResultChanged(GetActiveWebContents(),
                                             autocomplete_result);

  // The suggestion service should hint expected_prerender_url, and prerendering
  // for this url should start.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  // Prefetch should be triggered as well.
  auto prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          base::ASCIIToUTF16(prerender_query));
  EXPECT_TRUE(prefetch_status.has_value());
}

}  // namespace
