// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/page_load_metrics/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/observers/ad_metrics/frame_data.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/from_ad_state.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

using content::RenderFrameHost;
using testing::CreateSuffixRule;

class AdTaggingBrowserTest : public SubresourceFilterBrowserTest {
 public:
  AdTaggingBrowserTest() : SubresourceFilterBrowserTest() {}
  ~AdTaggingBrowserTest() override {}

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {CreateSuffixRule("ad_script.js"), CreateSuffixRule("ad=true")});
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Used for giving identifiers to frames that can easily be searched for
  // with content::FrameMatchingPredicate.
  std::string GetUniqueFrameName() {
    return base::StringPrintf("frame_%d", frame_count_++);
  }

  // Create a frame that navigates via the src attribute. It's created by ad
  // script. Returns after navigation has completed.
  content::RenderFrameHost* CreateSrcFrameFromAdScript(
      const content::ToRenderFrameHost& adapter,
      const GURL& url) {
    return CreateFrameImpl(adapter, url, true /* ad_script */);
  }

  // Create a frame that navigates via the src attribute. Returns after
  // navigation has completed.
  content::RenderFrameHost* CreateSrcFrame(
      const content::ToRenderFrameHost& adapter,
      const GURL& url) {
    return CreateFrameImpl(adapter, url, false /* ad_script */);
  }

  // Creates a frame and doc.writes the frame into it. Returns after
  // navigation has completed.
  content::RenderFrameHost* CreateDocWrittenFrame(
      const content::ToRenderFrameHost& adapter) {
    return CreateDocWrittenFrameImpl(adapter, false /* ad_script */);
  }

  // Creates a frame and doc.writes the frame into it. The script creating the
  // frame is an ad script. Returns after navigation has completed.
  content::RenderFrameHost* CreateDocWrittenFrameFromAdScript(
      const content::ToRenderFrameHost& adapter) {
    return CreateDocWrittenFrameImpl(adapter, true /* ad_script */);
  }

  // Given a RenderFrameHost, navigates the page to the given |url| and waits
  // for the navigation to complete before returning.
  void NavigateFrame(content::RenderFrameHost* render_frame_host,
                     const GURL& url);

  GURL GetURL(const std::string& page) {
    return embedded_test_server()->GetURL("/ad_tagging/" + page);
  }

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        GetWebContents());
  }

 private:
  content::RenderFrameHost* CreateFrameImpl(
      const content::ToRenderFrameHost& adapter,
      const GURL& url,
      bool ad_script);

  content::RenderFrameHost* CreateDocWrittenFrameImpl(
      const content::ToRenderFrameHost& adapter,
      bool ad_script);

  uint32_t frame_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AdTaggingBrowserTest);
};

content::RenderFrameHost* AdTaggingBrowserTest::CreateFrameImpl(
    const content::ToRenderFrameHost& adapter,
    const GURL& url,
    bool ad_script) {
  content::RenderFrameHost* rfh = adapter.render_frame_host();
  std::string name = GetUniqueFrameName();
  std::string script = base::StringPrintf(
      "%s('%s','%s');", ad_script ? "createAdFrame" : "createFrame",
      url.spec().c_str(), name.c_str());
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  content::TestNavigationObserver navigation_observer(web_contents, 1);
  EXPECT_TRUE(content::ExecuteScript(rfh, script));
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded())
      << navigation_observer.last_net_error_code();
  return content::FrameMatchingPredicate(
      web_contents, base::BindRepeating(&content::FrameMatchesName, name));
}

content::RenderFrameHost* AdTaggingBrowserTest::CreateDocWrittenFrameImpl(
    const content::ToRenderFrameHost& adapter,
    bool ad_script) {
  content::RenderFrameHost* rfh = adapter.render_frame_host();
  std::string name = GetUniqueFrameName();
  std::string script = base::StringPrintf(
      "%s('%s', '%s');",
      ad_script ? "createDocWrittenAdFrame" : "createDocWrittenFrame",
      name.c_str(), GetURL("").spec().c_str());
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  content::TestNavigationObserver navigation_observer(web_contents, 1);
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(rfh, script, &result));
  EXPECT_TRUE(result);
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded())
      << navigation_observer.last_net_error_code();
  return content::FrameMatchingPredicate(
      web_contents, base::BindRepeating(&content::FrameMatchesName, name));
}

// Given a RenderFrameHost, navigates the page to the given |url| and waits
// for the navigation to complete before returning.
void AdTaggingBrowserTest::NavigateFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url) {
  std::string script =
      base::StringPrintf(R"(window.location='%s')", url.spec().c_str());
  content::TestNavigationObserver navigation_observer(GetWebContents(), 1);
  EXPECT_TRUE(content::ExecuteScript(render_frame_host, script));
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded())
      << navigation_observer.last_net_error_code();
}

IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, FramesByURL) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));
  EXPECT_FALSE(observer.GetIsAdSubframe(
      GetWebContents()->GetMainFrame()->GetFrameTreeNodeId()));

  // (1) Vanilla child.
  content::RenderFrameHost* vanilla_child =
      CreateSrcFrame(GetWebContents(), GetURL("frame_factory.html?1"));
  EXPECT_FALSE(*observer.GetIsAdSubframe(vanilla_child->GetFrameTreeNodeId()));

  // (2) Ad child.
  RenderFrameHost* ad_child =
      CreateSrcFrame(GetWebContents(), GetURL("frame_factory.html?2&ad=true"));
  EXPECT_TRUE(*observer.GetIsAdSubframe(ad_child->GetFrameTreeNodeId()));

  // (3) Ad child of 2.
  RenderFrameHost* ad_child_2 =
      CreateSrcFrame(ad_child, GetURL("frame_factory.html?sub=1&3&ad=true"));
  EXPECT_TRUE(*observer.GetIsAdSubframe(ad_child_2->GetFrameTreeNodeId()));

  // (4) Vanilla child of 2.
  RenderFrameHost* vanilla_child_2 =
      CreateSrcFrame(ad_child, GetURL("frame_factory.html?4"));
  EXPECT_TRUE(*observer.GetIsAdSubframe(vanilla_child_2->GetFrameTreeNodeId()));

  // (5) Vanilla child of 1. This tests something subtle.
  // frame_factory.html?ad=true loads the same script that frame_factory.html
  // uses to load frames. This tests that even though the script is tagged as an
  // ad in the ad iframe, it's not considered an ad in the main frame, hence
  // it's able to create an iframe that's not labeled as an ad.
  RenderFrameHost* vanilla_child_3 =
      CreateSrcFrame(vanilla_child, GetURL("frame_factory.html?5"));
  EXPECT_FALSE(
      *observer.GetIsAdSubframe(vanilla_child_3->GetFrameTreeNodeId()));
}

const char kSubresourceFilterOriginStatusHistogram[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "OriginStatus";
const char kWindowOpenFromAdStateHistogram[] = "Blink.WindowOpen.FromAdState";

IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, VerifySameOriginWithoutNavigate) {
  base::HistogramTester histogram_tester;

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Ad frame via doc write.
  CreateDocWrittenFrameFromAdScript(GetWebContents());

  // Navigate away and ensure we report same origin.
  ui_test_utils::NavigateToURL(browser(), GetURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(kSubresourceFilterOriginStatusHistogram,
                                      FrameData::OriginStatus::kSame, 1);
}

IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, VerifyCrossOriginWithoutNavigate) {
  base::HistogramTester histogram_tester;

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Regular frame that's cross origin and has a doc write ad of its own.
  RenderFrameHost* regular_child = CreateSrcFrame(
      GetWebContents(), embedded_test_server()->GetURL(
                            "b.com", "/ad_tagging/frame_factory.html"));
  CreateDocWrittenFrameFromAdScript(regular_child);

  // Navigate away and ensure we report cross origin.
  ui_test_utils::NavigateToURL(browser(), GetURL(url::kAboutBlankURL));

  // TODO(johnidel): Check that frame was reported properly. See
  // crbug.com/914893.
}

// Ad script creates a frame and navigates it cross origin.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       VerifyCrossOriginWithImmediateNavigate) {
  base::HistogramTester histogram_tester;

  auto waiter = CreatePageLoadMetricsTestWaiter();
  // Create the main frame and cross origin subframe from an ad script.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));
  CreateSrcFrameFromAdScript(GetWebContents(),
                             embedded_test_server()->GetURL(
                                 "b.com", "/ads_observer/same_origin_ad.html"));
  // Wait for all of the subresources to finish loading (includes favicon).
  // Waiting for the navigation to finish is not sufficient, as it blocks on the
  // main resource load finishing, not the iframe resource. Page loads 4
  // resources, a favicon, and 2 resources for the iframe.
  waiter->AddMinimumCompleteResourcesExpectation(8);
  waiter->Wait();

  // Navigate away and ensure we report cross origin.
  ui_test_utils::NavigateToURL(browser(), GetURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(kSubresourceFilterOriginStatusHistogram,
                                      FrameData::OriginStatus::kCross, 1);
}

// Ad script creates a frame and navigates it same origin.
// It is then renavigated cross origin.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       VerifySameOriginWithCrossOriginRenavigate) {
  base::HistogramTester histogram_tester;

  // Create the main frame and same origin subframe from an ad script.
  // This triggers the subresource_filter ad detection.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));
  RenderFrameHost* ad_child = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html"));

  // Navigate the subframe to a cross origin site.
  NavigateFrame(ad_child, embedded_test_server()->GetURL(
                              "b.com", "/ad_tagging/frame_factory.html"));

  // Navigate away and ensure we report same origin.
  ui_test_utils::NavigateToURL(browser(), GetURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(kSubresourceFilterOriginStatusHistogram,
                                      FrameData::OriginStatus::kSame, 1);
}

// Test that a subframe with a non-ad url but loaded by ad script is an ad.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, FrameLoadedByAdScript) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Child frame created by ad script.
  RenderFrameHost* ad_child = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html?1"));
  EXPECT_TRUE(*observer.GetIsAdSubframe(ad_child->GetFrameTreeNodeId()));
}

// Test that same-origin doc.write created iframes are tagged as ads.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, SameOriginFrameTagging) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // (1) Vanilla child.
  content::RenderFrameHost* vanilla_frame =
      CreateDocWrittenFrame(GetWebContents());
  EXPECT_FALSE(observer.GetIsAdSubframe(vanilla_frame->GetFrameTreeNodeId()));

  // (2) Ad child.
  content::RenderFrameHost* ad_frame =
      CreateDocWrittenFrameFromAdScript(GetWebContents());
  EXPECT_TRUE(*observer.GetIsAdSubframe(ad_frame->GetFrameTreeNodeId()));
}

void ExpectWindowOpenUkmEntry(const ukm::TestUkmRecorder& ukm_recorder,
                              bool from_main_frame,
                              const GURL& main_frame_url,
                              bool from_ad_subframe,
                              bool from_ad_script) {
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AbusiveExperienceHeuristic_WindowOpen::kEntryName);
  EXPECT_EQ(1u, entries.size());

  // Check that the event is keyed to |main_frame_url| only if it was from the
  // top frame.
  if (from_main_frame) {
    ukm_recorder.ExpectEntrySourceHasUrl(entries.back(), main_frame_url);
  } else {
    EXPECT_FALSE(ukm_recorder.GetSourceForSourceId(entries.back()->source_id));
  }

  // Check that a DocumentCreated entry was created, and it's keyed to
  // |main_frame_url| only if it was from the top frame. However, we can always
  // use the navigation source ID to link this source to |main_frame_url|.
  const ukm::mojom::UkmEntry* dc_entry =
      ukm_recorder.GetDocumentCreatedEntryForSourceId(
          entries.back()->source_id);
  EXPECT_TRUE(dc_entry);
  EXPECT_EQ(entries.back()->source_id, dc_entry->source_id);
  if (from_main_frame) {
    ukm_recorder.ExpectEntrySourceHasUrl(dc_entry, main_frame_url);
  } else {
    EXPECT_FALSE(ukm_recorder.GetSourceForSourceId(dc_entry->source_id));
  }

  const ukm::UkmSource* navigation_source =
      ukm_recorder.GetSourceForSourceId(*ukm_recorder.GetEntryMetric(
          dc_entry, ukm::builders::DocumentCreated::kNavigationSourceIdName));
  EXPECT_EQ(main_frame_url, navigation_source->url());

  EXPECT_EQ(from_main_frame,
            *ukm_recorder.GetEntryMetric(
                dc_entry, ukm::builders::DocumentCreated::kIsMainFrameName));

  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::AbusiveExperienceHeuristic_WindowOpen::kFromAdSubframeName,
      from_ad_subframe);
  ukm_recorder.ExpectEntryMetric(
      entries.back(),
      ukm::builders::AbusiveExperienceHeuristic_WindowOpen::kFromAdScriptName,
      from_ad_script);
}

void ExpectWindowOpenUmaEntry(const base::HistogramTester& histogram_tester,
                              bool from_ad_subframe,
                              bool from_ad_script) {
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  blink::FromAdState state =
      blink::GetFromAdState(from_ad_subframe, from_ad_script);
  histogram_tester.ExpectBucketCount(kWindowOpenFromAdStateHistogram, state,
                                     1 /* expected_count */);
}

enum class NavigationInitiationType {
  kWindowOpen,
  kSetLocation,
  kAnchorLinkActivate,
};

class AdClickNavigationBrowserTest
    : public AdTaggingBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<NavigationInitiationType, bool /* gesture */>> {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Popups without user gesture is blocked by default. Turn off the switch
    // here to unblock that, so as to be able to test that the UseCounter not
    // being recorded was due to the filtering in our recording function, rather
    // than the default popup blocking.
    command_line->AppendSwitch(::switches::kDisablePopupBlocking);
  }
};

IN_PROC_BROWSER_TEST_P(AdClickNavigationBrowserTest, UseCounter) {
  NavigationInitiationType type;
  bool gesture;
  std::tie(type, gesture) = GetParam();
  auto web_feature_waiter =
      std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
          GetWebContents());
  blink::mojom::WebFeature ad_click_navigation_feature =
      blink::mojom::WebFeature::kAdClickNavigation;
  web_feature_waiter->AddWebFeatureExpectation(ad_click_navigation_feature);
  GURL url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");

  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* main_tab = GetWebContents();
  RenderFrameHost* child = CreateSrcFrame(
      main_tab, embedded_test_server()->GetURL(
                    "a.com", "/ad_tagging/frame_factory.html?1&ad=true"));

  std::string script;
  switch (type) {
    case NavigationInitiationType::kWindowOpen:
      script = "window.open('frame_factory.html')";
      break;
    case NavigationInitiationType::kSetLocation:
      script = "location='frame_factory.html'";
      break;
    case NavigationInitiationType::kAnchorLinkActivate:
      script =
          "var a = document.createElement('a');"
          "a.setAttribute('href', 'frame_factory.html');"
          "a.click();";
      break;
  }

  if (gesture) {
    EXPECT_TRUE(ExecJs(child, script));
    web_feature_waiter->Wait();
  } else {
    switch (type) {
      case NavigationInitiationType::kSetLocation:
      case NavigationInitiationType::kAnchorLinkActivate: {
        content::TestNavigationObserver navigation_observer(web_contents());
        EXPECT_TRUE(ExecuteScriptWithoutUserGesture(child, script));
        // To report metrics.
        navigation_observer.Wait();
        break;
      }
      case NavigationInitiationType::kWindowOpen: {
        EXPECT_TRUE(ExecuteScriptWithoutUserGesture(child, script));
        // To report metrics.
        ASSERT_EQ(2, browser()->tab_strip_model()->count());
        browser()->tab_strip_model()->MoveSelectedTabsTo(0);
        ui_test_utils::NavigateToURL(browser(), url);
        break;
      }
    }
    EXPECT_FALSE(
        web_feature_waiter->DidObserveWebFeature(ad_click_navigation_feature));
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AdClickNavigationBrowserTest,
    ::testing::Combine(
        ::testing::Values(NavigationInitiationType::kWindowOpen,
                          NavigationInitiationType::kSetLocation,
                          NavigationInitiationType::kAnchorLinkActivate),
        ::testing::Bool()));

class AdTaggingEventFromSubframeBrowserTest
    : public AdTaggingBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<bool /* cross_origin */, bool /* from_ad_subframe */>> {};

// crbug.com/997410. The test is flaky on multiple platforms.
IN_PROC_BROWSER_TEST_P(AdTaggingEventFromSubframeBrowserTest,
                       DISABLED_WindowOpenFromSubframe) {
  bool cross_origin;
  bool from_ad_subframe;
  std::tie(cross_origin, from_ad_subframe) = GetParam();
  SCOPED_TRACE(::testing::Message()
               << "cross_origin = " << cross_origin << ", "
               << "from_ad_subframe = " << from_ad_subframe);

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;
  GURL main_frame_url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);
  content::WebContents* main_tab = GetWebContents();

  std::string hostname = cross_origin ? "b.com" : "a.com";
  std::string suffix = from_ad_subframe ? "&ad=true" : "";
  RenderFrameHost* child = CreateSrcFrame(
      main_tab, embedded_test_server()->GetURL(
                    hostname, "/ad_tagging/frame_factory.html?1" + suffix));

  EXPECT_TRUE(content::ExecuteScript(child, "window.open();"));

  bool from_ad_script = from_ad_subframe;
  ExpectWindowOpenUkmEntry(ukm_recorder, false /* from_main_frame */,
                           main_frame_url, from_ad_subframe, from_ad_script);
  ExpectWindowOpenUmaEntry(histogram_tester, from_ad_subframe, from_ad_script);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AdTaggingEventFromSubframeBrowserTest,
    ::testing::Combine(::testing::Bool(), ::testing::Bool()));

class AdTaggingEventWithScriptInStackBrowserTest
    : public AdTaggingBrowserTest,
      public ::testing::WithParamInterface<bool /* from_ad_script */> {};

// crbug.com/998405. The test is flaky on multiple platforms.
IN_PROC_BROWSER_TEST_P(AdTaggingEventWithScriptInStackBrowserTest,
                       DISABLED_WindowOpenWithScriptInStack) {
  bool from_ad_script = GetParam();
  SCOPED_TRACE(::testing::Message() << "from_ad_script = " << from_ad_script);

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::HistogramTester histogram_tester;
  GURL main_frame_url = GetURL("frame_factory.html");
  ui_test_utils::NavigateToURL(browser(), main_frame_url);
  content::WebContents* main_tab = GetWebContents();

  std::string script = from_ad_script ? "windowOpenFromAdScript();"
                                      : "windowOpenFromNonAdScript();";

  EXPECT_TRUE(content::ExecuteScript(main_tab, script));

  bool from_ad_subframe = false;
  ExpectWindowOpenUkmEntry(ukm_recorder, true /* from_main_frame */,
                           main_frame_url, from_ad_subframe, from_ad_script);
  ExpectWindowOpenUmaEntry(histogram_tester, from_ad_subframe, from_ad_script);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    AdTaggingEventWithScriptInStackBrowserTest,
    ::testing::Bool());

}  // namespace

}  // namespace subresource_filter
