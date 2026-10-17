// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/site_based_search_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace actor {

namespace {

constexpr char kStartUrlPath[] = "/actor/blank.html?start";
constexpr char kSearchUrlPath[] = "/actor/blank.html?q={searchTerms}";

class ActorSiteBasedSearchToolBrowserTest : public ActorToolsTest {
 public:
  ActorSiteBasedSearchToolBrowserTest() = default;
  ~ActorSiteBasedSearchToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void RefreshExtractedPageContent() {
    auto* extraction_service = page_content_annotations::
        PageContentExtractionServiceFactory::GetForProfile(GetProfile());
    ASSERT_TRUE(extraction_service);
    base::test::TestFuture<
        std::optional<page_content_annotations::ExtractedPageContentResult>>
        refresh_future;
    extraction_service->RefreshExtractedPageContentAndEligibilityForPage(
        web_contents()->GetPrimaryPage(), refresh_future.GetCallback());
    ASSERT_TRUE(refresh_future.Take().has_value());
  }
};

IN_PROC_BROWSER_TEST_F(ActorSiteBasedSearchToolBrowserTest,
                       FailsWhenQueryEmpty) {
  const GURL url_start =
      embedded_https_test_server().GetURL("example.com", kStartUrlPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  std::unique_ptr<ToolRequest> action =
      MakeSiteBasedSearchRequest(*active_tab(), "");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ActorSiteBasedSearchToolBrowserTest,
                       FailsWhenDocumentIdentifierEmpty) {
  const GURL url_start =
      embedded_https_test_server().GetURL("example.com", kStartUrlPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  std::unique_ptr<ToolRequest> action = MakeSiteBasedSearchRequest(
      *active_tab(), "search terms", /*document_identifier=*/"");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ActorSiteBasedSearchToolBrowserTest,
                       FailsWhenDocumentIdentifierMismatch) {
  const GURL url_start =
      embedded_https_test_server().GetURL("example.com", kStartUrlPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(template_url_service);

  TemplateURLData data;
  data.SetURL(embedded_https_test_server()
                  .GetURL("example.com", kSearchUrlPath)
                  .spec());
  data.SetShortName(u"SiteSearch");
  data.SetKeyword(TemplateURL::GenerateKeyword(url_start));
  TemplateURL* site_turl =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(site_turl);

  std::unique_ptr<ToolRequest> action = MakeSiteBasedSearchRequest(
      *active_tab(), "search terms",
      /*document_identifier=*/"mismatched_document_token");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kFrameWentAway);
}

IN_PROC_BROWSER_TEST_F(ActorSiteBasedSearchToolBrowserTest,
                       FailsWhenNoSearchAvailable) {
  const GURL url_start =
      embedded_https_test_server().GetURL("example.com", kStartUrlPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  std::unique_ptr<ToolRequest> action =
      MakeSiteBasedSearchRequest(*active_tab(), "search terms");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kSiteBasedSearchNotAvailable);
}

IN_PROC_BROWSER_TEST_F(ActorSiteBasedSearchToolBrowserTest,
                       SearchViaOpenSearchTemplate) {
  const GURL url_start =
      embedded_https_test_server().GetURL("example.com", kStartUrlPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(template_url_service);

  TemplateURLData data;
  data.SetURL(embedded_https_test_server()
                  .GetURL("example.com", kSearchUrlPath)
                  .spec());
  data.SetShortName(u"SiteSearch");
  data.SetKeyword(TemplateURL::GenerateKeyword(url_start));
  TemplateURL* site_turl =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(site_turl);

  const GURL expected_search_url = site_turl->GenerateSearchURL(
      template_url_service->search_terms_data(), u"opensearch query");

  std::unique_ptr<ToolRequest> action =
      MakeSiteBasedSearchRequest(*active_tab(), "opensearch query");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(web_contents()->GetURL(), expected_search_url);
}

IN_PROC_BROWSER_TEST_F(ActorSiteBasedSearchToolBrowserTest,
                       FailsWhenNoTemplateUrlRegistered) {
  const GURL url_start =
      embedded_https_test_server().GetURL("example.com", kStartUrlPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(template_url_service);
  ASSERT_EQ(template_url_service->GetTemplateURLForHost(url_start.GetHost()),
            nullptr);
  ASSERT_EQ(template_url_service->GetTemplateURLForKeyword(
                TemplateURL::GenerateKeyword(url_start)),
            nullptr);

  std::unique_ptr<ToolRequest> action =
      MakeSiteBasedSearchRequest(*active_tab(), "opensearch query");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kSiteBasedSearchNotAvailable);

  EXPECT_EQ(web_contents()->GetURL(), url_start);
}

IN_PROC_BROWSER_TEST_F(ActorSiteBasedSearchToolBrowserTest,
                       SearchViaDomInputSearch) {
  const GURL url_start =
      embedded_https_test_server().GetURL("example.com", kStartUrlPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));
  ASSERT_TRUE(content::ExecJs(web_contents(), R"(
    document.body.innerHTML =
        '<form action="/actor/blank.html">' +
        '  <input type="search" name="q">' +
        '</form>';
  )"));
  RefreshExtractedPageContent();

  std::unique_ptr<ToolRequest> action =
      MakeSiteBasedSearchRequest(*active_tab(), "dom query");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(web_contents()->GetURL(),
            embedded_https_test_server().GetURL(
                "example.com", "/actor/blank.html?q=dom+query"));
}

}  // namespace

}  // namespace actor
