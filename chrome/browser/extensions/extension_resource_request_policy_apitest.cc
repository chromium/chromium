// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "url/gurl.h"

namespace extensions {

class ExtensionResourceRequestPolicyTest : public ExtensionApiTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void OpenUrlInSubFrameAndVerifyNavigationBlocked(
      const GURL& target_url,
      const std::string& target_frame_name,
      const GURL& expected_navigation_url) {
    GURL main_url = embedded_test_server()->GetURL(
        "/frame_tree/page_with_two_frames_remote_and_local.html");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

    // Navigate |target_frame_name| to |target_url|.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver nav_observer(web_contents, 1);
    ASSERT_TRUE(content::ExecJs(
        web_contents, content::JsReplace("window.open($1, $2)", target_url,
                                         target_frame_name)));
    nav_observer.Wait();

    // Verify that the navigation has failed.
    //
    // It is important that the failure mode below is the same in _all_ of the
    // tests like (to prevent fingerprinting):
    // - WebNavigationToNonWebAccessibleResource...
    // - WebNavigationToNonExistentResource
    // - WebNavigationToNonExistentExtension
    // - ...
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
    EXPECT_EQ(expected_navigation_url, nav_observer.last_navigation_url());
  }

  void OpenUrlInLocalFrameAndVerifyNavigationBlocked(const GURL& target_url) {
    // Tentatively check that the renderer-side validation took place.  Without
    // renderer-side navigation we would still expect browser-side validation to
    // result in ERR_BLOCKED_BY_CLIENT (with a different final URL though) -
    // this is why the test assertion below is secondary / not that important.
    GURL url_blocked_by_renderer("chrome-extension://invalid/");

    OpenUrlInSubFrameAndVerifyNavigationBlocked(target_url, "local-frame",
                                                url_blocked_by_renderer);
  }

  // Used to test that javascript history.back() navigations to a target
  // non-web accessible resource are blocked, using remote and local iframes.
  void OpenUrlInSubFrameAndVerifyBackNavigationBlocked(
      const GURL& target_url,
      const std::string& target_frame_id,
      const GURL& expected_navigation_url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Load up an iframe we can navigate.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "/frame_tree/page_with_two_frames_remote_and_local.html")));
    const char kNavigateScriptTemplate[] = R"(
      var iframe = document.getElementById($1);
      iframe.src = $2;
    )";

    {
      // Navigate the iframe to an inaccessible resource and expect an error.
      content::TestNavigationObserver nav_observer(web_contents);
      ASSERT_TRUE(content::ExecJs(
          web_contents, content::JsReplace(kNavigateScriptTemplate,
                                           target_frame_id, target_url)));
      nav_observer.Wait();

      EXPECT_FALSE(nav_observer.last_navigation_succeeded());
      EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
      EXPECT_EQ(expected_navigation_url, nav_observer.last_navigation_url());
    }

    {
      // Navigate the iframe to an accessible page (about:blank).
      content::TestNavigationObserver nav_observer(web_contents);
      ASSERT_TRUE(content::ExecJs(
          web_contents,
          content::JsReplace(kNavigateScriptTemplate, target_frame_id,
                             GURL("about:blank"))));
      nav_observer.Wait();
      EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    }

    {
      // Finally, trigger a back navigation which should lead to a blocked page.
      const char kNavigateBackScriptTemplate[] = R"(
        var iframe = document.getElementById($1);
        iframe.contentWindow.history.back();
      )";
      content::TestNavigationObserver nav_observer(web_contents);
      ASSERT_TRUE(content::ExecJs(
          web_contents,
          content::JsReplace(kNavigateBackScriptTemplate, target_frame_id)));
      nav_observer.Wait();

      EXPECT_FALSE(nav_observer.last_navigation_succeeded());
      EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
      EXPECT_EQ(expected_navigation_url, nav_observer.last_navigation_url());
    }
  }
};

// Note, this mostly tests the logic of chrome/renderer/extensions/
// extension_resource_request_policy.*, but we have it as a browser test so that
// can make sure it works end-to-end.
IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, OriginPrivileges) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("extension")));

  GURL web_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "index.html"));

  GURL::Replacements make_host_a_com;
  make_host_a_com.SetHostStr("a.com");

  GURL::Replacements make_host_b_com;
  make_host_b_com.SetHostStr("b.com");

  // A web host that has permission.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), web_resource.ReplaceComponents(make_host_a_com)));
  EXPECT_EQ(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"),
      "Loaded");

  // A web host that loads a non-existent extension.
  GURL non_existent_extension(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "non_existent_extension.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_existent_extension));
  EXPECT_EQ(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"),
      "Image failed to load");

  // A data URL. Data URLs should always be able to load chrome-extension://
  // resources.
  std::string file_source;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(
        test_data_dir_.AppendASCII("extension_resource_request_policy")
            .AppendASCII("index.html"),
        &file_source));
  }
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL(std::string("data:text/html;charset=utf-8,") + file_source)));
  EXPECT_EQ(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"),
      "Loaded");

  // A different extension. Legacy (manifest_version 1) extensions should always
  // be able to load each other's resources.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("extension2")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://pbkkcbgdkliohhfaeefcijaghglkahja/index.html")));
  EXPECT_EQ(
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"),
      "Loaded");
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       ExtensionCanLoadHostedAppIcons) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("hosted_app")));

  ASSERT_TRUE(RunExtensionTest(
      "extension_resource_request_policy/extension2/",
      {.extension_url = "can_load_icons_from_hosted_apps.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, Audio) {
  EXPECT_TRUE(RunExtensionTest("extension_resource_request_policy/extension2",
                               {.extension_url = "audio.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, Video) {
  EXPECT_TRUE(RunExtensionTest("extension_resource_request_policy/extension2",
                               {.extension_url = "video.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       WebAccessibleResources) {
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("web_accessible")));

  GURL accessible_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/accessible_resource.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), accessible_resource));
  EXPECT_EQ("Loaded", content::EvalJs(
                          browser()->tab_strip_model()->GetActiveWebContents(),
                          "document.title"));

  GURL xhr_accessible_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/xhr_accessible_resource.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), xhr_accessible_resource));
  EXPECT_EQ(
      "XHR completed with status: 200",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"));

  GURL xhr_inaccessible_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/xhr_inaccessible_resource.html"));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), xhr_inaccessible_resource));
  EXPECT_EQ(
      "XHR failed to load resource",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"));

  GURL nonaccessible_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/nonaccessible_resource.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), nonaccessible_resource));
  EXPECT_EQ(
      "Image failed to load",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"));

  GURL nonexistent_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/nonexistent_resource.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), nonexistent_resource));
  EXPECT_EQ(
      "Image failed to load",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"));

  GURL newtab_page("chrome://newtab");
  GURL accessible_newtab_override(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/accessible_history_navigation.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), newtab_page));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), accessible_newtab_override, 1);
  EXPECT_EQ(
      "New Tab Page Loaded Successfully",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "document.title"));
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       LinkToWebAccessibleResources) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("web_accessible"));
  ASSERT_TRUE(extension);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = web_contents->GetController();

  GURL accessible_linked_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/accessible_link_resource.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), accessible_linked_resource, 1);
  GURL accessible_url = extension->GetResourceURL("/test.png");
  EXPECT_EQ(accessible_url, content::EvalJs(web_contents, "document.URL"));
  EXPECT_EQ(content::PAGE_TYPE_NORMAL,
            controller.GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(accessible_url,
            web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());

  GURL nonaccessible_linked_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/nonaccessible_link_resource.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), nonaccessible_linked_resource, 1);
  EXPECT_EQ("chrome-error://chromewebdata/",
            content::EvalJs(web_contents, "document.URL"));
  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            controller.GetLastCommittedEntry()->GetPageType());
  GURL invalid_url("chrome-extension://invalid/");
  EXPECT_EQ(invalid_url,
            web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());

  // Redirects can sometimes occur before the load event, so use a
  // UrlLoadObserver instead of blocking waiting for two load events.
  ui_test_utils::UrlLoadObserver accessible_observer(accessible_url);
  GURL accessible_client_redirect_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/accessible_redirect_resource.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), accessible_client_redirect_resource));
  accessible_observer.Wait();
  EXPECT_EQ(content::PAGE_TYPE_NORMAL,
            controller.GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(accessible_url, web_contents->GetLastCommittedURL());

  ui_test_utils::UrlLoadObserver nonaccessible_observer(invalid_url);
  GURL nonaccessible_client_redirect_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/nonaccessible_redirect_resource.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), nonaccessible_client_redirect_resource));
  nonaccessible_observer.Wait();
  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            controller.GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(invalid_url, web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       WebAccessibleResourcesWithCSP) {
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("web_accessible")));

  GURL accessible_resource_with_csp(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/accessible_resource_with_csp.html"));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), accessible_resource_with_csp));
  EXPECT_EQ("Loaded", content::EvalJs(
                          browser()->tab_strip_model()->GetActiveWebContents(),
                          "document.title"));
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, Iframe) {
  // Load another extension, which the test one shouldn't be able to get
  // resources from.
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("inaccessible")));
  EXPECT_TRUE(
      RunExtensionTest("extension_resource_request_policy/web_accessible",
                       {.extension_url = "iframe.html"}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       IframeNavigateToInaccessible) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("some_accessible")));

  GURL iframe_navigate_url(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "iframe_navigate.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), iframe_navigate_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL private_page(
      "chrome-extension://kegmjfcnjamahdnldjmlpachmpielcdk/private.html");
  ASSERT_TRUE(content::ExecJs(web_contents, "navigateFrameNow()"));
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  EXPECT_NE(private_page, web_contents->GetLastCommittedURL());

  // The iframe should not load |private_page|, which is not web-accessible.
  //
  // TODO(alexmos): Make this check stricter, as extensions are now fully
  // isolated. The failure mode is that the request is canceled and we stay on
  // public.html (see https://crbug.com/656752).
  EXPECT_NE("Private",
            EvalJs(ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0),
                   "document.body.innerText"));
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       IframeNavigateToInaccessibleViaServerRedirect) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Any valid extension that happens to have a web accessible resource.
  const Extension* patsy = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("some_accessible"));

  // An extension with a non-webaccessible resource.
  const Extension* target = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("inaccessible"));

  // Start with an http iframe.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/iframe.html")));

  // Send it to a web accessible resource of a valid extension.
  GURL patsy_url = patsy->GetResourceURL("public.html");
  content::NavigateIframeToURL(web_contents, "test", patsy_url);

  // Now send it to a NON-web-accessible resource of any other extension, via
  // http redirect.
  GURL target_url = target->GetResourceURL("inaccessible-iframe-contents.html");
  GURL http_redirect_to_target_url =
      embedded_test_server()->GetURL("/server-redirect?" + target_url.spec());
  content::NavigateIframeToURL(web_contents, "test",
                               http_redirect_to_target_url);

  // That should not have been allowed.
  EXPECT_NE(url::Origin::Create(target_url).GetURL(),
            ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0)
                ->GetLastCommittedOrigin()
                .GetURL());
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       WebNavigationToNonWebAccessibleResource_LocalSubframe) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("inaccessible"));
  ASSERT_TRUE(extension);
  const GURL non_web_accessible_url =
      extension->GetResourceURL("inaccessible-iframe-contents.html");

  OpenUrlInLocalFrameAndVerifyNavigationBlocked(non_web_accessible_url);
}

// This test tries to ensure that there is no difference between
// 1) navigating to a non-web-accessible-resource of an existing extension
//    (tested by WebNavigationToNonWebAccessibleResource_... tests)
// and
// 2a) navigating to a non-existent resource of an existing extension
//     (the WebNavigationToNonExistentResource test here)
// and
// 2b) navigating to a resource of a non-existent extension
//     (the WebNavigationToNonExistentExtension test below)
//
// The lack of differences is important to prevent web pages from fingerprinting
// (by making it difficult for web pages to detect which extensions are
// present).
IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       WebNavigationToNonExistentResource) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("inaccessible"));
  ASSERT_TRUE(extension);
  const GURL non_existent_resource_url =
      extension->GetResourceURL("no-such-extension-resource.html");

  OpenUrlInLocalFrameAndVerifyNavigationBlocked(non_existent_resource_url);
}

// This test tries to ensure that there is no difference between
// 1) navigating to a non-web-accessible-resource of an existing extension
//    (tested by WebNavigationToNonWebAccessibleResource_... tests)
// and
// 2a) navigating to a non-existent resource of an existing extension
//     (the WebNavigationToNonExistentResource test above)
// and
// 2b) navigating to a resource of a non-existent extension
//     (the WebNavigationToNonExistentExtension test here)
//
// The lack of differences is important to prevent web pages from fingerprinting
// (by making it difficult for web pages to detect which extensions are
// present).
IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       WebNavigationToNonExistentExtension) {
  const GURL non_existent_extension_url(
      "chrome-extension://aaaaabbbbbcccccdddddeeeeefffffgg/blah.png");

  OpenUrlInLocalFrameAndVerifyNavigationBlocked(non_existent_extension_url);
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       WebNavigationToNonWebAccessibleResource_RemoteSubframe) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("inaccessible"));
  ASSERT_TRUE(extension);
  const GURL non_web_accessible_url =
      extension->GetResourceURL("inaccessible-iframe-contents.html");

  OpenUrlInSubFrameAndVerifyNavigationBlocked(
      non_web_accessible_url, "remote-frame", non_web_accessible_url);
}

// This is a regression test for https://crbug.com/442579.
IN_PROC_BROWSER_TEST_F(
    ExtensionResourceRequestPolicyTest,
    WebNavigationToNonWebAccessibleResource_FormTargetingNewWindow) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("inaccessible"));
  ASSERT_TRUE(extension);
  const GURL non_web_accessible_url =
      extension->GetResourceURL("inaccessible-iframe-contents.html");

  GURL main_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Inject and submit a form that will navigate a new window to a
  // non-web-accessible-resource.  This replicates the repro steps
  // from https://crbug.com/442579 (although a simpler repro might
  // exist - window.open(non-war-url, '_blank')).
  content::WebContentsAddedObserver new_window_observer;
  content::WebContents* old_window =
      browser()->tab_strip_model()->GetActiveWebContents();
  const char* kScriptTemplate = R"(
      var f = document.createElement('form');
      f.target = "extWindow";
      f.action = $1;
      f.method = "post";
      document.body.appendChild(f);
      f.submit();
  )";
  ASSERT_TRUE(content::ExecJs(
      old_window, content::JsReplace(kScriptTemplate, non_web_accessible_url)));
  content::WebContents* new_window = new_window_observer.GetWebContents();
  content::TestNavigationObserver nav_observer(new_window, 1);
  nav_observer.Wait();

  // Verify that the navigation has failed.
  //
  // It is important that the failure mode below is the same in _all_ of the
  // tests like (to prevent fingerprinting):
  // - WebNavigationToNonWebAccessibleResource...
  // - WebNavigationToNonExistentResource
  // - WebNavigationToNonExistentExtension
  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
}

// Tests that a service worker for a web origin can't use client.navigate() to
// navigate to a non-web accessible resource of a Chrome extension.
IN_PROC_BROWSER_TEST_F(
    ExtensionResourceRequestPolicyTest,
    WebNavigationToNonWebAccessibleResource_ViaServiceWorkerNavigate) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("inaccessible"));
  ASSERT_TRUE(extension);
  const GURL non_web_accessible_url =
      extension->GetResourceURL("inaccessible-iframe-contents.html");

  // Load a page that registers a service worker.
  GURL web_page_url = embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), web_page_url));
  EXPECT_EQ("DONE", EvalJs(web_contents, "register('client_api_worker.js');"));

  // Load the page again so we are controlled.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  "!!navigator.serviceWorker.controller"));

  // Have the service worker call client.navigate() on the page.
  content::TestNavigationObserver nav_observer(web_contents, 1);
  const char kNavigateScriptTemplate[] = R"(
    (async () => {
      const registration = await navigator.serviceWorker.ready;
      registration.active.postMessage({command: 'navigate', url: $1});
      return true;
    })();
  )";
  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  content::JsReplace(kNavigateScriptTemplate,
                                                     non_web_accessible_url)));

  // Verify that the navigation was blocked.
  nav_observer.Wait();
  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
  EXPECT_EQ(non_web_accessible_url, nav_observer.last_navigation_url());
  ASSERT_TRUE(nav_observer.last_initiator_origin().has_value());
  EXPECT_EQ(url::Origin::Create(web_page_url),
            nav_observer.last_initiator_origin().value());
}

// Tests that a service worker for a web origin can't use the openWindow API to
// navigate to a non-web accessible resource of a Chrome extension.
IN_PROC_BROWSER_TEST_F(
    ExtensionResourceRequestPolicyTest,
    WebNavigationToNonWebAccessibleResource_ViaServiceWorkerOpenWindow) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("inaccessible"));
  ASSERT_TRUE(extension);
  const GURL non_web_accessible_url =
      extension->GetResourceURL("inaccessible-iframe-contents.html");

  // Load a page that registers a service worker.
  GURL web_page_url = embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), web_page_url));
  EXPECT_EQ("DONE",
            content::EvalJs(web_contents, "register('client_api_worker.js');"));

  // Simulate clicking a notification - this will prompt the test service worker
  // to call clients.openWindow(non_web_accessible_url).
  content::WebContents* new_window = nullptr;
  {
    GURL target_url = non_web_accessible_url;
    blink::PlatformNotificationData notification_data;
    notification_data.body = base::UTF8ToUTF16(target_url.spec());

    GURL scope_url = embedded_test_server()->GetURL("/service_worker/");
    content::ServiceWorkerContext* context = GetServiceWorkerContext();

    content::WebContentsAddedObserver new_window_observer;
    content::DispatchServiceWorkerNotificationClick(context, scope_url,
                                                    notification_data);
    new_window = new_window_observer.GetWebContents();
  }

  // Verify that the navigation in the new window will be blocked - we are
  // disallowing navigations to non-web-accessible-resources.
  content::TestNavigationObserver nav_observer(new_window, 1);
  nav_observer.Wait();
  EXPECT_FALSE(nav_observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
  EXPECT_EQ(non_web_accessible_url, nav_observer.last_navigation_url());
  ASSERT_TRUE(nav_observer.last_initiator_origin().has_value());
  EXPECT_EQ(url::Origin::Create(web_page_url),
            nav_observer.last_initiator_origin().value());
}

// Tests that a page can't use history.back() on another page to navigate to a
// non-web accessible resource of an extension.
// Regression test for https://crbug.com/1043965.
IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       WebNavigationToNonWebAccessibleResource_ViaHistoryBack) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("inaccessible"));
  ASSERT_TRUE(extension);
  const GURL non_web_accessible_url =
      extension->GetResourceURL("inaccessible-iframe-contents.html");

  GURL main_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Have a page open a new window with JS and retain a reference to it.
  content::WebContentsAddedObserver new_window_observer;
  content::WebContents* old_window =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecJs(
      old_window,
      content::JsReplace("var newWindow = open($1);", non_web_accessible_url)));
  content::WebContents* new_window = new_window_observer.GetWebContents();
  content::WaitForLoadStop(new_window);
  // As this resource is non-web accessible, we expect an error page.
  // NOTE: It would be nice to check for the actual ERR_BLOCKED_BY_CLIENT error,
  // but the observer we are using to grab the new page doesn't keep track of
  // the navigation handle or any of the specific error codes.
  EXPECT_EQ(non_web_accessible_url, new_window->GetLastCommittedURL());
  EXPECT_EQ(content::PAGE_TYPE_ERROR,
            new_window->GetController().GetLastCommittedEntry()->GetPageType());

  {
    // Navigate the second window from the first to about:blank.
    content::TestNavigationObserver nav_observer(new_window, 1);
    ASSERT_TRUE(content::ExecJs(old_window,
                                "newWindow.location.href = 'about:blank';"));
    nav_observer.Wait();
    EXPECT_EQ("about:blank", new_window->GetLastCommittedURL());
  }

  {
    // Navigate the second window back using history, which should be blocked.
    content::TestNavigationObserver nav_observer(new_window, 1);
    ASSERT_TRUE(content::ExecJs(old_window, "newWindow.history.back();"));
    nav_observer.Wait();
    EXPECT_EQ(non_web_accessible_url, new_window->GetLastCommittedURL());

    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
    EXPECT_EQ(non_web_accessible_url, nav_observer.last_navigation_url());
  }
}

// Tests that a page can't use history.back() on a remote iframe to navigate to
// a non-web accessible resource of an extension.
IN_PROC_BROWSER_TEST_F(
    ExtensionResourceRequestPolicyTest,
    WebNavigationToNonWebAccessibleResource_ViaHistoryBackRemoteIframe) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("inaccessible"));
  ASSERT_TRUE(extension);

  GURL inaccessible_resource =
      extension->GetResourceURL("inaccessible-iframe-contents.html");

  OpenUrlInSubFrameAndVerifyBackNavigationBlocked(
      inaccessible_resource, "remote-frame", inaccessible_resource);
}

// Tests that a page can't use history.back() on a local iframe to navigate to a
// non-web accessible resource of an extension.
IN_PROC_BROWSER_TEST_F(
    ExtensionResourceRequestPolicyTest,
    WebNavigationToNonWebAccessibleResource_ViaHistoryBackLocalIframe) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
          .AppendASCII("inaccessible"));
  ASSERT_TRUE(extension);

  GURL inaccessible_resource =
      extension->GetResourceURL("inaccessible-iframe-contents.html");
  GURL url_blocked_by_renderer("chrome-extension://invalid/");

  OpenUrlInSubFrameAndVerifyBackNavigationBlocked(
      inaccessible_resource, "local-frame", url_blocked_by_renderer);
}

// Regression test for crbug.com/649869. Ensures that on navigation to an
// invalid extension resource (or more generally for navigations blocked by the
// browser with net::ERR_BLOCKED_BY_CLIENT), the error page doesn't incorrectly
// attribute extensions as the cause of the blocked request.
IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       NavigationToInvalidExtensionPage) {
  std::string url =
      base::StringPrintf("chrome-extension://%s/manifest.json",
                         crx_file::id_util::GenerateId("foo").c_str());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string body =
      content::EvalJs(tab, "document.body.textContent").ExtractString();

  std::string expected_error;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  expected_error = "This page has been blocked by Chrome";
#else
  expected_error = "This page has been blocked by Chromium";
#endif

  EXPECT_TRUE(base::Contains(body, expected_error));
  EXPECT_FALSE(
      base::Contains(body, "This page has been blocked by an extension"));
  EXPECT_FALSE(base::Contains(body, "Try disabling your extensions."));
}

}  // namespace extensions
