// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_isolation/site_details.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/metrics/metrics_memory_details.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/metrics_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using content::WebContents;
using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::ListBuilder;
using extensions::TestExtensionDir;
using testing::ElementsAre;
using testing::PrintToString;

namespace {

class TestMemoryDetails : public MetricsMemoryDetails {
 public:
  TestMemoryDetails() : MetricsMemoryDetails(base::DoNothing()) {}

  void StartFetchAndWait() {
    uma_.reset(new base::HistogramTester());
    StartFetch();
    content::RunMessageLoop();
  }

  // Returns a HistogramTester which observed the most recent call to
  // StartFetchAndWait().
  base::HistogramTester* uma() { return uma_.get(); }

  int GetOutOfProcessIframeCount() {
    std::vector<Bucket> buckets =
        uma_->GetAllSamples("SiteIsolation.OutOfProcessIframes");
    CHECK_EQ(1U, buckets.size());
    return buckets[0].min;
  }

  size_t CountPageTitles() {
    size_t count = 0;
    for (const ProcessMemoryInformation& process : ChromeBrowser()->processes) {
      if (process.process_type == content::PROCESS_TYPE_RENDERER) {
        count += process.titles.size();
      }
    }
    return count;
  }

 private:
  ~TestMemoryDetails() override {}

  void OnDetailsAvailable() override {
    MetricsMemoryDetails::OnDetailsAvailable();
    // Exit the loop initiated by StartFetchAndWait().
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

  std::unique_ptr<base::HistogramTester> uma_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryDetails);
};

// This matcher takes three other matchers as arguments, and applies one of them
// depending on the current site isolation mode. The first applies if no site
// isolation mode is active; the second applies under --isolate-extensions mode;
// and the third applies under --site-per-process mode.
MATCHER_P3(DependingOnPolicy,
           isolate_nothing,
           isolate_extensions,
           isolate_all_sites,
#if defined(OS_ANDROID)
           std::string("(with oopifs disabled) ") +
               PrintToString(isolate_nothing)
#else
           content::AreAllSitesIsolatedForTesting()
               ? std::string("(under --site-per-process) ") +
                     PrintToString(isolate_all_sites)
               : std::string("(under --isolate-extensions) ") +
                     PrintToString(isolate_extensions)
#endif
) {
#if defined(OS_ANDROID)
  return ExplainMatchResult(isolate_nothing, arg, result_listener);
#else
  return content::AreAllSitesIsolatedForTesting()
             ? ExplainMatchResult(isolate_all_sites, arg, result_listener)
             : ExplainMatchResult(isolate_extensions, arg, result_listener);
#endif
}

// Matcher for base::Bucket objects that allows bucket_min to be a matcher.
MATCHER_P2(Sample,
           bucket_min,
           count,
           std::string("is a Bucket whose count is ") + PrintToString(count) +
               std::string(" and whose value is ") +
               PrintToString(bucket_min)) {
  return ExplainMatchResult(count, arg.count, result_listener) &&
         ExplainMatchResult(bucket_min, arg.min, result_listener);
}

// Allow matchers to be pretty-printed when passed to PrintToString() for the
// cases we care about.
template <typename P1, typename P2, typename P3>
void PrintTo(const DependingOnPolicyMatcherP3<P1, P2, P3>& matcher,
             std::ostream* os) {
  testing::Matcher<int> matcherCast = matcher;
  matcherCast.DescribeTo(os);
}

template <typename P1, typename P2>
void PrintTo(const SampleMatcherP2<P1, P2>& matcher, std::ostream* os) {
  testing::Matcher<Bucket> matcherCast = matcher;
  matcherCast.DescribeTo(os);
}

// Matches a container of histogram samples, for the common case where the
// histogram received just one sample.
#define HasOneSample(x) ElementsAre(Sample(x, 1))

}  // namespace

class SiteDetailsBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  SiteDetailsBrowserTest() {}
  ~SiteDetailsBrowserTest() override {}

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data so we can use cross_site_iframe_factory.html
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("content/test/data/"));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Create and install an extension that has a couple of web-accessible
  // resources and, optionally, a background process.
  const Extension* CreateExtension(const std::string& name,
                                   bool has_background_process) {
    std::unique_ptr<TestExtensionDir> dir(new TestExtensionDir);

    DictionaryBuilder manifest;
    manifest.Set("name", name)
        .Set("version", "1.0")
        .Set("manifest_version", 2)
        .Set("web_accessible_resources", ListBuilder()
                                             .Append("blank_iframe.html")
                                             .Append("http_iframe.html")
                                             .Append("two_http_iframes.html")
                                             .Build());

    if (has_background_process) {
      manifest.Set(
          "background",
          DictionaryBuilder()
              .Set("scripts", ListBuilder().Append("script.js").Build())
              .Build());
      dir->WriteFile(FILE_PATH_LITERAL("script.js"),
                     "console.log('" + name + " running');");
    }

    dir->WriteFile(FILE_PATH_LITERAL("blank_iframe.html"),
                   base::StringPrintf("<html><body>%s, blank iframe:"
                                      "  <iframe width=80 height=80></iframe>"
                                      "</body></html>",
                                      name.c_str()));
    std::string iframe_url =
        embedded_test_server()
            ->GetURL("w.com", "/cross_site_iframe_factory.html?w")
            .spec();
    std::string iframe_url2 =
        embedded_test_server()
            ->GetURL("x.com", "/cross_site_iframe_factory.html?x")
            .spec();
    dir->WriteFile(
        FILE_PATH_LITERAL("http_iframe.html"),
        base::StringPrintf("<html><body>%s, http:// iframe:"
                           "  <iframe width=80 height=80 src='%s'></iframe>"
                           "</body></html>",
                           name.c_str(), iframe_url.c_str()));
    dir->WriteFile(FILE_PATH_LITERAL("two_http_iframes.html"),
                   base::StringPrintf(
                       "<html><body>%s, two http:// iframes:"
                       "  <iframe width=80 height=80 src='%s'></iframe>"
                       "  <iframe width=80 height=80 src='%s'></iframe>"
                       "</body></html>",
                       name.c_str(), iframe_url.c_str(), iframe_url2.c_str()));
    dir->WriteManifest(manifest.ToJSON());

    const Extension* extension = LoadExtension(dir->UnpackedPath());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(std::move(dir));
    return extension;
  }

  const Extension* CreateHostedApp(const std::string& name,
                                   const GURL& app_url) {
    std::unique_ptr<TestExtensionDir> dir(new TestExtensionDir);

    DictionaryBuilder manifest;
    manifest.Set("name", name)
        .Set("version", "1.0")
        .Set("manifest_version", 2)
        .Set(
            "app",
            DictionaryBuilder()
                .Set("urls", ListBuilder().Append(app_url.spec()).Build())
                .Set("launch",
                     DictionaryBuilder().Set("web_url", app_url.spec()).Build())
                .Build());
    dir->WriteManifest(manifest.ToJSON());

    const Extension* extension = LoadExtension(dir->UnpackedPath());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(std::move(dir));
    return extension;
  }

  int GetRenderProcessCount() {
    return content::RenderProcessHost::GetCurrentRenderProcessCountForTesting();
  }

 private:
  std::vector<std::unique_ptr<TestExtensionDir>> temp_dirs_;
  DISALLOW_COPY_AND_ASSIGN(SiteDetailsBrowserTest);
};

// Test the accuracy of SiteDetails process estimation, in the presence of
// multiple iframes, navigation, multiple BrowsingInstances, and multiple tabs
// in the same BrowsingInstance.
//
// Disabled since it's flaky: https://crbug.com/830318.
IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest, DISABLED_ManyIframes) {
  // Page with 14 nested oopifs across 9 sites (a.com through i.com).
  // None of these are https.
  GURL abcdefghi_url = embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?a(b(a(b,c,d,e,f,g,h)),c,d,e,i(f))");
  ui_test_utils::NavigateToURL(browser(), abcdefghi_url);

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(1U, details->CountPageTitles());
  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      HasOneSample(1));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(1, 1, 9));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 0, 14));
  EXPECT_THAT(details->uma()->GetAllSamples("SiteIsolation.ProxyCount"),
              HasOneSample(DependingOnPolicy(0, 0, 114)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.ProxyCountPerBrowsingInstance"),
              HasOneSample(DependingOnPolicy(0, 0, 114)));

  // Navigate to a different, disjoint set of 7 sites.
  GURL pqrstuv_url = embedded_test_server()->GetURL(
      "p.com",
      "/cross_site_iframe_factory.html?p(q(r),r(s),s(t),t(q),u(u),v(p))");
  ui_test_utils::NavigateToURL(browser(), pqrstuv_url);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(1U, details->CountPageTitles());
  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      HasOneSample(1));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(1, 1, 7));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 0, 11));
  EXPECT_THAT(details->uma()->GetAllSamples("SiteIsolation.ProxyCount"),
              HasOneSample(DependingOnPolicy(0, 0, 68)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.ProxyCountPerBrowsingInstance"),
              HasOneSample(DependingOnPolicy(0, 0, 68)));

  // Open a second tab (different BrowsingInstance) with 4 sites (a through d).
  GURL abcd_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(d())))");
  AddTabAtIndex(1, abcd_url, ui::PAGE_TRANSITION_TYPED);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_EQ(2U, details->CountPageTitles());
  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      HasOneSample(2));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(2, 2, 11));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 0, 14));
  EXPECT_THAT(details->uma()->GetAllSamples("SiteIsolation.ProxyCount"),
              HasOneSample(DependingOnPolicy(0, 0, 81)));
  EXPECT_THAT(
      details->uma()->GetAllSamples(
          "SiteIsolation.ProxyCountPerBrowsingInstance"),
      DependingOnPolicy(ElementsAre(Bucket(0, 2)), ElementsAre(Bucket(0, 2)),
                        ElementsAre(Bucket(12, 1), Bucket(68, 1))));

  // Open a third tab (different BrowsingInstance) with the same 4 sites.
  AddTabAtIndex(2, abcd_url, ui::PAGE_TRANSITION_TYPED);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      HasOneSample(3));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));

  // For --site-per-process, the total process count will be 12 instead of 15,
  // because the third tab's subframes (b, c, d) will reuse matching subframe
  // processes from the second tab (across BrowsingInstances).  This subframe
  // process consolidation was added as part of https://crbug.com/512560.  Note
  // that the a.com main frame in tab 3 won't reuse tab 2's main frame process,
  // so this is still one process higher than the lower bound.
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(3, 3, 12));

  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 0, 17));
  EXPECT_THAT(details->uma()->GetAllSamples("SiteIsolation.ProxyCount"),
              HasOneSample(DependingOnPolicy(0, 0, 96)));
  EXPECT_THAT(
      details->uma()->GetAllSamples(
          "SiteIsolation.ProxyCountPerBrowsingInstance"),
      DependingOnPolicy(ElementsAre(Bucket(0, 3)), ElementsAre(Bucket(0, 3)),
                        ElementsAre(Bucket(12, 2), Bucket(68, 1))));

  // From the third tab, window.open() a fourth tab in the same
  // BrowsingInstance, to a page using the same four sites "a-d" as third tab,
  // plus an additional site "e". The estimated process counts should increase
  // by one (not five) from the previous scenario, as the new tab can reuse the
  // four processes already in the BrowsingInstance.
  GURL dcbae_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?d(c(b(a(e))))");
  ui_test_utils::UrlLoadObserver load_complete(
      dcbae_url, content::NotificationService::AllSources());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('" + dcbae_url.spec() + "');"));
  ASSERT_EQ(4, browser()->tab_strip_model()->count());
  load_complete.Wait();

  details = new TestMemoryDetails();
  details->StartFetchAndWait();

  EXPECT_THAT(
      details->uma()->GetAllSamples("SiteIsolation.BrowsingInstanceCount"),
      HasOneSample(3));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(3, 3, 13));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 0, 21));
  EXPECT_THAT(details->uma()->GetAllSamples("SiteIsolation.ProxyCount"),
              HasOneSample(DependingOnPolicy(0, 0, 114)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.ProxyCountPerBrowsingInstance"),
              DependingOnPolicy(
                  ElementsAre(Bucket(0, 3)), ElementsAre(Bucket(0, 3)),
                  ElementsAre(Bucket(12, 1), Bucket(29, 1), Bucket(68, 1))));
}

// TODO(crbug.com/671891): This test is flaky.
IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest, DISABLED_IsolateExtensions) {
  // We start on "about:blank", which should be credited with a process in this
  // case.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), 1);
  EXPECT_EQ(0, details->GetOutOfProcessIframeCount());

  // Install one script-injecting extension with background page, and an
  // extension with web accessible resources.
  const Extension* extension1 = CreateExtension("Extension One", true);
  const Extension* extension2 = CreateExtension("Extension Two", false);

  // Open two a.com tabs (with cross site http iframes). IsolateExtensions mode
  // should have no effect so far, since there are no frames straddling the
  // extension/web boundary.
  GURL tab1_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)");
  ui_test_utils::NavigateToURL(browser(), tab1_url);
  WebContents* tab1 = browser()->tab_strip_model()->GetWebContentsAt(0);
  GURL tab2_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(d,e)");
  AddTabAtIndex(1, tab2_url, ui::PAGE_TRANSITION_TYPED);
  WebContents* tab2 = browser()->tab_strip_model()->GetWebContentsAt(1);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(3, 3, 7));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 0, 4));

  // Test that "one process per extension" applies even when web content has an
  // extension iframe.

  // Tab1 navigates its first iframe to a resource of extension1. This shouldn't
  // result in a new extension process (it should share with extension1's
  // background page).
  content::NavigateIframeToURL(
      tab1, "child-0", extension1->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(3, 3, 6));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 1, 4));

  // Tab2 navigates its first iframe to a resource of extension1. This also
  // shouldn't result in a new extension process (it should share with the
  // background page and the other iframe).
  content::NavigateIframeToURL(
      tab2, "child-0", extension1->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(3, 3, 5));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 2, 4));

  // Tab1 navigates its second iframe to a resource of extension2. This SHOULD
  // result in a new process since extension2 had no existing process.
  content::NavigateIframeToURL(
      tab1, "child-1", extension2->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(3, 4, 5));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 3, 4));

  // Tab2 navigates its second iframe to a resource of extension2. This should
  // share the existing extension2 process.
  content::NavigateIframeToURL(
      tab2, "child-1", extension2->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(3, 4, 4));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 4, 4));

  // Install extension3 (identical config to extension2)
  const Extension* extension3 = CreateExtension("Extension Three", false);

  // Navigate Tab2 to a top-level page from extension3. There are four processes
  // now: one for tab1's main frame, and one for each of the extensions:
  // extension1 has a process because it has a background page; extension2 is
  // used as an iframe in tab1, and extension3 is the top-level frame in tab2.
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(3, 4, 4));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 2, 2));

  // Navigate tab2 to a different extension3 page containing a web iframe. The
  // iframe should get its own process. The lower bound number indicates that,
  // in theory, the iframe could share a process with tab1's main frame.
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("http_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(3, 5, 5));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 3, 3));

  // Navigate tab1 to an extension3 page with an extension3 iframe. There should
  // be three processes estimated by IsolateExtensions: one for extension3, one
  // for extension1's background page, and one for the web iframe in tab2.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(2, 3, 3));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 1, 1));

  // Now navigate tab1 to an extension3 page with a web iframe. This could share
  // a process with tab2's iframe (the LowerBound number), or it could get its
  // own process (the Estimate number).
  ui_test_utils::NavigateToURL(browser(),
                               extension3->GetResourceURL("http_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));

  // There should be four total renderer processes: one for each of the two web
  // iframes, one for extension3, and one for extension 1's background page.
  // Note that the optimization in https://crbug.com/512560, where subframes
  // that require a dedicated process reuse existing processes where possible,
  // does not apply to web iframes in extensions anymore -- see
  // https://crbug.com/899418.
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(2, 4, 4));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 2, 2));
}

// Exercises accounting in the case where an extension has two different-site
// web iframes.
IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest, ExtensionWithTwoWebIframes) {
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  // Install one script-injecting extension with background page, and an
  // extension with web accessible resources.
  const Extension* extension = CreateExtension("Test Extension", false);

  ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("/two_http_iframes.html"));

  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  // TODO(nick): https://crbug.com/512560 Make the number below agree with the
  // estimates above, which assume consolidation of subframe processes.
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(1, 3, 3));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 2, 2));
}

// Verifies that --isolate-extensions doesn't isolate hosted apps.
//
// Disabled since it's flaky: https://crbug.com/830318.
IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest,
                       DISABLED_IsolateExtensionsHostedApps) {
  GURL app_with_web_iframe_url = embedded_test_server()->GetURL(
      "app.org", "/cross_site_iframe_factory.html?app.org(b.com)");
  GURL app_in_web_iframe_url = embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b.com(app.org)");

  // No hosted app is installed: app.org just behaves like a normal domain.
  ui_test_utils::NavigateToURL(browser(), app_with_web_iframe_url);
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(1, 1, 2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 0, 1));

  ui_test_utils::NavigateToURL(browser(), app_in_web_iframe_url);
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(1, 1, 2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 0, 1));

  // Now install app.org as a hosted app.
  CreateHostedApp("App", GURL("http://app.org"));

  // Reload the same two pages, and verify that the hosted app still is not
  // isolated by --isolate-extensions, but is isolated by --site-per-process.
  ui_test_utils::NavigateToURL(browser(), app_with_web_iframe_url);
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(1, 1, 2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 0, 1));

  ui_test_utils::NavigateToURL(browser(), app_in_web_iframe_url);
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.CurrentRendererProcessCount"),
              HasOneSample(GetRenderProcessCount()));
  EXPECT_THAT(GetRenderProcessCount(), DependingOnPolicy(1, 1, 2));
  EXPECT_THAT(details->GetOutOfProcessIframeCount(),
              DependingOnPolicy(0, 0, 1));
}

// Verifies that the UMA counter for SiteInstances in a BrowsingInstance is
// correct when using tabs with web pages.
//
// Disabled since it's flaky. https://crbug.com/934900
IN_PROC_BROWSER_TEST_F(SiteDetailsBrowserTest,
                       DISABLED_VerifySiteInstanceCountInBrowsingInstance) {
  // Page with 14 nested oopifs across 9 sites (a.com through i.com).
  GURL abcdefghi_url = embedded_test_server()->GetURL(
      "a.com",
      "/cross_site_iframe_factory.html?a(b(a(b,c,d,e,f,g,h)),c,d,e,i(f))");
  ui_test_utils::NavigateToURL(browser(), abcdefghi_url);

  // Get the metrics.
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  // Since there are no extensions involved, the results in the default case
  // and extensions::IsIsolateExtensionsEnabled() are the same.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.SiteInstancesPerBrowsingInstance"),
              HasOneSample(DependingOnPolicy(1, 1, 9)));
  EXPECT_THAT(details->uma()->GetAllSamples("SiteIsolation.ProxyCount"),
              HasOneSample(DependingOnPolicy(0, 0, 114)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.ProxyCountPerBrowsingInstance"),
              HasOneSample(DependingOnPolicy(0, 0, 114)));

  // Open another tab through window.open(), which will be in the same
  // BrowsingInstance.
  GURL dcbae_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?d(c(b(j(k))))");
  ui_test_utils::UrlLoadObserver load_complete(
      dcbae_url, content::NotificationService::AllSources());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.open('" + dcbae_url.spec() + "');"));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  load_complete.Wait();

  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.SiteInstancesPerBrowsingInstance"),
              HasOneSample(DependingOnPolicy(1, 1, 11)));
  EXPECT_THAT(details->uma()->GetAllSamples("SiteIsolation.ProxyCount"),
              HasOneSample(DependingOnPolicy(0, 0, 160)));
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.ProxyCountPerBrowsingInstance"),
              HasOneSample(DependingOnPolicy(0, 0, 160)));

  // Open a tab, which will be in a different BrowsingInstance.
  GURL abcd_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(d())))");
  AddTabAtIndex(1, abcd_url, ui::PAGE_TRANSITION_TYPED);

  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(
      details->uma()->GetAllSamples(
          "SiteIsolation.SiteInstancesPerBrowsingInstance"),
      DependingOnPolicy(ElementsAre(Sample(1, 2)), ElementsAre(Sample(1, 2)),
                        ElementsAre(Sample(4, 1), Sample(11, 1))));
  EXPECT_THAT(details->uma()->GetAllSamples("SiteIsolation.ProxyCount"),
              HasOneSample(DependingOnPolicy(0, 0, 160)));
  EXPECT_THAT(
      details->uma()->GetAllSamples(
          "SiteIsolation.ProxyCountPerBrowsingInstance"),
      DependingOnPolicy(ElementsAre(Sample(0, 2)), ElementsAre(Sample(0, 2)),
                        ElementsAre(Sample(12, 1), Sample(160, 1))));
}

// Verifies that the UMA counter for SiteInstances in a BrowsingInstance is
// correct when extensions and web pages are mixed together.
//
// Disabled since it's flaky: https://crbug.com/830318.
IN_PROC_BROWSER_TEST_F(
    SiteDetailsBrowserTest,
    DISABLED_VerifySiteInstanceCountInBrowsingInstanceWithExtensions) {
  // Open two a.com tabs (with cross site http iframes). IsolateExtensions mode
  // should have no effect so far, since there are no frames straddling the
  // extension/web boundary.
  GURL tab_url = embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c,d(e))");
  ui_test_utils::NavigateToURL(browser(), tab_url);
  WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(0);
  scoped_refptr<TestMemoryDetails> details = new TestMemoryDetails();
  details->StartFetchAndWait();

  // Since there are no extensions loaded yet, the results in the default case
  // and extensions::IsIsolateExtensionsEnabled() are the same.
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.SiteInstancesPerBrowsingInstance"),
              HasOneSample(DependingOnPolicy(1, 1, 5)));

  // Load an extension without a background page, which will avoid creating a
  // BrowsingInstance for it.
  const Extension* extension1 = CreateExtension("Extension One", false);

  // Navigate the tab's first iframe to a resource of the extension. The
  // extension iframe will be put in the same BrowsingInstance as it is part
  // of the frame tree.
  content::NavigateIframeToURL(
      tab, "child-0", extension1->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.SiteInstancesPerBrowsingInstance"),
              HasOneSample(DependingOnPolicy(1, 2, 5)));

  // Now load an extension with a background page. This will result in a
  // BrowsingInstance for the background page.
  const Extension* extension2 = CreateExtension("Extension Two", true);
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.SiteInstancesPerBrowsingInstance"),
              DependingOnPolicy(ElementsAre(Bucket(1, 2)),
                                ElementsAre(Bucket(1, 1), Bucket(2, 1)),
                                ElementsAre(Bucket(1, 1), Bucket(5, 1))));

  // Navigate the second iframe of the tab to the second extension. It should
  // stay in the same BrowsingInstance as the page.
  content::NavigateIframeToURL(
      tab, "child-1", extension2->GetResourceURL("/blank_iframe.html"));
  details = new TestMemoryDetails();
  details->StartFetchAndWait();
  EXPECT_THAT(details->uma()->GetAllSamples(
                  "SiteIsolation.SiteInstancesPerBrowsingInstance"),
              DependingOnPolicy(ElementsAre(Bucket(1, 2)),
                                ElementsAre(Bucket(1, 1), Bucket(3, 1)),
                                ElementsAre(Bucket(1, 1), Bucket(5, 1))));
}
