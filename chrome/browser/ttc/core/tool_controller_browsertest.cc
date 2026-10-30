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

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tab_annotation_manager.h"
#include "chrome/browser/actor/tools/find_and_highlight_tool_request.h"
#include "chrome/browser/actor/tools/gemini_panel_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/open_known_page_tool_request.h"
#include "chrome/browser/actor/tools/open_url_tool_request.h"
#include "chrome/browser/actor/tools/perform_search_tool_request.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_registry.h"
#include "chrome/browser/actor/tools/switch_tab_tool_request.h"
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#include "chrome/browser/actor/tools/translate_page_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/window_management_tool_request.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_test_utils.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/ttc_core_browser_test_base.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/safety_list_manager.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/content/browser/translate_waiter.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/ttc/app/public/tool_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/media_start_stop_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"  // nogncheck
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"  // nogncheck
#include "chrome/browser/glic/test_support/glic_browser_test.h"  // nogncheck
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/actor/core/task_source_info.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "ui/base/base_window.h"
#endif

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
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TtcCoreBrowserTestBase::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_ANDROID)
    // Test devices are often offline. The offline status indicator that appears
    // a few seconds after startup resizes the top controls, which asserts if
    // the controls are not fully shown at that moment (e.g. during tab
    // fullscreen or a tab switch). See crbug.com/574927722.
    command_line->AppendSwitch(
        switches::kForceOnlineConnectionStateForIndicator);
#endif
  }

  void SetUpOnMainThread() override {
    TtcCoreBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    // For actor test pages such as /actor/media.html.
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "components/test/data");
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void TearDownOnMainThread() override {
    actor::SetSafetyListsForTesting(actor::SafetyListManager::GetInstance(),
                                    "{}");
    TtcCoreBrowserTestBase::TearDownOnMainThread();
  }

 protected:
  tabs::TabInterface* active_tab() {
    return tabs::TabInterface::GetFromContents(web_contents());
  }

  TabListInterface* tab_list() {
    return TabListInterface::From(active_tab()->GetBrowserWindowInterface());
  }

  void OpenForegroundTab(const GURL& url) {
    // Open in the background and activate explicitly: on Android, a foreground
    // open runs the new-tab animation, which can re-select the new tab
    // asynchronously after a subsequent tab switch.
    tabs::TabInterface* tab =
        tab_list()->OpenTab(url, /*index=*/-1, /*foreground=*/false);
    ASSERT_TRUE(tab);
    ASSERT_TRUE(content::WaitForLoadStop(tab->GetContents()));
    tab_list()->ActivateTab(tab->GetHandle());
    ASSERT_TRUE(base::test::RunUntil([tab]() { return tab->IsActivated(); }));
  }
};

// `navigate` has been replaced by `open_url` and is no longer dispatched.
IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, NavigateIsUnsupported) {
  const GURL start_url = web_contents()->GetLastCommittedURL();

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
  EXPECT_TOOL_ERROR(response, actor::mojom::ActionResultCode::kToolUnknown);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), start_url);
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
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  // Open a second tab so that closing the active one leaves the browser open.
  tabs::TabHandle first_tab = active_tab()->GetHandle();
  ASSERT_NO_FATAL_FAILURE(OpenForegroundTab(
      embedded_https_test_server().GetURL("example.com", "/title2.html")));
  ASSERT_EQ(tab_list()->GetTabCount(), 2);
  ASSERT_NE(tab_list()->GetActiveTab()->GetHandle(), first_tab);

  base::test::TestFuture<ToolResponse> future;

  ToolRequest tool_request;
  tool_request.name = std::string(actor::CloseTabToolRequest::kModelFacingName);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());
  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());

  EXPECT_EQ(tab_list()->GetTabCount(), 1);
  EXPECT_EQ(tab_list()->GetActiveTab()->GetHandle(), first_tab);
}

#if !BUILDFLAG(IS_ANDROID)
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
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, GoBackAndGoForward) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);
  const GURL first_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  const GURL second_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), first_url));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), second_url));

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
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

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

// The actor framework does not act on crashed tabs, so reload_page reloads
// them directly.
IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, ReloadPageOnCrashedTab) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::CrashTab(web_contents());
  ASSERT_TRUE(web_contents()->IsCrashed());

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::ReloadPageToolRequest::kModelFacingName);
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  EXPECT_TRUE(future.Take().Ok());
  EXPECT_FALSE(web_contents()->IsCrashed());
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
  ASSERT_NO_FATAL_FAILURE(OpenForegroundTab(active_url));
  ASSERT_EQ(tab_list()->GetActiveIndex(), 1);

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
  EXPECT_EQ(tab_list()->GetActiveIndex(), 0);
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

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, OpenUrlInCurrentTab) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  const int initial_tab_count = tab_list()->GetTabCount();

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  const GURL target_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html");
  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = std::string(actor::OpenUrlToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::OpenUrlToolRequest::kUrlParam,
                             target_url.spec());
  tool_request.arguments.Set(actor::OpenUrlToolRequest::kNewTabParam, false);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());
  EXPECT_EQ(tab_list()->GetTabCount(), initial_tab_count);
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), target_url);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, OpenUrlInNewTab) {
  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/title1.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  content::WebContents* source_contents = web_contents();
  const int initial_tab_count = tab_list()->GetTabCount();

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  const GURL target_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html");
  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = std::string(actor::OpenUrlToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::OpenUrlToolRequest::kUrlParam,
                             target_url.spec());
  tool_request.arguments.Set(actor::OpenUrlToolRequest::kNewTabParam, true);

  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TRUE(response.Ok());
  ASSERT_EQ(tab_list()->GetTabCount(), initial_tab_count + 1);
  content::WebContents* active_contents = web_contents();
  EXPECT_NE(active_contents, source_contents);
  EXPECT_EQ(active_contents->GetLastCommittedURL(), target_url);
  EXPECT_EQ(source_contents->GetLastCommittedURL(), start_url);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, OpenUrlInvalidArguments) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  struct {
    const char* description;
    std::optional<std::string> url;
    std::optional<base::Value> new_tab;
  } kCases[] = {
      {"missing url", std::nullopt, base::Value(true)},
      {"empty url", "", base::Value(true)},
      {"invalid url", "not a url", base::Value(true)},
      {"missing new_tab", "https://example.com/", std::nullopt},
      {"non-bool new_tab", "https://example.com/", base::Value("true")},
  };

  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.description);
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::OpenUrlToolRequest::kModelFacingName);
    if (test_case.url) {
      tool_request.arguments.Set(actor::OpenUrlToolRequest::kUrlParam,
                                 *test_case.url);
    }
    if (test_case.new_tab) {
      tool_request.arguments.Set(actor::OpenUrlToolRequest::kNewTabParam,
                                 test_case.new_tab->Clone());
    }

    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());

    ToolResponse response = future.Take();
    EXPECT_TOOL_ERROR(response,
                      actor::mojom::ActionResultCode::kArgumentsInvalid);
  }
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, FindAndHighlight) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  // title1.html's body is "This page has no title."
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
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

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
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

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/actor/media.html")));

  // Loop the 4s test video so it cannot reach end-of-stream on slow bots, and
  // wait for the browser process to register the active MediaSession player
  // before invoking `pause_video`.
  {
    content::MediaStartStopObserver start_observer(
        web_contents(), content::MediaStartStopObserver::Type::kStart);
    ASSERT_TRUE(content::ExecJs(web_contents(), "video.loop = true; play();"));
    start_observer.Wait();
  }
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

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
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

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/actor/media.html")));

  // Start playback and wait for the browser process to register the active
  // MediaSession player and its duration, then pause so that currentTime
  // doesn't drift during the seek.
  {
    content::MediaStartStopObserver start_observer(
        web_contents(), content::MediaStartStopObserver::Type::kStart);
    ASSERT_TRUE(content::ExecJs(web_contents(), "video.loop = true; play();"));
    start_observer.Wait();
  }
  {
    content::MediaStartStopObserver stop_observer(
        web_contents(), content::MediaStartStopObserver::Type::kStop);
    ASSERT_TRUE(content::ExecJs(web_contents(), "video.pause()"));
    stop_observer.Wait();
  }
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

  void NavigateAndWaitForLanguageDetection(const GURL& url) {
    ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
    ChromeTranslateClient* translate_client =
        ChromeTranslateClient::FromWebContents(web_contents());
    ASSERT_TRUE(translate_client);
    // Wait until language detection completes for `url`. On Android, the
    // initial `about:blank` page also sends an async `RegisterPage` IPC with an
    // empty `adopted_language`, whereas `DidFinishNavigation` resets
    // `source_language()` to empty and `url`'s `RegisterPage` populates a
    // non-empty language code (e.g. "und").
    while (translate_client->GetLanguageState().source_language().empty()) {
      translate::CreateTranslateWaiter(
          web_contents(),
          translate::TranslateWaiter::WaitEvent::kLanguageDetermined)
          ->Wait();
    }
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

  ASSERT_NO_FATAL_FAILURE(NavigateAndWaitForLanguageDetection(
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

  ASSERT_NO_FATAL_FAILURE(NavigateAndWaitForLanguageDetection(
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

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
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

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
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
  // Navigate to an actual web page so that on Android the tab and view
  // hierarchy are attached before exercising fullscreen.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  auto is_fullscreen = [&]() {
#if BUILDFLAG(IS_ANDROID)
    return web_contents()->IsFullscreen();
#else
    return browser()->GetWindow()->IsFullscreen();
#endif
  };
  ASSERT_FALSE(is_fullscreen());

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::EnterFullscreenToolRequest::kModelFacingName);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_TRUE(base::test::RunUntil([&]() { return is_fullscreen(); }));
  }

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::ExitFullscreenToolRequest::kModelFacingName);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
    EXPECT_TRUE(base::test::RunUntil([&]() { return !is_fullscreen(); }));
  }
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, ClickElement) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
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

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL(
          "example.com", "/actor/page_with_clickable_element.html")));
  std::string old_document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();

  // Navigate to a new document containing the same clickable element.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL(
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

// Runs with navigation gating confirming navigations to new origins, which is
// the default.
class ToolControllerNavigationGatingBrowserTest
    : public ToolControllerBrowserTest {
 public:
  ToolControllerNavigationGatingBrowserTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        actor::kGlicCrossOriginNavigationGating,
        {{"confirm_navigation_to_new_origins", "true"},
         {"confirm_navigation_to_new_origins_dark_launch", "false"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures that clicking a link to an origin the task hasn't visited, such as a
// search result, navigates to it rather than being blocked by navigation
// gating.
IN_PROC_BROWSER_TEST_F(ToolControllerNavigationGatingBrowserTest,
                       ClickElementNavigatesToNewOrigin) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  const GURL start_url =
      embedded_https_test_server().GetURL("example.com", "/actor/link.html");
  const GURL new_origin_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), start_url));
  ASSERT_TRUE(content::ExecJs(
      web_contents(), content::JsReplace("setLink($1);", new_origin_url)));

  std::optional<int> link_id =
      content::GetDOMNodeId(*web_contents()->GetPrimaryMainFrame(), "#link");
  ASSERT_TRUE(link_id);
  std::string document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "click_element";
  tool_request.arguments.Set("node_id", base::DictValue()
                                            .Set("document_id", document_id)
                                            .Set("dom_node_id", *link_id));
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  // Without a navigation confirmation, this fails with
  // kTriggeredNavigationBlocked.
  EXPECT_TRUE(future.Take().Ok());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), new_origin_url);
}

// Ensures that `open_url` follows a redirect to an origin other than the one
// it was asked to open (e.g. https://google.com -> https://www.google.com).
// `open_url` implicitly grants the origin of its URL, but not the origin it
// redirects to.
IN_PROC_BROWSER_TEST_F(ToolControllerNavigationGatingBrowserTest,
                       OpenUrlFollowsRedirectToNewOrigin) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  const GURL redirect_url = embedded_https_test_server().GetURL(
      "bar.com", "/cross-site/foo.com/actor/blank.html");
  const GURL new_origin_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/blank.html");

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = std::string(actor::OpenUrlToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::OpenUrlToolRequest::kUrlParam,
                             redirect_url.spec());
  tool_request.arguments.Set(actor::OpenUrlToolRequest::kNewTabParam, false);
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  // Without a navigation confirmation, this fails with
  // kTriggeredNavigationBlocked.
  EXPECT_TRUE(future.Take().Ok());
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), new_origin_url);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, SetText) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
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

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/actor/input.html")));
  std::string old_document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();

  // Navigate to a new document containing the same input element.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
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

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL(
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

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL(
                          "example.com", "/actor/select_tool.html")));
  std::string old_document_id =
      optimization_guide::DocumentIdentifierUserData::
          GetOrCreateForCurrentDocument(web_contents()->GetPrimaryMainFrame())
              ->serialized_token();

  // Navigate to a new document containing the same select element.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL(
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

class ToolControllerScrollPageBrowserTest : public ToolControllerBrowserTest {
 public:
  ToolControllerScrollPageBrowserTest() = default;
  ~ToolControllerScrollPageBrowserTest() override = default;

  // ToolControllerBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ToolControllerBrowserTest::SetUpCommandLine(command_line);
    // Page/document scrolls are user scrolls, which animate when smooth
    // scrolling is enabled. Disable it so offsets can be checked as soon as
    // the tool call completes.
    command_line->AppendSwitch(switches::kDisableSmoothScrolling);
  }

  void SetUpOnMainThread() override {
    ToolControllerBrowserTest::SetUpOnMainThread();
    ttc_service().StartSession();
    ASSERT_TRUE(ttc_service().session_controller());
    ASSERT_TRUE(content::NavigateToURL(
        web_contents(), embedded_https_test_server().GetURL(
                            "example.com", "/actor/scrollable_page.html")));
    // Wait for the initial compositor frame and any pending browser-controls
    // viewport resize on Android before sampling or scrolling the viewport.
    content::WaitForCopyableViewInWebContents(web_contents());
#if defined(USE_AURA) || BUILDFLAG(IS_ANDROID)
    content::WaitForResizeComplete(web_contents());
#endif
  }

 protected:
  // Returns the `node_id` argument for the element matching `selector` in the
  // main frame, or for the main viewport if `selector` is not set.
  base::DictValue MakeNodeId(std::optional<std::string_view> selector) {
    content::RenderFrameHost* main_frame =
        web_contents()->GetPrimaryMainFrame();
    int dom_node_id = actor::kRootElementDomNodeId;
    if (selector) {
      std::optional<int> id = content::GetDOMNodeId(*main_frame, *selector);
      CHECK(id);
      dom_node_id = *id;
    }
    return base::DictValue()
        .Set("document_id", optimization_guide::DocumentIdentifierUserData::
                                GetOrCreateForCurrentDocument(main_frame)
                                    ->serialized_token())
        .Set("dom_node_id", dom_node_id);
  }

  ToolResponse ScrollPage(base::DictValue arguments) {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name = "scroll_page";
    tool_request.arguments = std::move(arguments);
    ttc_service().session_controller()->ProcessToolCall(std::move(tool_request),
                                                        future.GetCallback());
    return future.Take();
  }

  ToolResponse ScrollPage(std::string_view granularity,
                          std::string_view direction,
                          std::optional<std::string_view> selector) {
    return ScrollPage(base::DictValue()
                          .Set("granularity", granularity)
                          .Set("direction", direction)
                          .Set("node_id", MakeNodeId(selector)));
  }

  // Evaluates `script` and returns the result rounded to an int, since scroll
  // offsets may be fractional.
  int EvalInt(std::string_view script) {
    return content::EvalJs(web_contents(),
                           base::StrCat({"Math.round(", script, ")"}))
        .ExtractInt();
  }
};

// A page scroll of the viewport moves by most, but not all, of the viewport,
// and a page scroll back returns to the top.
IN_PROC_BROWSER_TEST_F(ToolControllerScrollPageBrowserTest,
                       PageDownAndUpViewport) {
  const int client_height = EvalInt("document.scrollingElement.clientHeight");

  EXPECT_TRUE(ScrollPage("page", "down", /*selector=*/std::nullopt).Ok());
  const int scroll_y = EvalInt("window.scrollY");
  EXPECT_GT(scroll_y, client_height / 2);
  EXPECT_LE(scroll_y, client_height);

  EXPECT_TRUE(ScrollPage("page", "up", /*selector=*/std::nullopt).Ok());
  EXPECT_EQ(0, EvalInt("window.scrollY"));
}

// A document scroll of the viewport goes to the end, then back to the start.
IN_PROC_BROWSER_TEST_F(ToolControllerScrollPageBrowserTest,
                       DocumentDownAndUpViewport) {
  EXPECT_TRUE(ScrollPage("document", "down", /*selector=*/std::nullopt).Ok());
  // On fractional device scale factors (e.g. Android), Blink rounds
  // scrollHeight and clientHeight to CSS integers independently whereas
  // window.scrollY rounds the physical-pixel-clamped max scroll offset once.
  EXPECT_NEAR(EvalInt("document.scrollingElement.scrollHeight - "
                      "document.scrollingElement.clientHeight"),
              EvalInt("window.scrollY"), 1);

  EXPECT_TRUE(ScrollPage("document", "up", /*selector=*/std::nullopt).Ok());
  EXPECT_EQ(0, EvalInt("window.scrollY"));
}

// A horizontal page scroll of the viewport, and a page scroll back returns to
// the left edge.
IN_PROC_BROWSER_TEST_F(ToolControllerScrollPageBrowserTest,
                       PageRightAndLeftViewport) {
  EXPECT_TRUE(ScrollPage("page", "right", /*selector=*/std::nullopt).Ok());
  EXPECT_GT(EvalInt("window.scrollX"), 0);

  EXPECT_TRUE(ScrollPage("page", "left", /*selector=*/std::nullopt).Ok());
  EXPECT_EQ(0, EvalInt("window.scrollX"));
}

// A horizontal document scroll of the viewport goes to the right edge, then
// back to the left edge.
IN_PROC_BROWSER_TEST_F(ToolControllerScrollPageBrowserTest,
                       DocumentRightAndLeftViewport) {
  EXPECT_TRUE(ScrollPage("document", "right", /*selector=*/std::nullopt).Ok());
  EXPECT_NEAR(EvalInt("document.scrollingElement.scrollWidth - "
                      "document.scrollingElement.clientWidth"),
              EvalInt("window.scrollX"), 1);

  EXPECT_TRUE(ScrollPage("document", "left", /*selector=*/std::nullopt).Ok());
  EXPECT_EQ(0, EvalInt("window.scrollX"));
  EXPECT_EQ(0, EvalInt("window.scrollY"));
}

// Scrolling a sub-scroller scrolls it without scrolling the viewport.
IN_PROC_BROWSER_TEST_F(ToolControllerScrollPageBrowserTest, ScrollElement) {
  EXPECT_TRUE(ScrollPage("page", "down", "#scroller").Ok());
  EXPECT_GT(EvalInt("document.getElementById('scroller').scrollTop"), 0);
  EXPECT_EQ(0, EvalInt("window.scrollY"));

  EXPECT_TRUE(ScrollPage("document", "down", "#scroller").Ok());
  EXPECT_EQ(EvalInt("(() => { const s = document.getElementById('scroller');"
                    "         return s.scrollHeight - s.clientHeight; })()"),
            EvalInt("document.getElementById('scroller').scrollTop"));
  EXPECT_EQ(0, EvalInt("window.scrollY"));
}

// Scrolling up at the top of the viewport fails.
IN_PROC_BROWSER_TEST_F(ToolControllerScrollPageBrowserTest, PageUpAtTopFails) {
  ToolResponse response = ScrollPage("page", "up", /*selector=*/std::nullopt);
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kScrollOffsetDidNotChange);
  EXPECT_EQ(0, EvalInt("window.scrollY"));
}

IN_PROC_BROWSER_TEST_F(ToolControllerScrollPageBrowserTest, InvalidArguments) {
  const struct {
    std::string_view name;
    base::DictValue arguments;
  } kCases[] = {
      {"Missing granularity",
       base::DictValue()
           .Set("direction", "down")
           .Set("node_id", MakeNodeId(/*selector=*/std::nullopt))},
      {"Invalid granularity",
       base::DictValue()
           .Set("granularity", "line")
           .Set("direction", "down")
           .Set("node_id", MakeNodeId(/*selector=*/std::nullopt))},
      {"Missing direction",
       base::DictValue()
           .Set("granularity", "page")
           .Set("node_id", MakeNodeId(/*selector=*/std::nullopt))},
      {"Invalid direction",
       base::DictValue()
           .Set("granularity", "page")
           .Set("direction", "sideways")
           .Set("node_id", MakeNodeId(/*selector=*/std::nullopt))},
      {"Missing node_id",
       base::DictValue().Set("granularity", "page").Set("direction", "down")},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.name);
    ToolResponse response = ScrollPage(test_case.arguments.Clone());
    EXPECT_TOOL_ERROR(response,
                      actor::mojom::ActionResultCode::kArgumentsInvalid);
  }
  EXPECT_EQ(0, EvalInt("window.scrollY"));
}

// Ensures that scrolling with the document ID of a previously
// navigated-away-from document is rejected.
IN_PROC_BROWSER_TEST_F(ToolControllerScrollPageBrowserTest,
                       StaleDocumentIdAfterNavigation) {
  base::DictValue old_node_id = MakeNodeId(/*selector=*/std::nullopt);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL(
                          "foo.com", "/actor/scrollable_page.html")));
  ASSERT_NE(*old_node_id.FindString("document_id"),
            *MakeNodeId(/*selector=*/std::nullopt).FindString("document_id"));

  EXPECT_FALSE(ScrollPage(base::DictValue()
                              .Set("granularity", "page")
                              .Set("direction", "down")
                              .Set("node_id", std::move(old_node_id)))
                   .Ok());
  EXPECT_EQ(0, EvalInt("window.scrollY"));
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, GetScreenshot) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  content::WaitForCopyableViewInWebContents(web_contents());

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "get_screenshot";
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  ASSERT_TRUE(response.Ok());

  const std::string* mime_type = response.GetResult().FindString("mime_type");
  ASSERT_TRUE(mime_type);
  EXPECT_EQ(*mime_type, "image/jpeg");

  const std::string* image_b64 = response.GetResult().FindString("image_b64");
  ASSERT_TRUE(image_b64);
  EXPECT_FALSE(image_b64->empty());

  std::optional<std::vector<uint8_t>> decoded_bytes =
      base::Base64Decode(*image_b64);
  ASSERT_TRUE(decoded_bytes.has_value());
  ASSERT_FALSE(decoded_bytes->empty());

  // TODO(b/438825957): Also verify the captured pixels once CopyFromSurface
  // screenshot output is reliable in browser tests; it currently reads back
  // black on Linux and Windows bots.
  SkBitmap bitmap = gfx::JPEGCodec::Decode(*decoded_bytes);
  EXPECT_FALSE(bitmap.isNull());
  EXPECT_GT(bitmap.width(), 0);
  EXPECT_GT(bitmap.height(), 0);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, GetScreenshotNoActiveTab) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  Profile* test_profile = profile();
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      test_profile, ProfileKeepAliveOrigin::kBrowserWindow);
  CloseBrowserSynchronously(browser());

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "get_screenshot";
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response, actor::mojom::ActionResultCode::kTabWentAway);

  SetBrowser(CreateBrowser(test_profile));
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, GetScreenshotDisallowedUrl) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(content::NavigateToURL(web_contents(), GURL("chrome://version")));

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "get_screenshot";
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response, actor::mojom::ActionResultCode::kUrlBlocked);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       GetScreenshotBlockedBySafetyList) {
  actor::SetSafetyListsForTesting(actor::SafetyListManager::GetInstance(), R"({
    "navigation_blocked": [
      { "from": "*", "to": "[*.]blocked.example.com" }
    ]
  })");

  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("blocked.example.com",
                                                          "/title1.html")));

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "get_screenshot";
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response, actor::mojom::ActionResultCode::kUrlBlocked);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       GetScreenshotThrottledWhenConcurrent) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  content::WaitForCopyableViewInWebContents(web_contents());

  base::test::TestFuture<ToolResponse> first_future;
  base::test::TestFuture<ToolResponse> second_future;

  ToolRequest first_request;
  first_request.name = "get_screenshot";
  session_controller->ProcessToolCall(std::move(first_request),
                                      first_future.GetCallback());

  ToolRequest second_request;
  second_request.name = "get_screenshot";
  session_controller->ProcessToolCall(std::move(second_request),
                                      second_future.GetCallback());

  ToolResponse second_response = second_future.Take();
  EXPECT_TOOL_ERROR(
      second_response,
      actor::mojom::ActionResultCode::kScreenshotCaptureThrottled);

  ToolResponse first_response = first_future.Take();
  EXPECT_TRUE(first_response.Ok());
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       GetScreenshotTabClosedDuringUrlCheck) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  content::WaitForCopyableViewInWebContents(web_contents());

  Profile* test_profile = profile();
  ScopedKeepAlive keep_alive(KeepAliveOrigin::BROWSER,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      test_profile, ProfileKeepAliveOrigin::kBrowserWindow);

  base::WaitableEvent allow_safety_check;
  actor::SafetyListManager::GetInstance()->SetLoadSafetyListsClosure(
      base::BindLambdaForTesting(
          [&allow_safety_check]() -> std::optional<std::string> {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
            allow_safety_check.Wait();
            return "{}";
          }));

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "get_screenshot";
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  CloseBrowserSynchronously(browser());

  allow_safety_check.Signal();

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response, actor::mojom::ActionResultCode::kTabWentAway);

  SetBrowser(CreateBrowser(test_profile));
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       GetScreenshotNavigatedAwayDuringUrlCheck) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  content::WaitForCopyableViewInWebContents(web_contents());

  base::WaitableEvent allow_safety_check;
  actor::SafetyListManager::GetInstance()->SetLoadSafetyListsClosure(
      base::BindLambdaForTesting(
          [&allow_safety_check]() -> std::optional<std::string> {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
            allow_safety_check.Wait();
            return "{}";
          }));

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "get_screenshot";
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title2.html")));

  allow_safety_check.Signal();

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kScreenshotCaptureFailed);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, GetScreenshotCrashedTab) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  content::WaitForCopyableViewInWebContents(web_contents());

  base::WaitableEvent allow_safety_check;
  actor::SafetyListManager::GetInstance()->SetLoadSafetyListsClosure(
      base::BindLambdaForTesting(
          [&allow_safety_check]() -> std::optional<std::string> {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
            allow_safety_check.Wait();
            return "{}";
          }));

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "get_screenshot";
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  // Use `CrashTab()` (`RESULT_CODE_KILLED`) rather than `Shutdown(0)`: on
  // Windows a zero exit code counts as a normal exit, so `IsCrashed()` would
  // stay false.
  content::CrashTab(web_contents());
  ASSERT_TRUE(web_contents()->IsCrashed());

  allow_safety_check.Signal();

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response, actor::mojom::ActionResultCode::kRendererCrashed);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       GetScreenshotPageChangedDuringCapture) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  content::WaitForCopyableViewInWebContents(web_contents());

  const GURL target_url =
      embedded_https_test_server().GetURL("example.com", "/title2.html");
  content::TestNavigationManager nav_manager(web_contents(), target_url);

  content::NavigationController::LoadURLParams params(target_url);
  web_contents()->GetController().LoadURLWithParams(params);
  ASSERT_TRUE(nav_manager.WaitForResponse());

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "get_screenshot";
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  nav_manager.ResumeNavigation();
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(
      response, actor::mojom::ActionResultCode::kScreenshotCapturePageChanged);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       GetScreenshotTabClosedDuringCapture) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("example.com", "/title1.html")));
  content::WaitForCopyableViewInWebContents(web_contents());

  // Open a background tab so closing the first does not close the browser
  // window.
  tabs::TabHandle captured_tab = active_tab()->GetHandle();
  tabs::TabInterface* background_tab = tab_list()->OpenTab(
      embedded_https_test_server().GetURL("example.com", "/title2.html"),
      /*index=*/-1, /*foreground=*/false);
  ASSERT_TRUE(background_tab);
  ASSERT_TRUE(content::WaitForLoadStop(background_tab->GetContents()));
  ASSERT_EQ(tab_list()->GetTabCount(), 2);
  ASSERT_EQ(active_tab()->GetHandle(), captured_tab);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name = "get_screenshot";
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  // Close the active tab while screenshot capture is in flight.
  tab_list()->CloseTab(captured_tab);

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response, actor::mojom::ActionResultCode::kTabWentAway);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest,
                       OpenGeminiPanelMissingPrompt) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::PromptGeminiPanelToolRequest::kModelFacingName);
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ToolControllerBrowserTest, OpenGeminiPanelEmptyPrompt) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  base::test::TestFuture<ToolResponse> future;
  ToolRequest tool_request;
  tool_request.name =
      std::string(actor::PromptGeminiPanelToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::PromptGeminiPanelToolRequest::kPromptParam,
                             "");
  session_controller->ProcessToolCall(std::move(tool_request),
                                      future.GetCallback());

  ToolResponse response = future.Take();
  EXPECT_TOOL_ERROR(response,
                    actor::mojom::ActionResultCode::kArgumentsInvalid);
}

#if !BUILDFLAG(IS_ANDROID)
using ToolControllerGeminiPanelBrowserTest =
    glic::GlicBrowserTestMixin<TtcCoreBrowserTestBase>;

IN_PROC_BROWSER_TEST_F(ToolControllerGeminiPanelBrowserTest,
                       OpenAndCloseGeminiPanel) {
  ttc_service().StartSession();
  auto* session_controller = ttc_service().session_controller();
  ASSERT_TRUE(session_controller);

  auto* glic_service = glic::GlicKeyedService::Get(profile());
  ASSERT_TRUE(glic_service);
  ASSERT_FALSE(glic_service->instance_coordinator().IsPanelShowingForBrowser(
      *GetBrowser()));

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::PromptGeminiPanelToolRequest::kModelFacingName);
    tool_request.arguments.Set(
        actor::PromptGeminiPanelToolRequest::kPromptParam,
        "Generate an image of a cat");
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
  }

  EXPECT_TRUE(glic_service->instance_coordinator().IsPanelShowingForBrowser(
      *GetBrowser()));

  {
    base::test::TestFuture<ToolResponse> future;
    ToolRequest tool_request;
    tool_request.name =
        std::string(actor::CloseGeminiPanelToolRequest::kModelFacingName);
    session_controller->ProcessToolCall(std::move(tool_request),
                                        future.GetCallback());
    EXPECT_TRUE(future.Take().Ok());
  }

  EXPECT_FALSE(glic_service->instance_coordinator().IsPanelShowingForBrowser(
      *GetBrowser()));
}
#endif  // !BUILDFLAG(IS_ANDROID)

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
      actor::ToolId::kOpenUrl,
      actor::ToolId::kPerformSearch,
      actor::ToolId::kGoBack,
      actor::ToolId::kGoForward,
      actor::ToolId::kReloadPage,
      actor::ToolId::kFindAndHighlight,
      actor::ToolId::kPlayVideo,
      actor::ToolId::kPauseVideo,
      actor::ToolId::kSeekToTimestamp,
      actor::ToolId::kTranslatePage,
      actor::ToolId::kCloseTab,
      actor::ToolId::kSwitchTab,
      actor::ToolId::kEnterFullscreen,
      actor::ToolId::kExitFullscreen,
      actor::ToolId::kOpenKnownPage,
      actor::ToolId::kPromptGeminiPanel,
      actor::ToolId::kCloseGeminiPanel,
  };

  std::vector<const actor::ToolDefinition*> expected_tools =
      actor_service->tool_registry().GetToolsByIds(kExpectedToolIds);
  ASSERT_EQ(expected_tools.size(), kExpectedToolIds.size());

  constexpr size_t kNumLegacyTools = 5;
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

  const ToolDefinition& scroll_page = tools[expected_tools.size() + 3];
  EXPECT_EQ(scroll_page.name, "scroll_page");
  EXPECT_FALSE(scroll_page.description.empty());
  EXPECT_EQ(scroll_page.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(scroll_page.verbalization,
            ToolDefinition::Verbalization::kSilentAction);

  const base::DictValue& scroll_schema = scroll_page.parameters_json_schema;
  const std::string* scroll_schema_type = scroll_schema.FindString("type");
  ASSERT_TRUE(scroll_schema_type);
  EXPECT_EQ(*scroll_schema_type, "object");

  const std::string* scroll_granularity_type =
      scroll_schema.FindStringByDottedPath("properties.granularity.type");
  ASSERT_TRUE(scroll_granularity_type);
  EXPECT_EQ(*scroll_granularity_type, "string");
  const base::ListValue* scroll_granularity_enum =
      scroll_schema.FindListByDottedPath("properties.granularity.enum");
  ASSERT_TRUE(scroll_granularity_enum);
  EXPECT_EQ(*scroll_granularity_enum,
            base::ListValue().Append("page").Append("document"));

  const std::string* scroll_direction_type =
      scroll_schema.FindStringByDottedPath("properties.direction.type");
  ASSERT_TRUE(scroll_direction_type);
  EXPECT_EQ(*scroll_direction_type, "string");
  const base::ListValue* scroll_direction_enum =
      scroll_schema.FindListByDottedPath("properties.direction.enum");
  ASSERT_TRUE(scroll_direction_enum);
  EXPECT_EQ(*scroll_direction_enum,
            base::ListValue().Append("up").Append("down").Append("left").Append(
                "right"));

  const std::string* scroll_node_id_type =
      scroll_schema.FindStringByDottedPath("properties.node_id.type");
  ASSERT_TRUE(scroll_node_id_type);
  EXPECT_EQ(*scroll_node_id_type, "integer");

  const base::ListValue* scroll_required = scroll_schema.FindList("required");
  ASSERT_TRUE(scroll_required);
  EXPECT_EQ(*scroll_required, base::ListValue()
                                  .Append("granularity")
                                  .Append("direction")
                                  .Append("node_id"));

  const ToolDefinition& get_screenshot = tools[expected_tools.size() + 4];
  EXPECT_EQ(get_screenshot.name, "get_screenshot");
  EXPECT_FALSE(get_screenshot.description.empty());
  EXPECT_EQ(get_screenshot.behavior, ToolDefinition::Behavior::kBlocking);
  EXPECT_EQ(get_screenshot.verbalization,
            ToolDefinition::Verbalization::kSilentAction);
  EXPECT_TRUE(get_screenshot.parameters_json_schema.empty());
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
  tool_request.name = std::string(actor::OpenUrlToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::OpenUrlToolRequest::kUrlParam,
                             embedded_https_test_server()
                                 .GetURL("example.com", "/title1.html")
                                 .spec());
  tool_request.arguments.Set(actor::OpenUrlToolRequest::kNewTabParam, false);
  base::test::TestFuture<ToolResponse> future;
  ttc_service().session_controller()->ProcessToolCall(std::move(tool_request),
                                                      future.GetCallback());
  ASSERT_TRUE(future.Take().Ok());
  ttc_service().EndSession();
  EXPECT_EQ(last_task_states[acting_task_id],
            actor::ActorTask::State::kFinished);
}

#if !BUILDFLAG(IS_ANDROID)
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
  tool_request.name = std::string(actor::OpenUrlToolRequest::kModelFacingName);
  tool_request.arguments.Set(actor::OpenUrlToolRequest::kUrlParam, url.spec());
  tool_request.arguments.Set(actor::OpenUrlToolRequest::kNewTabParam, false);
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
      actor::ActorSurfaceHandle::From(tab->GetHandle()),
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
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

}  // namespace ttc
