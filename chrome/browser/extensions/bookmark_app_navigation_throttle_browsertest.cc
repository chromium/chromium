// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/bookmark_app_navigation_browsertest.h"
#include "chrome/browser/extensions/bookmark_app_navigation_throttle_utils.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/constants.h"
#include "net/base/escape.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_fetcher.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace test {

enum class StartIn {
  BROWSER,
  APP,
};

enum class WindowAccessResult {
  CAN_ACCESS,
  CANNOT_ACCESS,
  // Used for when there was an unexpected issue when accessing the other window
  // e.g. there is no other window, or the other window's URL does not match the
  // expected URL.
  OTHER,
};

namespace {

const char kTextPlainEncType[] = "text/plain";

const char kQueryParam[] = "test=";
const char kQueryParamName[] = "test";

// On macOS the Meta key is used as a modifier instead of Control.
#if defined(OS_MACOSX)
constexpr int kCtrlOrMeta = blink::WebInputEvent::kMetaKey;
#else
constexpr int kCtrlOrMeta = blink::WebInputEvent::kControlKey;
#endif

// Subclass of TestNavigationObserver that saves extra information about the
// navigation and watches all navigations to |target_url|.
class BookmarkAppNavigationObserver : public content::TestNavigationObserver {
 public:
  // Creates an observer that watches navigations to |target_url| on all
  // existing and newly added WebContents.
  explicit BookmarkAppNavigationObserver(const GURL& target_url)
      : content::TestNavigationObserver(target_url),
        last_navigation_is_post_(false) {
    WatchExistingWebContents();
    StartWatchingNewWebContents();
  }

  bool last_navigation_is_post() const { return last_navigation_is_post_; }

  const net::HttpRequestHeaders& last_request_headers() const {
    return last_request_headers_;
  }

  const scoped_refptr<network::ResourceRequestBody>&
  last_resource_request_body() const {
    return last_resource_request_body_;
  }

 private:
  void OnDidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    last_navigation_is_post_ = navigation_handle->IsPost();
    last_request_headers_ = navigation_handle->GetRequestHeaders();
    last_resource_request_body_ = navigation_handle->GetResourceRequestBody();
    content::TestNavigationObserver::OnDidFinishNavigation(navigation_handle);
  }

  // True if the last navigation was a post.
  bool last_navigation_is_post_;

  // The request headers of the last navigation.
  net::HttpRequestHeaders last_request_headers_;

  // The request body of the last navigation if it was a post request.
  scoped_refptr<network::ResourceRequestBody> last_resource_request_body_;
};

void ExpectNavigationResultHistogramEquals(
    const base::HistogramTester& histogram_tester,
    const std::vector<std::pair<BookmarkAppNavigationThrottleResult,
                                base::HistogramBase::Count>>& expected_counts) {
  std::vector<base::Bucket> expected_bucket_counts;
  for (const auto& pair : expected_counts) {
    expected_bucket_counts.push_back(base::Bucket(
        static_cast<base::HistogramBase::Sample>(pair.first), pair.second));
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Extensions.BookmarkApp.NavigationResult"),
      testing::UnorderedElementsAreArray(expected_bucket_counts));
}

// When an app is launched, whether it's in response to a navigation or click
// in a launch surface e.g. App Shelf, the first navigation in the app is
// an AUTO_BOOKMARK navigation.
std::pair<BookmarkAppNavigationThrottleResult, base::HistogramBase::Count>
GetAppLaunchedEntry() {
  return {BookmarkAppNavigationThrottleResult::kProceedTransitionAutoBookmark,
          1};
}

std::string CreateClientRedirect(const GURL& target_url) {
  const char* const kClientRedirectBase = "/client-redirect?";
  return kClientRedirectBase +
         net::EscapeQueryParamValue(target_url.spec(), false);
}

// Inserts an iframe in the main frame of |web_contents|.
void InsertIFrame(content::WebContents* web_contents) {
  ASSERT_TRUE(
      content::ExecuteScript(web_contents,
                             "let iframe = document.createElement('iframe');"
                             "document.body.appendChild(iframe);"));
}

void ExecuteContextMenuLinkCommandAndWait(content::WebContents* web_contents,
                                          const GURL& target_url,
                                          int command_id) {
  auto observer =
      BookmarkAppNavigationBrowserTest::GetTestNavigationObserver(target_url);
  content::ContextMenuParams params;
  params.page_url = web_contents->GetLastCommittedURL();
  params.link_url = target_url;
  TestRenderViewContextMenu menu(web_contents->GetMainFrame(), params);
  menu.Init();
  menu.ExecuteCommand(command_id, 0 /* event_flags */);
  observer->WaitForNavigationFinished();
}

void OpenPopupAndWait(content::WebContents* web_contents,
                      const GURL& target_url) {
  auto observer =
      BookmarkAppNavigationBrowserTest::GetTestNavigationObserver(target_url);
  const std::string script = base::StringPrintf(
      "(() => {"
      "  window.openedWindow = window.open('%s', '_blank', 'toolbar=no');"
      "})();",
      target_url.spec().c_str());
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
  observer->WaitForNavigationFinished();
}

// Calls window.open() with |target_url| on the main frame of |web_contents|.
// Returns once the new window has navigated to |target_url|.
void WindowOpenAndWait(content::WebContents* web_contents,
                       const GURL& target_url) {
  auto observer =
      BookmarkAppNavigationBrowserTest::GetTestNavigationObserver(target_url);
  const std::string script = base::StringPrintf(
      "(() => {"
      "  window.openedWindow = window.open('%s');"
      "})();",
      target_url.spec().c_str());
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
  observer->WaitForNavigationFinished();
}

// Calls window.open() with |target_url| on the main frame of |web_contents|.
// Returns true if the resulting child window is allowed to access members of
// its opener.
WindowAccessResult CanChildWindowAccessOpener(
    content::WebContents* web_contents,
    const GURL& target_url) {
  WindowOpenAndWait(web_contents, target_url);

  content::WebContents* new_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();

  const std::string script = base::StringPrintf(
      "(() => {"
      "  const [CAN_ACCESS, CANNOT_ACCESS, OTHER] = [0, 1, 2];"
      "  let result = OTHER;"
      "  try {"
      "    if (window.opener.location.href === '%s')"
      "      result = CAN_ACCESS;"
      "  } catch (e) {"
      "    if (e.name === 'SecurityError')"
      "      result = CANNOT_ACCESS;"
      "  }"
      "  window.domAutomationController.send(result);"
      "})();",
      web_contents->GetLastCommittedURL().spec().c_str());

  int access_result;
  CHECK(content::ExecuteScriptAndExtractInt(new_contents, script,
                                            &access_result));

  switch (access_result) {
    case 0:
      return WindowAccessResult::CAN_ACCESS;
    case 1:
      return WindowAccessResult::CANNOT_ACCESS;
    case 2:
      return WindowAccessResult::OTHER;
    default:
      NOTREACHED();
      return WindowAccessResult::OTHER;
  }
}

// Adds a query parameter to |base_url|.
GURL AddTestQueryParam(const GURL& base_url) {
  GURL::Replacements replacements;
  std::string query(kQueryParam);
  replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
  return base_url.ReplaceComponents(replacements);
}

// Creates a <form> element with a |target_url| action and |method| method. Adds
// the form to the DOM with a button and clicks the button. Returns once
// |target_url| has been loaded.
//
// If |method| is net::URLFetcher::RequestType::GET, |target_url| should contain
// an empty query string, since that URL will be loaded when submitting the form
// e.g. "https://www.example.com/?".
void SubmitFormAndWait(content::WebContents* web_contents,
                       const GURL& target_url,
                       net::URLFetcher::RequestType method) {
  bool is_post = true;
  if (method == net::URLFetcher::RequestType::GET) {
    is_post = false;
    ASSERT_TRUE(target_url.has_query());
    ASSERT_EQ(kQueryParam, target_url.query());
  }

  BookmarkAppNavigationObserver observer(target_url);
  std::string script = base::StringPrintf(
      "(() => {"
      "const form = document.createElement('form');"
      "form.action = '%s';"
      "form.method = '%s';"
      "form.enctype = '%s';"
      "const input = document.createElement('input');"
      "input.name = '%s';"
      "form.appendChild(input);"
      "const button = document.createElement('input');"
      "button.type = 'submit';"
      "form.appendChild(button);"
      "document.body.appendChild(form);"
      "button.dispatchEvent(new MouseEvent('click', {'view': window}));"
      "})();",
      target_url.spec().c_str(),
      method == net::URLFetcher::RequestType::POST ? "post" : "get",
      kTextPlainEncType, kQueryParamName);
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
  observer.WaitForNavigationFinished();

  EXPECT_EQ(is_post, observer.last_navigation_is_post());
  if (is_post) {
    const net::HttpRequestHeaders& headers = observer.last_request_headers();
    std::string post_content_type;
    headers.GetHeader(net::HttpRequestHeaders::kContentType,
                      &post_content_type);
    EXPECT_EQ(kTextPlainEncType, post_content_type);

    const std::vector<network::DataElement>* elements =
        observer.last_resource_request_body()->elements();
    EXPECT_EQ(1u, elements->size());
    const auto& element = elements->front();

    // The text/plain enconding algorithm appends "\r\n".
    EXPECT_EQ(std::string(kQueryParam) + "\r\n",
              std::string(element.bytes(), element.length()));
  }
}

// Uses |params| to navigate to a URL. Blocks until the URL is loaded.
void NavigateToURLAndWait(NavigateParams* params) {
  auto observer =
      BookmarkAppNavigationBrowserTest::GetTestNavigationObserver(params->url);
  ui_test_utils::NavigateToURL(params);
  observer->WaitForNavigationFinished();
}

// Wrapper so that we can use base::BindOnce with NavigateToURL.
void NavigateToURLWrapper(NavigateParams* params) {
  ui_test_utils::NavigateToURL(params);
}

}  // namespace

// Tests for the behavior when the kDesktopPWAsLinkCapturing flag is on.
class BookmarkAppNavigationThrottleExperimentalBrowserTest
    : public BookmarkAppNavigationBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAWindowing, features::kDesktopPWAsLinkCapturing},
        {});
    BookmarkAppNavigationBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the result is the same regardless of the 'rel' attribute of links.
class BookmarkAppNavigationThrottleExperimentalLinkBrowserTest
    : public BookmarkAppNavigationThrottleExperimentalBrowserTest,
      public ::testing::WithParamInterface<std::string> {};

// Tests that navigating to the Web App's app_url doesn't open a new window
// if features::kDesktopPWAsLinkCapturing is disabled before installing the app.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       FeatureDisable_BeforeInstall) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kDesktopPWAsLinkCapturing});
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  TestTabActionDoesNotOpenAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that navigating to the Web App's app_url doesn't open a new window
// if features::kDesktopPWAWindowing is disabled after installing the app.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       FeatureDisable_AfterInstall) {
  InstallTestBookmarkApp();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kDesktopPWAsLinkCapturing});
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  TestTabActionDoesNotOpenAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that most transition types for navigations to in-scope or
// out-of-scope URLs do not result in new app windows.
class BookmarkAppNavigationThrottleExperimentalTransitionBrowserTest
    : public BookmarkAppNavigationThrottleExperimentalBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, ui::PageTransition>> {};

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleExperimentalTransitionBrowserTest,
    testing::Combine(
        testing::Values(
            BookmarkAppNavigationBrowserTest::GetInScopeUrlPath(),
            BookmarkAppNavigationBrowserTest::GetOutOfScopeUrlPath()),
        testing::Range(ui::PAGE_TRANSITION_FIRST,
                       ui::PAGE_TRANSITION_LAST_CORE)));

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
#define MAYBE_MainFrameNavigations DISABLED_MainFrameNavigations
#else
#define MAYBE_MainFrameNavigations MainFrameNavigations
#endif
IN_PROC_BROWSER_TEST_P(
    BookmarkAppNavigationThrottleExperimentalTransitionBrowserTest,
    MAYBE_MainFrameNavigations) {
  InstallTestBookmarkApp();

  GURL target_url =
      https_server().GetURL(GetAppUrlHost(), std::get<0>(GetParam()));
  ui::PageTransition transition = std::get<1>(GetParam());
  NavigateParams params(browser(), target_url, transition);

  if (!ui::PageTransitionIsMainFrame(transition)) {
    // Subframe navigations require a different setup. See
    // BookmarkAppNavigationThrottleExperimentalBrowserTest.SubframeNavigation.
    return;
  }

  if (ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK, transition) &&
      target_url ==
          https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath())) {
    TestTabActionOpensAppWindow(target_url,
                                base::BindOnce(&NavigateToURLAndWait, &params));
  } else {
    TestTabActionDoesNotOpenAppWindow(
        target_url, base::BindOnce(&NavigateToURLAndWait, &params));
  }
}

// Tests that navigations in subframes don't open new app windows.
//
// The transition type for subframe navigations is not set until
// after NavigationThrottles run. Because of this, our
// BookmarkAppNavigationThrottle will not see the transition type as
// PAGE_TRANSITION_AUTO_SUBFRAME/PAGE_TRANSITION_MANUAL_SUBFRAME, even though,
// by the end of the navigation, the transition type is
// PAGE_TRANSITION_AUTO_SUBFRAME/PAGE_TRANSITON_MANUAL_SUBFRAME.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       AutoSubframeNavigation) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  InsertIFrame(initial_tab);

  content::RenderFrameHost* iframe = GetIFrame(initial_tab);
  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());

  NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_LINK);
  params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
  content::TestFrameNavigationObserver observer(iframe);
  TestIFrameActionDoesNotOpenAppWindow(
      app_url, base::BindOnce(&NavigateToURLWrapper, &params));

  ASSERT_TRUE(ui::PageTransitionCoreTypeIs(observer.transition_type(),
                                           ui::PAGE_TRANSITION_AUTO_SUBFRAME));

  // Subframe navigations are completely ignored, so there should be no change
  // in the histogram.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       ManualSubframeNavigation) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  InsertIFrame(initial_tab);

  {
    // Navigate the iframe once, so that the next navigation is a
    // MANUAL_SUBFRAME navigation.
    content::RenderFrameHost* iframe = GetIFrame(initial_tab);
    NavigateParams params(browser(), GetLaunchingPageURL(),
                          ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
    ASSERT_TRUE(TestIFrameActionDoesNotOpenAppWindow(
        GetLaunchingPageURL(), base::BindOnce(&NavigateToURLWrapper, &params)));
  }

  content::RenderFrameHost* iframe = GetIFrame(initial_tab);
  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());

  NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_LINK);
  params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
  content::TestFrameNavigationObserver observer(iframe);
  TestIFrameActionDoesNotOpenAppWindow(
      app_url, base::BindOnce(&NavigateToURLWrapper, &params));

  ASSERT_TRUE(ui::PageTransitionCoreTypeIs(
      observer.transition_type(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));

  // Subframe navigations are completely ignored, so there should be no change
  // in the histogram.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that pasting an in-scope URL into the address bar and navigating to it,
// does not open an app window.
// https://crbug.com/782004
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       LinkNavigationFromAddressBar) {
  InstallTestBookmarkApp();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  // Fake a navigation with a TRANSITION_LINK core type and a
  // TRANSITION_FROM_ADDRESS_BAR qualifier. This matches the transition that
  // results from pasting a URL into the address and navigating to it.
  NavigateParams params(
      browser(), app_url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
      app_url, base::BindOnce(&NavigateToURLWrapper, &params)));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kProceedTransitionFromAddressBar,
        1}});
}

// Tests that going back to an in-scope URL does not open a new app window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       BackNavigation) {
  InstallTestBookmarkApp();
  NavigateToTestAppURL();

  // Navigate to an in-scope URL to generate a link navigation that didn't
  // get intercepted. The navigation won't get intercepted because the target
  // URL is in-scope of the app for the current URL.
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
      in_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     in_scope_url, LinkTarget::SELF, "" /* rel */)));

  // Navigate to an out-of-scope URL.
  {
    const GURL out_of_scope_url =
        https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
    NavigateParams params(browser(), out_of_scope_url,
                          ui::PAGE_TRANSITION_TYPED);
    ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
        out_of_scope_url, base::BindOnce(&NavigateToURLWrapper, &params)));
  }

  base::HistogramTester scoped_histogram;
  TestTabActionDoesNotOpenAppWindow(
      in_scope_url,
      base::BindOnce(
          [](content::WebContents* web_contents, const GURL& in_scope_url) {
            auto observer = GetTestNavigationObserver(in_scope_url);
            web_contents->GetController().GoBack();
            observer->WaitForNavigationFinished();
          },
          browser()->tab_strip_model()->GetActiveWebContents(), in_scope_url));

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kProceedTransitionForwardBack,
        1}});
}

// Tests that clicking a link to an app that launches in a tab does not open a
// a new app window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       TabApp) {
  InstallTestBookmarkApp();

  // Set a pref indicating that the user wants to launch in a regular tab.
  extensions::SetLaunchType(browser()->profile(), test_bookmark_app()->id(),
                            extensions::LAUNCH_TYPE_REGULAR);

  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  TestTabActionDoesNotOpenAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::SELF, GetParam()));

  // Non-windowed apps are not considered when looking for a target app, so it's
  // as if there was no app installed for the URL.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that clicking a link to an app that isn't locally installed does not
// open a new app window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       NotLocallyInstalledApp) {
  InstallTestBookmarkApp();

  // Part of the installation process (setting that this is a locally installed
  // app) runs asynchronously. Wait for that to complete before setting locally
  // installed to false.
  base::RunLoop().RunUntilIdle();
  SetBookmarkAppIsLocallyInstalled(browser()->profile(), test_bookmark_app(),
                                   false /* is_locally_installed */);

  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  TestTabActionDoesNotOpenAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::SELF, GetParam()));

  // Non-locally installed apps are not considered when looking for a target
  // app, so it's as if there was no app installed for the URL.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that clicking a link with target="_self" to the app's app_url opens the
// Bookmark App.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       AppUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  TestTabActionOpensAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a link with target="_blank" to the app's app_url opens
// the Bookmark App.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       AppUrlBlank) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  TestTabActionOpensAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::BLANK, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kDeferMovingContentsToNewAppWindow,
        1}});
}

// Tests that Ctrl + Clicking a link to the app's app_url opens a new background
// tab and not the app.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       AppUrlCtrlClick) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(GetAppUrlPath());
  TestTabActionOpensBackgroundTab(
      app_url,
      base::BindOnce(&ClickLinkWithModifiersAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::SELF, GetParam(), kCtrlOrMeta));

  ExpectNavigationResultHistogramEquals(
      global_histogram(), {{BookmarkAppNavigationThrottleResult::
                                kProceedDispositionNewBackgroundTab,
                            1}});
}

// Tests that clicking a link with target="_self" and for which the server
// redirects to the app's app_url opens the Bookmark App.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       ServerRedirectToAppUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  const GURL redirecting_url = https_server().GetURL(
      GetLaunchingPageHost(), CreateServerRedirect(app_url));
  TestTabActionOpensAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWaitForURL,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     redirecting_url, app_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a link with target="_blank" and for which the server
// redirects to the app's app_url opens the Bookmark App.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       ServerRedirectToAppUrlBlank) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  const GURL redirecting_url = https_server().GetURL(
      GetLaunchingPageHost(), CreateServerRedirect(app_url));
  TestTabActionOpensAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWaitForURL,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     redirecting_url, app_url, LinkTarget::BLANK, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kDeferMovingContentsToNewAppWindow,
        1}});
}

// Tests that clicking a link with target="_self" and for which the client
// redirects to the app's app_url opens the Bookmark App. The initial tab will
// be left on the redirecting URL.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       ClientRedirectToAppUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL initial_url = initial_tab->GetLastCommittedURL();
  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  const GURL redirecting_url = https_server().GetURL(
      GetLaunchingPageHost(), CreateClientRedirect(app_url));
  ClickLinkAndWaitForURL(browser()->tab_strip_model()->GetActiveWebContents(),
                         redirecting_url, app_url, LinkTarget::SELF,
                         GetParam());

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_NE(browser(), chrome::FindLastActive());

  EXPECT_EQ(redirecting_url, initial_tab->GetLastCommittedURL());
  EXPECT_EQ(app_url, chrome::FindLastActive()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL());

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a link with target="_blank" and for which the client
// redirects to the app's app_url opens the Bookmark App. The new tab will be
// left on the redirecting URL.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       ClientRedirectToAppUrlBlank) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL initial_url = initial_tab->GetLastCommittedURL();
  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  const GURL redirecting_url = https_server().GetURL(
      GetLaunchingPageHost(), CreateClientRedirect(app_url));
  ClickLinkAndWaitForURL(browser()->tab_strip_model()->GetActiveWebContents(),
                         redirecting_url, app_url, LinkTarget::BLANK,
                         GetParam());

  EXPECT_EQ(++num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_NE(browser(), chrome::FindLastActive());

  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_tab, initial_tab);

  EXPECT_EQ(redirecting_url, new_tab->GetLastCommittedURL());
  EXPECT_EQ(app_url, chrome::FindLastActive()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL());

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a link with target="_self" to a URL in the Web App's
// scope opens a new browser window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       InScopeUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  TestTabActionOpensAppWindow(
      in_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     in_scope_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a link with target="_self" to a URL out of the Web App's
// scope but with the same origin doesn't open a new browser window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       OutOfScopeUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     out_of_scope_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that prerender links don't open the app.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       PrerenderLinks) {
  prerender::PrerenderManager::SetMode(
      prerender::PrerenderManager::PRERENDER_MODE_ENABLED);

  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  TestTabActionDoesNotNavigateOrOpenAppWindow(base::BindOnce(
      [](content::WebContents* web_contents, const GURL& target_url) {
        std::string script = base::StringPrintf(
            "(() => {"
            "const prerender_link = document.createElement('link');"
            "prerender_link.rel = 'prerender';"
            "prerender_link.href = '%s';"
            "prerender_link.addEventListener('webkitprerenderstop',"
            "() => window.domAutomationController.send(true));"
            "document.body.appendChild(prerender_link);"
            "})();",
            target_url.spec().c_str());
        bool result;
        ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, script,
                                                         &result));
        ASSERT_TRUE(result);
      },
      browser()->tab_strip_model()->GetActiveWebContents(),
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath())));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelPrerenderContents, 1}});
}

// Tests fetch calls don't open a new App window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       Fetch) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  TestTabActionDoesNotNavigateOrOpenAppWindow(base::BindOnce(
      [](content::WebContents* web_contents, const GURL& target_url) {
        std::string script = base::StringPrintf(
            "(() => {"
            "fetch('%s').then(response => {"
            "  window.domAutomationController.send(response.ok);"
            "});"
            "})();",
            target_url.spec().c_str());
        bool result;
        ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, script,
                                                         &result));
        ASSERT_TRUE(result);
      },
      browser()->tab_strip_model()->GetActiveWebContents(),
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath())));

  // Fetch requests don't go through our NavigationHandle.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that clicking "Open link in new tab" to an in-scope URL opens a new
// tab in the background.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       ContextMenuNewTab) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  TestTabActionOpensBackgroundTab(
      app_url,
      base::BindOnce(&ExecuteContextMenuLinkCommandAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kProceedStartedFromContextMenu,
        1}});
}

// Tests that clicking "Open link in new window" to an in-scope URL opens a new
// window in the foreground.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       ContextMenuNewWindow) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  TestTabActionOpensForegroundWindow(
      app_url,
      base::BindOnce(&ExecuteContextMenuLinkCommandAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kProceedStartedFromContextMenu,
        1}});
}

// Tests that clicking "Open link in new tab" in an app to an in-scope URL opens
// a new foreground tab in a regular browser window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       InAppContextMenuNewTab) {
  InstallTestBookmarkApp();
  Browser* app_browser = OpenTestBookmarkApp();

  base::HistogramTester scoped_histogram;
  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  TestAppActionOpensForegroundTab(
      app_browser, app_url,
      base::BindOnce(&ExecuteContextMenuLinkCommandAndWait,
                     app_browser->tab_strip_model()->GetActiveWebContents(),
                     app_url, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kProceedStartedFromContextMenu,
        1}});
}

// Tests that clicking "Open link in incognito window" to an in-scope URL opens
// an incognito window and not an app window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       OpenInIncognito) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  size_t num_browsers = chrome::GetBrowserCount(profile());
  int num_tabs = browser()->tab_strip_model()->count();
  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL initial_url = initial_tab->GetLastCommittedURL();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  auto observer = GetTestNavigationObserver(in_scope_url);
  content::ContextMenuParams params;
  params.page_url = initial_url;
  params.link_url = in_scope_url;
  TestRenderViewContextMenu menu(initial_tab->GetMainFrame(), params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
                      0 /* event_flags */);
  observer->WaitForNavigationFinished();

  Browser* incognito_browser = chrome::FindLastActive();
  EXPECT_EQ(incognito_browser->profile(), profile()->GetOffTheRecordProfile());
  EXPECT_NE(browser(), incognito_browser);
  EXPECT_EQ(in_scope_url, incognito_browser->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetLastCommittedURL());

  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(initial_tab, browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());

  // Incognito navigations are completely ignored.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that clicking a link to an in-scope URL when in incognito does not open
// an App window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       InScopeUrlIncognito) {
  InstallTestBookmarkApp();
  Browser* incognito_browser = CreateIncognitoBrowser();
  NavigateToLaunchingPage(incognito_browser);

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  TestActionDoesNotOpenAppWindow(
      incognito_browser, in_scope_url,
      base::BindOnce(
          &ClickLinkAndWait,
          incognito_browser->tab_strip_model()->GetActiveWebContents(),
          in_scope_url, LinkTarget::SELF, GetParam()));

  // Incognito navigations are completely ignored.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that clicking a target=_self link from a URL out of the Web App's scope
// but with the same origin to an in-scope URL results in a new App window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       FromOutOfScopeUrlToInScopeUrlSelf) {
  InstallTestBookmarkApp();

  // Navigate to out-of-scope URL. Shouldn't open a new window.
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  NavigateParams params(browser(), out_of_scope_url, ui::PAGE_TRANSITION_TYPED);
  ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url, base::BindOnce(&NavigateToURLWrapper, &params)));

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  TestTabActionOpensAppWindow(
      in_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     in_scope_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a target=_blank link from a URL out of the Web App's
// scope but with the same origin to an in-scope URL results in a new App
// window.
// TODO(crbug.com/837277): Deflake and reenable.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       DISABLED_FromOutOfScopeUrlToInScopeUrlBlank) {
  InstallTestBookmarkApp();

  // Navigate to out-of-scope URL. Shouldn't open a new window.
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  NavigateParams params(browser(), out_of_scope_url, ui::PAGE_TRANSITION_TYPED);
  ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url, base::BindOnce(&NavigateToURLWrapper, &params)));

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  TestTabActionOpensAppWindow(
      in_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     in_scope_url, LinkTarget::BLANK, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kDeferMovingContentsToNewAppWindow,
        1}});
}

// Tests that clicking links inside a website for an installed app doesn't open
// a new browser window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       InWebsiteNavigation) {
  InstallTestBookmarkApp();
  NavigateToTestAppURL();

  base::HistogramTester scoped_histogram;
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  TestTabActionDoesNotOpenAppWindow(
      in_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     in_scope_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kProceedInBrowserSameScope, 1}});
}

class BookmarkAppNavigationThrottleExperimentalWindowOpenBrowserTest
    : public BookmarkAppNavigationThrottleExperimentalBrowserTest,
      public ::testing::WithParamInterface<std::string> {};

// Tests that same-origin or cross-origin apps created with window.open() from
// a regular browser window have an opener.
IN_PROC_BROWSER_TEST_P(
    BookmarkAppNavigationThrottleExperimentalWindowOpenBrowserTest,
    WindowOpenInBrowser) {
  InstallTestBookmarkApp();
  InstallOtherTestBookmarkApp();

  NavigateToTestAppURL();

  // Call window.open() with |target_url|.
  const GURL target_url = https_server().GetURL(GetParam(), GetAppUrlPath());
  TestTabActionOpensAppWindowWithOpener(
      target_url,
      base::BindOnce(&WindowOpenAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     target_url));
}

// Tests that same-origin or cross-origin apps created with window.open() from
// another app window have an opener.
#if defined(OS_MACOSX)
#define MAYBE_WindowOpenInApp DISABLED_WindowOpenInApp
#else
#define MAYBE_WindowOpenInApp WindowOpenInApp
#endif
IN_PROC_BROWSER_TEST_P(
    BookmarkAppNavigationThrottleExperimentalWindowOpenBrowserTest,
    MAYBE_WindowOpenInApp) {
  InstallTestBookmarkApp();
  InstallOtherTestBookmarkApp();

  // Open app window.
  Browser* app_browser = OpenTestBookmarkApp();
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  // Call window.open() with |target_url|.
  const GURL target_url = https_server().GetURL(GetParam(), GetAppUrlPath());
  TestAppActionOpensAppWindowWithOpener(
      app_browser, target_url,
      base::BindOnce(&WindowOpenAndWait, app_web_contents, target_url));
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleExperimentalWindowOpenBrowserTest,
    testing::Values(BookmarkAppNavigationBrowserTest::GetAppUrlHost(),
                    BookmarkAppNavigationBrowserTest::GetOtherAppUrlHost()));

// Tests that a child window can access its opener, when the opener is a regular
// browser tab.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       AccessOpenerBrowserWindowFromChildWindow) {
  InstallTestBookmarkApp();
  InstallOtherTestBookmarkApp();

  NavigateToTestAppURL();

  content::WebContents* current_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  EXPECT_EQ(WindowAccessResult::CAN_ACCESS,
            CanChildWindowAccessOpener(current_tab, app_url));

  const GURL other_app_url =
      https_server().GetURL(GetOtherAppUrlHost(), GetAppUrlPath());
  EXPECT_EQ(WindowAccessResult::CANNOT_ACCESS,
            CanChildWindowAccessOpener(current_tab, other_app_url));
}

// Tests that a child window can access its opener, when the opener is another
// app.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       AccessOpenerAppWindowFromChildWindow) {
  InstallTestBookmarkApp();
  InstallOtherTestBookmarkApp();

  // Open app window.
  Browser* app_browser = OpenTestBookmarkApp();
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  const GURL app_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());
  EXPECT_EQ(WindowAccessResult::CAN_ACCESS,
            CanChildWindowAccessOpener(app_web_contents, app_url));

  const GURL other_app_url =
      https_server().GetURL(GetOtherAppUrlHost(), GetAppUrlPath());
  EXPECT_EQ(WindowAccessResult::CANNOT_ACCESS,
            CanChildWindowAccessOpener(app_web_contents, other_app_url));
}

// Tests that in-browser navigations with all the following characteristics
// don't open a new app window or move the tab:
//  1. Are to in-scope URLs
//  2. Result in a AUTO_BOOKMARK transtion
//  3. Redirect to another in-scope URL.
//
// Clicking on sites in the New Tab Page is one of the ways to trigger this
// type of navigation.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       AutoBookmarkInScopeRedirect) {
  const Extension* app =
      InstallImmediateRedirectingApp(GetAppUrlHost(), GetInScopeUrlPath());

  const GURL app_url = AppLaunchInfo::GetFullLaunchURL(app);
  NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  TestTabActionDoesNotOpenAppWindow(
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath()),
      base::BindOnce(&NavigateToURLWrapper, &params));

  // The first AUTO_BOOKMARK navigation is the one we triggered and the second
  // one is the redirect.
  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kProceedTransitionAutoBookmark,
        2}});
}

// Tests that in-browser navigations with all the following characteristics
// don't open a new app window or move the tab:
//  1. Are to in-scope URLs
//  2. Result in a AUTO_BOOKMARK transtion
//  3. Redirect to an out-of-scope URL.
//
// Clicking on sites in the New Tab Page is one of the ways to trigger this
// type of navigation.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       AutoBookmarkOutOfScopeRedirect) {
  const Extension* app = InstallImmediateRedirectingApp(GetLaunchingPageHost(),
                                                        GetLaunchingPagePath());

  const GURL app_url = AppLaunchInfo::GetFullLaunchURL(app);
  NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  TestTabActionDoesNotOpenAppWindow(
      GetLaunchingPageURL(), base::BindOnce(&NavigateToURLWrapper, &params));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kProceedTransitionAutoBookmark,
        1}});
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));

// Base class for testing the behavior is the same regardless of the
// kDesktopPWAsLinkCapturing feature flag.
class BookmarkAppNavigationThrottleBaseCommonBrowserTest
    : public BookmarkAppNavigationBrowserTest {
 protected:
  void EnableFeaturesForTest(bool should_enable_link_capturing) {
    // These tests expect that navigation to an out of scope URL will open in a
    // new window, however, this behaviour is in the process of being changed
    // (with the flag desktop-pwas-stay-in-window), so until the change is made
    // permanent and these tests are removed, explicitly disable the flag.
    if (should_enable_link_capturing) {
      scoped_feature_list_.InitWithFeatures(
          {features::kDesktopPWAWindowing, features::kDesktopPWAsLinkCapturing},
          {features::kDesktopPWAsStayInWindow});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {features::kDesktopPWAWindowing},
          {features::kDesktopPWAsLinkCapturing,
           features::kDesktopPWAsStayInWindow});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Class for testing the behavior is the same regardless of the
// kDesktopPWAsLinkCapturing feature flag.
class BookmarkAppNavigationThrottleCommonBrowserTest
    : public BookmarkAppNavigationThrottleBaseCommonBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    EnableFeaturesForTest(GetParam());
    BookmarkAppNavigationThrottleBaseCommonBrowserTest::SetUp();
  }
};

// Tests that an app that immediately redirects to an out-of-scope URL opens a
// new foreground tab.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleCommonBrowserTest,
                       ImmediateOutOfScopeRedirect) {
  size_t num_browsers = chrome::GetBrowserCount(profile());
  int num_tabs_browser = browser()->tab_strip_model()->count();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL initial_url = initial_tab->GetLastCommittedURL();

  const Extension* redirecting_app = InstallImmediateRedirectingApp(
      GetLaunchingPageHost(), GetLaunchingPagePath());

  auto observer = GetTestNavigationObserver(GetLaunchingPageURL());
  LaunchAppBrowser(redirecting_app);
  observer->WaitForNavigationFinished();

  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));

  EXPECT_EQ(browser(), chrome::FindLastActive());
  EXPECT_EQ(++num_tabs_browser, browser()->tab_strip_model()->count());

  EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());

  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(initial_tab, new_tab);
  EXPECT_EQ(GetLaunchingPageURL(), new_tab->GetLastCommittedURL());

  // When kDesktopPWAsLinkCapturing is enabled, app launches get histogrammed,
  // but when it's disabled, they don't.
  if (GetParam()) {
    ExpectNavigationResultHistogramEquals(
        global_histogram(), {GetAppLaunchedEntry(),
                             {BookmarkAppNavigationThrottleResult::
                                  kOpenInChromeProceedOutOfScopeLaunch,
                              1}});
  } else {
    ExpectNavigationResultHistogramEquals(
        global_histogram(), {{BookmarkAppNavigationThrottleResult::
                                  kOpenInChromeProceedOutOfScopeLaunch,
                              1}});
  }
}

// Tests that popups to out-of-scope URLs are opened in regular popup windows
// and not in app windows.
// TODO(crbug.com/849163) Times out flakily on MacOS.
#if defined(OS_MACOSX)
#define MAYBE_OutOfScopePopup DISABLED_OutOfScopePopup
#else
#define MAYBE_OutOfScopePopup OutOfScopePopup
#endif
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleCommonBrowserTest,
                       MAYBE_OutOfScopePopup) {
  InstallTestBookmarkApp();
  Browser* app_browser = OpenTestBookmarkApp();

  base::HistogramTester scoped_histogram;

  size_t num_browsers = chrome::GetBrowserCount(profile());
  int num_tabs_browser = browser()->tab_strip_model()->count();
  int num_tabs_app_browser = app_browser->tab_strip_model()->count();

  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL initial_app_url = app_web_contents->GetLastCommittedURL();
  GURL initial_tab_url = initial_tab->GetLastCommittedURL();

  // Open a popup to an out-of-scope URL.
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  OpenPopupAndWait(app_web_contents, out_of_scope_url);

  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));

  Browser* new_popup_browser = chrome::FindLastActive();
  EXPECT_NE(new_popup_browser, browser());
  EXPECT_NE(new_popup_browser, app_browser);
  EXPECT_TRUE(new_popup_browser->is_type_popup());
  EXPECT_FALSE(new_popup_browser->is_app());

  EXPECT_EQ(num_tabs_browser, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_tabs_app_browser, app_browser->tab_strip_model()->count());

  EXPECT_EQ(initial_app_url, app_web_contents->GetLastCommittedURL());

  content::WebContents* new_popup_web_contents =
      new_popup_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(out_of_scope_url, new_popup_web_contents->GetLastCommittedURL());

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::
            kReparentIntoPopupProceedOutOfScopeInitialNavigation,
        1}});
}

// Tests that popups to in-scope URLs are opened in App windows.
// TODO(crbug.com/849163) Times out flakily on MacOS.
#if defined(OS_MACOSX)
#define MAYBE_InScopePopup DISABLED_InScopePopup
#else
#define MAYBE_InScopePopup InScopePopup
#endif
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleCommonBrowserTest,
                       MAYBE_InScopePopup) {
  InstallTestBookmarkApp();
  Browser* app_browser = OpenTestBookmarkApp();

  base::HistogramTester scoped_histogram;

  // Open a popup to an out-of-scope URL.
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  TestAppActionOpensAppWindowWithOpener(
      app_browser, in_scope_url,
      base::Bind(&OpenPopupAndWait,
                 app_browser->tab_strip_model()->GetActiveWebContents(),
                 in_scope_url));

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kProceedInAppSameScope, 1}});
}

// Apps are only restored during startup on Chrome OS.
#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleCommonBrowserTest,
                       PRE_RestoreApp) {
  InstallTestBookmarkApp();
  OpenTestBookmarkApp();
  SessionStartupPref::SetStartupPref(
      profile(), SessionStartupPref(SessionStartupPref::LAST));
}

IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleCommonBrowserTest,
                       RestoreApp) {
  // There should be two windows. The app window and a regular browser window
  // the test opens.
  EXPECT_EQ(2u, chrome::GetBrowserCount(profile()));
  Browser* app_browser = nullptr;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->is_app()) {
      EXPECT_FALSE(app_browser) << "Found multiple app browsers.";
      app_browser = browser;
    }
  }
  ASSERT_TRUE(app_browser);

  const Extension* app =
      ExtensionRegistry::Get(profile())->enabled_extensions().GetByID(
          web_app::GetAppIdFromApplicationName(app_browser->app_name()));
  EXPECT_EQ(GetAppName(), app->name());
}
#endif  // OS_CHROMEOS

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleCommonBrowserTest,
    testing::Bool());

// Set of tests to make sure form submissions have the correct behavior
// regardless of the kDesktopPWAsLinkCapturing feature flag.
class BookmarkAppNavigationThrottleCommonFormSubmissionBrowserTest
    : public BookmarkAppNavigationThrottleBaseCommonBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<bool,
                     StartIn,
                     std::string,
                     net::URLFetcher::RequestType,
                     std::string>> {
 public:
  void SetUp() override {
    EnableFeaturesForTest(std::get<0>(GetParam()));
    BookmarkAppNavigationThrottleBaseCommonBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_P(
    BookmarkAppNavigationThrottleCommonFormSubmissionBrowserTest,
    FormSubmission) {
  bool should_enable_link_capturing;
  StartIn start_in;
  std::string start_url_path;
  net::URLFetcher::RequestType request_type;
  std::string target_url_path;
  std::tie(should_enable_link_capturing, start_in, start_url_path, request_type,
           target_url_path) = GetParam();

  InstallTestBookmarkApp();

  // Pick where the test should start.
  Browser* current_browser;
  if (start_in == StartIn::APP)
    current_browser = OpenTestBookmarkApp();
  else
    current_browser = browser();

  // If in a regular browser window, navigate to the start URL.
  if (start_in == StartIn::BROWSER) {
    GURL start_url;
    if (start_url_path == GetLaunchingPagePath())
      start_url = GetLaunchingPageURL();
    else
      start_url = https_server().GetURL(GetAppUrlHost(), GetAppUrlPath());

    NavigateParams params(current_browser, start_url,
                          ui::PAGE_TRANSITION_TYPED);
    ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
        start_url, base::BindOnce(&NavigateToURLWrapper, &params)));
  } else if (start_url_path == GetLaunchingPagePath()) {
    // Return early here since the app window can't start with an out-of-scope
    // URL.
    return;
  }

  // If the submit method is GET then add an query params.
  GURL target_url = https_server().GetURL(GetAppUrlHost(), target_url_path);
  if (request_type == net::URLFetcher::RequestType::GET) {
    target_url = AddTestQueryParam(target_url);
  }

  base::HistogramTester scoped_histogram;

  // All form submissions that start in the browser should be kept in the
  // browser.
  if (start_in == StartIn::BROWSER) {
    TestActionDoesNotOpenAppWindow(
        current_browser, target_url,
        base::BindOnce(
            &SubmitFormAndWait,
            current_browser->tab_strip_model()->GetActiveWebContents(),
            target_url, request_type));

    // Navigations to out-of-scope URLs are considered regular navigations and
    // therefore not recorded. Nothing gets histogrammed when
    // kDesktopPWAsLinkCapturing is disabled, because the navigation is not
    // happening in an app window.
    if (target_url_path == GetInScopeUrlPath() &&
        should_enable_link_capturing) {
      ExpectNavigationResultHistogramEquals(
          scoped_histogram, {{BookmarkAppNavigationThrottleResult::
                                  kProceedInBrowserFormSubmission,
                              1}});
    }
    return;
  }

  // When in an app, in-scope navigations should always be kept in the app
  // window.
  if (target_url_path == GetInScopeUrlPath()) {
    TestActionDoesNotOpenAppWindow(
        current_browser, target_url,
        base::BindOnce(
            &SubmitFormAndWait,
            current_browser->tab_strip_model()->GetActiveWebContents(),
            target_url, request_type));

    ExpectNavigationResultHistogramEquals(
        scoped_histogram,
        {{BookmarkAppNavigationThrottleResult::kProceedInAppSameScope, 1}});
    return;
  }

  // When in an app, out-of-scope navigations should always open a new
  // foreground tab.
  DCHECK_EQ(target_url_path, GetOutOfScopeUrlPath());
  TestAppActionOpensForegroundTab(
      current_browser, target_url,
      base::BindOnce(&SubmitFormAndWait,
                     current_browser->tab_strip_model()->GetActiveWebContents(),
                     target_url, request_type));

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kDeferOpenNewTabInAppOutOfScope,
        1}});
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleCommonFormSubmissionBrowserTest,
    testing::Combine(
        testing::Bool(),
        testing::Values(StartIn::BROWSER, StartIn::APP),
        testing::Values(
            BookmarkAppNavigationBrowserTest::GetLaunchingPagePath(),
            BookmarkAppNavigationBrowserTest::GetAppUrlPath()),
        testing::Values(net::URLFetcher::RequestType::GET,
                        net::URLFetcher::RequestType::POST),
        testing::Values(
            BookmarkAppNavigationBrowserTest::GetOutOfScopeUrlPath(),
            BookmarkAppNavigationBrowserTest::GetInScopeUrlPath())));

// Tests that the result is the same regardless of the 'rel' attribute of links
// and of the kDesktopPWAsLinkCapturing feature flag.
class BookmarkAppNavigationThrottleCommonLinkBrowserTest
    : public BookmarkAppNavigationThrottleBaseCommonBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, std::string>> {
 public:
  void SetUp() override {
    EnableFeaturesForTest(std::get<0>(GetParam()));
    BookmarkAppNavigationThrottleBaseCommonBrowserTest::SetUp();
  }
};

// Tests that clicking links inside the app to in-scope URLs doesn't open new
// browser windows.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleCommonLinkBrowserTest,
                       InAppInScopeNavigation) {
  InstallTestBookmarkApp();
  Browser* app_browser = OpenTestBookmarkApp();
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  int num_tabs_browser = browser()->tab_strip_model()->count();
  int num_tabs_app_browser = app_browser->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  base::HistogramTester scoped_histogram;
  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  ClickLinkAndWait(app_web_contents, in_scope_url, LinkTarget::SELF,
                   std::get<1>(GetParam()));

  EXPECT_EQ(num_tabs_browser, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_tabs_app_browser, app_browser->tab_strip_model()->count());
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(app_browser, chrome::FindLastActive());

  EXPECT_EQ(in_scope_url, app_web_contents->GetLastCommittedURL());

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kProceedInAppSameScope, 1}});
}

// Tests that clicking links inside the app to out-of-scope URLs opens a new
// tab in an existing browser window, instead of navigating the existing
// app window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleCommonLinkBrowserTest,
                       InAppOutOfScopeNavigation) {
  InstallTestBookmarkApp();
  Browser* app_browser = OpenTestBookmarkApp();

  const base::HistogramTester scoped_histogram;
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  TestAppActionOpensForegroundTab(
      app_browser, out_of_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     app_browser->tab_strip_model()->GetActiveWebContents(),
                     out_of_scope_url, LinkTarget::SELF,
                     std::get<1>(GetParam())));

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kDeferOpenNewTabInAppOutOfScope,
        1}});
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleCommonLinkBrowserTest,
    testing::Combine(
        testing::Bool(),
        testing::Values("", "noopener", "noreferrer", "nofollow")));

}  // namespace test
}  // namespace extensions
