// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_browser_test_base.h"

#include "base/containers/adapters.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_request.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kSuggestDomain[] = "suggest.com";
constexpr char16_t kSuggestDomain16[] = u"suggest.com";
constexpr char kSearchDomain[] = "search.com";
constexpr char16_t kSearchDomain16[] = u"search.com";
constexpr char kClientHintsURL[] = "/accept_ch.html";
constexpr char kLoadInSubframe[] = "/load_in_subframe";

}  // namespace

SearchPrefetchBaseBrowserTest::SearchPrefetchBaseBrowserTest() {
  search_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  search_server_->ServeFilesFromSourceDirectory("chrome/test/data");
  search_server_->ServeFilesFromSourceDirectory(
      "chrome/test/data/client_hints");
  search_server_->RegisterRequestHandler(
      base::BindRepeating(&SearchPrefetchBaseBrowserTest::HandleSearchRequest,
                          base::Unretained(this)));
  EXPECT_TRUE(search_server_->Start());

  search_suggest_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  search_suggest_server_->ServeFilesFromSourceDirectory("chrome/test/data");
  search_suggest_server_->RegisterRequestHandler(base::BindRepeating(
      &SearchPrefetchBaseBrowserTest::HandleSearchSuggestRequest,
      base::Unretained(this)));
  EXPECT_TRUE(search_suggest_server_->Start());
}

SearchPrefetchBaseBrowserTest::~SearchPrefetchBaseBrowserTest() = default;

void SearchPrefetchBaseBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule(kSearchDomain, "127.0.0.1");
  host_resolver()->AddRule(kSuggestDomain, "127.0.0.1");

  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(model);
  search_test_utils::WaitForTemplateURLServiceToLoad(model);
  ASSERT_TRUE(model->loaded());

  SetDSEWithURL(
      GetSearchServerQueryURL(
          "{searchTerms}&{google:assistedQueryStats}{google:prefetchSource}"),
      false);

  mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
}

void SearchPrefetchBaseBrowserTest::SetUpInProcessBrowserTestFixture() {
  mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void SearchPrefetchBaseBrowserTest::TearDownInProcessBrowserTestFixture() {
  mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
}

void SearchPrefetchBaseBrowserTest::SetUpCommandLine(base::CommandLine* cmd) {
  cmd->AppendSwitch("ignore-certificate-errors");

  mock_cert_verifier_.SetUpCommandLine(cmd);

  // TODO(crbug.com/40285326): This fails with the field trial testing config.
  cmd->AppendSwitch("disable-field-trial-config");
}

GURL SearchPrefetchBaseBrowserTest::GetSearchServerQueryURL(
    const std::string& path) const {
  return search_server_->GetURL(kSearchDomain, "/search_page.html?q=" + path);
}

GURL SearchPrefetchBaseBrowserTest::GetSearchServerQueryURLWithNoQuery(
    const std::string& path) const {
  return search_server_->GetURL(kSearchDomain, path);
}

GURL SearchPrefetchBaseBrowserTest::GetCanonicalSearchURL(
    const GURL& prefetch_url) {
  GURL canonical_search_url;
  EXPECT_TRUE(HasCanonicalPreloadingOmniboxSearchURL(
      prefetch_url, browser()->profile(), &canonical_search_url));
  return canonical_search_url;
}

GURL SearchPrefetchBaseBrowserTest::GetSearchServerQueryURLWithSubframeLoad(
    const std::string& path) const {
  return search_server_->GetURL(
      kSearchDomain,
      std::string(kLoadInSubframe).append("/search_page.html?q=").append(path));
}

GURL SearchPrefetchBaseBrowserTest::GetSuggestServerURL(
    const std::string& path) const {
  return search_suggest_server_->GetURL(kSuggestDomain, path);
}

std::tuple<GURL, GURL>
SearchPrefetchBaseBrowserTest::GetSearchPrefetchAndNonPrefetch(
    const std::string& search_terms) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());

  TemplateURLRef::SearchTermsArgs search_terms_args =
      TemplateURLRef::SearchTermsArgs(base::ASCIIToUTF16(search_terms));
  search_terms_args.prefetch_param = "";

  GURL search_url =
      GURL(template_url_service->GetDefaultSearchProvider()
               ->url_ref()
               .ReplaceSearchTerms(search_terms_args,
                                   template_url_service->search_terms_data(),
                                   nullptr));

  search_terms_args.prefetch_param = "cs";

  GURL prefetch_url =
      GURL(template_url_service->GetDefaultSearchProvider()
               ->url_ref()
               .ReplaceSearchTerms(search_terms_args,
                                   template_url_service->search_terms_data(),
                                   nullptr));

  return std::make_tuple(prefetch_url, search_url);
}

void SearchPrefetchBaseBrowserTest::WaitUntilStatusChangesTo(
    const GURL& canonical_search_url,
    std::optional<SearchPrefetchStatus> status) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  while (search_prefetch_service->GetSearchPrefetchStatusForTesting(
             canonical_search_url) != status) {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
}

GURL SearchPrefetchBaseBrowserTest::GetRealPrefetchUrlForTesting(
    const GURL& canonical_search_url) {
  auto* search_prefetch_service =
      SearchPrefetchServiceFactory::GetForProfile(browser()->profile());
  return search_prefetch_service->GetRealPrefetchUrlForTesting(
      canonical_search_url);
}

content::WebContents* SearchPrefetchBaseBrowserTest::GetWebContents() const {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

std::string SearchPrefetchBaseBrowserTest::GetDocumentInnerHTML() const {
  return content::EvalJs(GetWebContents(), "document.documentElement.innerHTML")
      .ExtractString();
}

void SearchPrefetchBaseBrowserTest::WaitForDuration(base::TimeDelta duration) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), duration);
  run_loop.Run();
}

void SearchPrefetchBaseBrowserTest::ClearBrowsingCacheData(
    std::optional<GURL> url_origin) {
  auto filter = content::BrowsingDataFilterBuilder::Create(
      url_origin ? content::BrowsingDataFilterBuilder::Mode::kDelete
                 : content::BrowsingDataFilterBuilder::Mode::kPreserve);
  if (url_origin)
    filter->AddOrigin(url::Origin::Create(url_origin.value()));
  content::BrowsingDataRemover* remover =
      browser()->profile()->GetBrowsingDataRemover();
  content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveWithFilterAndReply(
      base::Time(), base::Time::Max(),
      content::BrowsingDataRemover::DATA_TYPE_CACHE,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      std::move(filter), &completion_observer);
}

void SearchPrefetchBaseBrowserTest::SetDSEWithURL(const GURL& url,
                                                  bool dse_allows_prefetch) {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  TemplateURLData data;
  data.SetShortName(kSearchDomain16);
  data.SetKeyword(data.short_name());
  data.SetURL(url.spec());
  data.suggestions_url =
      search_suggest_server_->GetURL(kSuggestDomain, "/?q={searchTerms}")
          .spec();
  data.prefetch_likely_navigations = dse_allows_prefetch;
  data.side_search_param = "side_search";
  data.side_image_search_param = "side_search_image";

  TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  model->SetUserSelectedDefaultSearchProvider(template_url);
}

// This is sufficient to cause observer calls about updated template URL, but
// doesn't change DSE at all.
void SearchPrefetchBaseBrowserTest::UpdateButChangeNothingInDSE() {
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  TemplateURLData data;
  data.SetShortName(kSuggestDomain16);
  data.SetKeyword(data.short_name());
  data.SetURL(
      search_suggest_server_->GetURL(kSuggestDomain, "/?q={searchTerms}")
          .spec());
  data.suggestions_url =
      search_suggest_server_->GetURL(kSuggestDomain, "/?q={searchTerms}")
          .spec();

  model->Add(std::make_unique<TemplateURL>(data));
}

void SearchPrefetchBaseBrowserTest::OpenDevToolsWindow(
    content::WebContents* tab) {
  window_ = DevToolsWindowTesting::OpenDevToolsWindowSync(tab, true);
}

void SearchPrefetchBaseBrowserTest::CloseDevToolsWindow() {
  DevToolsWindowTesting::CloseDevToolsWindowSync(window_);
}

void SearchPrefetchBaseBrowserTest::RegisterStaticFile(
    const std::string& relative_url,
    const std::string& content,
    const std::string& content_type) {
  static_files_[relative_url] = std::make_pair(content, content_type);
}

void SearchPrefetchBaseBrowserTest::AddNewSuggestionRule(
    std::string origin_query,
    std::vector<std::string> suggestions,
    int prefetch_index,
    int prerender_index) {
  search_suggestion_rules_.emplace_back(origin_query, suggestions,
                                        prefetch_index, prerender_index);
}

std::unique_ptr<net::test_server::HttpResponse>
SearchPrefetchBaseBrowserTest::HandleSearchRequest(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().spec().find("favicon") != std::string::npos)
    return nullptr;

  if (request.relative_url == kClientHintsURL)
    return nullptr;

  bool is_prefetch =
      request.headers.find("Purpose") != request.headers.end() &&
      request.headers.find("Purpose")->second == "prefetch" &&
      request.headers.find("Sec-Purpose") != request.headers.end() &&
      request.headers.find("Sec-Purpose")->second == "prefetch";

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SearchPrefetchBaseBrowserTest::
                                    MonitorSearchResourceRequestOnUIThread,
                                base::Unretained(this), request, is_prefetch));

  if (base::Contains(static_files_, request.relative_url)) {
    return CreateDeferrableResponse(
        net::HTTP_OK,
        {{"cache-control", "private, max-age=0"},
         {"content-type", static_files_[request.relative_url].second}},
        static_files_[request.relative_url].first);
  }

  // If this is an embedded search for load in iframe, parse out the iframe
  // URL and serve it as an iframe in the returned HTML.
  if (request.relative_url.find(kLoadInSubframe) == 0) {
    std::string subframe_path =
        request.relative_url.substr(std::string(kLoadInSubframe).size());
    std::string content = "<html><body><iframe src=\"";
    content.append(subframe_path);
    content.append("\"/></body></html>");

    return CreateDeferrableResponse(
        is_prefetch ? net::HTTP_BAD_GATEWAY : net::HTTP_OK,
        {{"cache-control", "private, max-age=0"},
         {"content-type", "text/html"}},
        content);
  }

  if (request.GetURL().spec().find("502_on_prefetch") != std::string::npos &&
      is_prefetch) {
    return CreateDeferrableResponse(net::HTTP_BAD_GATEWAY,
                                    {{"content-type", "text/html"}},
                                    "<html><body>prefetch</body></html>");
  }
  std::string content = "<html><body> ";
  content.append(is_prefetch ? "prefetch" : "regular");
  content.append(" </body></html>");
  return CreateDeferrableResponse(
      net::HTTP_OK,
      {{"content-type", "text/html"}, {"cache-control", "private, max-age=0"}},
      content);
}

void SearchPrefetchBaseBrowserTest::MonitorSearchResourceRequestOnUIThread(
    net::test_server::HttpRequest request,
    bool has_prefetch_header) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  search_server_request_count_++;
  search_server_requests_.push_back(request);
  if (has_prefetch_header) {
    search_server_prefetch_request_count_++;
  }
}

std::unique_ptr<net::test_server::HttpResponse>
SearchPrefetchBaseBrowserTest::HandleSearchSuggestRequest(
    const net::test_server::HttpRequest& request) {
  // |content| is a json request that contains the search suggest response.
  // The first item is the query (not used), the second is the results list,
  // the third is descriptions, fifth is an extra data dictionary. The
  // google:clientdata contains "phi" which is the prefetch index (i.e., which
  // suggest can be prefetched).
  std::string content = R"([
      "empty",
      ["empty", "porgs"],
      ["", ""],
      [],
      {}])";

  // $1    : origin query
  // $2    : suggested results. It should be like: "suggestion_1",
  // "suggestion_2", ..., "suggestion_n".
  // $3    : a json that contains preload hints.
  std::string content_template = R"([
      "$1",
      [$2],
      ["", ""],
      [],
      {
      $3
      }])";

  // $1: the index of prefetch hint in suggested results.
  // $2: the index of prerender hint in suggested results.
  std::string preload_hint_template = R"(
        "google:clientdata": {
          "phi": $1,
          "pre": $2
        }
    )";

  for (const auto& suggestion_rule : base::Reversed(search_suggestion_rules_)) {
    // Origin query matches a predefined rule.
    if (request.GetURL().spec().find(suggestion_rule.origin_query) ==
        std::string::npos)
      continue;
    // Make up suggestion list to resect the protocol of a suggestion
    // response.
    std::vector<std::string> formatted_suggestions(
        suggestion_rule.suggestions.size());
    for (size_t i = 0; i < suggestion_rule.suggestions.size(); ++i) {
      formatted_suggestions[i] = "\"" + suggestion_rule.suggestions[i] + "\"";
    }
    std::string prefetch_hint_json = base::ReplaceStringPlaceholders(
        preload_hint_template,
        {base::NumberToString(suggestion_rule.prefetch_hint_index),
         base::NumberToString(suggestion_rule.prerender_hint_index)},
        nullptr);
    std::string suggestions_string =
        base::JoinString(formatted_suggestions, ",");
    content = base::ReplaceStringPlaceholders(
        content_template,
        {suggestion_rule.origin_query, suggestions_string, prefetch_hint_json},
        nullptr);
    break;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> resp =
      std::make_unique<net::test_server::BasicHttpResponse>();
  resp->set_code(net::HTTP_OK);
  resp->set_content_type("application/json");
  resp->set_content(content);
  return resp;
}

SearchPrefetchBaseBrowserTest::SearchSuggestionTuple::SearchSuggestionTuple(
    std::string origin_query,
    std::vector<std::string> suggestions,
    int prefetch_index,
    int prerender_index)
    : origin_query(origin_query),
      suggestions(suggestions),
      prefetch_hint_index(prefetch_index),
      prerender_hint_index(prerender_index) {}

SearchPrefetchBaseBrowserTest::SearchSuggestionTuple::~SearchSuggestionTuple() =
    default;

SearchPrefetchBaseBrowserTest::SearchSuggestionTuple::SearchSuggestionTuple(
    const SearchSuggestionTuple& other) = default;

AutocompleteMatch SearchPrefetchBaseBrowserTest::CreateSearchSuggestionMatch(
    const std::string& original_query,
    const std::string& search_terms,
    bool prefetch_hint) {
  AutocompleteMatch match;
  match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
      base::UTF8ToUTF16(search_terms));
  match.search_terms_args->original_query = base::UTF8ToUTF16(original_query);
  match.destination_url = GetSearchServerQueryURL(search_terms);
  match.keyword = base::UTF8ToUTF16(original_query);
  if (prefetch_hint)
    match.RecordAdditionalInfo("should_prefetch", "true");
  match.allowed_to_be_default_match = true;
  return match;
}
