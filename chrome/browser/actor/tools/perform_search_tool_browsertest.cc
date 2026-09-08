// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/perform_search_tool_request.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace actor {

namespace {

constexpr char kStartUrlPath[] = "/actor/blank.html?start";
constexpr char kSearchUrlPath[] = "/actor/blank.html?q={searchTerms}";
constexpr char16_t kKeyword[] = u"test";

class ActorPerformSearchToolBrowserTest : public ActorToolsTest {
 public:
  ActorPerformSearchToolBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicActor);
  }
  ~ActorPerformSearchToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
    ASSERT_TRUE(template_url_service);

    TemplateURLData data;
    data.SetURL(embedded_test_server()->GetURL(kSearchUrlPath).spec());
    data.SetShortName(kKeyword);
    data.SetKeyword(kKeyword);
    auto* default_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(default_url);
  }

  GURL GetDefaultSearchURLForQuery(std::string_view query) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
    CHECK(template_url_service);
    const TemplateURL* default_provider =
        template_url_service->GetDefaultSearchProvider();
    CHECK(default_provider);
    return default_provider->GenerateSearchURL(
        template_url_service->search_terms_data(), base::UTF8ToUTF16(query));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test PerformSearch in current tab.
IN_PROC_BROWSER_TEST_F(ActorPerformSearchToolBrowserTest,
                       PerformSearch_Success) {
  constexpr char kQuery[] = "test query";
  const GURL url_start = embedded_test_server()->GetURL(kStartUrlPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url_start));

  std::unique_ptr<ToolRequest> action =
      MakePerformSearchRequest(*active_tab(), kQuery);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(web_contents()->GetURL(), GetDefaultSearchURLForQuery(kQuery));
}

IN_PROC_BROWSER_TEST_F(ActorPerformSearchToolBrowserTest, EmptyQueryFails) {
  std::unique_ptr<ToolRequest> action =
      MakePerformSearchRequest(*active_tab(), "");
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ActorPerformSearchToolBrowserTest, TabWentAwayFails) {
  constexpr char kQuery[] = "test query";
  int index = browser()->tab_strip_model()->count();
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->GetProfile()));
  browser()->tab_strip_model()->AppendWebContents(std::move(new_contents),
                                                  /*foreground=*/true);
  tabs::TabInterface* new_tab =
      browser()->tab_strip_model()->GetTabAtIndex(index);

  std::unique_ptr<ToolRequest> action =
      MakePerformSearchRequest(*new_tab, kQuery);

  browser()->tab_strip_model()->CloseWebContentsAt(index,
                                                   TabCloseTypes::CLOSE_NONE);

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kTabWentAway);
}

IN_PROC_BROWSER_TEST_F(ActorPerformSearchToolBrowserTest,
                       NoDefaultSearchProviderSetFails) {
  constexpr char kQuery[] = "test query";
  DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(true);
  base::ScopedClosureRunner reset_fallback(base::BindOnce([]() {
    DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(false);
  }));

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(template_url_service);
  template_url_service->SetUserSelectedDefaultSearchProvider(nullptr);

  std::unique_ptr<ToolRequest> action =
      MakePerformSearchRequest(*active_tab(), kQuery);
  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(action)), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kDefaultSearchProviderNotSet);
}

}  // namespace

}  // namespace actor
