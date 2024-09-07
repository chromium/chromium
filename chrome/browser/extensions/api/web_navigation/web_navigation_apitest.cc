// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <set>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/download/download_browsertest_utils.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ssl/https_upgrades_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"

using content::WebContents;

namespace extensions {

namespace {

// Waits for a WC to be created. Once it starts loading |delay_url| (after at
// least the first navigation has committed), it delays the load, executes
// |script| in the last committed RVH and resumes the load when a URL ending in
// |until_url_suffix| commits. This class expects |script| to trigger the load
// of an URL ending in |until_url_suffix|.
class DelayLoadStartAndExecuteJavascript : public TabStripModelObserver,
                                           public content::WebContentsObserver {
 public:
  DelayLoadStartAndExecuteJavascript(Browser* browser,
                                     const GURL& delay_url,
                                     const std::string& script,
                                     const std::string& until_url_suffix)
      : content::WebContentsObserver(),
        delay_url_(delay_url),
        until_url_suffix_(until_url_suffix),
        script_(script) {
    browser->tab_strip_model()->AddObserver(this);
  }

  DelayLoadStartAndExecuteJavascript(
      const DelayLoadStartAndExecuteJavascript&) = delete;
  DelayLoadStartAndExecuteJavascript& operator=(
      const DelayLoadStartAndExecuteJavascript&) = delete;

  ~DelayLoadStartAndExecuteJavascript() override {}

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kInserted)
      return;

    content::WebContentsObserver::Observe(
        change.GetInsert()->contents[0].contents);
    tab_strip_model->RemoveObserver(this);
  }

  // WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL() != delay_url_ || !render_frame_host_) {
      return;
    }

    auto throttle =
        std::make_unique<WillStartRequestObserverThrottle>(navigation_handle);
    throttle_ = throttle->AsWeakPtr();
    navigation_handle->RegisterThrottleForTesting(std::move(throttle));

    if (has_user_gesture_) {
      render_frame_host_->ExecuteJavaScriptWithUserGestureForTests(
          base::UTF8ToUTF16(script_), base::NullCallback(),
          content::ISOLATED_WORLD_ID_GLOBAL);
    } else {
      render_frame_host_->ExecuteJavaScriptForTests(
          base::UTF8ToUTF16(script_), base::NullCallback(),
          content::ISOLATED_WORLD_ID_GLOBAL);
    }
    script_was_executed_ = true;
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage())
      return;

    if (script_was_executed_ &&
        base::EndsWith(navigation_handle->GetURL().spec(), until_url_suffix_,
                       base::CompareCase::SENSITIVE)) {
      content::WebContentsObserver::Observe(nullptr);
      if (throttle_)
        throttle_->Unblock();
    }

    if (navigation_handle->IsInMainFrame())
      render_frame_host_ = navigation_handle->GetRenderFrameHost();
  }

  void set_has_user_gesture(bool has_user_gesture) {
    has_user_gesture_ = has_user_gesture;
  }

 private:
  class WillStartRequestObserverThrottle : public content::NavigationThrottle {
   public:
    explicit WillStartRequestObserverThrottle(content::NavigationHandle* handle)
        : NavigationThrottle(handle) {}
    ~WillStartRequestObserverThrottle() override {}

    const char* GetNameForLogging() override {
      return "WillStartRequestObserverThrottle";
    }

    void Unblock() {
      DCHECK(throttled_);
      Resume();
    }

    base::WeakPtr<WillStartRequestObserverThrottle> AsWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
      throttled_ = true;
      return NavigationThrottle::DEFER;
    }

    bool throttled_ = false;

    base::WeakPtrFactory<WillStartRequestObserverThrottle> weak_ptr_factory_{
        this};
  };

  base::WeakPtr<WillStartRequestObserverThrottle> throttle_;

  GURL delay_url_;
  std::string until_url_suffix_;
  std::string script_;
  bool has_user_gesture_ = false;
  bool script_was_executed_ = false;
  raw_ptr<content::RenderFrameHost, AcrossTasksDanglingUntriaged>
      render_frame_host_ = nullptr;
};

// Handles requests for URLs with paths of "/test*" sent to the test server, so
// tests request a URL that receives a non-error response.
std::unique_ptr<net::test_server::HttpResponse> HandleTestRequest(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, "/test",
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_content("This space intentionally left blank.");
  return std::move(response);
}

}  // namespace

class WebNavigationApiTest : public ExtensionApiTest {
 public:
  explicit WebNavigationApiTest(ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleTestRequest));
  }
  ~WebNavigationApiTest() override = default;
  WebNavigationApiTest(const WebNavigationApiTest&) = delete;
  WebNavigationApiTest& operator=(const WebNavigationApiTest&) = delete;

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    FrameNavigationState::set_allow_extension_scheme(true);

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // Some builders are flaky due to slower loading interacting
    // with deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

class WebNavigationApiBackForwardCacheTest : public WebNavigationApiTest {
 public:
  WebNavigationApiBackForwardCacheTest() {
    feature_list_.InitWithFeaturesAndParameters(
        content::GetBasicBackForwardCacheFeatureForTesting(
            {{features::kBackForwardCache, {}}}),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }
  ~WebNavigationApiBackForwardCacheTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

using ContextType = extensions::ExtensionBrowserTest::ContextType;

class WebNavigationApiTestWithContextType
    : public WebNavigationApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  WebNavigationApiTestWithContextType() : WebNavigationApiTest(GetParam()) {}
  ~WebNavigationApiTestWithContextType() override = default;
  WebNavigationApiTestWithContextType(
      const WebNavigationApiTestWithContextType&) = delete;
  WebNavigationApiTestWithContextType& operator=(
      const WebNavigationApiTestWithContextType&) = delete;

 protected:
  [[nodiscard]] bool RunTest(const char* name,
                             bool allow_in_incognito = false) {
    return RunExtensionTest(name, {},
                            {.allow_in_incognito = allow_in_incognito});
  }
};

class WebNavigationApiPrerenderTestWithContextType
    : public WebNavigationApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  WebNavigationApiPrerenderTestWithContextType()
      : WebNavigationApiTest(GetParam()) {}
  ~WebNavigationApiPrerenderTestWithContextType() override = default;
  WebNavigationApiPrerenderTestWithContextType(
      const WebNavigationApiPrerenderTestWithContextType&) = delete;
  WebNavigationApiPrerenderTestWithContextType& operator=(
      const WebNavigationApiPrerenderTestWithContextType&) = delete;

 private:
  content::test::ScopedPrerenderFeatureList prerender_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, Api) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/api")) << message_;
}

// TODO(crbug.com/40858121): Flakily timing out.
IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, DISABLED_GetFrame) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/getFrame")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiPrerenderTestWithContextType, GetFrame) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/getFrame")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, GetFrameIncognito) {
  // TODO(crbug.com/40937027): Convert test to use HTTPS and then remove.
  ScopedAllowHttpForHostnamesForTesting allow_http({"a.com"},
                                                   profile()->GetPrefs());
  ASSERT_TRUE(StartEmbeddedTestServer());

  GURL url = embedded_test_server()->GetURL("a.com", "/empty.html");

  Browser* incognito_browser = OpenURLOffTheRecord(browser()->profile(), url);
  ASSERT_TRUE(incognito_browser);

  // Now that we have a OTR browser, run the extension test.
  ASSERT_TRUE(RunExtensionTest("webnavigation/getFrameIncognito", {},
                               {.allow_in_incognito = true}))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         WebNavigationApiTestWithContextType,
                         testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         WebNavigationApiTestWithContextType,
                         testing::Values(ContextType::kServiceWorker));
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         WebNavigationApiPrerenderTestWithContextType,
                         testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         WebNavigationApiPrerenderTestWithContextType,
                         testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, ClientRedirect) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/clientRedirect")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, ServerRedirect) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/serverRedirect")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, FormSubmission) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/formSubmission")) << message_;
}

class WebNavigationApiPrerenderTestWithServiceWorker
    : public WebNavigationApiTest {
 public:
  WebNavigationApiPrerenderTestWithServiceWorker()
      // This test uses chrome.tabs.executeScript, which is not available in
      // MV3 or later. See crbug.com/332328868.
      : WebNavigationApiTest(ContextType::kServiceWorkerMV2) {

  }
  ~WebNavigationApiPrerenderTestWithServiceWorker() override = default;
  WebNavigationApiPrerenderTestWithServiceWorker(
      const WebNavigationApiPrerenderTestWithServiceWorker&) = delete;
  WebNavigationApiPrerenderTestWithServiceWorker& operator=(
      const WebNavigationApiPrerenderTestWithServiceWorker&) = delete;
};

// Tests that prerender events emit the correct events in the expected order.
IN_PROC_BROWSER_TEST_F(WebNavigationApiPrerenderTestWithServiceWorker,
                       Prerendering) {
  // TODO(crbug.com/40248833): Use https in the test and remove this allowlist
  // entry.
  ScopedAllowHttpForHostnamesForTesting scoped_allow_http(
      {"a.test"}, browser()->profile()->GetPrefs());

  ASSERT_TRUE(StartEmbeddedTestServer());
  EXPECT_TRUE(RunExtensionTest("webnavigation/prerendering")) << message_;
}

// TODO(crbug.com/40791797):
// WebNavigationApiTestWithContextType.Download test is flaky.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Download DISABLED_Download
#else
#define MAYBE_Download Download
#endif
IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, MAYBE_Download) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  content::DownloadManager* download_manager =
      browser()->profile()->GetDownloadManager();
  content::DownloadTestObserverTerminal observer(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  bool result = RunTest("webnavigation/download");
  observer.WaitForFinished();
  ASSERT_TRUE(result) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType,
                       ServerRedirectSingleProcess) {
  // TODO(crbug.com/40248833): Use https in the test and remove these allowlist
  // entries.
  ScopedAllowHttpForHostnamesForTesting scoped_allow_http(
      {"www.a.com", "www.b.com"}, browser()->profile()->GetPrefs());

  ASSERT_TRUE(StartEmbeddedTestServer());

  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/serverRedirectSingleProcess"))
      << message_;

  WebContents* tab = GetWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  ResultCatcher catcher;
  GURL url(
      base::StringPrintf("http://www.a.com:%u/extensions/api_test/"
                         "webnavigation/serverRedirectSingleProcess/a.html",
                         embedded_test_server()->port()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  url = GURL(base::StringPrintf(
      "http://www.b.com:%u/server-redirect?http://www.b.com:%u/test",
      embedded_test_server()->port(), embedded_test_server()->port()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, ForwardBack) {
  ASSERT_TRUE(RunTest("webnavigation/forwardBack")) << message_;
}

// TODO(crbug.com/40221198): Flaky on several platforms.
IN_PROC_BROWSER_TEST_F(WebNavigationApiBackForwardCacheTest,
                       DISABLED_ForwardBack) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/backForwardCache")) << message_;
}

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// https://crbug.com/1223028
#define MAYBE_IFrame DISABLED_IFrame
#else
#define MAYBE_IFrame IFrame
#endif
IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, MAYBE_IFrame) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/iframe")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, SrcDoc) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/srcdoc")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, OpenTab) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/openTab")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, ReferenceFragment) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/referenceFragment")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, SimpleLoad) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/simpleLoad")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, Failures) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/failures")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, FilteredTest) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/filtered")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, UserAction) {
  content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/userAction")) << message_;

  WebContents* tab = GetWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  ResultCatcher catcher;

  const extensions::Extension* extension =
      extension_registry()->enabled_extensions().GetByID(
          last_loaded_extension_id());
  GURL url = extension->GetResourceURL(
      "a.html?" + base::NumberToString(embedded_test_server()->port()));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // This corresponds to "Open link in new tab".
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  params.page_url = url;
  params.frame_url = url;
  params.frame_origin = url::Origin::Create(params.frame_url);
  params.link_url = extension->GetResourceURL("b.html");

  // Get the child frame, which will be the one associated with the context
  // menu.
  content::RenderFrameHost* child_frame = ChildFrameAt(tab, 0);
  ASSERT_TRUE(child_frame);

  TestRenderViewContextMenu menu(*child_frame, params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, RequestOpenTab) {
  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/requestOpenTab")) << message_;

  WebContents* tab = GetWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  ResultCatcher catcher;

  const extensions::Extension* extension =
      extension_registry()->enabled_extensions().GetByID(
          last_loaded_extension_id());
  GURL url = extension->GetResourceURL("a.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // There's a link on a.html. Middle-click on it to open it in a new tab.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kMiddle;
  mouse_event.SetPositionInWidget(7, 7);
  mouse_event.click_count = 1;
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, TargetBlank) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/targetBlank")) << message_;

  WebContents* tab = GetWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  ResultCatcher catcher;

  GURL url = embedded_test_server()->GetURL(
      "/extensions/api_test/webnavigation/targetBlank/a.html");

  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // There's a link with target=_blank on a.html. Click on it to open it in a
  // new tab.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(7, 7);
  mouse_event.click_count = 1;
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType,
                       TargetBlankIncognito) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/targetBlank", {},
                               {.allow_in_incognito = true}))
      << message_;

  ResultCatcher catcher;

  GURL url = embedded_test_server()->GetURL(
      "/extensions/api_test/webnavigation/targetBlank/a.html");

  Browser* otr_browser = OpenURLOffTheRecord(browser()->profile(), url);
  WebContents* tab = otr_browser->tab_strip_model()->GetActiveWebContents();

  // There's a link with target=_blank on a.html. Click on it to open it in a
  // new tab.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(7, 7);
  mouse_event.click_count = 1;
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  tab->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, History) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/history")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, CrossProcess) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadExtension(test_data_dir_.AppendASCII("webnavigation").AppendASCII("app"));

  // See crossProcess/d.html.
  DelayLoadStartAndExecuteJavascript call_script(
      browser(), embedded_test_server()->GetURL("/test1"), "navigate2()",
      "empty.html");

  DelayLoadStartAndExecuteJavascript call_script_user_gesture(
      browser(), embedded_test_server()->GetURL("/test2"), "navigate2()",
      "empty.html");
  call_script_user_gesture.set_has_user_gesture(true);

  ASSERT_TRUE(RunExtensionTest("webnavigation/crossProcess")) << message_;
}

// crbug.com/708139.
IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, DISABLED_CrossProcessFragment) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // See crossProcessFragment/f.html.
  DelayLoadStartAndExecuteJavascript call_script3(
      browser(), embedded_test_server()->GetURL("/test3"), "updateFragment()",
      base::StringPrintf("f.html?%u#foo", embedded_test_server()->port()));

  // See crossProcessFragment/g.html.
  DelayLoadStartAndExecuteJavascript call_script4(
      browser(), embedded_test_server()->GetURL("/test4"), "updateFragment()",
      base::StringPrintf("g.html?%u#foo", embedded_test_server()->port()));

  ASSERT_TRUE(RunExtensionTest("webnavigation/crossProcessFragment"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType,
                       CrossProcessHistory) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // See crossProcessHistory/e.html.
  DelayLoadStartAndExecuteJavascript call_script2(
      browser(), embedded_test_server()->GetURL("/test2"), "updateHistory()",
      "empty.html");

  // See crossProcessHistory/h.html.
  DelayLoadStartAndExecuteJavascript call_script5(
      browser(), embedded_test_server()->GetURL("/test5"), "updateHistory()",
      "empty.html");

  // See crossProcessHistory/i.html.
  DelayLoadStartAndExecuteJavascript call_script6(
      browser(), embedded_test_server()->GetURL("/test6"), "updateHistory()",
      "empty.html");

  ASSERT_TRUE(RunExtensionTest("webnavigation/crossProcessHistory"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType,
                       CrossProcessIframe) {
  content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/crossProcessIframe")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, PendingDeletion) {
  content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/pendingDeletion")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, Crash) {
  // TODO(crbug.com/40248833): Use https in the test and remove this allowlist
  // entry.
  ScopedAllowHttpForHostnamesForTesting scoped_allow_http(
      {"www.a.com"}, browser()->profile()->GetPrefs());

  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/crash")) << message_;

  WebContents* tab = GetWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(tab));

  ResultCatcher catcher;

  GURL url(embedded_test_server()->GetURL(
      "www.a.com", "/extensions/api_test/webnavigation/crash/a.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::RenderProcessHostWatcher process_watcher(
      tab, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(blink::kChromeUICrashURL)));
  process_watcher.Wait();

  url = GURL(embedded_test_server()->GetURL(
      "www.a.com", "/extensions/api_test/webnavigation/crash/b.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40187463): Re-enable this test.
#define MAYBE_Xslt DISABLED_Xslt
#else
#define MAYBE_Xslt Xslt
#endif
IN_PROC_BROWSER_TEST_P(WebNavigationApiTestWithContextType, MAYBE_Xslt) {
  content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/xslt")) << message_;
}

class WebNavigationApiFencedFrameTest : public WebNavigationApiTest {
 protected:
  WebNavigationApiFencedFrameTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{blink::features::kFencedFrames, {}},
                              {blink::features::kFencedFramesAPIChanges, {}},
                              {blink::features::kFencedFramesDefaultMode, {}},
                              {features::kPrivacySandboxAdsAPIsOverride, {}}},
        /*disabled_features=*/{features::kSpareRendererForSitePerProcess});
    // Fenced frames are only allowed in a secure context.
    UseHttpsTestServer();
  }
  ~WebNavigationApiFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebNavigationApiFencedFrameTest, Load) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/fencedFrames")) << message_;
}
}  // namespace extensions
