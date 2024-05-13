// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
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
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
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

  AdTaggingBrowserTest(const AdTaggingBrowserTest&) = delete;
  AdTaggingBrowserTest& operator=(const AdTaggingBrowserTest&) = delete;

  ~AdTaggingBrowserTest() override {}

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    // For subdocument resources, allowlist rules are always checked.
    SetRulesetWithRules({CreateSuffixRule("ad_script.js"),
                         CreateSuffixRule("ad=true"),
                         CreateAllowlistSuffixRule("allowed=true")});

    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

    // The test assumes page gets deleted after navigation, triggering metrics
    // recording. Disable back/forward cache to ensure that pages don't get
    // preserved in the cache.
    content::DisableBackForwardCacheForTesting(
        web_contents(), content::BackForwardCache::TEST_REQUIRES_NO_CACHING);
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

  void ExpectWindowOpenUkmEntry(bool from_root_frame,
                                const GURL& root_frame_url,
                                bool from_ad_frame,
                                bool from_ad_script) {
    auto entries = ukm_recorder_->GetEntriesByName(
        ukm::builders::AbusiveExperienceHeuristic_WindowOpen::kEntryName);
    EXPECT_EQ(1u, entries.size());

    // Check that the event is keyed to |root_frame_url| only if it was from the
    // top frame.
    if (from_root_frame) {
      ukm_recorder_->ExpectEntrySourceHasUrl(entries.back(), root_frame_url);
    } else {
      EXPECT_FALSE(
          ukm_recorder_->GetSourceForSourceId(entries.back()->source_id));
    }

    // Check that a DocumentCreated entry was created, and it's keyed to
    // |root_frame_url| only if it was from the top frame. However, we can
    // always use the navigation source ID to link this source to
    // |root_frame_url|.
    const ukm::mojom::UkmEntry* dc_entry =
        ukm_recorder_->GetDocumentCreatedEntryForSourceId(
            entries.back()->source_id);
    EXPECT_TRUE(dc_entry);
    EXPECT_EQ(entries.back()->source_id, dc_entry->source_id);
    if (from_root_frame) {
      ukm_recorder_->ExpectEntrySourceHasUrl(dc_entry, root_frame_url);
    } else {
      EXPECT_FALSE(ukm_recorder_->GetSourceForSourceId(dc_entry->source_id));
    }

    const ukm::UkmSource* navigation_source =
        ukm_recorder_->GetSourceForSourceId(*ukm_recorder_->GetEntryMetric(
            dc_entry, ukm::builders::DocumentCreated::kNavigationSourceIdName));
    EXPECT_EQ(root_frame_url, navigation_source->url());

    EXPECT_EQ(from_root_frame,
              *ukm_recorder_->GetEntryMetric(
                  dc_entry, ukm::builders::DocumentCreated::kIsMainFrameName));

    ukm_recorder_->ExpectEntryMetric(
        entries.back(),
        ukm::builders::AbusiveExperienceHeuristic_WindowOpen::
            kFromAdSubframeName,
        from_ad_frame);
    ukm_recorder_->ExpectEntryMetric(
        entries.back(),
        ukm::builders::AbusiveExperienceHeuristic_WindowOpen::kFromAdScriptName,
        from_ad_script);
  }

  bool HasAdClickMainFrameNavigationUseCounterForUrl(const GURL& url) const {
    int count = 0;

    auto entries = ukm_recorder_->GetEntriesByName(
        ukm::builders::Blink_UseCounter::kEntryName);
    for (const ukm::mojom::UkmEntry* entry : entries) {
      const ukm::UkmSource* src =
          ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (src->url() != url) {
        continue;
      }

      const int64_t* metric = ukm_recorder_->GetEntryMetric(
          entry, ukm::builders::Blink_UseCounter::kFeatureName);
      if (!metric) {
        continue;
      }

      if (*metric !=
          static_cast<int>(
              blink::mojom::WebFeature::kAdClickMainFrameNavigation)) {
        continue;
      }

      count++;
    }

    CHECK_LE(count, 1);
    return (count == 1);
  }

  void NavigateAwayToFlushUseCounterUKM(content::WebContents* web_contents) {
    ASSERT_TRUE(
        content::NavigateToURL(web_contents, GURL(url::kAboutBlankURL)));
  }

 protected:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

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
  EXPECT_EQ(true, content::EvalJs(rfh, script));
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded())
      << navigation_observer.last_net_error_code();
  return content::FrameMatchingPredicate(
      rfh->GetPage(), base::BindRepeating(&content::FrameMatchesName, name));
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
  EXPECT_TRUE(content::ExecJs(rfh, script));
  EXPECT_EQ(base::ASCIIToUTF16(name), title_watcher.WaitAndGetTitle());
  return content::FrameMatchingPredicate(
      rfh->GetPage(), base::BindRepeating(&content::FrameMatchesName, name));
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
  EXPECT_TRUE(content::ExecJs(rfh, script));
  EXPECT_EQ(base::ASCIIToUTF16(name), title_watcher.WaitAndGetTitle());
  return content::FrameMatchingPredicate(
      rfh->GetPage(), base::BindRepeating(&content::FrameMatchesName, name));
}

// Given a RenderFrameHost, navigates the page to the given |url| and waits
// for the navigation to complete before returning.
void AdTaggingBrowserTest::NavigateFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url) {
  std::string script =
      base::StringPrintf(R"(window.location='%s')", url.spec().c_str());
  content::TestNavigationObserver navigation_observer(GetWebContents(), 1);
  EXPECT_TRUE(content::ExecJs(render_frame_host, script));
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
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  // Create an ad frame.
  GURL ad_url = GetURL("frame_factory.html?2&ad=true");
  RenderFrameHost* ad_frame = CreateSrcFrame(GetWebContents(), ad_url);

  // Verify that we are not evaluating child frame loads.
  EXPECT_FALSE(observer.GetChildFrameLoadPolicy(ad_url).has_value());
  EXPECT_FALSE(observer.GetIsAdFrame(ad_frame->GetFrameTreeNodeId()));

  // Child frame created by ad script.
  RenderFrameHost* ad_frame_tagged_by_script = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html?1"));

  // No frames should be detected by script heuristics.
  EXPECT_FALSE(
      observer.GetIsAdFrame(ad_frame_tagged_by_script->GetFrameTreeNodeId()));
}

IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       AdContentSettingBlocked_AdTaggingEnabled) {
  HostContentSettingsMapFactory::GetForProfile(
      web_contents()->GetBrowserContext())
      ->SetDefaultContentSetting(ContentSettingsType::ADS,
                                 CONTENT_SETTING_BLOCK);

  TestSubresourceFilterObserver observer(web_contents());
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  // Create an ad frame.
  GURL ad_url = GetURL("frame_factory.html?2&ad=true");
  RenderFrameHost* ad_frame = CreateSrcFrame(GetWebContents(), ad_url);

  // Verify that we are evaluating child frame loads.
  EXPECT_TRUE(observer.GetChildFrameLoadPolicy(ad_url).has_value());
  EXPECT_TRUE(observer.GetIsAdFrame(ad_frame->GetFrameTreeNodeId()));

  // Child frame created by ad script.
  RenderFrameHost* ad_frame_tagged_by_script = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html?1"));

  // Frames should be detected by script heuristics.
  EXPECT_TRUE(
      observer.GetIsAdFrame(ad_frame_tagged_by_script->GetFrameTreeNodeId()));
}

IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, FramesByURL) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));
  EXPECT_FALSE(observer.GetIsAdFrame(
      GetWebContents()->GetPrimaryMainFrame()->GetFrameTreeNodeId()));

  // (1) Vanilla child.
  content::RenderFrameHost* vanilla_child =
      CreateSrcFrame(GetWebContents(), GetURL("frame_factory.html?1"));
  EXPECT_FALSE(observer.GetIsAdFrame(vanilla_child->GetFrameTreeNodeId()));

  // (2) Ad child.
  RenderFrameHost* ad_child =
      CreateSrcFrame(GetWebContents(), GetURL("frame_factory.html?2&ad=true"));
  EXPECT_TRUE(observer.GetIsAdFrame(ad_child->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_child, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript));

  // (3) Ad child of 2.
  RenderFrameHost* ad_child_2 =
      CreateSrcFrame(ad_child, GetURL("frame_factory.html?sub=1&3&ad=true"));
  EXPECT_TRUE(observer.GetIsAdFrame(ad_child_2->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_child_2, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  // (4) Vanilla child of 2.
  RenderFrameHost* vanilla_child_2 =
      CreateSrcFrame(ad_child, GetURL("frame_factory.html?4"));
  EXPECT_TRUE(observer.GetIsAdFrame(vanilla_child_2->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      vanilla_child_2, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  // (5) Vanilla child of 1. This tests something subtle.
  // frame_factory.html?ad=true loads the same script that frame_factory.html
  // uses to load frames. This tests that even though the script is tagged as an
  // ad in the ad iframe, it's not considered an ad in the main frame, hence
  // it's able to create an iframe that's not labeled as an ad.
  RenderFrameHost* vanilla_child_3 =
      CreateSrcFrame(vanilla_child, GetURL("frame_factory.html?5"));
  EXPECT_FALSE(observer.GetIsAdFrame(vanilla_child_3->GetFrameTreeNodeId()));
}

const char kSubresourceFilterOriginStatusHistogram[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "OriginStatus";

IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, VerifySameOriginWithoutNavigate) {
  // The test assumes pages gets deleted after navigation, triggering histogram
  // recording. Disable back/forward cache to ensure that pages don't get
  // preserved in the cache.
  // TODO(crbug.com/40189815): Investigate if this needs further fix.
  content::DisableBackForwardCacheForTesting(
      browser()->tab_strip_model()->GetActiveWebContents(),
      content::BackForwardCache::TEST_REQUIRES_NO_CACHING);

  base::HistogramTester histogram_tester;

  // Main frame.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  // Ad frame via doc write.
  CreateDocWrittenFrameFromAdScript(GetWebContents());

  // Navigate away and ensure we report same origin.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  histogram_tester.ExpectUniqueSample(kSubresourceFilterOriginStatusHistogram,
                                      page_load_metrics::OriginStatus::kSame,
                                      1);
}

IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, VerifyCrossOriginWithoutNavigate) {
  base::HistogramTester histogram_tester;

  // Main frame.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  // Regular frame that's cross origin and has a doc write ad of its own.
  RenderFrameHost* regular_child = CreateSrcFrame(
      GetWebContents(), embedded_test_server()->GetURL(
                            "b.com", "/ad_tagging/frame_factory.html"));
  CreateDocWrittenFrameFromAdScript(regular_child);

  // Navigate away and ensure we report cross origin.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // TODO(johnidel): Check that frame was reported properly. See
  // crbug.com/914893.
}

// Ad script creates a frame and navigates it cross origin.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       VerifyCrossOriginWithImmediateNavigate) {
  base::HistogramTester histogram_tester;

  auto waiter = CreateAdTaggingPageLoadMetricsTestWaiter();
  // Create the main frame and cross origin subframe from an ad script.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));
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
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
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
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));
  RenderFrameHost* ad_child = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html?ad=true"));

  // Navigate the subframe to a cross origin site.
  NavigateFrame(ad_child, embedded_test_server()->GetURL(
                              "b.com", "/ad_tagging/frame_factory.html"));

  // Navigate away and ensure we report same origin.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  histogram_tester.ExpectUniqueSample(kSubresourceFilterOriginStatusHistogram,
                                      page_load_metrics::OriginStatus::kSame,
                                      1);
}

// Test that a subframe with a non-ad url but loaded by ad script is an ad.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, FrameLoadedByAdScript) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  // Child frame created by ad script.
  RenderFrameHost* ad_child = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html?1"));
  EXPECT_TRUE(observer.GetIsAdFrame(ad_child->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_child, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
}

// Test that same-origin doc.write created iframes are tagged as ads.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, SameOriginFrameTagging) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  // (1) Vanilla child.
  content::RenderFrameHost* vanilla_frame =
      CreateDocWrittenFrame(GetWebContents());
  EXPECT_FALSE(observer.GetIsAdFrame(vanilla_frame->GetFrameTreeNodeId()));

  // (2) Ad child.
  content::RenderFrameHost* ad_frame =
      CreateDocWrittenFrameFromAdScript(GetWebContents());
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_frame, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
}

// Test that the children same-origin doc.write created iframes are tagged as
// ads where appropriate.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       ChildrenOfSameOriginFrames_CorrectlyTagged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  // Vanilla frame and descendants
  content::RenderFrameHost* vanilla_frame =
      CreateDocWrittenFrame(GetWebContents());
  EXPECT_FALSE(observer.GetIsAdFrame(vanilla_frame->GetFrameTreeNodeId()));

  content::RenderFrameHost* vanilla_child_of_vanilla =
      CreateSrcFrame(vanilla_frame, GetURL("frame_factory.html"));
  EXPECT_FALSE(
      observer.GetIsAdFrame(vanilla_child_of_vanilla->GetFrameTreeNodeId()));

  content::RenderFrameHost* ad_child_of_vanilla =
      CreateSrcFrameFromAdScript(vanilla_frame, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdFrame(ad_child_of_vanilla->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_child_of_vanilla, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  // Ad frame and descendants
  content::RenderFrameHost* ad_frame =
      CreateDocWrittenFrameFromAdScript(GetWebContents());
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_frame, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  content::RenderFrameHost* vanilla_child_of_ad =
      CreateSrcFrame(ad_frame, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdFrame(vanilla_child_of_ad->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      vanilla_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  content::RenderFrameHost* ad_child_of_ad =
      CreateSrcFrameFromAdScript(ad_frame, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdFrame(ad_child_of_ad->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
}

// Test that frames with an aborted initial load due to a doc.write are still
// correctly tagged as ads or not.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       FramesWithDocWriteAbortedLoad_StillCorrectlyTagged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  // Vanilla child.
  content::RenderFrameHost* vanilla_frame_with_aborted_load =
      CreateFrameWithDocWriteAbortedLoad(GetWebContents());
  EXPECT_FALSE(observer.GetIsAdFrame(
      vanilla_frame_with_aborted_load->GetFrameTreeNodeId()));

  // Child created by ad script.
  content::RenderFrameHost* ad_frame_with_aborted_load =
      CreateFrameWithDocWriteAbortedLoadFromAdScript(GetWebContents());
  EXPECT_TRUE(
      observer.GetIsAdFrame(ad_frame_with_aborted_load->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_frame_with_aborted_load,
      /*parent_is_ad=*/false, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  // Child with ad parent.
  content::RenderFrameHost* ad_frame = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html"));
  content::RenderFrameHost* child_frame_of_ad_with_aborted_load =
      CreateFrameWithDocWriteAbortedLoad(ad_frame);
  EXPECT_TRUE(observer.GetIsAdFrame(ad_frame->GetFrameTreeNodeId()));
  EXPECT_TRUE(observer.GetIsAdFrame(
      child_frame_of_ad_with_aborted_load->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      child_frame_of_ad_with_aborted_load,
      /*parent_is_ad=*/true, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
}

// Test that frames with an aborted initial load due to a window.stop are still
// correctly tagged as ads or not.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       FramesWithWindowStopAbortedLoad_StillCorrectlyTagged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  // Vanilla child.
  content::RenderFrameHost* vanilla_frame_with_aborted_load =
      CreateFrameWithWindowStopAbortedLoad(GetWebContents());
  EXPECT_FALSE(observer.GetIsAdFrame(
      vanilla_frame_with_aborted_load->GetFrameTreeNodeId()));

  // Child created by ad script.
  content::RenderFrameHost* ad_frame_with_aborted_load =
      CreateFrameWithWindowStopAbortedLoadFromAdScript(GetWebContents());
  EXPECT_TRUE(
      observer.GetIsAdFrame(ad_frame_with_aborted_load->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_frame_with_aborted_load,
      /*parent_is_ad=*/false, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  // Child with ad parent.
  content::RenderFrameHost* ad_frame = CreateSrcFrameFromAdScript(
      GetWebContents(), GetURL("frame_factory.html"));
  content::RenderFrameHost* child_frame_of_ad_with_aborted_load =
      CreateFrameWithWindowStopAbortedLoad(ad_frame);
  EXPECT_TRUE(observer.GetIsAdFrame(ad_frame->GetFrameTreeNodeId()));
  EXPECT_TRUE(observer.GetIsAdFrame(
      child_frame_of_ad_with_aborted_load->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      child_frame_of_ad_with_aborted_load,
      /*parent_is_ad=*/true, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
}

// Test that the children of a frame with its initial load aborted due to a
// doc.write are reported correctly as vanilla or ad frames.
IN_PROC_BROWSER_TEST_F(
    AdTaggingBrowserTest,
    ChildrenOfFrameWithDocWriteAbortedLoad_StillCorrectlyTagged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  // Create a frame and abort its initial load in vanilla script. The children
  // of this vanilla frame should be tagged correctly.
  content::RenderFrameHost* vanilla_frame_with_aborted_load =
      CreateFrameWithDocWriteAbortedLoad(GetWebContents());

  content::RenderFrameHost* vanilla_child_of_vanilla = CreateSrcFrame(
      vanilla_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_FALSE(
      observer.GetIsAdFrame(vanilla_child_of_vanilla->GetFrameTreeNodeId()));

  content::RenderFrameHost* ad_child_of_vanilla = CreateSrcFrameFromAdScript(
      vanilla_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdFrame(ad_child_of_vanilla->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_child_of_vanilla, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  // Create a frame and abort its initial load in ad script. The children of
  // this ad frame should be tagged as ads.
  content::RenderFrameHost* ad_frame_with_aborted_load =
      CreateFrameWithDocWriteAbortedLoadFromAdScript(GetWebContents());
  EXPECT_TRUE(
      observer.GetIsAdFrame(ad_frame_with_aborted_load->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_frame_with_aborted_load,
      /*parent_is_ad=*/false, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  content::RenderFrameHost* vanilla_child_of_ad =
      CreateSrcFrame(ad_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdFrame(vanilla_child_of_ad->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      vanilla_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  content::RenderFrameHost* ad_child_of_ad = CreateSrcFrameFromAdScript(
      ad_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdFrame(ad_child_of_ad->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
}

// Test that the children of a frame with its initial load aborted due to a
// window.stop are reported correctly as vanilla or ad frames.
// This test is flaky. See crbug.com/1069346.
IN_PROC_BROWSER_TEST_F(
    AdTaggingBrowserTest,
    ChildrenOfFrameWithWindowStopAbortedLoad_StillCorrectlyTagged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  // Create a frame and abort its initial load in vanilla script. The children
  // of this vanilla frame should be taggged correctly.
  content::RenderFrameHost* vanilla_frame_with_aborted_load =
      CreateFrameWithWindowStopAbortedLoad(GetWebContents());

  content::RenderFrameHost* vanilla_child_of_vanilla = CreateSrcFrame(
      vanilla_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_FALSE(
      observer.GetIsAdFrame(vanilla_child_of_vanilla->GetFrameTreeNodeId()));

  content::RenderFrameHost* ad_child_of_vanilla = CreateSrcFrameFromAdScript(
      vanilla_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdFrame(ad_child_of_vanilla->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_child_of_vanilla, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  // Create a frame and abort its initial load in ad script. The children of
  // this ad frame should be tagged as ads.
  content::RenderFrameHost* ad_frame_with_aborted_load =
      CreateFrameWithWindowStopAbortedLoadFromAdScript(GetWebContents());
  EXPECT_TRUE(
      observer.GetIsAdFrame(ad_frame_with_aborted_load->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      ad_frame_with_aborted_load,
      /*parent_is_ad=*/false, blink::mojom::FilterListResult::kNotChecked,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  content::RenderFrameHost* vanilla_child_of_ad =
      CreateSrcFrame(ad_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdFrame(vanilla_child_of_ad->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      vanilla_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  content::RenderFrameHost* ad_child_of_ad = CreateSrcFrameFromAdScript(
      ad_frame_with_aborted_load, GetURL("frame_factory.html"));
  EXPECT_TRUE(observer.GetIsAdFrame(ad_child_of_ad->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      vanilla_child_of_ad, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
}

// Test that navigating a frame to a URL with a less restrictive load policy
// updates the latest filter list result, but not the most-restrictive one.
IN_PROC_BROWSER_TEST_F(
    AdTaggingBrowserTest,
    NavigationToLessRestrictiveUrl_MostRestrictiveLoadPolicyUnchanged) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html")));

  content::RenderFrameHost* test_frame = CreateSrcFrame(
      GetWebContents(), GetURL("frame_factory.html?allowed=true"));
  EXPECT_FALSE(observer.GetIsAdFrame(test_frame->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      test_frame, /*parent_is_ad=*/false,
      /*latest_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedAllowingRule,
      /*most_restrictive_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedAllowingRule,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript));

  NavigateFrame(test_frame, GetURL("frame_factory.html"));
  test_frame = content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_FALSE(observer.GetIsAdFrame(test_frame->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      test_frame, /*parent_is_ad=*/false,
      /*latest_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedNoRules,
      /*most_restrictive_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript));

  NavigateFrame(test_frame, GetURL("frame_factory.html?allowed=true"));
  test_frame = content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_FALSE(observer.GetIsAdFrame(test_frame->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      test_frame, /*parent_is_ad=*/false,
      /*latest_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedAllowingRule,
      /*most_restrictive_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript));

  NavigateFrame(test_frame, GetURL("frame_factory.html?ad=true"));
  test_frame = content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(observer.GetIsAdFrame(test_frame->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      test_frame, /*parent_is_ad=*/false,
      /*latest_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      /*most_restrictive_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript));

  NavigateFrame(test_frame, GetURL("frame_factory.html"));
  test_frame = content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(observer.GetIsAdFrame(test_frame->GetFrameTreeNodeId()));
  EXPECT_TRUE(EvidenceForFrameComprises(
      test_frame, /*parent_is_ad=*/false,
      /*latest_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedNoRules,
      /*most_restrictive_filter_list_result=*/
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript));
}

// Basic vanilla stylesheet with vanilla font and image.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       VanillaExternalStylesheet_ResourcesNotTagged) {
  auto waiter = CreateAdTaggingPageLoadMetricsTestWaiter();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("test_div.html")));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetURL("test_div.html")));
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

// Regression test for crbug.com/336753737.
IN_PROC_BROWSER_TEST_F(
    AdTaggingBrowserTest,
    SameDocumentHistoryNavigationAbortedByParentHistoryNavigation) {
  GURL url = embedded_test_server()->GetURL(
      "a.com", "/ad_tagging/abort_regression.html");

  content::TestNavigationObserver navigation_observer(web_contents());

  // We should not crash in this line.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
}

class AdClickMetricsBrowserTest : public AdTaggingBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Popups without user gesture is blocked by default. Turn off the switch
    // here so as to test more variations of the landing page's status.
    command_line->AppendSwitch(embedder_support::kDisablePopupBlocking);
  }
};

IN_PROC_BROWSER_TEST_F(AdClickMetricsBrowserTest, BrowserInitiated) {
  GURL url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");

  content::TestNavigationObserver navigation_observer(web_contents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_EQ(
      navigation_observer.last_navigation_initiator_activation_and_ad_status(),
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kDidNotStartWithTransientActivation);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 1u);
  ukm_recorder_->ExpectEntryMetric(
      entries[0], ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName,
      1);
  ukm_recorder_->ExpectEntryMetric(
      entries[0],
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 0);

  EXPECT_FALSE(HasAdClickMainFrameNavigationUseCounterForUrl(url));
}

IN_PROC_BROWSER_TEST_F(AdClickMetricsBrowserTest,
                       WindowOpen_FromAdScriptWithoutGesture) {
  GURL url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GURL popup_url =
      embedded_test_server()->GetURL("c.com", "/ad_tagging/frame_factory.html");

  content::WebContentsAddedObserver observer;
  content::ExecuteScriptAsyncWithoutUserGesture(
      GetWebContents()->GetPrimaryMainFrame(),
      content::JsReplace("windowOpenFromAdScript($1)", popup_url));

  content::WebContents* new_web_contents = observer.GetWebContents();
  content::TestNavigationObserver navigation_observer(new_web_contents);
  navigation_observer.Wait();

  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  EXPECT_EQ(
      navigation_observer.last_navigation_initiator_activation_and_ad_status(),
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kDidNotStartWithTransientActivation);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName, 0);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 0);

  NavigateAwayToFlushUseCounterUKM(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  EXPECT_FALSE(HasAdClickMainFrameNavigationUseCounterForUrl(url));
}

IN_PROC_BROWSER_TEST_F(AdClickMetricsBrowserTest,
                       WindowOpen_FromNonAdScriptWithGesture) {
  GURL url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GURL popup_url =
      embedded_test_server()->GetURL("c.com", "/ad_tagging/frame_factory.html");

  content::WebContentsAddedObserver observer;
  content::ExecuteScriptAsync(GetWebContents()->GetPrimaryMainFrame(),
                              content::JsReplace("window.open($1)", popup_url));

  content::WebContents* new_web_contents = observer.GetWebContents();
  content::TestNavigationObserver navigation_observer(new_web_contents);
  navigation_observer.Wait();

  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  EXPECT_EQ(
      navigation_observer.last_navigation_initiator_activation_and_ad_status(),
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kStartedWithTransientActivationFromNonAd);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName, 1);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 0);

  NavigateAwayToFlushUseCounterUKM(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  EXPECT_FALSE(HasAdClickMainFrameNavigationUseCounterForUrl(url));
}

IN_PROC_BROWSER_TEST_F(AdClickMetricsBrowserTest,
                       WindowOpen_FromAdScriptWithGesture) {
  GURL url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GURL popup_url =
      embedded_test_server()->GetURL("c.com", "/ad_tagging/frame_factory.html");

  content::WebContentsAddedObserver observer;
  content::ExecuteScriptAsync(
      GetWebContents()->GetPrimaryMainFrame(),
      content::JsReplace("windowOpenFromAdScript($1)", popup_url));

  content::WebContents* new_web_contents = observer.GetWebContents();
  content::TestNavigationObserver navigation_observer(new_web_contents);
  navigation_observer.Wait();

  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  EXPECT_EQ(
      navigation_observer.last_navigation_initiator_activation_and_ad_status(),
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kStartedWithTransientActivationFromAd);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName, 1);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 1);

  NavigateAwayToFlushUseCounterUKM(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  EXPECT_TRUE(HasAdClickMainFrameNavigationUseCounterForUrl(url));
}

IN_PROC_BROWSER_TEST_F(AdClickMetricsBrowserTest,
                       WindowOpen_FromAdIframeWithoutGesture) {
  GURL url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GURL popup_url =
      embedded_test_server()->GetURL("c.com", "/ad_tagging/frame_factory.html");

  RenderFrameHost* child =
      CreateSrcFrame(GetWebContents(),
                     embedded_test_server()->GetURL(
                         "b.com", "/ad_tagging/frame_factory.html?1&ad=true"));

  content::WebContentsAddedObserver observer;
  content::ExecuteScriptAsyncWithoutUserGesture(
      child, content::JsReplace("window.open($1)", popup_url));

  content::WebContents* new_web_contents = observer.GetWebContents();
  content::TestNavigationObserver navigation_observer(new_web_contents);
  navigation_observer.Wait();

  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  EXPECT_EQ(
      navigation_observer.last_navigation_initiator_activation_and_ad_status(),
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kDidNotStartWithTransientActivation);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName, 0);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 0);

  NavigateAwayToFlushUseCounterUKM(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  EXPECT_FALSE(HasAdClickMainFrameNavigationUseCounterForUrl(url));
}

IN_PROC_BROWSER_TEST_F(AdClickMetricsBrowserTest,
                       WindowOpen_FromAdIframeWithGesture) {
  GURL url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GURL popup_url =
      embedded_test_server()->GetURL("c.com", "/ad_tagging/frame_factory.html");

  RenderFrameHost* child =
      CreateSrcFrame(GetWebContents(),
                     embedded_test_server()->GetURL(
                         "b.com", "/ad_tagging/frame_factory.html?1&ad=true"));

  content::WebContentsAddedObserver observer;
  content::ExecuteScriptAsync(child,
                              content::JsReplace("window.open($1)", popup_url));

  content::WebContents* new_web_contents = observer.GetWebContents();
  content::TestNavigationObserver navigation_observer(new_web_contents);
  navigation_observer.Wait();

  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  EXPECT_EQ(
      navigation_observer.last_navigation_initiator_activation_and_ad_status(),
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kStartedWithTransientActivationFromAd);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName, 1);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 1);

  NavigateAwayToFlushUseCounterUKM(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  EXPECT_TRUE(HasAdClickMainFrameNavigationUseCounterForUrl(url));
}

IN_PROC_BROWSER_TEST_F(
    AdClickMetricsBrowserTest,
    AdClickMetrics_AnchorClickTopNavigationFromCrossOriginAdIframe) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "a.com", "/ad_tagging/page_with_full_viewport_style_frame.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL ad_iframe_url = embedded_test_server()->GetURL(
      "b.com", "/ad_tagging/page_with_full_viewport_link.html?ad=true");

  GURL top_navigation_url =
      embedded_test_server()->GetURL("c.com", "/ad_tagging/frame_factory.html");

  content::TestNavigationObserver iframe_navigation_observer(GetWebContents(),
                                                             1);
  EXPECT_TRUE(content::ExecJs(GetWebContents()->GetPrimaryMainFrame(),
                              content::JsReplace(R"(
        const ad_iframe = document.createElement('iframe');
        ad_iframe.id = 'frame-id';
        ad_iframe.src = $1;
        document.body.appendChild(ad_iframe);
      )",
                                                 ad_iframe_url)));
  iframe_navigation_observer.Wait();

  RenderFrameHost* child_rfh =
      ChildFrameAt(GetWebContents()->GetPrimaryMainFrame(), 0);

  EXPECT_TRUE(content::ExecJs(
      child_rfh,
      content::JsReplace("document.getElementsByTagName('a')[0].href = $1; "
                         "document.getElementsByTagName('a')[0].target='_top';",
                         top_navigation_url)));

  // We should wait for the hit-test data to be ready before sending the click
  // event below to avoid flakiness.
  content::WaitForHitTestData(child_rfh);

  // Ensure the compositor thread is aware of the event listener.
  content::MainThreadFrameObserver compositor_thread(
      child_rfh->GetRenderWidgetHost());
  compositor_thread.Wait();

  content::TestNavigationObserver top_navigation_observer(web_contents());
  content::SimulateMouseClick(web_contents(),
                              blink::WebInputEvent::kNoModifiers,
                              blink::WebMouseEvent::Button::kLeft);
  top_navigation_observer.Wait();

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName, 1);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 1);

  EXPECT_TRUE(HasAdClickMainFrameNavigationUseCounterForUrl(main_frame_url));
}

IN_PROC_BROWSER_TEST_F(AdClickMetricsBrowserTest,
                       AdClickMetrics_AnchorClickPopupFromCrossOriginAdIframe) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "a.com", "/ad_tagging/page_with_full_viewport_style_frame.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL ad_iframe_url = embedded_test_server()->GetURL(
      "b.com", "/ad_tagging/page_with_full_viewport_link.html?ad=true");

  GURL popup_url =
      embedded_test_server()->GetURL("c.com", "/ad_tagging/frame_factory.html");

  content::TestNavigationObserver iframe_navigation_observer(GetWebContents(),
                                                             1);
  EXPECT_TRUE(content::ExecJs(GetWebContents()->GetPrimaryMainFrame(),
                              content::JsReplace(R"(
        const ad_iframe = document.createElement('iframe');
        ad_iframe.src = $1;
        document.body.appendChild(ad_iframe);
      )",
                                                 ad_iframe_url)));
  iframe_navigation_observer.Wait();

  RenderFrameHost* child_rfh =
      ChildFrameAt(GetWebContents()->GetPrimaryMainFrame(), 0);

  EXPECT_TRUE(content::ExecJs(
      child_rfh, content::JsReplace(
                     "document.getElementsByTagName('a')[0].href = $1; "
                     "document.getElementsByTagName('a')[0].target='_blank';",
                     popup_url)));

  // We should wait for the hit-test data to be ready before sending the click
  // event below to avoid flakiness.
  content::WaitForHitTestData(child_rfh);

  // Ensure the compositor thread is aware of the event listener.
  content::MainThreadFrameObserver compositor_thread(
      child_rfh->GetRenderWidgetHost());
  compositor_thread.Wait();

  content::WebContentsAddedObserver popup_web_contents_observer;
  content::SimulateMouseClick(web_contents(),
                              blink::WebInputEvent::kNoModifiers,
                              blink::WebMouseEvent::Button::kLeft);
  content::WebContents* new_web_contents =
      popup_web_contents_observer.GetWebContents();

  content::TestNavigationObserver popup_navigation_observer(new_web_contents);
  popup_navigation_observer.Wait();

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName, 1);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 1);

  NavigateAwayToFlushUseCounterUKM(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  EXPECT_TRUE(HasAdClickMainFrameNavigationUseCounterForUrl(main_frame_url));
}

IN_PROC_BROWSER_TEST_F(AdClickMetricsBrowserTest,
                       SetTopLocationFromCrossOriginAdIframe) {
  GURL url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  RenderFrameHost* child =
      CreateSrcFrame(GetWebContents(),
                     embedded_test_server()->GetURL(
                         "b.com", "/ad_tagging/frame_factory.html?1&ad=true"));

  GURL new_url =
      embedded_test_server()->GetURL("c.com", "/ad_tagging/frame_factory.html");

  content::TestNavigationObserver navigation_observer(web_contents());
  EXPECT_TRUE(
      content::ExecJs(child, content::JsReplace("top.location = $1", new_url)));
  navigation_observer.Wait();

  EXPECT_EQ(
      navigation_observer.last_navigation_initiator_activation_and_ad_status(),
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kStartedWithTransientActivationFromAd);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName, 1);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 1);

  EXPECT_TRUE(HasAdClickMainFrameNavigationUseCounterForUrl(url));
}

IN_PROC_BROWSER_TEST_F(AdClickMetricsBrowserTest,
                       SetTopLocationFromCrossOriginNonAdIframe) {
  GURL url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  RenderFrameHost* child = CreateSrcFrame(
      GetWebContents(), embedded_test_server()->GetURL(
                            "b.com", "/ad_tagging/frame_factory.html"));

  GURL new_url =
      embedded_test_server()->GetURL("c.com", "/ad_tagging/frame_factory.html");

  content::TestNavigationObserver navigation_observer(web_contents());
  EXPECT_TRUE(
      content::ExecJs(child, content::JsReplace("top.location = $1", new_url)));
  navigation_observer.Wait();

  EXPECT_EQ(
      navigation_observer.last_navigation_initiator_activation_and_ad_status(),
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kStartedWithTransientActivationFromNonAd);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName, 1);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 0);

  EXPECT_FALSE(HasAdClickMainFrameNavigationUseCounterForUrl(url));
}

IN_PROC_BROWSER_TEST_F(AdClickMetricsBrowserTest,
                       NavigateCrossOriginIframeFromNonAdScript) {
  GURL url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  {
    content::TestNavigationObserver navigation_observer(web_contents(), 1);
    CreateSrcFrame(GetWebContents(),
                   embedded_test_server()->GetURL(
                       "b.com", "/ad_tagging/frame_factory.html?1&ad=true"));

    EXPECT_EQ(navigation_observer
                  .last_navigation_initiator_activation_and_ad_status(),
              blink::mojom::NavigationInitiatorActivationAndAdStatus::
                  kStartedWithTransientActivationFromNonAd);
  }

  {
    GURL new_url = embedded_test_server()->GetURL(
        "c.com", "/ad_tagging/frame_factory.html");

    content::TestNavigationObserver navigation_observer(GetWebContents(), 1);
    EXPECT_TRUE(content::ExecJs(
        GetWebContents()->GetPrimaryMainFrame(),
        content::JsReplace("document.getElementsByName('frame_0')[0].src = $1",
                           new_url)));
    navigation_observer.Wait();

    EXPECT_EQ(navigation_observer
                  .last_navigation_initiator_activation_and_ad_status(),
              blink::mojom::NavigationInitiatorActivationAndAdStatus::
                  kStartedWithTransientActivationFromNonAd);
  }

  // No event is recorded for subframe navigation. The recorded event is for the
  // initial page load.
  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 1u);

  NavigateAwayToFlushUseCounterUKM(GetWebContents());

  EXPECT_FALSE(HasAdClickMainFrameNavigationUseCounterForUrl(url));
}

IN_PROC_BROWSER_TEST_F(AdClickMetricsBrowserTest,
                       NavigateCrossOriginIframeFromAdScript) {
  GURL url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  {
    content::TestNavigationObserver navigation_observer(GetWebContents(), 1);
    CreateSrcFrame(GetWebContents(),
                   embedded_test_server()->GetURL(
                       "b.com", "/ad_tagging/frame_factory.html?1&ad=true"));

    EXPECT_EQ(navigation_observer
                  .last_navigation_initiator_activation_and_ad_status(),
              blink::mojom::NavigationInitiatorActivationAndAdStatus::
                  kStartedWithTransientActivationFromNonAd);
  }

  {
    GURL new_url = embedded_test_server()->GetURL(
        "c.com", "/ad_tagging/frame_factory.html");

    content::TestNavigationObserver navigation_observer(GetWebContents(), 1);
    EXPECT_TRUE(content::ExecJs(
        GetWebContents()->GetPrimaryMainFrame(),
        content::JsReplace("navigateIframeFromAdScript('frame_0', $1)",
                           new_url)));
    navigation_observer.Wait();

    EXPECT_EQ(navigation_observer
                  .last_navigation_initiator_activation_and_ad_status(),
              blink::mojom::NavigationInitiatorActivationAndAdStatus::
                  kStartedWithTransientActivationFromAd);
  }

  // No event is recorded for subframe navigation. The recorded event is for the
  // initial page load.
  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 1u);

  NavigateAwayToFlushUseCounterUKM(GetWebContents());

  EXPECT_FALSE(HasAdClickMainFrameNavigationUseCounterForUrl(url));
}

class AdTaggingEventFromSubframeBrowserTest
    : public AdTaggingBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<bool /* cross_origin */, bool /* from_ad_frame */>> {};

// crbug.com/997410. The test is flaky on multiple platforms.
IN_PROC_BROWSER_TEST_P(AdTaggingEventFromSubframeBrowserTest,
                       DISABLED_WindowOpenFromSubframe) {
  auto [cross_origin, from_ad_frame] = GetParam();
  SCOPED_TRACE(::testing::Message() << "cross_origin = " << cross_origin << ", "
                                    << "from_ad_frame = " << from_ad_frame);

  base::HistogramTester histogram_tester;
  GURL root_frame_url =
      embedded_test_server()->GetURL("a.com", "/ad_tagging/frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), root_frame_url));
  content::WebContents* main_tab = GetWebContents();

  std::string hostname = cross_origin ? "b.com" : "a.com";
  std::string suffix = from_ad_frame ? "&ad=true" : "";
  RenderFrameHost* child = CreateSrcFrame(
      main_tab, embedded_test_server()->GetURL(
                    hostname, "/ad_tagging/frame_factory.html?1" + suffix));

  EXPECT_TRUE(content::ExecJs(child, "window.open();"));

  bool from_ad_script = from_ad_frame;
  ExpectWindowOpenUkmEntry(false /* from_root_frame */, root_frame_url,
                           from_ad_frame, from_ad_script);
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

  base::HistogramTester histogram_tester;
  GURL root_frame_url = GetURL("frame_factory.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), root_frame_url));
  content::WebContents* main_tab = GetWebContents();

  std::string script = from_ad_script ? "windowOpenFromAdScript();"
                                      : "windowOpenFromNonAdScript();";

  EXPECT_TRUE(content::ExecJs(main_tab, script));

  bool from_ad_frame = false;
  ExpectWindowOpenUkmEntry(true /* from_root_frame */, root_frame_url,
                           from_ad_frame, from_ad_script);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AdTaggingEventWithScriptInStackBrowserTest,
    ::testing::Bool());

class AdTaggingFencedFrameBrowserTest : public AdTaggingBrowserTest {
 public:
  AdTaggingFencedFrameBrowserTest() = default;
  ~AdTaggingFencedFrameBrowserTest() override = default;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

  void SetUpOnMainThread() override {
    AdTaggingBrowserTest::SetUpOnMainThread();
    observer_ = std::make_unique<TestSubresourceFilterObserver>(web_contents());
    https_server_.ServeFilesFromSourceDirectory("components/test/data");
    ASSERT_TRUE(https_server_.Start());
  }

  bool EvaluatedChildFrameLoad(const GURL url) {
    return observer_->GetChildFrameLoadPolicy(url).has_value();
  }

  bool IsAdFrame(RenderFrameHost* host) {
    return observer_->GetIsAdFrame(host->GetFrameTreeNodeId());
  }

  RenderFrameHost* PrimaryMainFrame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  GURL GetURL(const std::string& page) {
    return https_server_.GetURL("/ad_tagging/" + page);
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  std::unique_ptr<TestSubresourceFilterObserver> observer_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

// Test that a fenced frame itself can be tagged as an ad and that iframes
// nested within it see it as an ancestor ad frame.
IN_PROC_BROWSER_TEST_F(AdTaggingFencedFrameBrowserTest,
                       FencedFrameMatchingSuffixRuleIsTagged) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURL("frame_factory.html?primary")));

  // Create a fenced frame which should be tagged as an ad.
  const GURL kUrl = GetURL("frame_factory.html?fencedframe&ad=true");
  RenderFrameHost* fenced_frame = CreateFencedFrame(PrimaryMainFrame(), kUrl);

  EXPECT_TRUE(EvaluatedChildFrameLoad(kUrl));
  EXPECT_TRUE(IsAdFrame(fenced_frame));
  EXPECT_TRUE(EvidenceForFrameComprises(
      fenced_frame, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript));

  // Create an iframe in the fenced frame that doesn't match any rules. Since
  // the fenced frame was tagged as an ad, this should be as well.
  GURL kInnerUrl = GetURL("test_div.html");
  RenderFrameHost* inner_frame = CreateSrcFrame(fenced_frame, kInnerUrl);

  EXPECT_TRUE(EvaluatedChildFrameLoad(kInnerUrl));
  EXPECT_TRUE(IsAdFrame(inner_frame));
  // Note: kCreatedByAdScript is expected since any script running in an ad is
  // considered ad script.
  EXPECT_TRUE(EvidenceForFrameComprises(
      inner_frame, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
}

// Test that a fenced frame created by ad script is tagged as an ad.
IN_PROC_BROWSER_TEST_F(AdTaggingFencedFrameBrowserTest,
                       FencedFrameCreatedByAdScriptIsTagged) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURL("frame_factory.html?primary")));

  {
    // Create a fenced frame from an ad script so it should be tagged as an ad.
    const GURL kUrl = GetURL("frame_factory.html?fencedframe");
    RenderFrameHost* fenced_frame =
        CreateFencedFrameFromAdScript(PrimaryMainFrame(), kUrl);

    EXPECT_TRUE(EvaluatedChildFrameLoad(kUrl));
    EXPECT_TRUE(IsAdFrame(fenced_frame));
    EXPECT_TRUE(EvidenceForFrameComprises(
        fenced_frame, /*parent_is_ad=*/false,
        blink::mojom::FilterListResult::kMatchedNoRules,
        blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

    // Create a plain frame in the fenced frame. Since the fenced frame was
    // tagged as an ad, this should be as well.
    const GURL kInnerUrl = GetURL("test_div.html");
    RenderFrameHost* inner_frame = CreateSrcFrame(fenced_frame, kInnerUrl);

    EXPECT_TRUE(EvaluatedChildFrameLoad(kInnerUrl));
    EXPECT_TRUE(IsAdFrame(inner_frame));
    EXPECT_TRUE(EvidenceForFrameComprises(
        inner_frame, /*parent_is_ad=*/true,
        blink::mojom::FilterListResult::kMatchedNoRules,
        blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
  }

  // Create a fenced frame from a non-ad script, ensure it isn't tagged.
  {
    const GURL kUrl = GetURL("frame_factory.html?non+ad+fencedframe");
    RenderFrameHost* fenced_frame = CreateFencedFrame(PrimaryMainFrame(), kUrl);

    EXPECT_TRUE(EvaluatedChildFrameLoad(kUrl));
    EXPECT_FALSE(IsAdFrame(fenced_frame));
  }
}

// Test that a fenced frame not matching any rules, created in an ad-frame is
// also tagged as an ad.
IN_PROC_BROWSER_TEST_F(AdTaggingFencedFrameBrowserTest,
                       FencedFrameWithAdParentIsTagged) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURL("frame_factory.html?primary")));

  {
    // Create a fenced frame to an ad URL.
    const GURL kAdUrl = GetURL("frame_factory.html?fencedframe&ad=true");
    RenderFrameHost* ad_frame = CreateFencedFrame(PrimaryMainFrame(), kAdUrl);

    ASSERT_TRUE(EvaluatedChildFrameLoad(kAdUrl));
    ASSERT_TRUE(IsAdFrame(ad_frame));

    // Create another fenced frame, nested in the first one, that doesn't match
    // any rules. Since the first fenced frame was tagged as an ad, the inner
    // one should be as well.
    const GURL kInnerUrl = GetURL("test_div.html?nested+in+fenced+frame");
    RenderFrameHost* inner_frame = CreateFencedFrame(ad_frame, kInnerUrl);

    EXPECT_TRUE(EvaluatedChildFrameLoad(kInnerUrl));
    EXPECT_TRUE(IsAdFrame(inner_frame));
    // Note: kCreatedByAdScript is expected since any script running in an ad
    // is considered ad script.
    EXPECT_TRUE(EvidenceForFrameComprises(
        inner_frame, /*parent_is_ad=*/true,
        blink::mojom::FilterListResult::kMatchedNoRules,
        blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
  }

  {
    // Create an iframe to an ad URL
    const GURL kAdUrl = GetURL("frame_factory.html?iframe&ad=true");
    RenderFrameHost* ad_frame = CreateSrcFrame(PrimaryMainFrame(), kAdUrl);

    ASSERT_TRUE(EvaluatedChildFrameLoad(kAdUrl));
    ASSERT_TRUE(IsAdFrame(ad_frame));

    // Create a fenced frame, nested in the ad iframe, that doesn't match any
    // rules. Since the iframe was tagged as an ad, the fenced frame should be
    // as well.
    const GURL kInnerUrl = GetURL("test_div.html?nested+in+iframe");
    RenderFrameHost* inner_frame = CreateFencedFrame(ad_frame, kInnerUrl);

    EXPECT_TRUE(EvaluatedChildFrameLoad(kInnerUrl));
    EXPECT_TRUE(IsAdFrame(inner_frame));
    // Note: kCreatedByAdScript is expected since any script running in an ad
    // is considered ad script.
    EXPECT_TRUE(EvidenceForFrameComprises(
        inner_frame, /*parent_is_ad=*/true,
        blink::mojom::FilterListResult::kMatchedNoRules,
        blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
  }
}

// Test ad tagging for iframes nested within a fenced frame.
IN_PROC_BROWSER_TEST_F(AdTaggingFencedFrameBrowserTest,
                       IFrameNestedInFencedFrameIsTagged) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURL("frame_factory.html?primary")));

  // Create a fenced frame into which we'll nest iframes.
  const GURL kOuterUrl = GetURL("frame_factory.html?fencedframe");
  RenderFrameHost* fenced_frame =
      CreateFencedFrame(PrimaryMainFrame(), kOuterUrl);

  ASSERT_TRUE(EvaluatedChildFrameLoad(kOuterUrl));
  ASSERT_FALSE(IsAdFrame(fenced_frame));

  // Create an iframe inside the fenced frame that matches an ad rule. It
  // should be tagged as an ad.
  {
    const GURL kAdUrl = GetURL("test_div.html?ad=true");
    RenderFrameHost* inner_frame = CreateSrcFrame(fenced_frame, kAdUrl);

    EXPECT_TRUE(EvaluatedChildFrameLoad(kAdUrl));
    EXPECT_TRUE(IsAdFrame(inner_frame));
    EXPECT_TRUE(EvidenceForFrameComprises(
        inner_frame, /*parent_is_ad=*/false,
        blink::mojom::FilterListResult::kMatchedBlockingRule,
        blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript));
  }

  // Create an iframe inside the fenced frame that doesn't match any rules but
  // is created from an ad script. It should be tagged as an ad.
  {
    const GURL kNotAdUrl = GetURL("test_div.html?not+an+ad");
    RenderFrameHost* inner_frame =
        CreateSrcFrameFromAdScript(fenced_frame, kNotAdUrl);

    EXPECT_TRUE(EvaluatedChildFrameLoad(kNotAdUrl));
    EXPECT_TRUE(IsAdFrame(inner_frame));
    EXPECT_TRUE(EvidenceForFrameComprises(
        inner_frame, /*parent_is_ad=*/false,
        blink::mojom::FilterListResult::kMatchedNoRules,
        blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
  }

  // Create an iframe inside the fenced frame that doesn't match any rules. It
  // should not be tagged as an ad.
  {
    const GURL kNotAdUrl = GetURL("test_div.html?also+not+an+ad");
    RenderFrameHost* inner_frame = CreateSrcFrame(fenced_frame, kNotAdUrl);

    EXPECT_TRUE(EvaluatedChildFrameLoad(kNotAdUrl));
    EXPECT_FALSE(IsAdFrame(inner_frame));
  }
}

// Test ad tagging for fenced frames nested within a fenced frame.
IN_PROC_BROWSER_TEST_F(AdTaggingFencedFrameBrowserTest,
                       FencedFrameNestedInFencedFrameIsTagged) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GetURL("frame_factory.html?primary")));

  // Create a fenced frame into which we'll nest other fenced frames.
  const GURL kOuterUrl = GetURL("frame_factory.html?fencedframe");
  RenderFrameHost* fenced_frame =
      CreateFencedFrame(PrimaryMainFrame(), kOuterUrl);

  ASSERT_TRUE(EvaluatedChildFrameLoad(kOuterUrl));
  ASSERT_FALSE(IsAdFrame(fenced_frame));

  // Create a fenced frame inside the fenced frame that matches an ad rule. It
  // should be tagged as an ad.
  {
    const GURL kAdUrl = GetURL("test_div.html?ad=true");
    RenderFrameHost* inner_frame = CreateFencedFrame(fenced_frame, kAdUrl);

    EXPECT_TRUE(EvaluatedChildFrameLoad(kAdUrl));
    EXPECT_TRUE(IsAdFrame(inner_frame));
    EXPECT_TRUE(EvidenceForFrameComprises(
        inner_frame, /*parent_is_ad=*/false,
        blink::mojom::FilterListResult::kMatchedBlockingRule,
        blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript));
  }

  // Create a fenced frame inside the fenced frame that doesn't match any rules
  // but is created from an ad script. It should be tagged as an ad.
  {
    const GURL kNotAdUrl = GetURL("test_div.html?not+an+ad");
    RenderFrameHost* inner_frame =
        CreateFencedFrameFromAdScript(fenced_frame, kNotAdUrl);

    EXPECT_TRUE(EvaluatedChildFrameLoad(kNotAdUrl));
    EXPECT_TRUE(IsAdFrame(inner_frame));
    EXPECT_TRUE(EvidenceForFrameComprises(
        inner_frame, /*parent_is_ad=*/false,
        blink::mojom::FilterListResult::kMatchedNoRules,
        blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));
  }

  // Create a fenced frame inside the fenced frame that doesn't match any
  // rules. It should not be tagged as an ad.
  {
    const GURL kNotAdUrl = GetURL("test_div.html?also+not+an+ad");
    RenderFrameHost* inner_frame = CreateFencedFrame(fenced_frame, kNotAdUrl);

    EXPECT_TRUE(EvaluatedChildFrameLoad(kNotAdUrl));
    EXPECT_FALSE(IsAdFrame(inner_frame));
  }
}

IN_PROC_BROWSER_TEST_F(AdTaggingFencedFrameBrowserTest,
                       AdClickMetrics_PopupFromAdFencedFrame) {
  GURL main_frame_url = GetURL("frame_factory.html?primary");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  const GURL kOuterUrl = GetURL("frame_factory.html?fencedframe&ad=true");
  RenderFrameHost* fenced_frame =
      CreateFencedFrame(PrimaryMainFrame(), kOuterUrl);

  ASSERT_TRUE(EvaluatedChildFrameLoad(kOuterUrl));
  ASSERT_TRUE(IsAdFrame(fenced_frame));

  GURL new_url = GetURL("frame_factory.html?primary123");

  content::WebContentsAddedObserver observer;
  content::ExecuteScriptAsync(fenced_frame,
                              content::JsReplace("window.open($1)", new_url));

  content::WebContents* new_web_contents = observer.GetWebContents();
  content::TestNavigationObserver navigation_observer(new_web_contents);
  navigation_observer.Wait();

  EXPECT_EQ(
      navigation_observer.last_navigation_initiator_activation_and_ad_status(),
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kStartedWithTransientActivationFromAd);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries[1], ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName,
      1);
  ukm_recorder_->ExpectEntryMetric(
      entries[1],
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 1);

  NavigateAwayToFlushUseCounterUKM(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  EXPECT_TRUE(HasAdClickMainFrameNavigationUseCounterForUrl(main_frame_url));
}

IN_PROC_BROWSER_TEST_F(AdTaggingFencedFrameBrowserTest,
                       AdClickMetrics_PopupFromNonAdFencedFrame) {
  GURL main_frame_url = GetURL("frame_factory.html?primary");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  const GURL kOuterUrl = GetURL("frame_factory.html?fencedframe");
  RenderFrameHost* fenced_frame =
      CreateFencedFrame(PrimaryMainFrame(), kOuterUrl);

  ASSERT_TRUE(EvaluatedChildFrameLoad(kOuterUrl));
  ASSERT_FALSE(IsAdFrame(fenced_frame));

  GURL new_url = GetURL("frame_factory.html?primary");

  content::WebContentsAddedObserver observer;
  content::ExecuteScriptAsync(fenced_frame,
                              content::JsReplace("window.open($1)", new_url));

  content::WebContents* new_web_contents = observer.GetWebContents();
  content::TestNavigationObserver navigation_observer(new_web_contents);
  navigation_observer.Wait();

  EXPECT_EQ(
      navigation_observer.last_navigation_initiator_activation_and_ad_status(),
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kStartedWithTransientActivationFromNonAd);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries[1], ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName,
      1);
  ukm_recorder_->ExpectEntryMetric(
      entries[1],
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 0);

  NavigateAwayToFlushUseCounterUKM(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  EXPECT_FALSE(HasAdClickMainFrameNavigationUseCounterForUrl(main_frame_url));
}

IN_PROC_BROWSER_TEST_F(
    AdTaggingFencedFrameBrowserTest,
    AdClickMetrics_TopNavigationFromOpaqueModeAdFencedFrame) {
  GURL main_url = GetURL("frame_factory.html?primary");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  const GURL kOuterUrl = GetURL("frame_factory.html?fencedframe&ad=true");
  content::RenderFrameHost* fenced_frame =
      fenced_frame_test_helper().CreateFencedFrame(
          PrimaryMainFrame(), kOuterUrl, net::OK,
          blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds);

  GURL new_url = GetURL("frame_factory.html?primary");

  content::TestNavigationObserver top_navigation_observer(web_contents());
  EXPECT_TRUE(
      ExecJs(fenced_frame,
             content::JsReplace("window.open($1, '_unfencedTop')", new_url)));
  top_navigation_observer.Wait();

  EXPECT_EQ(top_navigation_observer
                .last_navigation_initiator_activation_and_ad_status(),
            blink::mojom::NavigationInitiatorActivationAndAdStatus::
                kStartedWithTransientActivationFromAd);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries[1], ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName,
      1);
  ukm_recorder_->ExpectEntryMetric(
      entries[1],
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 1);

  EXPECT_TRUE(HasAdClickMainFrameNavigationUseCounterForUrl(main_url));
}

IN_PROC_BROWSER_TEST_F(AdTaggingFencedFrameBrowserTest,
                       AdClickMetrics_ReloadFromOpaqueModeAdFencedFrame) {
  GURL main_url = GetURL("frame_factory.html?primary");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  const GURL kOuterUrl = GetURL("frame_factory.html?fencedframe&ad=true");
  content::RenderFrameHost* fenced_frame =
      fenced_frame_test_helper().CreateFencedFrame(
          PrimaryMainFrame(), kOuterUrl, net::OK,
          blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds);

  content::TestFrameNavigationObserver observer(fenced_frame);
  EXPECT_TRUE(ExecJs(fenced_frame, content::JsReplace("location.reload()")));
  observer.Wait();

  // No event is recorded for fenced frame reload. The recorded event is for the
  // initial page load.
  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 1u);

  EXPECT_FALSE(HasAdClickMainFrameNavigationUseCounterForUrl(main_url));
}

IN_PROC_BROWSER_TEST_F(
    AdTaggingFencedFrameBrowserTest,
    AdClickMetrics_AnchorClickTopNavigationFromOpaqueModeAdFencedFrame) {
  GURL main_frame_url = GetURL("page_with_full_viewport_style_frame.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL ad_fenced_frame_url =
      GetURL("page_with_full_viewport_link.html?ad=true");

  content::RenderFrameHost* fenced_frame =
      fenced_frame_test_helper().CreateFencedFrame(
          PrimaryMainFrame(), ad_fenced_frame_url, net::OK,
          blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds);

  GURL top_navigation_url = GetURL("frame_factory.html?primary");

  EXPECT_TRUE(content::ExecJs(
      fenced_frame,
      content::JsReplace(
          "document.getElementsByTagName('a')[0].href = $1; "
          "document.getElementsByTagName('a')[0].target='_unfencedTop';",
          top_navigation_url)));

  // We should wait for the hit-test data to be ready before sending the click
  // event below to avoid flakiness.
  content::WaitForHitTestData(fenced_frame);

  // Ensure the compositor thread is aware of the event listener.
  content::MainThreadFrameObserver compositor_thread(
      fenced_frame->GetRenderWidgetHost());
  compositor_thread.Wait();

  content::TestNavigationObserver top_navigation_observer(web_contents());
  content::SimulateMouseClick(web_contents(),
                              blink::WebInputEvent::kNoModifiers,
                              blink::WebMouseEvent::Button::kLeft);
  top_navigation_observer.Wait();

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName, 1);
  ukm_recorder_->ExpectEntryMetric(
      entries.back(),
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 1);

  EXPECT_TRUE(HasAdClickMainFrameNavigationUseCounterForUrl(main_frame_url));
}

IN_PROC_BROWSER_TEST_F(
    AdTaggingFencedFrameBrowserTest,
    AdClickMetrics_TopNavigationFromOpaqueModeNonAdFencedFrame) {
  GURL main_url = GetURL("frame_factory.html?primary");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  const GURL kOuterUrl = GetURL("frame_factory.html?fencedframe");
  content::RenderFrameHost* fenced_frame =
      fenced_frame_test_helper().CreateFencedFrame(
          PrimaryMainFrame(), kOuterUrl, net::OK,
          blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds);

  GURL new_url = GetURL("frame_factory.html?primary");

  content::TestNavigationObserver top_navigation_observer(web_contents());
  EXPECT_TRUE(
      ExecJs(fenced_frame,
             content::JsReplace("window.open($1, '_unfencedTop')", new_url)));
  top_navigation_observer.Wait();

  EXPECT_EQ(top_navigation_observer
                .last_navigation_initiator_activation_and_ad_status(),
            blink::mojom::NavigationInitiatorActivationAndAdStatus::
                kStartedWithTransientActivationFromNonAd);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::PageLoadInitiatorForAdTagging::kEntryName);
  EXPECT_EQ(entries.size(), 2u);
  ukm_recorder_->ExpectEntryMetric(
      entries[1], ukm::builders::PageLoadInitiatorForAdTagging::kFromUserName,
      1);
  ukm_recorder_->ExpectEntryMetric(
      entries[1],
      ukm::builders::PageLoadInitiatorForAdTagging::kFromAdClickName, 0);

  EXPECT_FALSE(HasAdClickMainFrameNavigationUseCounterForUrl(main_url));
}

}  // namespace

}  // namespace subresource_filter
