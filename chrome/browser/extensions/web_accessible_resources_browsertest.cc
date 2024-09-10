// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

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

// Exercise web accessible resources with experimental extension features.
class WebAccessibleResourcesBrowserTest : public ExtensionBrowserTest {
 public:
  explicit WebAccessibleResourcesBrowserTest(bool enable_feature = true) {
    feature_list_.InitWithFeatureState(
        extensions_features::kExtensionDynamicURLRedirection, enable_feature);
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    EXPECT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  ScopedCurrentChannel current_channel_{version_info::Channel::CANARY};
};

// Exercise web accessible resources without experimental extension features.
class WebAccessibleResourcesNonGuidBrowserTest
    : public WebAccessibleResourcesBrowserTest {
 public:
  WebAccessibleResourcesNonGuidBrowserTest()
      : WebAccessibleResourcesBrowserTest(false) {}
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

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
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
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

// A test suite that will run both with and without the dynamic URL feature
// enabled.
class ParameterizedWebAccessibleResourcesBrowserTest
    : public WebAccessibleResourcesBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ParameterizedWebAccessibleResourcesBrowserTest()
      : WebAccessibleResourcesBrowserTest(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         ParameterizedWebAccessibleResourcesBrowserTest,
                         testing::Bool());

// DNR, WAR, and use_dynamic_url with the extension feature. DNR does not
// currently succeed when redirecting to a resource using use_dynamic_url with
// query parameters.
IN_PROC_BROWSER_TEST_P(ParameterizedWebAccessibleResourcesBrowserTest,
                       DeclarativeNetRequest) {
  ExtensionTestMessageListener listener("ready");
  auto file_path = test_data_dir_.AppendASCII("web_accessible_resources/dnr");
  const Extension* extension = LoadExtension(file_path);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Navigate to a non-extension web page before beginning the test. This might
  // not be needed, but it will at the very least put the tab on a known url.
  {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    GURL gurl = embedded_test_server()->GetURL("example.com", "/simple.html");
    content::TestNavigationObserver navigation_observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
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
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
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

// If `use_dynamic_url` is set to true in manifest.json, then the associated web
// accessible resource(s) can only be loaded using the dynamic url if using the
// extension feature. If not using the extension feature, dynamic URLs can be
// loaded using static urls.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesNonGuidBrowserTest,
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  std::string script =
      base::StringPrintf(kFetchResourceScriptTemplate,
                         extension->guid().c_str(), extension->id().c_str(), R"(
      ["Load a static resource with a dynamic url", 'static.html', true, false],
      ["Load a static resource with a static url", 'static.html', false, true],
      ["Load dynamic resource with a dynamic url", 'dynamic.html', true, false],
      ["Load dynamic resource with a static url", 'dynamic.html', false, true],
      )");
  ASSERT_TRUE(content::EvalJs(web_contents, script).ExtractBool());
}

// Verify setting script.src from a content script that relies on web request to
// redirect to a web accessible resource. It's important to set `script.src`
// using a script so that `CanRequestResource` has `upstream_url` set to
// something other than a chrome extension.
IN_PROC_BROWSER_TEST_P(ParameterizedWebAccessibleResourcesBrowserTest,
                       WebRequestRedirectFromScript) {
  ExtensionTestMessageListener listener("ready");
  auto file_path = test_data_dir_.AppendASCII(
      "web_accessible_resources/web_request/redirect_from_script");
  const Extension* extension = LoadExtension(file_path);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Navigate to a non extension page.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL gurl = embedded_test_server()->GetURL("example.com", "/empty.html");
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(gurl, web_contents->GetLastCommittedURL());
  EXPECT_EQ(net::Error::OK, navigation_observer.last_net_error_code());
}

// Tests an extension using webRequest to redirect a resource included in a
// page's static html.
IN_PROC_BROWSER_TEST_P(ParameterizedWebAccessibleResourcesBrowserTest,
                       WebRequestRedirectFromPage) {
  ExtensionTestMessageListener listener("ready");
  auto file_path = test_data_dir_.AppendASCII(
      "web_accessible_resources/web_request/redirect_from_page");
  const Extension* extension = LoadExtension(file_path);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Navigate to a non extension page.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL gurl = embedded_test_server()->GetURL(
      "example.com", "/extensions/api_test/webrequest/script/index.html");
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(gurl, web_contents->GetLastCommittedURL());
  EXPECT_EQ(net::Error::OK, navigation_observer.last_net_error_code());
}

// Succeed when DNR redirects a script to a WAR where use_dynamic_url is true.
IN_PROC_BROWSER_TEST_P(ParameterizedWebAccessibleResourcesBrowserTest,
                       DNRRedirect) {
  auto file_path =
      test_data_dir_.AppendASCII("web_accessible_resources/dnr/redirect");
  const Extension* extension = LoadExtension(file_path);
  ASSERT_TRUE(extension);

  // Navigate to a non extension page.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL gurl =
      embedded_test_server()->GetURL("example.com", "/simple_with_script.html");
  content::TestNavigationObserver navigation_observer(web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
  EXPECT_EQ(gurl, web_contents->GetLastCommittedURL());
  EXPECT_EQ(net::Error::OK, navigation_observer.last_net_error_code());
  auto result = EvalJs(web_contents, "document.body.textContent");
  EXPECT_EQ("dnr redirect success", result.ExtractString());
}

class WebAccessibleResourcesBrowserRedirectTest
    : public WebAccessibleResourcesBrowserTest {
 protected:
  void TestBrowserRedirect(const char* kManifest, const char* kHistogramName) {
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
      auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
      content::TestNavigationObserver observer(web_contents);
      EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), gurl));
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
    server_redirect(net::OK, "resource.html", false);
  }
};

// Test server redirect to a web accessible or extension resource.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserRedirectTest, MV2) {
  TestBrowserRedirect(
      R"({
      "name": "Test browser redirect",
      "version": "0.1",
      "manifest_version": 2,
      "web_accessible_resources": ["web_accessible_resource.html"]
    })",
      "Extensions.WAR.XOriginWebAccessible.MV2");
}

// Test server redirect to a web accessible or extension resource.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesBrowserRedirectTest, MV3) {
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
}

}  // namespace
}  // namespace extensions
