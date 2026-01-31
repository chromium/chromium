// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/captcha_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/captcha_provider_manager.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using page_load_metrics::CaptchaProviderManager;
using CaptchaFrameAgentContext =
    CaptchaMetricsObserver::CaptchaFrameAgentContext;

class CaptchaMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<CaptchaMetricsObserver>());
    CaptchaProviderManager::GetInstance()->SetCaptchaProviders(
        {"*captcha.com/*"});
  }

  RenderFrameHost* AppendChildFrameAndNavigateAndCommit(RenderFrameHost* parent,
                                                        const char* frame_name,
                                                        const GURL& url) {
    RenderFrameHost* subframe =
        RenderFrameHostTester::For(parent)->AppendChild(frame_name);
    std::unique_ptr<NavigationSimulator> simulator =
        NavigationSimulator::CreateRendererInitiated(url, subframe);
    simulator->Commit();
    return simulator->GetFinalRenderFrameHost();
  }
};

TEST_F(CaptchaMetricsObserverTest, NoCaptchaLoad) {
  NavigateAndCommit(GURL("https://www.top-level-site.com/"));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL("https://www.not-captcha.com/subframe.html"));
  RenderFrameHostTester::For(subframe)->SimulateUserActivation();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoad", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameActivation", 0);

  EXPECT_EQ(tester()
                ->test_ukm_recorder()
                .GetEntries("PageLoad.CaptchaFrameLoad", {"AgentContext"})
                .size(),
            0u);
  EXPECT_EQ(tester()
                ->test_ukm_recorder()
                .GetEntries("PageLoad.CaptchaFrameActivation", {"AgentContext"})
                .size(),
            0u);
}

TEST_F(CaptchaMetricsObserverTest, CaptchaLoadAndClick) {
  NavigateAndCommit(GURL("https://www.top-level-site.com/"));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL("https://www.captcha.com/subframe.html"));

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoad", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.CaptchaFrameLoad",
      CaptchaFrameAgentContext::kNoAgentActiveOnTab, 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameActivation", 0);

  EXPECT_EQ(tester()
                ->test_ukm_recorder()
                .GetEntries("PageLoad.CaptchaFrameLoad", {"AgentContext"})
                .size(),
            1u);
  EXPECT_EQ(tester()
                ->test_ukm_recorder()
                .GetEntries("PageLoad.CaptchaFrameActivation", {"AgentContext"})
                .size(),
            0u);

  RenderFrameHostTester::For(subframe)->SimulateUserActivation();
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameActivation", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.Clients.CaptchaFrameActivation",
      CaptchaFrameAgentContext::kNoAgentActiveOnTab, 1);

  EXPECT_EQ(tester()
                ->test_ukm_recorder()
                .GetEntries("PageLoad.CaptchaFrameLoad", {"AgentContext"})
                .size(),
            1u);
  EXPECT_EQ(tester()
                ->test_ukm_recorder()
                .GetEntries("PageLoad.CaptchaFrameActivation", {"AgentContext"})
                .size(),
            1u);
}
