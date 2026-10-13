// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/tool_controller.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/tab_annotation_manager.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/actor/tools/find_and_highlight_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/open_known_page_tool_request.h"
#include "chrome/browser/actor/tools/perform_search_tool_request.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_registry.h"
#include "chrome/browser/actor/tools/switch_tab_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/translate_page_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/window_management_tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/ttc/app/public/tool_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace ttc {

namespace {

// Asserts that `response` is an error and expects its code to be
// `expected_code`. Must be used in a void-returning function.
#define EXPECT_TOOL_ERROR(response, expected_code)       \
  do {                                                   \
    ASSERT_FALSE((response).Ok());                       \
    EXPECT_EQ((response).error().code, (expected_code)); \
  } while (0)

class ToolControllerBrowserTest : public TtcCoreBrowserTestBase {
 public:
  ToolControllerBrowserTest() = default;
  ~ToolControllerBrowserTest() override = default;

  // TtcCoreBrowserTestBase:
  void SetUpOnMainThread() override {
    TtcCoreBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    // For actor test pages such as /actor/media.html.
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "components/test/data");
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, NavigateCurrentTab) {
  // Verify ActorKeyedService is available.
  auto* actor_service = actor::ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ToolRequest tool_request;
  tool_request.name = std::string(actor::NavigateToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::NavigateToolRequest::kUrlParam, url.spec());

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());

  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, NavigateMissingUrl) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name = std::string(actor::NavigateToolRequest::kModelFacingName);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, PerformSearchCurrentTab) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  // Point the default search engine at the test server so the search
  // navigation actually commits.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(template_url_service);
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

  TemplateURLData data;
  data.SetShortName(u"test");
  data.SetKeyword(u"test");
  data.SetURL(embedded_https_test_server()
                  .GetURL("example.com", "/title1.html?q={searchTerms}")
                  .spec());
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  ASSERT_TRUE(template_url);
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::PerformSearchToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::PerformSearchToolRequest::kQueryParam,
                             "kittens");

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());

  EXPECT_EQ(web_contents()->GetLastCommittedURL(),
            embedded_https_test_server().GetURL("example.com",
                                                "/title1.html?q=kittens"));
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, PerformSearchMissingQuery) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::PerformSearchToolRequest::kModelFacingName);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, CloseCurrentTab) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  // Open a second tab so that closing the active one leaves the browser open.
  TabStripModel* tab_strip = browser()->GetTabStripModel();
  tabs::TabHandle first_tab = tab_strip->GetActiveTab()->GetHandle();
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(tab_strip->count(), 2);
  ASSERT_NE(tab_strip->GetActiveTab()->GetHandle(), first_tab);

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name = std::string(actor::CloseTabToolRequest::kModelFacingName);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());
  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());

  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_EQ(tab_strip->GetActiveTab()->GetHandle(), first_tab);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       CloseOnlyTabInWindowClosesWindow) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  // The default browser window has one tab.
  TabStripModel* first_tab_strip = browser()->GetTabStripModel();
  ASSERT_EQ(first_tab_strip->count(), 1);

  // Create a second browser window with a single tab. CreateBrowser activates
  // it.
  BrowserWindowInterface* second_browser = CreateBrowser(profile());
  TabStripModel* second_tab_strip = second_browser->GetTabStripModel();
  ASSERT_EQ(second_tab_strip->count(), 1);

  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(profile());
  ASSERT_TRUE(collection);
  ASSERT_EQ(collection->GetSize(), 2u);
  ASSERT_EQ(collection->GetLastActiveBrowser(), second_browser);

  ui_test_utils::BrowserDestroyedObserver destroyed_observer(second_browser);
  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = std::string(actor::CloseTabToolRequest::kModelFacingName);
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());
  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());
  destroyed_observer.Wait();

  EXPECT_EQ(collection->GetSize(), 1u);
  EXPECT_EQ(collection->GetLastActiveBrowser(), browser());
  // The session should still be alive.
  EXPECT_NE(ttc_service().session_controller(), nullptr);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, GoBackAndGoForward) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);
  const GURL first_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), first_url));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), second_url));

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::HistoryBackToolRequest::kModelFacingName);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), first_url);
  }

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::HistoryForwardToolRequest::kModelFacingName);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), second_url);
  }
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, GoBackWithoutHistory) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::HistoryBackToolRequest::kModelFacingName);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kHistoryNoBackEntries);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, ReloadPage) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::TestNavigationObserver navigation_observer(web_contents());

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::ReloadPageToolRequest::kModelFacingName);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  EXPECT_TRUE(future.Take().Ok());

  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, SwitchTab) {
  // Navigate tab 0 to a page with title "Title Of Awesomeness".
  const GURL target_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), target_url));

  // Open tab 1 in the foreground so tab 0 is in the background.
  const GURL active_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), active_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->GetTabStripModel()->active_index(), 1);

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::SwitchTabToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::SwitchTabToolRequest::kQueryParam,
                             "Awesomeness");

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());
  EXPECT_EQ(browser()->GetTabStripModel()->active_index(), 0);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), target_url);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, SwitchTabInvalidArguments) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::SwitchTabToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::SwitchTabToolRequest::kQueryParam, "");

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, OpenKnownPage) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));

  // Add a bookmark matching "Awesomeness" (with no second tab open, verifying
  // that OpenKnownPageTool's bookmark fallback is executed).
  const GURL bookmark_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html");
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile());
  ASSERT_TRUE(bookmark_model);
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  bookmark_model->AddURL(bookmark_model->other_node(), 0,
                         u"Title Of Awesomeness", bookmark_url);

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::OpenKnownPageToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::OpenKnownPageToolRequest::kQueryParam,
                             "Awesomeness");

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), bookmark_url);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       OpenKnownPageInvalidArguments) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::OpenKnownPageToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::OpenKnownPageToolRequest::kQueryParam, "");

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, FindAndHighlight) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  // title1.html's body is "This page has no title."
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::FindAndHighlightToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::FindAndHighlightToolRequest::kQueryParam,
                             "no title");

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  EXPECT_TRUE(future.Take().Ok());

  auto* annotation_manager =
      actor::TabAnnotationManager::FromWebContents(web_contents());
  ASSERT_TRUE(annotation_manager);
  EXPECT_TRUE(annotation_manager->HasActiveHighlight());
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       FindAndHighlightTextNotFound) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::FindAndHighlightToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::FindAndHighlightToolRequest::kQueryParam,
                             "nonexistent text");

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(
      response, actor::mojom::ActionResultCode::kFindAndHighlightTextNotFound);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       FindAndHighlightMissingQuery) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::FindAndHighlightToolRequest::kModelFacingName);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, PauseAndPlayVideo) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/actor/media.html")));

  // Start playback so there's an active media session.
  ASSERT_TRUE(content::ExecJs(web_contents(), "play()"));
  ASSERT_EQ(true, content::EvalJs(web_contents(), "waitForEvent('play')"));

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::PauseMediaToolRequest::kModelFacingName);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_EQ(true, content::EvalJs(web_contents(), "waitForEvent('pause')"));
  }

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::PlayMediaToolRequest::kModelFacingName);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_EQ(true, content::EvalJs(web_contents(), "waitForEvent('play')"));
  }
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, PlayVideoNoMedia) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::PlayMediaToolRequest::kModelFacingName);
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  ASSERT_FALSE(response.Ok());
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kMediaControlNoMedia);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, SeekToTimestamp) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/actor/media.html")));

  // Start playback to initialize the media session, then pause so that
  // currentTime doesn't drift during the seek.
  ASSERT_TRUE(content::ExecJs(web_contents(), "play()"));
  ASSERT_EQ(true, content::EvalJs(web_contents(), "waitForEvent('play')"));
  ASSERT_TRUE(content::ExecJs(web_contents(), "video.pause()"));
  ASSERT_EQ(true, content::EvalJs(web_contents(), "waitForEvent('pause')"));

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::SeekMediaToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::SeekMediaToolRequest::kTimecodeParam,
                             "0:01");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  EXPECT_TRUE(future.Take().Ok());
  EXPECT_EQ(true, content::EvalJs(web_contents(), "waitForSeek(1.0)"));
  EXPECT_EQ(
      1.0,
      content::EvalJs(web_contents(), "video.currentTime").ExtractDouble());
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       SeekToTimestampInvalidTimecode) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  for (const char* timecode : {"abc", "-5", "1:2:3:4", ""}) {
    SCOPED_TRACE(timecode);
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::SeekMediaToolRequest::kModelFacingName);
    tool_request.arguments.Set(actor::SeekMediaToolRequest::kTimecodeParam,
                               timecode);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());

    ToolResponse response = future.Take();
    EXPECT_TOOL_ERROR(response,
                      actor::mojom::ActionResultCode::kArgumentsInvalid);
  }
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       SeekToTimestampMissingTimecode) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::SeekMediaToolRequest::kModelFacingName);
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kArgumentsInvalid);
}

// Serves a mock translate script so that translations complete without
// reaching the real translate service.
class ToolControllerTranslateBrowserTest : public ToolControllerBrowserTest {
 public:
  // ToolControllerBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ToolControllerBrowserTest::SetUpCommandLine(command_line);
    // The base fixture starts embedded_test_server() itself, so the mock
    // script is served from a separate server.
    translate_script_server_.RegisterRequestHandler(
        base::BindRepeating(&ToolControllerTranslateBrowserTest::HandleRequest,
                            base::Unretained(this)));
    ASSERT_TRUE(translate_script_server_.Start());
    command_line->AppendSwitchASCII(
        translate::switches::kTranslateScriptURL,
        translate_script_server_.GetURL("/mock_translate_script.js").spec());
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().GetPath() != "/mock_translate_script.js") {
      return nullptr;
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(R"JS(
      var google = {};
      google.translate = (function() {
        return {
          TranslateService: function() {
            return {
              isAvailable : function() { return true; },
              restore : function() { return; },
              getDetectedLanguage : function() { return "es"; },
              translatePage : function(sourceLang, targetLang,
                                       onTranslateProgress) {
                onTranslateProgress(100, true, false);
              }
            };
          }
        };
      })();
      cr.googleTranslate.onTranslateElementLoad();
    )JS");
    http_response->set_content_type("text/javascript");
    return std::move(http_response);
  }

  net::EmbeddedTestServer translate_script_server_;
};

IN_PROC_BROWSER_TEST_F(ToolControllerTranslateBrowserTest,
                       TranslatePageDefaultLanguage) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/empty.html")));

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::TranslatePageToolRequest::kModelFacingName);
  tool_request.arguments.Set(
      actor::TranslatePageToolRequest::kTargetLanguageParam, "");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());
  EXPECT_TRUE(future.Take().Ok());

  ChromeTranslateClient* translate_client =
      ChromeTranslateClient::FromWebContents(web_contents());
  ASSERT_TRUE(translate_client);
  std::string source_language;
  std::string expected_target_language;
  translate_client->GetTranslateLanguages(web_contents(), &source_language,
                                          &expected_target_language,
                                          /*for_display=*/false);
  EXPECT_EQ(translate_client->GetLanguageState().current_language(),
            expected_target_language);
}

IN_PROC_BROWSER_TEST_F(ToolControllerTranslateBrowserTest,
                       TranslatePageSpecificTargetLanguage) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/empty.html")));

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::TranslatePageToolRequest::kModelFacingName);
  tool_request.arguments.Set(
      actor::TranslatePageToolRequest::kTargetLanguageParam, "fr");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());
  EXPECT_TRUE(future.Take().Ok());

  ChromeTranslateClient* translate_client =
      ChromeTranslateClient::FromWebContents(web_contents());
  ASSERT_TRUE(translate_client);
  EXPECT_EQ(translate_client->GetLanguageState().current_language(), "fr");
}

IN_PROC_BROWSER_TEST_F(ToolControllerTranslateBrowserTest,
                       TranslatePageMissingTargetLanguage) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/empty.html")));

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::TranslatePageToolRequest::kModelFacingName);
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ToolControllerTranslateBrowserTest,
                       TranslatePageUnsupportedLanguage) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/empty.html")));

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::TranslatePageToolRequest::kModelFacingName);
  tool_request.arguments.Set(
      actor::TranslatePageToolRequest::kTargetLanguageParam,
      "unsupported-lang-xyz");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(
      response, actor::mojom::ActionResultCode::kTranslateUnsupportedLanguage);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, EnterAndExitFullscreen) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);
  ASSERT_FALSE(browser()->GetWindow()->IsFullscreen());

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::EnterFullscreenToolRequest::kModelFacingName);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_TRUE(browser()->GetWindow()->IsFullscreen());
  }

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::ExitFullscreenToolRequest::kModelFacingName);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_FALSE(browser()->GetWindow()->IsFullscreen());
  }
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, ClickElement) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL(
          "example.com", "/actor/page_with_clickable_element.html")));
  ASSERT_EQ(false, content::EvalJs(web_contents(), "button_clicked"));

  std::optional<int> button_id = content::GetDOMNodeId(
      *web_contents()->GetPrimaryMainFrame(), "button#clickable");
  ASSERT_TRUE(button_id);
  std::string document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "click_element";
  tool_request.arguments.Set("node_id", base::DictValue()
                                            .Set("document_id", document_id)
                                            .Set("dom_node_id", *button_id));
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  EXPECT_TRUE(future.Take().Ok());
  EXPECT_EQ(true, content::EvalJs(web_contents(), "button_clicked"));
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, ClickElementMissingNodeId) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "click_element";
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kArgumentsInvalid);
}

// Ensures that a click targeting a DOM node that is valid in the current
// document, but carrying the document ID of a previously navigated-away-from
// document, is rejected.
IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       ClickElementStaleDocumentIdAfterNavigation) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL(
          "example.com", "/actor/page_with_clickable_element.html")));
  std::string old_document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();

  // Navigate to a new document containing the same clickable element.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(
                     "foo.com", "/actor/page_with_clickable_element.html")));
  ASSERT_EQ(false, content::EvalJs(web_contents(), "button_clicked"));

  std::string new_document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();
  ASSERT_NE(old_document_id, new_document_id);

  // The node ID is valid in the new document.
  std::optional<int> button_id = content::GetDOMNodeId(
      *web_contents()->GetPrimaryMainFrame(), "button#clickable");
  ASSERT_TRUE(button_id);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "click_element";
  tool_request.arguments.Set("node_id", base::DictValue()
                                            .Set("document_id", old_document_id)
                                            .Set("dom_node_id", *button_id));
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  EXPECT_FALSE(future.Take().Ok());
  EXPECT_EQ(false, content::EvalJs(web_contents(), "button_clicked"));
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, SetText) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/actor/input.html")));
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "document.getElementById('input').value = "
                              "'old value';"));

  std::optional<int> input_id = content::GetDOMNodeId(
      *web_contents()->GetPrimaryMainFrame(), "input#input");
  ASSERT_TRUE(input_id);
  std::string document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "set_text";
  tool_request.arguments.Set("node_id", base::DictValue()
                                            .Set("document_id", document_id)
                                            .Set("dom_node_id", *input_id));
  tool_request.arguments.Set(actor::TypeToolRequest::kTextParam, "new value");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  EXPECT_TRUE(future.Take().Ok());
  EXPECT_EQ("new value",
            content::EvalJs(web_contents(),
                            "document.getElementById('input').value"));
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, SetTextMissingNodeId) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "set_text";
  tool_request.arguments.Set(actor::TypeToolRequest::kTextParam, "new value");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, SetTextMissingText) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "set_text";
  tool_request.arguments.Set(
      "node_id",
      base::DictValue().Set("document_id", "token").Set("dom_node_id", 1));
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kArgumentsInvalid);
}

// Ensures that setting text on a DOM node that is valid in the current
// document, but carrying the document ID of a previously navigated-away-from
// document, is rejected.
IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       SetTextStaleDocumentIdAfterNavigation) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("example.com", "/actor/input.html")));
  std::string old_document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();

  // Navigate to a new document containing the same input element.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("foo.com", "/actor/input.html")));
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "document.getElementById('input').value = "
                              "'old value';"));

  std::string new_document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();
  ASSERT_NE(old_document_id, new_document_id);

  // The node ID is valid in the new document.
  std::optional<int> input_id = content::GetDOMNodeId(
      *web_contents()->GetPrimaryMainFrame(), "input#input");
  ASSERT_TRUE(input_id);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "set_text";
  tool_request.arguments.Set("node_id", base::DictValue()
                                            .Set("document_id", old_document_id)
                                            .Set("dom_node_id", *input_id));
  tool_request.arguments.Set(actor::TypeToolRequest::kTextParam, "new value");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  EXPECT_FALSE(future.Take().Ok());
  EXPECT_EQ("old value",
            content::EvalJs(web_contents(),
                            "document.getElementById('input').value"));
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, SelectOption) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(
                     "example.com", "/actor/select_tool.html")));
  ASSERT_EQ("alpha", content::EvalJs(web_contents(),
                                     "document.getElementById('plainSelect')"
                                     ".value"));

  std::optional<int> select_id = content::GetDOMNodeId(
      *web_contents()->GetPrimaryMainFrame(), "select#plainSelect");
  ASSERT_TRUE(select_id);
  std::string document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "select_option";
  tool_request.arguments.Set("node_id", base::DictValue()
                                            .Set("document_id", document_id)
                                            .Set("dom_node_id", *select_id));
  tool_request.arguments.Set("value", "last");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  EXPECT_TRUE(future.Take().Ok());
  EXPECT_EQ("last", content::EvalJs(web_contents(),
                                    "document.getElementById('plainSelect')"
                                    ".value"));
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, SelectOptionMissingNodeId) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "select_option";
  tool_request.arguments.Set("value", "last");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, SelectOptionMissingValue) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "select_option";
  tool_request.arguments.Set(
      "node_id",
      base::DictValue().Set("document_id", "token").Set("dom_node_id", 1));
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  ASSERT_FALSE(response.Ok());
  EXPECT_EQ(response.error().code,
            actor::mojom::ActionResultCode::kArgumentsInvalid);
}

// Ensures that selecting an option on a DOM node that is valid in the current
// document, but carrying the document ID of a previously navigated-away-from
// document, is rejected.
IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       SelectOptionStaleDocumentIdAfterNavigation) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(
                     "example.com", "/actor/select_tool.html")));
  std::string old_document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();

  // Navigate to a new document containing the same select element.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(
                     "foo.com", "/actor/select_tool.html")));
  ASSERT_EQ("alpha", content::EvalJs(web_contents(),
                                     "document.getElementById('plainSelect')"
                                     ".value"));

  std::string new_document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();
  ASSERT_NE(old_document_id, new_document_id);

  // The node ID is valid in the new document.
  std::optional<int> select_id = content::GetDOMNodeId(
      *web_contents()->GetPrimaryMainFrame(), "select#plainSelect");
  ASSERT_TRUE(select_id);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "select_option";
  tool_request.arguments.Set("node_id", base::DictValue()
                                            .Set("document_id", old_document_id)
                                            .Set("dom_node_id", *select_id));
  tool_request.arguments.Set("value", "last");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  EXPECT_FALSE(future.Take().Ok());
  EXPECT_EQ("alpha", content::EvalJs(web_contents(),
                                     "document.getElementById('plainSelect')"
                                     ".value"));
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, UnsupportedTool) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name = "unsupported_tool";

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response, actor::mojom::ActionResultCode::kToolUnknown);
  ASSERT_TRUE(response.error().message.has_value());
  EXPECT_EQ(*response.error().message, "Unsupported tool");
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, GetToolDefinitions) {
  auto* actor_service = actor::ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  const std::set<actor::ToolId> kExpectedToolIds = {
      actor::ToolId::kNavigate,        actor::ToolId::kPerformSearch,
      actor::ToolId::kGoBack,          actor::ToolId::kGoForward,
      actor::ToolId::kReloadPage,      actor::ToolId::kFindAndHighlight,
      actor::ToolId::kPlayVideo,       actor::ToolId::kPauseVideo,
      actor::ToolId::kSeekToTimestamp, actor::ToolId::kTranslatePage,
#if !BUILDFLAG(IS_ANDROID)
      actor::ToolId::kSwitchTab,       actor::ToolId::kOpenKnownPage,
      actor::ToolId::kCloseTab,        actor::ToolId::kEnterFullscreen,
      actor::ToolId::kExitFullscreen,
#endif
  };

  std::vector<const actor::ToolDefinition*> expected_tools =
      actor_service->tool_registry().GetToolsByIds(kExpectedToolIds);
  ASSERT_EQ(expected_tools.size(), kExpectedToolIds.size());

  constexpr size_t kNumLegacyTools = 3;
  std::vector<ToolDefinition> tools = session_controller->GetToolDefinitions();
  ASSERT_EQ(tools.size(), expected_tools.size() + kNumLegacyTools);

  // Verify the tool definitions queried from `actor::ToolRegistry`.
  for (size_t i = 0; i < expected_tools.size(); ++i) {
    SCOPED_TRACE(expected_tools[i]->name);
    EXPECT_EQ(tools[i].name, expected_tools[i]->name);
    EXPECT_EQ(tools[i].description, expected_tools[i]->description);
    EXPECT_EQ(tools[i].parameters_json_schema,
              expected_tools[i]->parameters_json_schema);
    EXPECT_EQ(tools[i].behavior, ToolDefinition::Behavior::kBlocking);
    EXPECT_EQ(tools[i].verbalization,
              ToolDefinition::Verbalization::kSilentAction);
  }

  // Verify the remaining legacy tool definitions that have not yet been
  // migrated to `actor::ToolRegistry`.
  const ToolDefinition& click_element = tools[expected_tools.size()];
  EXPECT_EQ(click_element.name, "click_element");
  EXPECT_FALSE(click_element.description.empty());
  EXPECT_EQ(click_element.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(click_element.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  const base::DictValue& click_schema = click_element.parameters_json_schema;
  const std::string* click_schema_type = click_schema.FindString("type");
  ASSERT_TRUE(click_schema_type);
  EXPECT_EQ(*click_schema_type, "object");

  const std::string* node_id_type =
      click_schema.FindStringByDottedPath("properties.node_id.type");
  ASSERT_TRUE(node_id_type);
  EXPECT_EQ(*node_id_type, "integer");

  const base::ListValue* click_required = click_schema.FindList("required");
  ASSERT_TRUE(click_required);
  EXPECT_EQ(*click_required, base::ListValue().Append("node_id"));

  const ToolDefinition& set_text = tools[expected_tools.size() + 1];
  EXPECT_EQ(set_text.name, "set_text");
  EXPECT_FALSE(set_text.description.empty());
  EXPECT_EQ(set_text.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(set_text.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  const base::DictValue& set_text_schema = set_text.parameters_json_schema;
  const std::string* set_text_schema_type = set_text_schema.FindString("type");
  ASSERT_TRUE(set_text_schema_type);
  EXPECT_EQ(*set_text_schema_type, "object");

  const std::string* set_text_node_id_type =
      set_text_schema.FindStringByDottedPath("properties.node_id.type");
  ASSERT_TRUE(set_text_node_id_type);
  EXPECT_EQ(*set_text_node_id_type, "integer");

  const std::string* set_text_text_type =
      set_text_schema.FindStringByDottedPath("properties.text.type");
  ASSERT_TRUE(set_text_text_type);
  EXPECT_EQ(*set_text_text_type, "string");

  const base::ListValue* set_text_required =
      set_text_schema.FindList("required");
  ASSERT_TRUE(set_text_required);
  EXPECT_EQ(*set_text_required,
            base::ListValue().Append("node_id").Append("text"));

  const ToolDefinition& select_option = tools[expected_tools.size() + 2];
  EXPECT_EQ(select_option.name, "select_option");
  EXPECT_FALSE(select_option.description.empty());
  EXPECT_EQ(select_option.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(select_option.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  const base::DictValue& select_schema = select_option.parameters_json_schema;
  const std::string* select_schema_type = select_schema.FindString("type");
  ASSERT_TRUE(select_schema_type);
  EXPECT_EQ(*select_schema_type, "object");

  const std::string* select_node_id_type =
      select_schema.FindStringByDottedPath("properties.node_id.type");
  ASSERT_TRUE(select_node_id_type);
  EXPECT_EQ(*select_node_id_type, "integer");

  const std::string* select_value_type =
      select_schema.FindStringByDottedPath("properties.value.type");
  ASSERT_TRUE(select_value_type);
  EXPECT_EQ(*select_value_type, "string");

  const base::ListValue* select_required = select_schema.FindList("required");
  ASSERT_TRUE(select_required);
  EXPECT_EQ(*select_required,
            base::ListValue().Append("node_id").Append("value"));
}

// The session's actor task is started with the session and stopped when it
// ends. A task that never acted is cancelled rather than finished.
IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       TaskIsCancelledUnlessSessionActed) {
  auto* actor_service = actor::ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);
  std::map<actor::TaskId, actor::ActorTask::State> last_task_states;
  base::CallbackListSubscription subscription =
      actor_service->AddTaskStateChangedCallback(
          base::BindLambdaForTesting([&](actor::ActorTask& task) {
            last_task_states[task.id()] = task.GetState();
          }));

  ttc_service().StartSession();
  ASSERT_EQ(actor_service->GetActiveTasks().size(), 1u);
  const actor::TaskId idle_task_id =
      actor_service->GetActiveTasks().begin()->first;
  ttc_service().EndSession();
  EXPECT_EQ(last_task_states[idle_task_id],
            actor::ActorTask::State::kCancelled);

  ttc_service().StartSession();
  ASSERT_EQ(actor_service->GetActiveTasks().size(), 1u);
  const actor::TaskId acting_task_id =
      actor_service->GetActiveTasks().begin()->first;
  ToolRequest tool_request;
  tool_request.name = std::string(actor::NavigateToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::NavigateToolRequest::kUrlParam,
                             embedded_https_test_server()
                                 .GetURL("example.com", "/title1.html")
                                 .spec());
  base::test::TestFuture<ToolResponse> future;
  ttc_service().session_controller()->ProcessToolCall(std::move(tool_request),
                                                      future.GetCallback());
  ASSERT_TRUE(future.Take().Ok());
  ttc_service().EndSession();
  EXPECT_EQ(last_task_states[acting_task_id],
            actor::ActorTask::State::kFinished);
}

// TTC actor tasks are given TtcKeyedService's ActorUiStateManager rather than
// the profile-wide one, so none of the tab-scoped actor UI the latter drives
// (the actor overlay, the handoff button, the tab indicator, the border glow)
// should be shown while TTC acts on a tab.
IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       NavigateDoesNotShowTabScopedActorUi) {
  auto* actor_service = actor::ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);

  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents());
  ASSERT_TRUE(tab);
  actor::ui::ActorUiTabControllerInterface* tab_controller =
      actor::ui::ActorUiTabControllerInterface::From(tab);
  ASSERT_TRUE(tab_controller);
  ASSERT_EQ(tab_controller->GetCurrentUiTabState(), actor::ui::UiTabState());

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ToolRequest tool_request;
  tool_request.name = std::string(actor::NavigateToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::NavigateToolRequest::kUrlParam, url.spec());
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());
  ASSERT_TRUE(future.Take().Ok());
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), url);

  // No UI should be showing as the TTC specific ActorUiStateManagerInterface
  // is used.
  EXPECT_EQ(tab_controller->GetCurrentUiTabState(), actor::ui::UiTabState());
  ttc_service().EndSession();

  // Control: the same navigation, performed by a task that is given the
  // profile-wide state manager, does show UI.

  const actor::TaskId control_task_id = actor_service->CreateTaskWithOptions(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTest, "control"),
      actor::GetNullEnterprisePolicyChecker(), /*options=*/nullptr,
      /*delegate=*/nullptr, actor::ui::ActorUiStateManager::Get(profile()));

  std::vector<std::unique_ptr<actor::ToolRequest>> actions;
  actions.push_back(std::make_unique<actor::NavigateToolRequest>(
      tab->GetHandle(),
      embedded_https_test_server().GetURL("example.com", "/title2.html")));
  base::test::TestFuture<std::vector<actor::ActionResultWithLatencyInfo>,
                         actor::TabObservationStrategy>
      actions_result;
  actor_service->PerformActions(control_task_id, std::move(actions),
                                actor::ActorTaskMetadata(),
                                actions_result.GetCallback());
  ASSERT_TRUE(actions_result.Wait());
  ASSERT_TRUE(actor::IsOk(*actions_result.Get<0>()[0].result));

  // There should be actor UI showing now.
  EXPECT_NE(tab_controller->GetCurrentUiTabState(), actor::ui::UiTabState());

  actor_service->StopTask(control_task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);
}

}  // namespace

}  // namespace ttc
