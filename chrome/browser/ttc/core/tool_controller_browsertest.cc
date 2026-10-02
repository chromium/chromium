// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/tool_controller.h"

#include <map>
#include <memory>
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
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ttc/app/public/tool_types.h"
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
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/translate_switches.h"
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

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, OpenUrlCurrentTab) {
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
  tool_request.name = "open_url";
  tool_request.arguments.Set("url", url.spec());
  tool_request.arguments.Set("new_tab", false);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());

  EXPECT_EQ(web_contents()->GetLastCommittedURL(), url);
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
  tool_request.name = "perform_search";
  tool_request.arguments.Set("query", "kittens");
  tool_request.arguments.Set("new_tab", false);

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
  tool_request.name = "perform_search";
  tool_request.arguments.Set("new_tab", false);

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
  tool_request.name = "close_current_tab";

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
  tool_request.name = "close_current_tab";
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
    tool_request.name = "go_back";
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_EQ(web_contents()->GetLastCommittedURL(), first_url);
  }

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name = "go_forward";
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
  tool_request.name = "go_back";

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
  tool_request.name = "reload_page";

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
  tool_request.name = "switch_tab";
  tool_request.arguments.Set("query", "Awesomeness");

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
  tool_request.name = "switch_tab";
  tool_request.arguments.Set("query", "");

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
  tool_request.name = "open_known_page";
  tool_request.arguments.Set("query", "Awesomeness");

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
  tool_request.name = "open_known_page";
  tool_request.arguments.Set("query", "");

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
  tool_request.name = "find_and_highlight";
  tool_request.arguments.Set("query", "no title");

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
  tool_request.name = "find_and_highlight";
  tool_request.arguments.Set("query", "nonexistent text");

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
  tool_request.name = "find_and_highlight";

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
    tool_request.name = "pause_video";
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_EQ(true, content::EvalJs(web_contents(), "waitForEvent('pause')"));
  }

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name = "play_video";
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
  tool_request.name = "play_video";
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
  tool_request.name = "seek_to_timestamp";
  tool_request.arguments.Set("timecode", "0:01");
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
    tool_request.name = "seek_to_timestamp";
    tool_request.arguments.Set("timecode", timecode);
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
  tool_request.name = "seek_to_timestamp";
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
  tool_request.name = "translate_page";
  tool_request.arguments.Set("target_language", "");
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
  tool_request.name = "translate_page";
  tool_request.arguments.Set("target_language", "fr");
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
  tool_request.name = "translate_page";
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
  tool_request.name = "translate_page";
  tool_request.arguments.Set("target_language", "unsupported-lang-xyz");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(
      response, actor::mojom::ActionResultCode::kTranslateUnsupportedLanguage);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, SetFullscreen) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);
  ASSERT_FALSE(browser()->GetWindow()->IsFullscreen());

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name = "set_fullscreen";
    tool_request.arguments.Set("fullscreen", true);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_TRUE(browser()->GetWindow()->IsFullscreen());
  }

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name = "set_fullscreen";
    tool_request.arguments.Set("fullscreen", false);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_FALSE(browser()->GetWindow()->IsFullscreen());
  }
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       SetFullscreenMissingFullscreen) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "set_fullscreen";
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kArgumentsInvalid);
  EXPECT_FALSE(browser()->GetWindow()->IsFullscreen());
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
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  std::vector<ToolDefinition> tools = session_controller->GetToolDefinitions();
  ASSERT_EQ(tools.size(), 14u);

  const ToolDefinition& open_url = tools[0];
  EXPECT_EQ(open_url.name, "open_url");
  EXPECT_FALSE(open_url.description.empty());
  EXPECT_EQ(open_url.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(open_url.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  const base::DictValue& schema = open_url.parameters_json_schema;
  const std::string* schema_type = schema.FindString("type");
  ASSERT_TRUE(schema_type);
  EXPECT_EQ(*schema_type, "object");

  const std::string* url_type =
      schema.FindStringByDottedPath("properties.url.type");
  ASSERT_TRUE(url_type);
  EXPECT_EQ(*url_type, "string");

  const std::string* new_tab_type =
      schema.FindStringByDottedPath("properties.new_tab.type");
  ASSERT_TRUE(new_tab_type);
  EXPECT_EQ(*new_tab_type, "boolean");

  const base::ListValue* required = schema.FindList("required");
  ASSERT_TRUE(required);
  EXPECT_EQ(*required, base::ListValue().Append("url").Append("new_tab"));

  const ToolDefinition& perform_search = tools[1];
  EXPECT_EQ(perform_search.name, "perform_search");
  EXPECT_FALSE(perform_search.description.empty());
  EXPECT_EQ(perform_search.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(perform_search.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  const base::DictValue& search_schema = perform_search.parameters_json_schema;
  const std::string* search_schema_type = search_schema.FindString("type");
  ASSERT_TRUE(search_schema_type);
  EXPECT_EQ(*search_schema_type, "object");

  const std::string* query_type =
      search_schema.FindStringByDottedPath("properties.query.type");
  ASSERT_TRUE(query_type);
  EXPECT_EQ(*query_type, "string");

  const std::string* search_new_tab_type =
      search_schema.FindStringByDottedPath("properties.new_tab.type");
  ASSERT_TRUE(search_new_tab_type);
  EXPECT_EQ(*search_new_tab_type, "boolean");

  const base::ListValue* search_required = search_schema.FindList("required");
  ASSERT_TRUE(search_required);
  EXPECT_EQ(*search_required,
            base::ListValue().Append("query").Append("new_tab"));

  const ToolDefinition& close_current_tab = tools[2];
  EXPECT_EQ(close_current_tab.name, "close_current_tab");
  EXPECT_FALSE(close_current_tab.description.empty());
  EXPECT_EQ(close_current_tab.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(close_current_tab.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  // The tool takes no arguments.
  EXPECT_TRUE(close_current_tab.parameters_json_schema.empty());

  const ToolDefinition& go_back = tools[3];
  EXPECT_EQ(go_back.name, "go_back");
  EXPECT_FALSE(go_back.description.empty());
  EXPECT_EQ(go_back.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(go_back.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  // The tool takes no arguments.
  EXPECT_TRUE(go_back.parameters_json_schema.empty());

  const ToolDefinition& go_forward = tools[4];
  EXPECT_EQ(go_forward.name, "go_forward");
  EXPECT_FALSE(go_forward.description.empty());
  EXPECT_EQ(go_forward.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(go_forward.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  // The tool takes no arguments.
  EXPECT_TRUE(go_forward.parameters_json_schema.empty());

  const ToolDefinition& reload_page = tools[5];
  EXPECT_EQ(reload_page.name, "reload_page");
  EXPECT_FALSE(reload_page.description.empty());
  EXPECT_EQ(reload_page.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(reload_page.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  // The tool takes no arguments.
  EXPECT_TRUE(reload_page.parameters_json_schema.empty());

  constexpr std::pair<size_t, std::string_view> kQueryTools[] = {
      {6u, "switch_tab"},
      {7u, "open_known_page"},
  };
  for (const auto& [index, expected_name] : kQueryTools) {
    const ToolDefinition& tool = tools[index];
    EXPECT_EQ(tool.name, expected_name);
    EXPECT_FALSE(tool.description.empty());
    EXPECT_EQ(tool.behavior, ToolDefinition::Behavior::kBlocking);
    EXPECT_EQ(tool.verbalization, ToolDefinition::Verbalization::kSilentAction);

    const base::DictValue& tool_schema = tool.parameters_json_schema;
    const std::string* tool_schema_type = tool_schema.FindString("type");
    ASSERT_TRUE(tool_schema_type);
    EXPECT_EQ(*tool_schema_type, "object");

    const std::string* tool_query_type =
        tool_schema.FindStringByDottedPath("properties.query.type");
    ASSERT_TRUE(tool_query_type);
    EXPECT_EQ(*tool_query_type, "string");

    const std::string* query_description =
        tool_schema.FindStringByDottedPath("properties.query.description");
    ASSERT_TRUE(query_description);
    EXPECT_FALSE(query_description->empty());

    const base::ListValue* tool_required = tool_schema.FindList("required");
    ASSERT_TRUE(tool_required);
    EXPECT_EQ(*tool_required, base::ListValue().Append("query"));
  }

  const ToolDefinition& find_and_highlight = tools[8];
  EXPECT_EQ(find_and_highlight.name, "find_and_highlight");
  EXPECT_FALSE(find_and_highlight.description.empty());
  EXPECT_EQ(find_and_highlight.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(find_and_highlight.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  const base::DictValue& highlight_schema =
      find_and_highlight.parameters_json_schema;
  const std::string* highlight_schema_type =
      highlight_schema.FindString("type");
  ASSERT_TRUE(highlight_schema_type);
  EXPECT_EQ(*highlight_schema_type, "object");

  const std::string* highlight_query_type =
      highlight_schema.FindStringByDottedPath("properties.query.type");
  ASSERT_TRUE(highlight_query_type);
  EXPECT_EQ(*highlight_query_type, "string");

  const base::ListValue* highlight_required =
      highlight_schema.FindList("required");
  ASSERT_TRUE(highlight_required);
  EXPECT_EQ(*highlight_required, base::ListValue().Append("query"));

  const ToolDefinition& play_video = tools[9];
  EXPECT_EQ(play_video.name, "play_video");
  EXPECT_FALSE(play_video.description.empty());
  EXPECT_EQ(play_video.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(play_video.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  // The tool takes no arguments.
  EXPECT_TRUE(play_video.parameters_json_schema.empty());

  const ToolDefinition& pause_video = tools[10];
  EXPECT_EQ(pause_video.name, "pause_video");
  EXPECT_FALSE(pause_video.description.empty());
  EXPECT_EQ(pause_video.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(pause_video.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  // The tool takes no arguments.
  EXPECT_TRUE(pause_video.parameters_json_schema.empty());

  const ToolDefinition& seek_to_timestamp = tools[11];
  EXPECT_EQ(seek_to_timestamp.name, "seek_to_timestamp");
  EXPECT_FALSE(seek_to_timestamp.description.empty());
  EXPECT_EQ(seek_to_timestamp.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(seek_to_timestamp.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  const base::DictValue& seek_schema = seek_to_timestamp.parameters_json_schema;
  const std::string* seek_schema_type = seek_schema.FindString("type");
  ASSERT_TRUE(seek_schema_type);
  EXPECT_EQ(*seek_schema_type, "object");

  const std::string* seek_timecode_type =
      seek_schema.FindStringByDottedPath("properties.timecode.type");
  ASSERT_TRUE(seek_timecode_type);
  EXPECT_EQ(*seek_timecode_type, "string");

  const base::ListValue* seek_required = seek_schema.FindList("required");
  ASSERT_TRUE(seek_required);
  EXPECT_EQ(*seek_required, base::ListValue().Append("timecode"));

  const ToolDefinition& translate_page = tools[12];
  EXPECT_EQ(translate_page.name, "translate_page");
  EXPECT_FALSE(translate_page.description.empty());
  EXPECT_EQ(translate_page.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(translate_page.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  const base::DictValue& translate_schema =
      translate_page.parameters_json_schema;
  const std::string* translate_schema_type =
      translate_schema.FindString("type");
  ASSERT_TRUE(translate_schema_type);
  EXPECT_EQ(*translate_schema_type, "object");

  const std::string* translate_target_language_type =
      translate_schema.FindStringByDottedPath(
          "properties.target_language.type");
  ASSERT_TRUE(translate_target_language_type);
  EXPECT_EQ(*translate_target_language_type, "string");

  const base::ListValue* translate_required =
      translate_schema.FindList("required");
  ASSERT_TRUE(translate_required);
  EXPECT_EQ(*translate_required, base::ListValue().Append("target_language"));

  const ToolDefinition& set_fullscreen = tools[13];
  EXPECT_EQ(set_fullscreen.name, "set_fullscreen");
  EXPECT_FALSE(set_fullscreen.description.empty());
  EXPECT_EQ(set_fullscreen.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(set_fullscreen.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  const base::DictValue& fullscreen_schema =
      set_fullscreen.parameters_json_schema;
  const std::string* fullscreen_schema_type =
      fullscreen_schema.FindString("type");
  ASSERT_TRUE(fullscreen_schema_type);
  EXPECT_EQ(*fullscreen_schema_type, "object");

  const std::string* fullscreen_type =
      fullscreen_schema.FindStringByDottedPath("properties.fullscreen.type");
  ASSERT_TRUE(fullscreen_type);
  EXPECT_EQ(*fullscreen_type, "boolean");

  const base::ListValue* fullscreen_required =
      fullscreen_schema.FindList("required");
  ASSERT_TRUE(fullscreen_required);
  EXPECT_EQ(*fullscreen_required, base::ListValue().Append("fullscreen"));
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
  tool_request.name = "open_url";
  tool_request.arguments.Set("url", embedded_https_test_server()
                                        .GetURL("example.com", "/title1.html")
                                        .spec());
  tool_request.arguments.Set("new_tab", false);
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
                       OpenUrlDoesNotShowTabScopedActorUi) {
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
  tool_request.name = "open_url";
  tool_request.arguments.Set("url", url.spec());
  tool_request.arguments.Set("new_tab", false);
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
