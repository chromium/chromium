// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/embedder_support/switches.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_tree_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/content/browser/ad_tagging_browser_test_utils.h"
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
using testing::CreateAllowlistSuffixRule;
using testing::CreateSuffixRule;

// Creates a link element with rel="stylesheet" and href equal to the specified
// url. Returns immediately after adding the element to the DOM.
void AddExternalStylesheet(const content::ToRenderFrameHost& adapter,
                           const GURL& url) {
  std::string script_template = R"(
      link = document.createElement("link");
      link.rel = "stylesheet";
      link.href = $1;
      document.head.appendChild(link);
  )";
  EXPECT_TRUE(content::ExecJs(adapter.render_frame_host(),
                              content::JsReplace(script_template, url)));
}

class AdTaggingPageLoadMetricsTestWaiter
    : public page_load_metrics::PageLoadMetricsTestWaiter {
 public:
  explicit AdTaggingPageLoadMetricsTestWaiter(
      content::WebContents* web_contents)
      : page_load_metrics::PageLoadMetricsTestWaiter(web_contents) {}

  void AddMinimumCompleteAdResourcesExpectation(int num_ad_resources) {
    expected_minimum_complete_ad_resources_ = num_ad_resources;
  }

  void AddMinimumCompleteVanillaResourcesExpectation(
      int num_vanilla_resources) {
    expected_minimum_complete_vanilla_resources_ = num_vanilla_resources;
  }

  int current_complete_ad_resources() { return current_complete_ad_resources_; }

  int current_complete_vanilla_resources() {
    return current_complete_vanilla_resources_;
  }

 protected:
  bool ExpectationsSatisfied() const override {
    return current_complete_ad_resources_ >=
               expected_minimum_complete_ad_resources_ &&
           current_complete_vanilla_resources_ >=
               expected_minimum_complete_vanilla_resources_ &&
           PageLoadMetricsTestWaiter::ExpectationsSatisfied();
  }

  void HandleResourceUpdate(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource)
      override {
    // Due to cache-aware loading of font resources, we see two resource loads
    // for resources not in the cache.  We ignore the first request, which
    // results in a cache miss, by using the fact that it will have no received
    // data. Note that this only impacts this class's expectations, not those in
    // PageLoadMetricsTestWaiter itself.
    if (resource->is_complete && resource->received_data_length != 0) {
      if (resource->reported_as_ad_resource) {
        current_complete_ad_resources_++;
      } else {
        current_complete_vanilla_resources_++;
      }
    }
  }

 private:
  int current_complete_ad_resources_ = 0;
  int expected_minimum_complete_ad_resources_ = 0;

  int current_complete_vanilla_resources_ = 0;
  int expected_minimum_complete_vanilla_resources_ = 0;
};

class AdTaggingBrowserTest : public SubresourceFilterBrowserTest {
 public:
  AdTaggingBrowserTest() : SubresourceFilterBrowserTest() {}
  ~AdTaggingBrowserTest() override {}

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    // Allowlist rules are only checked if there is a matching blocklist rule.
    SetRulesetWithRules({CreateSuffixRule("ad_script.js"),
                         CreateSuffixRule("ad=true"),
                         CreateSuffixRule("allowed=true"),
                         CreateAllowlistSuffixRule("allowed=true")});
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
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

  // Creates a frame and aborts the initial load with a doc.write. Returns after
  // navigation has completed.
  content::RenderFrameHost* CreateFrameWithDocWriteAbortedLoad(
      const content::ToRenderFrameHost& adapter) {
    return CreateFrameWithDocWriteAbortedLoadImpl(adapter,
                                                  false /* ad_script */);
  }

  // Creates a frame and aborts the initial load with a doc.write. The script
  // creating the frame is an ad script. Returns after navigation has completed.
  content::RenderFrameHost* CreateFrameWithDocWriteAbortedLoadFromAdScript(
      const content::ToRenderFrameHost& adapter) {
    return CreateFrameWithDocWriteAbortedLoadImpl(adapter,
                                                  true /* ad_script */);
  }

  // Creates a frame and aborts the initial load with a window.stop. Returns
  // after navigation has completed.
  content::RenderFrameHost* CreateFrameWithWindowStopAbortedLoad(
      const content::ToRenderFrameHost& adapter) {
    return CreateFrameWithWindowStopAbortedLoadImpl(adapter,
                                                    false /* ad_script */);
  }

  // Creates a frame and aborts the initial load with a window.stop. The script
  // creating the frame is an ad script. Returns after navigation has completed.
  content::RenderFrameHost* CreateFrameWithWindowStopAbortedLoadFromAdScript(
      const content::ToRenderFrameHost& adapter) {
    return CreateFrameWithWindowStopAbortedLoadImpl(adapter,
                                                    true /* ad_script */);
  }

  // Given a RenderFrameHost, navigates the page to the given |url| and waits
  // for the navigation to complete before returning.
  void NavigateFrame(content::RenderFrameHost* render_frame_host,
                     const GURL& url);

  GURL GetURL(const std::string& page) {
    return embedded_test_server()->GetURL("/ad_tagging/" + page);
  }

  std::unique_ptr<AdTaggingPageLoadMetricsTestWaiter>
  CreateAdTaggingPageLoadMetricsTestWaiter() {
    return std::make_unique<AdTaggingPageLoadMetricsTestWaiter>(
        GetWebContents());
  }

 private:
  content::RenderFrameHost* CreateDocWrittenFrameImpl(
      const content::ToRenderFrameHost& adapter,
      bool ad_script);

  content::RenderFrameHost* CreateFrameWithDocWriteAbortedLoadImpl(
      const content::ToRenderFrameHost& adapter,
      bool ad_script);

  content::RenderFrameHost* CreateFrameWithWindowStopAbortedLoadImpl(
      const content::ToRenderFrameHost& adapter,
      bool ad_script);

  DISALLOW_COPY_AND_ASSIGN(AdTaggingBrowserTest);
};

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

content::RenderFrameHost*
AdTaggingBrowserTest::CreateFrameWithDocWriteAbortedLoadImpl(
    const content::ToRenderFrameHost& adapter,
    bool ad_script) {
  content::RenderFrameHost* rfh = adapter.render_frame_host();
  std::string name = GetUniqueFrameName();
  std::string script =
      base::StringPrintf("%s('%s');",
                         ad_script ? "createAdFrameWithDocWriteAbortedLoad"
                                   : "createFrameWithDocWriteAbortedLoad",
                         name.c_str());
  // The executed scripts set the title to be the frame name when they have
  // finished loading.
  content::TitleWatcher title_watcher(GetWebContents(),
                                      base::ASCIIToUTF16(name));
  EXPECT_TRUE(content::ExecuteScript(rfh, script));
  EXPECT_EQ(base::ASCIIToUTF16(name), title_watcher.WaitAndGetTitle());
  return content::FrameMatchingPredicate(
      content::WebContents::FromRenderFrameHost(rfh),
      base::BindRepeating(&content::FrameMatchesName, name));
}

content::RenderFrameHost*
AdTaggingBrowserTest::CreateFrameWithWindowStopAbortedLoadImpl(
    const content::ToRenderFrameHost& adapter,
    bool ad_script) {
  content::RenderFrameHost* rfh = adapter.render_frame_host();
  std::string name = GetUniqueFrameName();
  std::string script =
      base::StringPrintf("%s('%s');",
                         ad_script ? "createAdFrameWithWindowStopAbortedLoad"
                                   : "createFrameWithWindowStopAbortedLoad",
                         name.c_str());
  // The executed scripts set the title to be the frame name when they have
  // finished loading.
  content::TitleWatcher title_watcher(GetWebContents(),
                                      base::ASCIIToUTF16(name));
  EXPECT_TRUE(content::ExecuteScript(rfh, script));
  EXPECT_EQ(base::ASCIIToUTF16(name), title_watcher.WaitAndGetTitle());
  return content::FrameMatchingPredicate(
      content::WebContents::FromRenderFrameHost(rfh),
      base::BindRepeating(&content::FrameMatchesName, name));
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

IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       AdContentSettingAllowed_AdTaggingDisabled) {
  HostContentSettingsMapFactory::GetForProfile(
      web_contents()->GetBrowserContext())
      ->SetDefaultContentSetting(ContentSettingsType::ADS,
                                 CONTENT_SETTING_ALLOW);

  TestSubresourceFilterObserver observer(web_contents());
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Create an ad frame.
  GURL ad_url = GetURL("frame_factory.html?2&ad=true");
  RenderFrameHost* ad_frame = CreateSrcFrame(GetWebContents(), ad_url);

  // Verify that we are not evaluating subframe loads.
  EXPECT_FALSE(observer.GetSubframeLoadPolicy(ad_url).has_value());
  EXPECT_FALSE(observer.GetIsAdSubframe(ad_frame->GetFrameTreeNodeId()));

  // Child frame created by ad script.
  RenderFrameHost* ad_frame_tagged_by_script = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html?1"));

  // No frames should be detected by script heuristics.
  EXPECT_FALSE(observer.GetIsAdSubframe(
      ad_frame_tagged_by_script->GetFrameTreeNodeId()));
}

IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       AdContentSettingBlocked_AdTaggingEnabled) {
  HostContentSettingsMapFactory::GetForProfile(
      web_contents()->GetBrowserContext())
      ->SetDefaultContentSetting(ContentSettingsType::ADS,
                                 CONTENT_SETTING_BLOCK);

  TestSubresourceFilterObserver observer(web_contents());
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Create an ad frame.
  GURL ad_url = GetURL("frame_factory.html?2&ad=true");
  RenderFrameHost* ad_frame = CreateSrcFrame(GetWebContents(), ad_url);

  // Verify that we are evaluating subframe loads.
  EXPECT_TRUE(observer.GetSubframeLoadPolicy(ad_url).has_value());
  EXPECT_TRUE(observer.GetIsAdSubframe(ad_frame->GetFrameTreeNodeId()));

  // Child frame created by ad script.
  RenderFrameHost* ad_frame_tagged_by_script = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html?1"));

  // Frames should be detected by script heuristics.
  EXPECT_TRUE(observer.GetIsAdSubframe(
      ad_frame_tagged_by_script->GetFrameTreeNodeId()));
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
  EXPECT_FALSE(observer.GetIsAdSubframe(vanilla_child->GetFrameTreeNodeId()));

  // (2) Ad child.
  RenderFrameHost* ad_child =
      CreateSrcFrame(GetWebContents(), GetURL("frame_factory.html?2&ad=true"));
  EXPECT_TRUE(observer.GetIsAdSubframe(ad_child->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_child, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript);

  // (3) Ad child of 2.
  RenderFrameHost* ad_child_2 =
      CreateSrcFrame(ad_child, GetURL("frame_factory.html?sub=1&3&ad=true"));
  EXPECT_TRUE(observer.GetIsAdSubframe(ad_child_2->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_child_2, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  // (4) Vanilla child of 2.
  RenderFrameHost* vanilla_child_2 =
      CreateSrcFrame(ad_child, GetURL("frame_factory.html?4"));
  EXPECT_TRUE(observer.GetIsAdSubframe(vanilla_child_2->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      vanilla_child_2, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  // (5) Vanilla child of 1. This tests something subtle.
  // frame_factory.html?ad=true loads the same script that frame_factory.html
  // uses to load frames. This tests that even though the script is tagged as an
  // ad in the ad iframe, it's not considered an ad in the main frame, hence
  // it's able to create an iframe that's not labeled as an ad.
  RenderFrameHost* vanilla_child_3 =
      CreateSrcFrame(vanilla_child, GetURL("frame_factory.html?5"));
  EXPECT_FALSE(observer.GetIsAdSubframe(vanilla_child_3->GetFrameTreeNodeId()));
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
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(kSubresourceFilterOriginStatusHistogram,
                                      page_load_metrics::OriginStatus::kSame,
                                      1);
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
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // TODO(johnidel): Check that frame was reported properly. See
  // crbug.com/914893.
}

// Ad script creates a frame and navigates it cross origin.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       VerifyCrossOriginWithImmediateNavigate) {
  base::HistogramTester histogram_tester;

  auto waiter = CreateAdTaggingPageLoadMetricsTestWaiter();
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
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(kSubresourceFilterOriginStatusHistogram,
                                      page_load_metrics::OriginStatus::kCross,
                                      1);
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
      GetWebContents(), GetURL("frame_factory.html?ad=true"));

  // Navigate the subframe to a cross origin site.
  NavigateFrame(ad_child, embedded_test_server()->GetURL(
                              "b.com", "/ad_tagging/frame_factory.html"));

  // Navigate away and ensure we report same origin.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  histogram_tester.ExpectUniqueSample(kSubresourceFilterOriginStatusHistogram,
                                      page_load_metrics::OriginStatus::kSame,
                                      1);
}

// Test that a subframe with a non-ad url but loaded by ad script is an ad.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, FrameLoadedByAdScript) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Child frame created by ad script.
  RenderFrameHost* ad_child = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html?1"));
  EXPECT_TRUE(observer.GetIsAdSubframe(ad_child->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_child, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
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
  ExpectFrameAdEvidence(
      ad_frame, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
}

// Test that the children same-origin doc.write created iframes are tagged as
// ads where appropriate.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       ChildrenOfSameOriginFrames_CorrectlyTagged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Vanilla frame and descendants
  content::RenderFrameHost* vanilla_frame =
      CreateDocWrittenFrame(GetWebContents());
  EXPECT_FALSE(observer.GetIsAdSubframe(vanilla_frame->GetFrameTreeNodeId()));

  content::RenderFrameHost* vanilla_child_of_vanilla =
      CreateSrcFrame(vanilla_frame, GetURL("frame_factory.html"));
  EXPECT_FALSE(
      observer.GetIsAdSubframe(vanilla_child_of_vanilla->GetFrameTreeNodeId()));

  content::RenderFrameHost* ad_child_of_vanilla =
      CreateSrcFrameFromAdScript(vanilla_frame, GetURL("frame_factory.html"));
  EXPECT_TRUE(
      observer.GetIsAdSubframe(ad_child_of_vanilla->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_child_of_vanilla, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  // Ad frame and descendants
  content::RenderFrameHost* ad_frame =
      CreateDocWrittenFrameFromAdScript(GetWebContents());
  ExpectFrameAdEvidence(
      ad_frame, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  content::RenderFrameHost* vanilla_child_of_ad =
      CreateSrcFrame(ad_frame, GetURL("frame_factory.html"));
  EXPECT_TRUE(
      observer.GetIsAdSubframe(vanilla_child_of_ad->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      vanilla_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  content::RenderFrameHost* ad_child_of_ad =
      CreateSrcFrameFromAdScript(ad_frame, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdSubframe(ad_child_of_ad->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
}

// Test that frames with an aborted initial load due to a doc.write are still
// correctly tagged as ads or not.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       FramesWithDocWriteAbortedLoad_StillCorrectlyTagged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Vanilla child.
  content::RenderFrameHost* vanilla_frame_with_aborted_load =
      CreateFrameWithDocWriteAbortedLoad(GetWebContents());
  EXPECT_FALSE(observer.GetIsAdSubframe(
      vanilla_frame_with_aborted_load->GetFrameTreeNodeId()));

  // Child created by ad script.
  content::RenderFrameHost* ad_frame_with_aborted_load =
      CreateFrameWithDocWriteAbortedLoadFromAdScript(GetWebContents());
  EXPECT_TRUE(observer.GetIsAdSubframe(
      ad_frame_with_aborted_load->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_frame_with_aborted_load,
      /*parent_is_ad=*/false, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  // Child with ad parent.
  content::RenderFrameHost* ad_frame = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html"));
  content::RenderFrameHost* child_frame_of_ad_with_aborted_load =
      CreateFrameWithDocWriteAbortedLoad(ad_frame);
  EXPECT_TRUE(observer.GetIsAdSubframe(ad_frame->GetFrameTreeNodeId()));
  EXPECT_TRUE(observer.GetIsAdSubframe(
      child_frame_of_ad_with_aborted_load->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      child_frame_of_ad_with_aborted_load,
      /*parent_is_ad=*/true, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
}

// Test that frames with an aborted initial load due to a window.stop are still
// correctly tagged as ads or not.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       FramesWithWindowStopAbortedLoad_StillCorrectlyTagged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Vanilla child.
  content::RenderFrameHost* vanilla_frame_with_aborted_load =
      CreateFrameWithWindowStopAbortedLoad(GetWebContents());
  EXPECT_FALSE(observer.GetIsAdSubframe(
      vanilla_frame_with_aborted_load->GetFrameTreeNodeId()));

  // Child created by ad script.
  content::RenderFrameHost* ad_frame_with_aborted_load =
      CreateFrameWithWindowStopAbortedLoadFromAdScript(GetWebContents());
  EXPECT_TRUE(observer.GetIsAdSubframe(
      ad_frame_with_aborted_load->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_frame_with_aborted_load,
      /*parent_is_ad=*/false, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  // Child with ad parent.
  content::RenderFrameHost* ad_frame = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html"));
  content::RenderFrameHost* child_frame_of_ad_with_aborted_load =
      CreateFrameWithWindowStopAbortedLoad(ad_frame);
  EXPECT_TRUE(observer.GetIsAdSubframe(ad_frame->GetFrameTreeNodeId()));
  EXPECT_TRUE(observer.GetIsAdSubframe(
      child_frame_of_ad_with_aborted_load->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      child_frame_of_ad_with_aborted_load,
      /*parent_is_ad=*/true, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
}

// Test that the children of a frame with its initial load aborted due to a
// doc.write are reported correctly as vanilla or ad frames.
IN_PROC_BROWSER_TEST_F(
    AdTaggingBrowserTest,
    ChildrenOfFrameWithDocWriteAbortedLoad_StillCorrectlyTagged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Create a frame and abort its initial load in vanilla script. The children
  // of this vanilla frame should be taggged correctly.
  content::RenderFrameHost* vanilla_frame_with_aborted_load =
      CreateFrameWithDocWriteAbortedLoad(GetWebContents());

  content::RenderFrameHost* vanilla_child_of_vanilla = CreateSrcFrame(
      vanilla_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_FALSE(
      observer.GetIsAdSubframe(vanilla_child_of_vanilla->GetFrameTreeNodeId()));

  content::RenderFrameHost* ad_child_of_vanilla = CreateSrcFrameFromAdScript(
      vanilla_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(
      observer.GetIsAdSubframe(ad_child_of_vanilla->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_child_of_vanilla, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  // Create a frame and abort its initial load in ad script. The children of
  // this ad frame should be tagged as ads.
  content::RenderFrameHost* ad_frame_with_aborted_load =
      CreateFrameWithDocWriteAbortedLoadFromAdScript(GetWebContents());
  EXPECT_TRUE(observer.GetIsAdSubframe(
      ad_frame_with_aborted_load->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_frame_with_aborted_load,
      /*parent_is_ad=*/false, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  content::RenderFrameHost* vanilla_child_of_ad =
      CreateSrcFrame(ad_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(
      observer.GetIsAdSubframe(vanilla_child_of_ad->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      vanilla_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  content::RenderFrameHost* ad_child_of_ad = CreateSrcFrameFromAdScript(
      ad_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdSubframe(ad_child_of_ad->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
}

// Test that the children of a frame with its initial load aborted due to a
// window.stop are reported correctly as vanilla or ad frames.
// This test is flaky. See crbug.com/1069346.
IN_PROC_BROWSER_TEST_F(
    AdTaggingBrowserTest,
    ChildrenOfFrameWithWindowStopAbortedLoad_StillCorrectlyTagged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Create a frame and abort its initial load in vanilla script. The children
  // of this vanilla frame should be taggged correctly.
  content::RenderFrameHost* vanilla_frame_with_aborted_load =
      CreateFrameWithWindowStopAbortedLoad(GetWebContents());

  content::RenderFrameHost* vanilla_child_of_vanilla = CreateSrcFrame(
      vanilla_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_FALSE(
      observer.GetIsAdSubframe(vanilla_child_of_vanilla->GetFrameTreeNodeId()));

  content::RenderFrameHost* ad_child_of_vanilla = CreateSrcFrameFromAdScript(
      vanilla_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(
      observer.GetIsAdSubframe(ad_child_of_vanilla->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_child_of_vanilla, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  // Create a frame and abort its initial load in ad script. The children of
  // this ad frame should be tagged as ads.
  content::RenderFrameHost* ad_frame_with_aborted_load =
      CreateFrameWithWindowStopAbortedLoadFromAdScript(GetWebContents());
  EXPECT_TRUE(observer.GetIsAdSubframe(
      ad_frame_with_aborted_load->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      ad_frame_with_aborted_load,
      /*parent_is_ad=*/false, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  content::RenderFrameHost* vanilla_child_of_ad =
      CreateSrcFrame(ad_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(
      observer.GetIsAdSubframe(vanilla_child_of_ad->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      vanilla_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);

  content::RenderFrameHost* ad_child_of_ad = CreateSrcFrameFromAdScript(
      ad_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdSubframe(ad_child_of_ad->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      vanilla_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript);
}

// Test that navigating a frame to a URL with a less restrictive load policy
// updates the latest filter list result, but not the most-restrictive one.
IN_PROC_BROWSER_TEST_F(
    AdTaggingBrowserTest,
    NavigationToLessRestrictiveUrl_MostRestrictiveLoadPolicyUnchanged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  content::RenderFrameHost* test_frame = CreateSrcFrame(
      GetWebContents(), GetURL("frame_factory.html?allowed=true"));
  EXPECT_FALSE(observer.GetIsAdSubframe(test_frame->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      test_frame, /*parent_is_ad=*/false,
      /*latest_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedAllowingRule,
      /*most_restrictive_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedAllowingRule,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript);

  NavigateFrame(test_frame, GetURL("frame_factory.html"));
  EXPECT_FALSE(observer.GetIsAdSubframe(test_frame->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      test_frame, /*parent_is_ad=*/false,
      /*latest_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedNoRules,
      /*most_restrictive_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript);

  NavigateFrame(test_frame, GetURL("frame_factory.html?allowed=true"));
  EXPECT_FALSE(observer.GetIsAdSubframe(test_frame->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      test_frame, /*parent_is_ad=*/false,
      /*latest_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedAllowingRule,
      /*most_restrictive_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript);

  NavigateFrame(test_frame, GetURL("frame_factory.html?ad=true"));
  EXPECT_TRUE(observer.GetIsAdSubframe(test_frame->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      test_frame, /*parent_is_ad=*/false,
      /*latest_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      /*most_restrictive_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript);

  NavigateFrame(test_frame, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdSubframe(test_frame->GetFrameTreeNodeId()));
  ExpectFrameAdEvidence(
      test_frame, /*parent_is_ad=*/false,
      /*latest_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedNoRules,
      /*most_restrictive_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript);
}

// Basic vanilla stylesheet with vanilla font and image.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       VanillaExternalStylesheet_ResourcesNotTagged) {
  auto waiter = CreateAdTaggingPageLoadMetricsTestWaiter();

  ui_test_utils::NavigateToURL(browser(), GetURL("test_div.html"));
  AddExternalStylesheet(GetWebContents(),
                        GetURL("sheet_with_vanilla_resources.css"));

  // Document, favicon, stylesheet, font, background-image
  waiter->AddMinimumCompleteVanillaResourcesExpectation(5);
  waiter->Wait();

  EXPECT_EQ(waiter->current_complete_vanilla_resources(), 5);
  EXPECT_EQ(waiter->current_complete_ad_resources(), 0);
}

// Basic ad stylesheet with vanilla font and image.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       AdExternalStylesheet_ResourcesTagged) {
  auto waiter = CreateAdTaggingPageLoadMetricsTestWaiter();

  ui_test_utils::NavigateToURL(browser(), GetURL("test_div.html"));
  AddExternalStylesheet(GetWebContents(),
                        GetURL("sheet_with_vanilla_resources.css?ad=true"));

  // Document, favicon
  waiter->AddMinimumCompleteVanillaResourcesExpectation(2);
  // Stylesheet, font, background-image
  waiter->AddMinimumCompleteAdResourcesExpectation(3);
  waiter->Wait();

  EXPECT_EQ(waiter->current_complete_vanilla_resources(), 2);
  EXPECT_EQ(waiter->current_complete_ad_resources(), 3);
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
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
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
    command_line->AppendSwitch(embedder_support::kDisablePopupBlocking);
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
    All,
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
    All,
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
    All,
    AdTaggingEventWithScriptInStackBrowserTest,
    ::testing::Bool());

}  // namespace

}  // namespace subresource_filter
