// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {
static constexpr char kManifestStub[] = R"({
  "name": "Test",
  "version": "0.1",
  "manifest_version": 3,
  "web_accessible_resources": [
    {
      "resources": [ "dynamic.html" ],
      "matches": [ "<all_urls>" ],
      "use_dynamic_url": true
    },
    {
      "resources": [ "static.html" ],
      "matches": [ "<all_urls>" ]
    }
  ]
})";

static constexpr char kFetchResourceScriptTemplate[] = R"(
  // Verify that the web accessible resource can be fetched.
  async function test(title, filename, useDynamicUrl, isAllowed) {
    return new Promise(async resolve => {
      const dynamicUrl = `chrome-extension://%s/${filename}`;
      const staticUrl = `chrome-extension://%s/${filename}`;
      const url = useDynamicUrl ? dynamicUrl : staticUrl;

      // Fetch and verify the contents of fetched web accessible resources.
      const verifyFetch = (actual) => {
        if (isAllowed == (filename == actual)) {
          resolve();
        } else {
          reject(`${title}. Expected: ${filename}. Actual: ${actual}`);
        }
      };
      fetch(url)
        .then(result => result.text())
        .catch(error => verifyFetch(error))
        .then(text => verifyFetch(text));
    });
  }

  // Run tests with list example: [[title, filename, useDynamicUrl, isAllowed]].
  const testCases = [%s];
  const tests = testCases.map(testCase => test(...testCase));
  Promise.all(tests).then(response => true);
)";

// Exercise web accessible resources.
class WebAccessibleResourcesBrowserTest : public ExtensionBrowserTest {
 public:
  WebAccessibleResourcesBrowserTest() = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    EXPECT_TRUE(embedded_test_server()->Start());
  }

 private:
  // TODO(https://crbug.com/40804030): Remove this when updated to use MV3.
  extensions::ScopedTestMV2Enabler mv2_enabler_;
};

// If `use_dynamic_url` is set to true in manifest.json, then the associated web
// accessible resource(s) can only be loaded using the dynamic url. Loading with
// the static url containing the extension id won't work.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       UseDynamicUrlInFetch) {
  // Load extension.
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifestStub);
  extension_dir.WriteFile(FILE_PATH_LITERAL("dynamic.html"), "dynamic.html");
  extension_dir.WriteFile(FILE_PATH_LITERAL("static.html"), "static.html");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());

  // Navigate to a test page and get the web contents.
  base::FilePath test_page;
  GURL gurl = embedded_test_server()->GetURL("example.com", "/simple.html");
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, gurl));

  std::string script =
      base::StringPrintf(kFetchResourceScriptTemplate,
                         extension->guid().c_str(), extension->id().c_str(), R"(
      ["Load a static resource with a dynamic url", 'static.html', true, true],
      ["Load a static resource with a static url", 'static.html', false, true],
      ["Load dynamic resource with a dynamic url", 'dynamic.html', true, true],
      ["Load dynamic resource with a static url", 'dynamic.html', false, false],
      )");
  ASSERT_TRUE(content::EvalJs(web_contents, script).ExtractBool());
}

// Exercise these resources being used in iframes in a web page. The navigation
// flow goes through a different path than resource fetching.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       UseDynamicUrlInIframe) {
  // Load an extension that has one web accessible resource.
  TestExtensionDir extension_dir;
  extension_dir.WriteManifest(kManifestStub);
  extension_dir.WriteFile(FILE_PATH_LITERAL("dynamic.html"),
                          "dynamic resource");
  extension_dir.WriteFile(FILE_PATH_LITERAL("static.html"), "static resource");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  EXPECT_TRUE(extension);

  auto navigate = [&](const GURL& target, const GURL& commit,
                      const std::string& expected) {
    // Navigate the main frame with a browser initiated navigation to a blank
    // web page. This should succeed.
    const GURL gurl = embedded_test_server()->GetURL("/iframe_blank.html");
    content::WebContents* web_contents = GetActiveWebContents();
    EXPECT_TRUE(content::NavigateToURL(web_contents, gurl));
    content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
    content::RenderFrameHost* iframe = content::ChildFrameAt(main_frame, 0);
    EXPECT_TRUE(iframe);

    // Navigate the iframe with a renderer initiated navigation to a web
    // accessible resource. This should succeed.
    content::TestNavigationObserver nav_observer(web_contents);
    EXPECT_TRUE(content::NavigateIframeToURL(web_contents, "test", target));
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(net::OK, nav_observer.last_net_error_code());
    iframe = content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
    EXPECT_EQ(commit, iframe->GetLastCommittedURL());
    EXPECT_EQ(expected, EvalJs(iframe, "document.body.innerText;"));
  };

  static struct {
    const char* title;
    const GURL target;
    const GURL commit;
    const std::string expected;
  } test_cases[] = {
      {
          "Static",
          extension->GetResourceURL("static.html"),
          extension->GetResourceURL("static.html"),
          "static resource",
      },
      {
          "Dynamic",
          Extension::GetResourceURL(extension->dynamic_url(), "dynamic.html"),
          extension->GetResourceURL("dynamic.html"),
          "dynamic resource",
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf("Error: '%s'", test_case.title));
    navigate(test_case.target, test_case.commit, test_case.expected);
  }
}

// Tests that navigating a main frame via location.href works if and only if
// the target resource is accessible to the main frame.
// Regression test for https://crbug.com/374503948.
IN_PROC_BROWSER_TEST_F(
    WebAccessibleResourcesBrowserTest,
    MainFrameLocationHrefUpdatesAreSubjectToAccessibleResources) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "version": "0.1",
           "manifest_version": 3,
           "web_accessible_resources": [
             {
               "resources": [ "accessible.html" ],
               "matches": [ "http://trusted.example/*" ],
               "use_dynamic_url": true
             }
           ]
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("accessible.html"), "accessible");
  test_dir.WriteFile(FILE_PATH_LITERAL("inaccessible.html"), "inaccessible");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL dynamic_accessible_url =
      Extension::GetResourceURL(extension->dynamic_url(), "accessible.html");
  const GURL static_accessible_url =
      extension->GetResourceURL("accessible.html");
  const GURL dynamic_inaccessible_url =
      Extension::GetResourceURL(extension->dynamic_url(), "inaccessible.html");
  const GURL static_inaccessible_url =
      extension->GetResourceURL("inaccessible.html");

  const GURL trusted_site =
      embedded_test_server()->GetURL("trusted.example", "/simple.html");
  const GURL untrusted_site =
      embedded_test_server()->GetURL("untrusted.example", "/simple.html");

  const GURL invalid_extension_url(kExtensionInvalidRequestURL);

  // Each test case will:
  // * Navigate to an initial site.
  // * Try to navigate that page to a target url with `location.replace();` this
  //   is a renderer-initiated navigation, and should be limited to
  //   web-accessible resource checks.
  // * Verify where the navigation reached.
  static struct {
    // The url to navigate the browser tab to.
    GURL site_url;
    // The url to navigate to via `document.location.replace()` (an extension
    // resource).
    GURL target_url;
    // The final url we expect; this should either be the static url of the
    // extension resource or chrome-extension://invalid.
    GURL final_url;
    // If non-null, the `document.body.innerHTML` we expect for the final page.
    const char* body_content;
  } test_cases[] = {
      {trusted_site, dynamic_accessible_url, static_accessible_url,
       "accessible"},
      {trusted_site, dynamic_inaccessible_url, invalid_extension_url, nullptr},
      {trusted_site, static_accessible_url, invalid_extension_url, nullptr},
      {trusted_site, static_inaccessible_url, invalid_extension_url, nullptr},
      {untrusted_site, dynamic_accessible_url, invalid_extension_url, nullptr},
      {untrusted_site, dynamic_inaccessible_url, invalid_extension_url,
       nullptr},
      {untrusted_site, static_accessible_url, invalid_extension_url, nullptr},
      {untrusted_site, static_inaccessible_url, invalid_extension_url, nullptr},
  };

  content::WebContents* web_contents = GetActiveWebContents();
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << "Site URL: " << test_case.site_url
                                    << "Target URL: " << test_case.target_url
                                    << "Final URL: " << test_case.final_url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, test_case.site_url));
    EXPECT_EQ(test_case.site_url, web_contents->GetLastCommittedURL());

    ASSERT_TRUE(content::ExecJs(
        web_contents, base::StringPrintf("document.location.replace('%s')",
                                         test_case.target_url.spec().c_str())));
    content::WaitForLoadStop(web_contents);
    EXPECT_EQ(test_case.final_url, web_contents->GetLastCommittedURL());
    if (test_case.body_content) {
      EXPECT_EQ(test_case.body_content,
                content::EvalJs(web_contents, "document.body.innerText"));
    }
  }
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Navigate to a web page and then try to load an extension subresource.
// TODO(crbug.com/390687767): Port to desktop Android. Currently the redirect
// doesn't happen.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       SubresourceReachabilityAfterServerRedirect) {
  // Load extension.
  TestExtensionDir extension_dir;
  constexpr char kManifest[] = R"({
    "name": "test",
    "version": "1",
    "manifest_version": 3,
    "web_accessible_resources": [{
      "resources": [ "accessible.html" ],
      "matches": [ "<all_urls>" ]
    }]
  })";
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("accessible.html"),
                          "accessible.html");
  extension_dir.WriteFile(FILE_PATH_LITERAL("inaccessible.html"),
                          "inaccessible.html");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL gurl(embedded_test_server()->GetURL("example.org", "/iframe.html"));

  struct {
    const char* title;
    const char* filename;
    net::Error error;
  } test_cases[] = {
      {"inaccessible", "inaccessible.html", net::ERR_BLOCKED_BY_CLIENT},
      {"accessible", "accessible.html", net::OK}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.title);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

    // Navigate to a web page and then fetch the supplied subresource.
    static constexpr char kScriptTemplate[] = R"(
      const serverOrigin = '%s';
      const resourceUrl = '%s';

      // Verify that web accessible resource can be fetched.
      async function run() {
        return new Promise(async (resolve, reject) => {
          const url = `${serverOrigin}?${resourceUrl}`;
          const iframe = document.getElementById('test');
          iframe.onload = event => resolve();
          iframe.src = url;
        });
      }

      run().then(response => true);
    )";
    GURL resource_url = extension->GetResourceURL(test_case.filename);
    std::string script =
        base::StringPrintf(kScriptTemplate,
                           embedded_test_server()
                               ->GetURL("example.com", "/server-redirect")
                               .spec(),
                           resource_url.spec());

    // Get the first child frame, which should be the only html child [iframe].
    auto* active_web_contents = GetActiveWebContents();
    content::RenderFrameHost* first_child =
        content::ChildFrameAt(active_web_contents, 0);

    // Determine if the subresource load was successful.
    content::TestFrameNavigationObserver nav_observer(first_child);
    ASSERT_TRUE(content::EvalJs(active_web_contents, script).ExtractBool());
    nav_observer.Wait();
    EXPECT_EQ(test_case.error, nav_observer.last_net_error_code());
    if (nav_observer.last_net_error_code() == net::OK) {
      ASSERT_TRUE(nav_observer.last_navigation_succeeded());
      ASSERT_EQ(resource_url, nav_observer.last_committed_url());
    }
  }
}

// Server redirect to a web accessible resource whereby `matches` doesn't match.
// TODO(crbug.com/390687767): Port to desktop Android. Currently the redirect
// doesn't happen.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       ServerRedirectSubresource) {
  // Load extension.
  TestExtensionDir extension_dir;
  constexpr char kManifest[] = R"({
    "name": "test",
    "version": "1",
    "manifest_version": 3,
    "web_accessible_resources": [{
      "resources": [ "accessible.html" ],
      "matches": [ "http://no.example.com/*" ]
    }]
  })";
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("accessible.html"),
                          "accessible.html");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL gurl(embedded_test_server()->GetURL("an.example.org", "/iframe.html"));
  const char* filename = "accessible.html";
  net::Error error = net::ERR_BLOCKED_BY_CLIENT;

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));

  // Navigate to a web page and then fetch the supplied subresource.
  static constexpr char kScriptTemplate[] = R"(
    const serverOrigin = '%s';
    const resourceUrl = '%s';

    // Verify that web accessible resource can be fetched.
    async function run() {
      return new Promise(async (resolve, reject) => {
        const url = `${serverOrigin}?${resourceUrl}`;
        const iframe = document.getElementById('test');
        iframe.onload = event => resolve();
        iframe.src = url;
      });
    }

    run().then(response => true);
  )";
  GURL resource_url = extension->GetResourceURL(filename);
  std::string script =
      base::StringPrintf(kScriptTemplate,
                         embedded_test_server()
                             ->GetURL("an.example.com", "/server-redirect")
                             .spec(),
                         resource_url.spec());

  // Get the first child frame, which should be the only html child [iframe].
  auto* active_web_contents = GetActiveWebContents();
  content::RenderFrameHost* first_child =
      content::ChildFrameAt(active_web_contents, 0);

  // Determine if the subresource load was successful.
  content::TestFrameNavigationObserver nav_observer(first_child);
  ASSERT_TRUE(content::EvalJs(active_web_contents, script).ExtractBool());
  nav_observer.Wait();
  EXPECT_EQ(error, nav_observer.last_net_error_code());
  if (nav_observer.last_net_error_code() == net::OK) {
    ASSERT_TRUE(nav_observer.last_navigation_succeeded());
    ASSERT_EQ(resource_url, nav_observer.last_committed_url());
  }
}

// Server redirect to a web accessible resource whereby `matches` doesn't match.
// TODO(crbug.com/390687767): Port to desktop Android. Currently the redirect
// doesn't happen.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       ServerRedirectMainframe) {
  // Load extension.
  TestExtensionDir extension_dir;
  constexpr char kManifest[] = R"({
    "name": "test",
    "version": "1",
    "manifest_version": 3,
    "web_accessible_resources": [{
      "resources": [ "accessible.html" ],
      "matches": [ "http://no.example.com/*" ]
    }]
  })";
  extension_dir.WriteManifest(kManifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("accessible.html"),
                          "accessible.html");
  const Extension* extension = LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  std::string url =
      base::StringPrintf("/server-redirect?%s",
                         extension->GetResourceURL("accessible.html").spec());
  GURL gurl(embedded_test_server()->GetURL("an.example.org", url));

  auto* web_contents = GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  observer.WaitForNavigationFinished();
  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, observer.last_net_error_code());
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// DNR, WAR, and use_dynamic_url with the extension feature. DNR does not
// currently succeed when redirecting to a resource using use_dynamic_url with
// query parameters.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       DeclarativeNetRequest) {
  ExtensionTestMessageListener listener("ready");
  auto file_path = test_data_dir_.AppendASCII("web_accessible_resources/dnr");
  const Extension* extension = LoadExtension(file_path);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Navigate to a non-extension web page before beginning the test. This might
  // not be needed, but it will at the very least put the tab on a known url.
  {
    content::WebContents* web_contents = GetActiveWebContents();
    GURL gurl = embedded_test_server()->GetURL("example.com", "/simple.html");
    content::TestNavigationObserver navigation_observer(web_contents);
    ASSERT_TRUE(NavigateToURL(web_contents, gurl));
    ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
    EXPECT_EQ(gurl, web_contents->GetLastCommittedURL());
  }

  // Redirect from a webpage to a web accessible resource that has
  // `use_dynamic_url` set to true. The route is from a web page through DNR,
  // WAR, and on to a webpage using `use_dynamic_url`.
  {
    // Initialize redirection from example.com to example.org through DNR + WAR.
    GURL end(embedded_test_server()->GetURL("example.org", "/empty.html"));
    GURL start(
        base::StringPrintf("https://example.com/url?q=%s", end.spec().c_str()));

    // Navigate from within the page instead of from the Omnibox. That's because
    // in manual testing, this would succeed when the url is pasted into the
    // Omnibox but not when the same url is clicked from a link withing the
    // page.
    content::WebContents* web_contents = GetActiveWebContents();
    content::TestNavigationObserver navigation_observer(web_contents);
    ASSERT_TRUE(ExecJs(web_contents->GetPrimaryMainFrame(),
                       base::StringPrintf("window.location.href = '%s';",
                                          start.spec().c_str())));
    navigation_observer.Wait();

    // Verify that the expected end url has been reached. Execution of the
    // script on the `start` should redirect to `end`.
    EXPECT_EQ(end, navigation_observer.last_navigation_url());
    EXPECT_EQ(end, web_contents->GetLastCommittedURL());
    EXPECT_EQ(net::Error::OK, navigation_observer.last_net_error_code());
    EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
  }
}

// Verify setting script.src from a content script that relies on web request to
// redirect to a web accessible resource. It's important to set `script.src`
// using a script so that `CanRequestResource` has `upstream_url` set to
// something other than a chrome extension.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       WebRequestRedirectFromScript) {
  ExtensionTestMessageListener listener("ready");
  auto file_path = test_data_dir_.AppendASCII(
      "web_accessible_resources/web_request/redirect_from_script");
  const Extension* extension = LoadExtension(file_path);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Navigate to a non extension page.
  content::WebContents* web_contents = GetActiveWebContents();
  GURL gurl = embedded_test_server()->GetURL("example.com", "/empty.html");
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(content::NavigateToURL(web_contents, gurl));
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(gurl, web_contents->GetLastCommittedURL());
  EXPECT_EQ(net::Error::OK, navigation_observer.last_net_error_code());
}

// Tests an extension using webRequest to redirect a resource included in a
// page's static html.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       WebRequestRedirectFromPage) {
  ExtensionTestMessageListener listener("ready");
  auto file_path = test_data_dir_.AppendASCII(
      "web_accessible_resources/web_request/redirect_from_page");
  const Extension* extension = LoadExtension(file_path);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Navigate to a non extension page.
  content::WebContents* web_contents = GetActiveWebContents();
  GURL gurl = embedded_test_server()->GetURL(
      "example.com", "/extensions/api_test/webrequest/script/index.html");
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(content::NavigateToURL(web_contents, gurl));
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(gurl, web_contents->GetLastCommittedURL());
  EXPECT_EQ(net::Error::OK, navigation_observer.last_net_error_code());
}

// Succeed when DNR redirects a script to a WAR where use_dynamic_url is true.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest, DNRRedirect) {
  auto file_path =
      test_data_dir_.AppendASCII("web_accessible_resources/dnr/redirect");
  const Extension* extension = LoadExtension(file_path);
  ASSERT_TRUE(extension);

  // Navigate to a non extension page.
  content::WebContents* web_contents = GetActiveWebContents();
  GURL gurl =
      embedded_test_server()->GetURL("example.com", "/simple_with_script.html");
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(content::NavigateToURL(web_contents, gurl));
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(gurl, web_contents->GetLastCommittedURL());
  EXPECT_EQ(net::Error::OK, navigation_observer.last_net_error_code());
  auto result = EvalJs(web_contents, "document.body.textContent");
  EXPECT_EQ("dnr redirect success", result.ExtractString());
}

class WebAccessibleResourcesServiceWorkerBrowserTest
    : public WebAccessibleResourcesBrowserTest {
 public:
  WebAccessibleResourcesServiceWorkerBrowserTest() {
    UseHttpsTestServer();

    // Add any host names used by tests from this class to the test server's SSL
    // config since tests will navigate there.
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {"example.com"};
    embedded_test_server()->SetSSLConfig(cert_config);
  }

  ~WebAccessibleResourcesServiceWorkerBrowserTest() override = default;
  WebAccessibleResourcesServiceWorkerBrowserTest(
      const WebAccessibleResourcesServiceWorkerBrowserTest&) = delete;
  WebAccessibleResourcesServiceWorkerBrowserTest& operator=(
      const WebAccessibleResourcesServiceWorkerBrowserTest&) = delete;

 protected:
  void RegisterServiceWorker(const std::string& host_name,
                             const std::string& worker_path,
                             const std::optional<std::string>& scope) {
    auto* web_contents = GetActiveWebContents();
    GURL url = embedded_test_server()->GetURL(
        host_name, "/service_worker/create_service_worker.html");
    EXPECT_TRUE(NavigateToURL(web_contents, url));
    std::string script = content::JsReplace("register($1, $2);", worker_path,
                                            scope ? *scope : std::string());
    EXPECT_EQ("DONE", EvalJs(web_contents, script));
  }
};

// Test that DNR redirects to the extension's web accessible resource work when
// the page has a service worker. Unlike the WebAccessibleResourcesBrowserTest
// version, the service worker causes a renderer level redirect check for the
// web accessible resource.
// Regression test for crbug.com/375395102.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesServiceWorkerBrowserTest,
                       // TODO(crbug.com/429438829): Re-enable this test
                       DISABLED_DNRRedirect) {
  // Register a service worker and navigate to a page it controls.
  RegisterServiceWorker("example.com", "fetch_event_pass_through.js",
                        std::nullopt);
  EXPECT_TRUE(NavigateToURL(
      browser_window_interface(),
      embedded_test_server()->GetURL("example.com",
                                     "/service_worker/fetch_from_page.html")));

  const Extension* extension = LoadExtension(test_data_dir_.AppendASCII(
      "web_accessible_resources/dnr/redirect_with_initiator"));
  ASSERT_TRUE(extension);

  // Fetch the english page. It should be redirected to the extension's web
  // accessible resource. Note: we "lose" the service worker if we attempt to
  // navigate to the page instead, so a fetch is used here.
  auto result =
      EvalJs(GetActiveWebContents(), "fetch_from_page('/english_page.html');");

  std::string expected_content =
      "// Redirect with initiator's web accessible resource!";
  EXPECT_TRUE(result.ExtractString().find(expected_content) !=
              std::string::npos)
      << expected_content << " not found in " << result.ExtractString();
}

// Test server redirect to a web accessible or extension resource.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       BrowserProcessRedirect) {
  auto TestBrowserRedirect = [&](const char* kManifest,
                                 const char* kHistogramName) {
    // Load extension.
    TestExtensionDir test_dir;
    test_dir.WriteManifest(kManifest);
    test_dir.WriteFile(FILE_PATH_LITERAL("resource.html"), "resource.html");
    test_dir.WriteFile(FILE_PATH_LITERAL("web_accessible_resource.html"),
                       "web_accessible_resource.html");
    const Extension* extension = LoadExtension(test_dir.UnpackedPath());

    base::HistogramTester histogram_tester;

    // Test extension resource accessibility.
    auto server_redirect = [&](int expect_net_error, const char* resource,
                               bool is_accessible) {
      GURL gurl = embedded_test_server()->GetURL(
          "example.com",
          base::StringPrintf(
              "/server-redirect?%s",
              extension->GetResourceURL(resource).spec().c_str()));
      auto* web_contents = GetActiveWebContents();
      content::TestNavigationObserver observer(web_contents);
      // Some test cases force a net error, so NavigateToURL() may return
      // either true or false. Discard its return value.
      (void)NavigateToURL(web_contents, gurl);
      observer.WaitForNavigationFinished();
      EXPECT_EQ(extension->GetResourceURL(resource),
                observer.last_navigation_url());
      EXPECT_EQ(expect_net_error == net::OK,
                observer.last_navigation_succeeded());
      EXPECT_EQ(expect_net_error, observer.last_net_error_code());
      histogram_tester.ExpectBucketCount(kHistogramName, is_accessible, 1);
    };

    // Test cases.
    server_redirect(net::OK, "web_accessible_resource.html", true);
    server_redirect(net::ERR_BLOCKED_BY_CLIENT, "resource.html", false);
  };

  // Android only supports manifest V3.
#if !BUILDFLAG(IS_ANDROID)
  auto TestBrowserRedirectMV2 = [&]() {
    TestBrowserRedirect(
        R"({
          "name": "Test browser redirect",
          "version": "0.1",
          "manifest_version": 2,
          "web_accessible_resources": ["web_accessible_resource.html"]
        })",
        "Extensions.WAR.XOriginWebAccessible.MV2");
  };
  TestBrowserRedirectMV2();
#endif  // BUILDFLAG(IS_ANDROID)

  auto TestBrowserRedirectMV3 = [&]() {
    TestBrowserRedirect(
        R"({
          "name": "Redirect Test",
          "version": "0.1",
          "manifest_version": 3,
          "web_accessible_resources": [
            {
              "resources": ["web_accessible_resource.html"],
              "matches": ["http://example.com/*"]
            }
          ]
        })",
        "Extensions.WAR.XOriginWebAccessible.MV3");
  };
  TestBrowserRedirectMV3();
}

// Verify browser process redirect to an non web accessible resource. Navigate
// to a webpage that's redirected by DNR to a web server that initiates a
// redirect to a non web accessible extension resource.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       BrowserProcessRedirectMainframeReachability) {
  auto TestBrowserRedirectImpl = [&](const std::string& manifest) {
    // Load extension.
    TestExtensionDir test_dir;
    test_dir.WriteManifest(manifest);
    test_dir.WriteFile(FILE_PATH_LITERAL("inaccessible.html"),
                       "inaccessible.html");
    test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                       base::StringPrintf(
                           R"(
              // Promisable function for getDynamicRules(), which is unavailable
              // before MV3. Returns a promise that can be resolved or rejected.
              async function getDynamicRules() {
                return new Promise(async (resolve, reject) => {
                  chrome.declarativeNetRequest.getDynamicRules(rules => {
                    if (chrome.runtime.lastError) {
                      return reject(chrome.runtime.lastError);
                    }

                    return resolve(rules);
                  });
                });
              }

              // Ensure that the expected rule has loaded before continuing.
              async function waitForRuleId(ruleId) {
                const start = Date.now();
                const timeout = 3000;
                const sleep = 100;
                let rules;
                while (Date.now() - start < timeout) {
                  try {
                    rules = await getDynamicRules();
                  } catch(error) {
                    // Try to get rules again, until either success or timeout.
                    continue;
                  }

                  if (rules.some(rule => rule.id === ruleId)) {
                    // Exit this function now that the rule has been awaited.
                    return true;
                  }

                  // Sleep for a bit before trying again to match the rule.
                  await new Promise(resolve => setTimeout(resolve, sleep));
                  continue;
                }

                // A matching rule id wasn't found.
                return false;
              }

              chrome.runtime.onInstalled.addListener(async () => {
                const ruleId = 1;
                await chrome.declarativeNetRequest.updateDynamicRules({
                  addRules: [{
                    "id": ruleId,
                    "action": {
                      "type": "redirect",
                      "redirect": {
                        "url":
                          `%s?${chrome.runtime.getURL('inaccessible.html')}`
                      }
                    },
                    "condition": {
                      "urlFilter": "example.com*/empty.html",
                      "resourceTypes": ["main_frame"]
                    }
                  }]
                });

                chrome.test.assertTrue(await waitForRuleId(ruleId));
                chrome.test.notifyPass();
              });
            )",
                           embedded_test_server()
                               ->GetURL("b.example.com", "/server-redirect")
                               .spec()));
    extensions::ResultCatcher catcher;
    const Extension* extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(catcher.GetNextResult());

    // Navigate to a webpage that eventually navigates to an extension resource.
    int expect_net_error = net::ERR_BLOCKED_BY_CLIENT;
    const char* resource = "inaccessible.html";
    GURL gurl = embedded_test_server()->GetURL("a.example.com", "/empty.html");
    auto* web_contents = GetActiveWebContents();
    content::TestNavigationObserver observer(web_contents);
    // A network error is expected and this version of NavigateToURL() returns
    // false on navigation errors, similar to content::NavigateToURL().
    EXPECT_FALSE(NavigateToURL(web_contents, gurl));
    observer.WaitForNavigationFinished();
    EXPECT_EQ(expect_net_error == net::OK,
              observer.last_navigation_succeeded());
    EXPECT_EQ(expect_net_error, observer.last_net_error_code());
    EXPECT_EQ(extension->GetResourceURL(resource),
              observer.last_navigation_url());
  };

  using ManifestVersion = enum { MV3, MV2 };
  auto TestBrowserRedirect = [&TestBrowserRedirectImpl](
                                 ManifestVersion manifest_version) {
    std::string manifest_base = base::StringPrintf(
        R"(
          "name": "test",
          "version": "1",
          "manifest_version": %d
        )",
        manifest_version == MV3 ? 3 : 2);
    std::string manifest;

    switch (manifest_version) {
      case MV3:
        manifest =
            R"(
              "background": {"service_worker": "background.js"},
              "permissions": [
                "declarativeNetRequest",
                "declarativeNetRequestWithHostAccess"
              ],
              "host_permissions": [
                "<all_urls>"
              ]
            )";
        break;
      case MV2:
        manifest =
            R"(
              "background": {"scripts": ["background.js"]},
              "permissions": [
                "declarativeNetRequest",
                "declarativeNetRequestWithHostAccess",
                "<all_urls>"
              ]
            )";
        break;
    }

    manifest = base::StringPrintf("{%s, %s}", manifest_base, manifest);
    TestBrowserRedirectImpl(manifest);
  };

  TestBrowserRedirect(MV3);
  // Android only supports manifest V3.
#if !BUILDFLAG(IS_ANDROID)
  TestBrowserRedirect(MV2);
#endif
}

// Test dynamic origins in web accessible resources.
class DynamicOriginBrowserTest : public ExtensionBrowserTest {
 public:
  DynamicOriginBrowserTest() = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    InstallExtension();
  }

 protected:
  const Extension* GetExtension() { return extension_; }

  content::RenderFrameHost* GetPrimaryMainFrame() {
    return GetActiveWebContents()->GetPrimaryMainFrame();
  }

 private:
  void InstallExtension() {
    dir_.WriteManifest(R"({
      "name": "Extension",
      "version": "1.0",
      "manifest_version": 3,
      "web_accessible_resources": [{
        "resources": ["web_accessible_resource.html", "ok.html"],
        "matches": ["<all_urls>"]
      }]
    })");
    std::vector<std::string> files(
        {"extension_resource.html", "web_accessible_resource.html", "ok.html"});
    for (const auto& filename : files) {
      dir_.WriteFile(base::FilePath::FromASCII(filename).value(), filename);
    }
    extension_ = LoadExtension(dir_.UnpackedPath());
    DCHECK(extension_);
  }

  raw_ptr<const Extension, DanglingUntriaged> extension_ = nullptr;
  TestExtensionDir dir_;
};

// Test a dynamic url as a web accessible resource.
IN_PROC_BROWSER_TEST_F(DynamicOriginBrowserTest, DynamicUrl) {
  auto* extension = GetExtension();

  // Resource and extension origin should match.
  {
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                              extension->GetResourceURL("ok.html")));
    ASSERT_EQ(extension->origin(),
              GetPrimaryMainFrame()->GetLastCommittedOrigin());
  }

  // Dynamic resource should resolve to static url.
  {
    GURL static_url = extension->url().Resolve("ok.html");
    GURL dynamic_url = extension->dynamic_url().Resolve("ok.html");
    ASSERT_NE(static_url, dynamic_url);
    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), dynamic_url));
    EXPECT_EQ(static_url, GetPrimaryMainFrame()->GetLastCommittedURL());
    EXPECT_EQ(extension->origin(),
              GetPrimaryMainFrame()->GetLastCommittedOrigin());
  }
}

// Error accessing resource from random guid.
IN_PROC_BROWSER_TEST_F(DynamicOriginBrowserTest,
                       InvalidDynamicResourceFailsToLoad) {
  auto* extension = GetExtension();

  auto run = [&](const GURL& gurl, int status) {
    content::WebContents* web_contents = GetActiveWebContents();
    content::TestNavigationObserver nav_observer(web_contents);
    web_contents->GetController().LoadURL(
        gurl, content::Referrer(), ui::PageTransition::PAGE_TRANSITION_TYPED,
        std::string());
    nav_observer.Wait();
    EXPECT_EQ(status == net::OK, nav_observer.last_navigation_succeeded());
    EXPECT_EQ(status, nav_observer.last_net_error_code());
  };

  auto random_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  GURL random_url =
      Extension::GetBaseURLFromExtensionId(random_guid).Resolve("ok.html");
  GURL dynamic_url = extension->dynamic_url().Resolve("ok.html");
  run(random_url, net::ERR_BLOCKED_BY_CLIENT);
  run(dynamic_url, net::OK);
}

// Web Accessible Resources.
IN_PROC_BROWSER_TEST_F(DynamicOriginBrowserTest, FetchGuidFromFrame) {
  auto* extension = GetExtension();

  // Fetch url from frame to verify with expectations.
  auto test_frame_with_fetch = [&](const GURL& frame_url,
                                   const GURL& expected_frame_url,
                                   const GURL& fetch_url,
                                   const char* expected_fetch_url_contents) {
    SCOPED_TRACE(testing::Message() << "test_frame_with_fetch"
                                    << ": frame_url = " << frame_url
                                    << "; fetch_url = " << fetch_url);
    // Fetch and test resource.
    content::WebContents* web_contents = GetActiveWebContents();
    ASSERT_TRUE(NavigateToURL(web_contents, frame_url));
    EXPECT_EQ(expected_frame_url,
              web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());

    constexpr char kFetchScriptTemplate[] =
        R"(
        fetch($1).then(result => {
          return result.text();
        }).catch(err => {
          return String(err);
        });)";
    EXPECT_EQ(
        expected_fetch_url_contents,
        content::EvalJs(web_contents, content::JsReplace(kFetchScriptTemplate,
                                                         fetch_url.spec())));
  };

  const struct {
    const char* title;
    GURL frame_url;
    GURL expected_frame_url;
    GURL fetch_url;
    const char* expected_fetch_url_contents;
  } test_cases[] = {
      {
          "Fetch web accessible resource from extension resource.",
          extension->url().Resolve("extension_resource.html"),
          extension->url().Resolve("extension_resource.html"),
          extension->url().Resolve("web_accessible_resource.html"),
          "web_accessible_resource.html",
      },
      {
          "Fetch dynamic web accessible resource from extension resource.",
          extension->url().Resolve("extension_resource.html"),
          extension->url().Resolve("extension_resource.html"),
          extension->dynamic_url().Resolve("web_accessible_resource.html"),
          "web_accessible_resource.html",
      },
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << test_case.title);
    test_frame_with_fetch(test_case.frame_url, test_case.expected_frame_url,
                          test_case.fetch_url,
                          test_case.expected_fetch_url_contents);
  }
}

// Fetch web accessible resources directly from a file:// page.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       FileSchemeInitiators_MainWorld) {
  // Load extension.
  TestExtensionDir extension_dir;
  const char* kManifestValue = R"({
    "name": "Test",
    "version": "0.1",
    "manifest_version": 3,
    "web_accessible_resources": [
      {
        "resources": [ "ok_0.html" ],
        "matches": [ "file://*/*" ]
      },
      {
        "resources": [ "ok_1.html" ],
        "matches": [ "<all_urls>" ]
      },
      {
        "resources": [ "no_0.html" ],
        "matches": [ "http://*.example.com/*" ]
      },
      {
        "resources": [ "no_1.html" ],
        "matches": [ "*://*/*" ]
      }
    ]
  })";
  extension_dir.WriteManifest(kManifestValue);
  extension_dir.WriteFile(FILE_PATH_LITERAL("ok_0.html"), "ok_0.html");
  extension_dir.WriteFile(FILE_PATH_LITERAL("ok_1.html"), "ok_1.html");
  extension_dir.WriteFile(FILE_PATH_LITERAL("no_0.html"), "no_0.html");
  extension_dir.WriteFile(FILE_PATH_LITERAL("no_1.html"), "no_1.html");
  const Extension* extension =
      LoadExtension(extension_dir.UnpackedPath(), {.allow_file_access = true});

  // Navigate to extension's index.html via file:// and test.
  base::FilePath test_page;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_page));
  test_page = test_page.AppendASCII("simple.html");
  GURL gurl = net::FilePathToFileURL(test_page);
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, gurl));
  static constexpr char kScriptTemplate[] = R"(
    // Verify that web accessible resource can be fetched.
    async function run(isAllowed, filename) {
      return new Promise(async (resolve, reject) => {
        const url = `chrome-extension://%s/${filename}`;

        // Fetch and verify the contents of fetched web accessible resources.
        const verifyFetch = (actual) => {
          if (isAllowed == (filename == actual)) {
            resolve();
          } else {
            reject(`Unexpected result. File: ${filename}. Found: ${actual}`);
          }
        };
        fetch(url)
            .then(result => result.text())
            .catch(error => verifyFetch(error))
            .then(text => verifyFetch(text));
      });
    }

    // Run tests.
    const testCases = [
      [true, 'ok_0.html'],
      [true, 'ok_1.html'],
      [false, 'no_0.html'],
      [false, 'no_1.html']
    ];
    const tests = testCases.map(testCase => run(...testCase));
    Promise.all(tests).then(response => true);
  )";
  std::string script =
      base::StringPrintf(kScriptTemplate, extension->id().c_str());
  ASSERT_TRUE(content::EvalJs(web_contents, script).ExtractBool());
}

// Test loading of subresources using an initiator coming from a file:// scheme,
// and, notably, from within a content script context.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserTest,
                       FileSchemeInitiators_ContentScript) {
  // Load extension.
  TestExtensionDir test_dir;
  const char* kManifestJson = R"({
    "name": "Test",
    "version": "0.1",
    "manifest_version": 3,
    "background": {"service_worker": "service_worker.js"},
    "host_permissions": ["file:///*"],
    "permissions": ["scripting"],
    "web_accessible_resources": [
      {
        "resources": [ "ok_0.html" ],
        "matches": [ "file://*/*" ]
      },
      {
        "resources": [ "ok_1.html" ],
        "matches": [ "<all_urls>" ]
      },
      {
        "resources": [ "no_0.html" ],
        "matches": [ "http://*.example.com/*" ]
      },
      {
        "resources": [ "no_1.html" ],
        "matches": [ "*://*/*" ]
      }
    ]
  })";
  test_dir.WriteManifest(kManifestJson);
  test_dir.WriteFile(FILE_PATH_LITERAL("ok_0.html"), "ok_0.html");
  test_dir.WriteFile(FILE_PATH_LITERAL("ok_1.html"), "ok_1.html");
  test_dir.WriteFile(FILE_PATH_LITERAL("no_0.html"), "no_0.html");
  test_dir.WriteFile(FILE_PATH_LITERAL("no_1.html"), "no_1.html");
  test_dir.WriteFile(FILE_PATH_LITERAL("service_worker.js"), "");
  const char* kTestJs = R"(
    // Verify that web accessible resource can be fetched.
    async function run(isAllowed, filename) {
      return new Promise(async resolve => {
        const url = chrome.runtime.getURL(filename);

        // Fetch and verify the contents of fetched web accessible resources.
        const verifyFetch = (actual) => {
          chrome.test.assertEq(isAllowed, filename == actual);
          resolve();
        };
        fetch(url)
            .then(result => result.text())
            .catch(error => verifyFetch(error))
            .then(text => verifyFetch(text));
      });
    }

    // Run tests.
    const testCases = [
      [true, 'ok_0.html'],
      [true, 'ok_1.html'],
      [false, 'no_0.html'],
      [false, 'no_1.html']
    ];
    const tests = testCases.map(testCase => run(...testCase));
    Promise.all(tests).then(() => chrome.test.succeed());
  )";
  test_dir.WriteFile(FILE_PATH_LITERAL("test.js"), kTestJs);
  const Extension* extension =
      LoadExtension(test_dir.UnpackedPath(), {.allow_file_access = true});

  // Navigate to extension's index.html via file:// and test.
  ResultCatcher catcher;
  base::FilePath test_page;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_page);
  test_page = test_page.AppendASCII("simple.html");
  GURL gurl = net::FilePathToFileURL(test_page);
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, gurl));
  const int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  static constexpr char kScript[] =
      R"((async () => {
        await chrome.scripting.executeScript(
          {target: {tabId: %d}, files: ['test.js']})
      })();)";
  BackgroundScriptExecutor::ExecuteScriptAsync(
      profile(), extension->id(), base::StringPrintf(kScript, tab_id));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Useful for testing web accessible resources loaded from a content script.
class WebAccessibleResourcesDynamicUrlScriptingBrowserTest
    : public WebAccessibleResourcesBrowserTest {
 public:
  WebAccessibleResourcesDynamicUrlScriptingBrowserTest() = default;

 protected:
  const Extension* GetExtension(const char* manifest_piece) {
    // manifest.json.
    static constexpr char kManifestJson[] = R"({
      "name": "Test",
      "version": "1.0",
      "manifest_version": 3,
      "host_permissions": ["<all_urls>"],
      "web_accessible_resources": [
        {
          "resources": ["dynamic_web_accessible_resource.html"],
          "matches": ["<all_urls>"],
          "use_dynamic_url": true
        },
        {
          "resources": ["web_accessible_resource.html"],
          "matches": ["<all_urls>"]
        }
      ],
      %s
    })";
    auto kManifest = base::StringPrintf(kManifestJson, manifest_piece);
    test_dir_.WriteManifest(kManifest);

    // content.js
    static constexpr char kTestScript[] = R"(
      // Verify that web accessible resource can be fetched.
      async function run(isAllowed, filename, identifier, query = '') {
        return new Promise(async resolve => {
          // Verify URL.
          let expected = chrome.runtime.getURL(`${filename}${query}`);
          let url = `chrome-extension://${identifier}/${filename}${query}`;
          chrome.test.assertEq(isAllowed, expected == url);

          // Verify contents of fetched web accessible resource.
          const verify = (actual) => {
            chrome.test.assertEq(isAllowed, filename == actual);
            resolve();
          };

          // Fetch web accessible resource.
          fetch(url)
              .then(result => {
                // With `use_dynamic_url` set to `true`, we redirect from the
                // first URL using the GUID to the static extension origin.
                // Ensure query parameters are not lost in the redirect.
                chrome.test.assertEq(new URL(result.url).search, query);
                return result.text();
              })
              .catch(error => verify(error))
              .then(text => verify(text));
        });
      }

      // Verify that identifiers don't match.
      const id = chrome.runtime.id;
      const dynamicId = chrome.runtime.dynamicId;
      chrome.test.assertTrue(id != dynamicId);

      // Run tests with arguments [[isAllowed, filename, identifier]].
      const testCases = [
        [true, 'dynamic_web_accessible_resource.html', dynamicId],
        [true, 'dynamic_web_accessible_resource.html', dynamicId, '?foo=bar'],
        [true, 'web_accessible_resource.html', id],
        [false, 'web_accessible_resource.html', 'error'],
        [false, 'dynamic_web_accessible_resource.html', 'error'],
      ];
      const tests = testCases.map(testCase => run(...testCase));
      Promise.all(tests).then(() => chrome.test.succeed());
    )";

    // Write files and load extension.
    WriteFile(FILE_PATH_LITERAL("content.js"), kTestScript);
    WriteFile(FILE_PATH_LITERAL("dynamic_web_accessible_resource.html"),
              "dynamic_web_accessible_resource.html");
    WriteFile(FILE_PATH_LITERAL("web_accessible_resource.html"),
              "web_accessible_resource.html");
    const Extension* extension = LoadExtension(test_dir_.UnpackedPath());
    return extension;
  }

  // Write file to extension directory.
  void WriteFile(const base::FilePath::CharType* filename,
                 const char* contents) {
    test_dir_.WriteFile(filename, contents);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedCurrentChannel current_channel_{version_info::Channel::CANARY};
  TestExtensionDir test_dir_;
};

// Load dynamic web accessible resource from a content script.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesDynamicUrlScriptingBrowserTest,
                       ContentScript) {
  static constexpr char kManifest[] = R"(
    "content_scripts": [
      {
        "matches": ["<all_urls>"],
        "js": ["content.js"],
        "run_at": "document_start"
      }
    ]
  )";
  const Extension* extension = GetExtension(kManifest);
  ASSERT_TRUE(extension);

  ResultCatcher catcher;
  GURL gurl = embedded_test_server()->GetURL("example.com", "/empty.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), gurl));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Load dynamic web accessible resources via chrome.scripting.executeScript().
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesDynamicUrlScriptingBrowserTest,
                       ExecuteScript) {
  // Load extension.
  WriteFile(FILE_PATH_LITERAL("worker.js"), "// Intentionally blank.");
  static constexpr char kManifest[] = R"(
    "permissions": ["scripting"],
    "background": {"service_worker": "worker.js"}
  )";
  const Extension* extension = GetExtension(kManifest);
  ASSERT_TRUE(extension);

  // Navigate to a non extension page and test.
  ResultCatcher catcher;
  GURL gurl = embedded_test_server()->GetURL("example.com", "/empty.html");
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, gurl));
  const int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  static constexpr char kScript[] =
      R"((async () => {
        await chrome.scripting.executeScript(
          {target: {tabId: %d}, files: ['content.js']})
      })();)";
  BackgroundScriptExecutor::ExecuteScriptAsync(
      profile(), extension->id(), base::StringPrintf(kScript, tab_id));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace
}  // namespace extensions
