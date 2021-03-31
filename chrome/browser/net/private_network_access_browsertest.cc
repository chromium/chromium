// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/files/file_util.h"
#include "base/strings/string_piece.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/dom_distiller/test_distillation_observers.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/test_distillability_observer.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

namespace {

using blink::mojom::WebFeature;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;

// We use a custom page that explicitly disables its own favicon (by providing
// an invalid data: URL for it) so as to prevent the browser from making an
// automatic request to /favicon.ico. This is because the automatic request
// messes with our tests, in which we want to trigger a single request from the
// web page to a resource of our choice and observe the side-effect in metrics.
constexpr char kNoFaviconPath[] = "/no-favicon.html";

// Same as kNoFaviconPath, except it carries a header that makes the browser
// consider it came from the `public` address space, irrespective of the fact
// that we loaded the web page from localhost.
constexpr char kTreatAsPublicAddressPath[] =
    "/no-favicon-treat-as-public-address.html";

GURL SecureURL(const net::EmbeddedTestServer& server, const std::string& path) {
  // Test HTTPS servers cannot lie about their hostname, so they yield URLs
  // starting with https://localhost. http://localhost is already a secure
  // context, so we do not bother instantiating an HTTPS server.
  return server.GetURL(path);
}

GURL NonSecureURL(const net::EmbeddedTestServer& server,
                  const std::string& path) {
  return server.GetURL("foo.test", path);
}

GURL LocalSecureURL(const net::EmbeddedTestServer& server) {
  return SecureURL(server, kNoFaviconPath);
}

GURL LocalNonSecureURL(const net::EmbeddedTestServer& server) {
  return NonSecureURL(server, kNoFaviconPath);
}

GURL PublicSecureURL(const net::EmbeddedTestServer& server) {
  return SecureURL(server, kTreatAsPublicAddressPath);
}

GURL PublicNonSecureURL(const net::EmbeddedTestServer& server) {
  return NonSecureURL(server, kTreatAsPublicAddressPath);
}

// Similar to LocalNonSecure() but can be fetched by any origin.
GURL LocalNonSecureWithCrossOriginCors(const net::EmbeddedTestServer& server) {
  return SecureURL(server, "/cors-ok.txt");
}

// The returned script evaluates to a boolean indicating whether the fetch
// succeeded or not.
std::string FetchScript(const GURL& url) {
  return content::JsReplace(
      "fetch($1).then(response => true).catch(error => false)", url);
}

constexpr WebFeature kAllAddressSpaceFeatures[] = {
    WebFeature::kAddressSpacePrivateSecureContextEmbeddedLocal,
    WebFeature::kAddressSpacePrivateNonSecureContextEmbeddedLocal,
    WebFeature::kAddressSpacePublicSecureContextEmbeddedLocal,
    WebFeature::kAddressSpacePublicNonSecureContextEmbeddedLocal,
    WebFeature::kAddressSpaceUnknownSecureContextEmbeddedLocal,
    WebFeature::kAddressSpaceUnknownNonSecureContextEmbeddedLocal,
    WebFeature::kAddressSpacePublicSecureContextEmbeddedPrivate,
    WebFeature::kAddressSpacePublicNonSecureContextEmbeddedPrivate,
    WebFeature::kAddressSpaceUnknownSecureContextEmbeddedPrivate,
    WebFeature::kAddressSpaceUnknownNonSecureContextEmbeddedPrivate,
    WebFeature::kAddressSpacePrivateSecureContextNavigatedToLocal,
    WebFeature::kAddressSpacePrivateNonSecureContextNavigatedToLocal,
    WebFeature::kAddressSpacePublicSecureContextNavigatedToLocal,
    WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal,
    WebFeature::kAddressSpaceUnknownSecureContextNavigatedToLocal,
    WebFeature::kAddressSpaceUnknownNonSecureContextNavigatedToLocal,
    WebFeature::kAddressSpacePublicSecureContextNavigatedToPrivate,
    WebFeature::kAddressSpacePublicNonSecureContextNavigatedToPrivate,
    WebFeature::kAddressSpaceUnknownSecureContextNavigatedToPrivate,
    WebFeature::kAddressSpaceUnknownNonSecureContextNavigatedToPrivate,
};

// Returns a map of WebFeature to bucket count. Skips buckets with zero counts.
std::map<WebFeature, int> GetAddressSpaceFeatureBucketCounts(
    const base::HistogramTester& tester) {
  std::map<WebFeature, int> counts;
  for (WebFeature feature : kAllAddressSpaceFeatures) {
    int count = tester.GetBucketCount("Blink.UseCounter.Features", feature);
    if (count == 0) {
      continue;
    }

    counts.emplace(feature, count);
  }
  return counts;
}

// Private Network Access is a web platform specification aimed at securing
// requests made from public websites to the private network and localhost. It
// is entirely implemented in content/. Its integration with Blink UseCounters
// cannot be tested in content/, however, thus we define this standalone test
// here.
//
// See also:
//
//  - specification: https://wicg.github.io/private-network-access.
//  - feature browsertests in content/: RenderFrameHostImplTest.
//
class PrivateNetworkAccessBrowserTest : public InProcessBrowserTest {
 public:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool NavigateAndFlushHistograms() {
    // Commit a new navigation in order to flush UseCounters incremented during
    // the last navigation to the browser process, so they are reflected in
    // histograms.
    return content::NavigateToURL(web_contents(), GURL("about:blank"));
  }

  // Never returns nullptr. The returned server is already Start()ed.
  //
  // NOTE: This is defined as a method on the test fixture instead of a free
  // function because GetChromeTestDataDir() is a test fixture method itself.
  // We return a unique_ptr because EmbeddedTestServer is not movable and C++17
  // support is not available at time of writing.
  std::unique_ptr<net::EmbeddedTestServer> NewServer(
      net::EmbeddedTestServer::Type server_type =
          net::EmbeddedTestServer::TYPE_HTTP) {
    std::unique_ptr<net::EmbeddedTestServer> server =
        std::make_unique<net::EmbeddedTestServer>(server_type);
    server->AddDefaultHandlers(GetChromeTestDataDir());
    EXPECT_TRUE(server->Start());
    return server;
  }

 private:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// This test verifies that no feature is counted for the initial navigation from
// a new tab to a page served by localhost.
//
// Regression test for https://crbug.com/1134601.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DoesNotRecordAddressSpaceFeatureForInitialNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));
  EXPECT_TRUE(NavigateAndFlushHistograms());

  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());
}

// This test verifies that no feature is counted for top-level navigations from
// a public page to a local page.
//
// TODO(crbug.com/1129326): Revisit this once the story around top-level
// navigations is closer to being resolved. Counting these events will help
// decide what to do.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DoesNotRecordAddressSpaceFeatureForRegularNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));
  EXPECT_TRUE(content::NavigateToURL(web_contents(), LocalSecureURL(*server)));
  EXPECT_TRUE(NavigateAndFlushHistograms());

  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());
}

// This test verifies that when a secure context served from the public address
// space loads a resource from the local network, the correct WebFeature is
// use-counted.
// Disabled, as explained in https://crbug.com/1143206
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       DISABLED_RecordsAddressSpaceFeatureForFetch) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(content::NavigateToURL(web_contents(), PublicSecureURL(*server)));
  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("defaultresponse").then(response => response.ok)
  )"));
  EXPECT_TRUE(NavigateAndFlushHistograms());

  EXPECT_THAT(
      GetAddressSpaceFeatureBucketCounts(histogram_tester),
      ElementsAre(
          Pair(WebFeature::kAddressSpacePublicSecureContextEmbeddedLocal, 1)));
}

// This test verifies that when a non-secure context served from the public
// address space loads a resource from the local network, the correct WebFeature
// is use-counted.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    DISABLED_RecordsAddressSpaceFeatureForFetchInNonSecureContext) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("defaultresponse").then(response => response.ok)
  )"));
  EXPECT_TRUE(NavigateAndFlushHistograms());

  EXPECT_THAT(
      GetAddressSpaceFeatureBucketCounts(histogram_tester),
      ElementsAre(Pair(
          WebFeature::kAddressSpacePublicNonSecureContextEmbeddedLocal, 1)));
}

// This test verifies that when page embeds an empty iframe pointing to
// about:blank, no address space feature is recorded. It serves as a basis for
// comparison with the following tests, which test behavior with iframes.
IN_PROC_BROWSER_TEST_F(
    PrivateNetworkAccessBrowserTest,
    DoesNotRecordAddressSpaceFeatureForAboutBlankNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    new Promise(resolve => {
      const child = document.createElement("iframe");
      child.src = "about:blank";
      child.onload = () => { resolve(true); };
      document.body.appendChild(child);
    })
  )"));
  EXPECT_TRUE(NavigateAndFlushHistograms());

  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());
}

// This test verifies that when a non-secure context served from the public
// address space loads a child frame from the local network, the correct
// WebFeature is use-counted.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       RecordsAddressSpaceFeatureForChildNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));

  base::StringPiece script_template = R"(
    new Promise(resolve => {
      const child = document.createElement("iframe");
      child.src = $1;
      child.onload = () => { resolve(true); };
      document.body.appendChild(child);
    })
  )";
  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template,
                                               LocalNonSecureURL(*server))));
  EXPECT_TRUE(NavigateAndFlushHistograms());

  EXPECT_THAT(
      GetAddressSpaceFeatureBucketCounts(histogram_tester),
      ElementsAre(Pair(
          WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1)));
}

// This test verifies that when a non-secure context served from the public
// address space loads a grand-child frame from the local network, the correct
// WebFeature is use-counted. If inheritance did not work correctly, the
// intermediate about:blank frame might confuse the address space logic.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessBrowserTest,
                       RecordsAddressSpaceFeatureForGrandchildNavigation) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));

  base::StringPiece script_template = R"(
    function addChildFrame(doc, src) {
      return new Promise(resolve => {
        const child = doc.createElement("iframe");
        child.src = src;
        child.onload = () => { resolve(child); };
        doc.body.appendChild(child);
      });
    }

    addChildFrame(document, "about:blank")
      .then(child => addChildFrame(child.contentDocument, $1))
      .then(grandchild =>  true);
  )";
  EXPECT_EQ(true,
            content::EvalJs(web_contents(),
                            content::JsReplace(script_template,
                                               LocalNonSecureURL(*server))));
  EXPECT_TRUE(NavigateAndFlushHistograms());

  EXPECT_THAT(
      GetAddressSpaceFeatureBucketCounts(histogram_tester),
      ElementsAre(Pair(
          WebFeature::kAddressSpacePublicNonSecureContextNavigatedToLocal, 1)));
}

class PrivateNetworkAccessWithFeatureEnabledBrowserTest
    : public PrivateNetworkAccessBrowserTest {
 public:
  PrivateNetworkAccessWithFeatureEnabledBrowserTest() {
    std::vector<base::Feature> enabled_features = {
        features::kBlockInsecurePrivateNetworkRequests,
        dom_distiller::kReaderMode,
    };
    std::vector<base::Feature> disabled_features;
    features_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableDomDistiller);
  }

 private:
  void SetUpOnMainThread() override {
    // The distiller needs to run in an isolated environment. For tests we
    // can simply use the last value available.
    if (!dom_distiller::DistillerJavaScriptWorldIdIsSet()) {
      dom_distiller::SetDistillerJavaScriptWorldId(
          content::ISOLATED_WORLD_ID_CONTENT_END);
    }
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  base::test::ScopedFeatureList features_;
};

// This test verifies that private network requests that are blocked do not
// result in a WebFeature being use-counted.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       DoesNotRecordAddressSpaceFeatureForBlockedRequests) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();

  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), PublicNonSecureURL(*server)));
  EXPECT_EQ(true, content::EvalJs(web_contents(), R"(
    fetch("defaultresponse").catch(() => true)
  )"));
  EXPECT_TRUE(NavigateAndFlushHistograms());

  EXPECT_THAT(GetAddressSpaceFeatureBucketCounts(histogram_tester), IsEmpty());
}

// This test verifies that the chrome-untrusted:// scheme is considered local
// for the purpose of Private Network Access computations.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       SpecialSchemeChromeUntrusted) {
  // The only way to have a page with a loaded chrome-untrusted:// url without
  // relying on platform specific or components features, is to use the
  // new-tab-page host. chrome-untrusted://new-tab-page is restricted to iframes
  // however so we load chrome://new-tab-page that embeds chrome-untrusted://
  // frame(s) by default.
  EXPECT_TRUE(
      content::NavigateToURL(web_contents(), GURL("chrome://new-tab-page")));
  std::vector<content::RenderFrameHost*> frames =
      web_contents()->GetAllFrames();
  ASSERT_GE(frames.size(), 2u);
  content::RenderFrameHost* iframe = frames[1];
  EXPECT_TRUE(iframe->GetLastCommittedURL().SchemeIs(
      content::kChromeUIUntrustedScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/591068): The chrome-untrusted:// page should be kLocal, and
  // not require a Private Network Access CORS preflight. However we have not
  // yet implemented the CORS preflight mechanism, and fixing the underlying
  // issue will not change the test result. Once CORS preflight is implemented,
  // review this test and delete this comment.
  // Note: CSP is blocking javascript eval, unless we run it in an isolated
  // world.
  EXPECT_EQ(true, content::EvalJs(iframe, FetchScript(fetch_url),
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  content::ISOLATED_WORLD_ID_CONTENT_END));
}

// This test verifies that the devtools:// scheme is considered local for the
// purpose of Private Network Access.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       SpecialSchemeDevtools) {
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), GURL("devtools://devtools/bundled/devtools_app.html")));
  EXPECT_TRUE(web_contents()->GetMainFrame()->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/591068): The devtools:// page should be kLocal, and not
  // require a Private Network Access CORS preflight. However we have not yet
  // implemented the CORS preflight mechanism, and fixing the underlying issue
  // will not change the test result. Once CORS preflight is implemented, review
  // this test and delete this comment.
  EXPECT_EQ(true, content::EvalJs(web_contents(), FetchScript(fetch_url)));
}

// This test verifies that the chrome-search:// scheme is considered local for
// the purpose of Private Network Access.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       SpecialSchemeChromeSearch) {
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), GURL("chrome-search://most-visited/title.html")));
  ASSERT_TRUE(web_contents()->GetMainFrame()->GetLastCommittedURL().SchemeIs(
      chrome::kChromeSearchScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/591068): The chrome-search:// page should be kLocal, and not
  // require a Private Network Access CORS preflight. However we have not yet
  // implemented the CORS preflight mechanism, and fixing the underlying issue
  // will not change the test result. Once CORS preflight is implemented, review
  // this test and delete this comment.
  // Note: CSP is blocking javascript eval, unless we run it in an isolated
  // world.
  EXPECT_EQ(true, content::EvalJs(web_contents(), FetchScript(fetch_url),
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  content::ISOLATED_WORLD_ID_CONTENT_END));
}

// This test verifies that the chrome-extension:// scheme is considered local
// for the purpose of Private Network Access.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       SpecialSchemeChromeExtension) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  static constexpr char kPageFile[] = "page.html";

  std::vector<base::Value> resources;
  resources.emplace_back(std::string(kPageFile));
  constexpr char kContents[] = R"(
  <html>
    <head>
      <title>IPAddressSpace of chrome-extension:// schemes.</title>
    </head>
    <body>
    </body>
  </html>
  )";
  base::WriteFile(temp_dir.GetPath().AppendASCII(kPageFile), kContents,
                  sizeof(kContents) - 1);

  extensions::ExtensionBuilder builder("test");
  builder.SetPath(temp_dir.GetPath())
      .SetVersion("1.0")
      .SetLocation(extensions::mojom::ManifestLocation::kExternalPolicyDownload)
      .SetManifestKey("web_accessible_resources", std::move(resources));

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  scoped_refptr<const extensions::Extension> extension = builder.Build();
  service->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(), 0);

  const GURL url = extension->GetResourceURL(kPageFile);

  EXPECT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(web_contents()->GetMainFrame()->GetLastCommittedURL().SchemeIs(
      extensions::kExtensionScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // TODO(crbug.com/591068): The chrome-extension:// page should be kLocal, and
  // not require a Private Network Access CORS preflight. However we have not
  // yet implemented the CORS preflight mechanism, and fixing the underlying
  // issue will not change the test result. Once CORS preflight is implemented,
  // review this test and delete this comment.
  // Note: CSP is blocking javascript eval, unless we run it in an isolated
  // world.
  EXPECT_EQ(true, content::EvalJs(web_contents(), FetchScript(fetch_url),
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  content::ISOLATED_WORLD_ID_CONTENT_END));
}

// This test verifies that the chrome-distiller:// scheme is considered public
// for the purpose of Private Network Access.
IN_PROC_BROWSER_TEST_F(PrivateNetworkAccessWithFeatureEnabledBrowserTest,
                       SpecialSchemeChromeDistiller) {
  // Load the base page to be distilled. Note that HTTPS has to be used
  // otherwise the page won't be distillable.
  std::unique_ptr<net::EmbeddedTestServer> https_server =
      NewServer(net::EmbeddedTestServer::TYPE_HTTPS);
  GURL article_url = https_server->GetURL("/dom_distiller/simple_article.html");

  dom_distiller::TestDistillabilityObserver distillability_observer(
      web_contents());
  dom_distiller::DistillabilityResult expected_result;
  expected_result.is_distillable = true;
  expected_result.is_last = false;
  expected_result.is_mobile_friendly = false;

  EXPECT_TRUE(content::NavigateToURL(web_contents(), article_url));
  // This blocks until the page is found to be distillable.
  distillability_observer.WaitForResult(expected_result);

  // Distill the page. It will be placed in a new WebContents replacing the old
  // one.
  DistillCurrentPageAndView(web_contents());
  dom_distiller::DistilledPageObserver(web_contents())
      .WaitUntilFinishedLoading();

  EXPECT_TRUE(web_contents()->GetMainFrame()->GetLastCommittedURL().SchemeIs(
      dom_distiller::kDomDistillerScheme));

  std::unique_ptr<net::EmbeddedTestServer> server = NewServer();
  GURL fetch_url = LocalNonSecureWithCrossOriginCors(*server);

  // Note: CSP is blocking javascript eval, unless we run it in an isolated
  // world.
  EXPECT_EQ(false, content::EvalJs(web_contents(), FetchScript(fetch_url),
                                   content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                   content::ISOLATED_WORLD_ID_CONTENT_END));
}

}  // namespace
