// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_BROWSER_TEST_BASE_H_

#include <string>

#include <map>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_request.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"

class DevToolsWindow;

// A base class with basic search suggestion environment set.
class SearchPrefetchBaseBrowserTest : public InProcessBrowserTest {
 public:
  SearchPrefetchBaseBrowserTest();
  ~SearchPrefetchBaseBrowserTest() override;

  // InProcessBrowserTest implementation.
  void SetUpOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;
  void SetUpCommandLine(base::CommandLine* cmd) override;

  GURL GetSearchServerQueryURL(const std::string& path) const;
  GURL GetSearchServerQueryURLWithNoQuery(const std::string& path) const;
  GURL GetCanonicalSearchURL(const GURL& prefetch_url);

  std::tuple<GURL, GURL> GetSearchPrefetchAndNonPrefetch(
      const std::string& search_terms);

  // Get a URL for a page that embeds the search |path| as an iframe.
  GURL GetSearchServerQueryURLWithSubframeLoad(const std::string& path) const;

  GURL GetSuggestServerURL(const std::string& path) const;

  void WaitUntilStatusChangesTo(const GURL& canonical_search_url,
                                absl::optional<SearchPrefetchStatus> status);

  content::WebContents* GetWebContents() const;

  std::string GetDocumentInnerHTML() const;

  void WaitForDuration(base::TimeDelta duration);

  void ClearBrowsingCacheData(absl::optional<GURL> url_origin);

  void SetDSEWithURL(const GURL& url, bool dse_allows_prefetch);

  // This is sufficient to cause observer calls about updated template URL, but
  // doesn't change DSE at all.
  void UpdateButChangeNothingInDSE();

  void OpenDevToolsWindow(content::WebContents* tab);
  void CloseDevToolsWindow();

  // Allows the search server to serve |content| with |content_type| when
  // |relative_url| is requested.
  void RegisterStaticFile(const std::string& relative_url,
                          const std::string& content,
                          const std::string& content_type);

  // Allows tests to add a new suggestion rule for the given `origin_query`.
  // See `SearchSuggestionTuple` for details.
  void AddNewSuggestionRule(std::string origin_query,
                            std::vector<std::string> suggestions,
                            int prefetch_index,
                            int prerender_index);

  size_t search_server_request_count() const {
    return search_server_request_count_;
  }

  size_t search_server_prefetch_request_count() const {
    return search_server_prefetch_request_count_;
  }

  const std::vector<net::test_server::HttpRequest>& search_server_requests()
      const {
    return search_server_requests_;
  }

  void set_should_hang_requests(bool should_hang_requests) {
    should_hang_requests_ = should_hang_requests;
  }

  void set_hang_requests_after_start(bool hang_requests_after_start) {
    hang_requests_after_start_ = hang_requests_after_start;
  }

  void set_delayed_response(bool delayed_response) {
    delayed_response_ = delayed_response;
  }

  // Create a search suggestion match with a prefetch signal when
  // |prefetch_hint| is true.
  AutocompleteMatch CreateSearchSuggestionMatch(
      const std::string& original_query,
      const std::string& search_terms,
      bool prefetch_hint);

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleSearchRequest(
      const net::test_server::HttpRequest& request);

  void MonitorSearchResourceRequestOnUIThread(
      net::test_server::HttpRequest request,
      bool has_prefetch_header);

  std::unique_ptr<net::test_server::HttpResponse> HandleSearchSuggestRequest(
      const net::test_server::HttpRequest& request);

  struct SearchSuggestionTuple {
    SearchSuggestionTuple(std::string origin_query,
                          std::vector<std::string> suggestions,
                          int prefetch_index,
                          int prerender_index);
    ~SearchSuggestionTuple();

    SearchSuggestionTuple(const SearchSuggestionTuple& other);

    // The string that users typed.
    std::string origin_query;

    // A list of search suggestions associated with `origin_query`.
    std::vector<std::string> suggestions;

    //  The index of prefetch hint in `suggestions`. Set to -1 if none of them
    //  should be prefetched.
    int prefetch_hint_index = -1;

    //  The index of prefetch hint in `suggestions`. Set to -1 if none of them
    //  should be prerendered.
    int prerender_hint_index = -1;
  };

  // Stores some hard-coded rules for testing.
  // Tests can also call `AddNewSuggestionRule` to append a new rule.
  // Note: they are order-sensitive! The last rule (the newest added rule) has
  // the highest priority.
  std::vector<SearchSuggestionTuple> search_suggestion_rules_{
      SearchSuggestionTuple("porgs",
                            {"porgs", "porgsandwich"},
                            /*prefetch_index=*/0,
                            /*prerender_index=*/-1),
      SearchSuggestionTuple("puffins",
                            {"puffins", "puffinsalad"},
                            /*prefetch_index=*/-1,
                            /*prerender_index=*/-1),
      SearchSuggestionTuple("502_on_prefetch",
                            {"502_on_prefetch"},
                            /*prefetch_index=*/0,
                            /*prerender_index=*/-1)};

  content::ContentMockCertVerifier mock_cert_verifier_;

  std::vector<net::test_server::HttpRequest> search_server_requests_;
  std::unique_ptr<net::EmbeddedTestServer> search_server_;

  std::unique_ptr<net::EmbeddedTestServer> search_suggest_server_;

  bool should_hang_requests_ = false;

  bool delayed_response_ = false;

  size_t search_server_request_count_ = 0;
  size_t search_server_prefetch_request_count_ = 0;

  // When set to true, serves a response that hangs after the start of the body.
  bool hang_requests_after_start_ = false;

  // Test cases can add path, content, content type tuples to be served.
  std::map<std::string /* path */,
           std::pair<std::string /* content */, std::string /* content_type */>>
      static_files_;

  raw_ptr<DevToolsWindow> window_ = nullptr;
};

#endif  // CHROME_BROWSER_PRELOADING_PREFETCH_SEARCH_PREFETCH_SEARCH_PREFETCH_BROWSER_TEST_BASE_H_
