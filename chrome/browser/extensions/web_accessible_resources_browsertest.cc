// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
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

// Test manifest.json's use_dynamic_url restriction requiring only dynamic urls.
class WebAccessibleResourcesBrowserTest : public ExtensionBrowserTest {
 public:
  WebAccessibleResourcesBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionDynamicURLRedirection);
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

  static constexpr char kScriptTemplate[] = R"(
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

    // Run tests.
    const testCases = [
      // Arguments: [title, filename, useDynamicUrl, isAllowed].
      ["Load a static resource with a dynamic url", 'static.html', true, true],
      ["Load a static resource with a static url", 'static.html', false, true],
      ["Load dynamic resource with a dynamic url", 'dynamic.html', true, true],
      ["Load dynamic resource with a static url", 'dynamic.html', false, false],
    ];
    const tests = testCases.map(testCase => test(...testCase));
    Promise.all(tests).then(response => true);
  )";

  std::string script = base::StringPrintf(
      kScriptTemplate, extension->guid().c_str(), extension->id().c_str());
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

class WebAccessibleResourcesNonGuidBrowserTest : public ExtensionBrowserTest {
 public:
  WebAccessibleResourcesNonGuidBrowserTest() {
    feature_list_.InitAndDisableFeature(
        extensions_features::kExtensionDynamicURLRedirection);
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

// DNR, WAR, and use_dynamic_url without the extension feature.
// TODO(crbug.com/354748637): Deduplicate from
// WebAccessibleResourcesNonGuidBrowserTest after merge.
IN_PROC_BROWSER_TEST_F(WebAccessibleResourcesNonGuidBrowserTest,
                       DeclarativeNetRequest) {
  ExtensionTestMessageListener listener("ready");
  auto file_path = test_data_dir_.AppendASCII("web_accessible_resources/dnr");
  const Extension* extension = LoadExtension(file_path);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Navigate to a blank page in the extension as though it's from the web. This
  // might not be needed. This will only put the tab on a known url.
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
// TODO(crbug.com/354748637): Deduplicate from
// WebAccessibleResourcesBrowserTest.UseDynamicUrlInFetch after merge.
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

  static constexpr char kScriptTemplate[] = R"(
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

    // Run tests.
    const testCases = [
      // Arguments: [title, filename, useDynamicUrl, isAllowed].
      ["Load a static resource with a static url", 'static.html', false, true],
      ["Load dynamic resource with a static url", 'dynamic.html', false, true],
    ];
    const tests = testCases.map(testCase => test(...testCase));
    Promise.all(tests).then(response => true);
  )";

  std::string script = base::StringPrintf(
      kScriptTemplate, extension->guid().c_str(), extension->id().c_str());
  ASSERT_TRUE(content::EvalJs(web_contents, script).ExtractBool());
}

// TODO(crbug.com/355668502): Add a test for content scripts.

// TODO(crbug.com/352455685): Write a test for DNR and WAR.

// TODO(crbug.com/352267920): Write a test to ensure that server redirects work
// fine from this point. It already exists at
// CrossExtensionEmbeddingOfWebAccessibleResources, but localize it here to
// detect early exit from IsResourceWebAccessible, such as:
// if (!upstream_url.is_empty() && !upstream_url.SchemeIs(kExtensionScheme)) {
//   // return false;
// }

// TODO(crbug.com/352267920): Create a test for guid based on
// accessible_link_resource.html;drc=9a60d160b6dfb2351ae0dad28341c3ca80f1ca59.

}  // namespace
}  // namespace extensions
