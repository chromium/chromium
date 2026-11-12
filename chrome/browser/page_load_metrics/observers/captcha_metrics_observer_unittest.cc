// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/captcha_metrics_observer.h"

#include <memory>

#include "chrome/browser/page_load_metrics/observers/captcha_provider_manager.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "ui/gfx/geometry/size.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "components/tabs/public/tab_interface.h"
#endif  // !BUILDFLAG(IS_ANDROID)

using content::NavigationSimulator;
using content::RenderFrameHost;
using content::RenderFrameHostTester;
using page_load_metrics::CaptchaProvider;
using page_load_metrics::CaptchaProviderManager;
using CaptchaFrameAgentContext =
    CaptchaMetricsObserver::CaptchaFrameAgentContext;
using HumanReadableUkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;

class CaptchaMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<CaptchaMetricsObserver>());
    CaptchaProviderManager::GetInstance()->SetCaptchaProviders({
        "*captcha.com/*",
        "*google.com/recaptcha/api2/anchor",
        "*hcaptcha.com/captcha/*",
        "*challenges.cloudflare.com/*",
    });
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

  bool HasUkmEntryForCaptchaProvider(std::vector<HumanReadableUkmEntry> entries,
                                     CaptchaProvider captcha_provider) {
    for (const auto& entry : entries) {
      if (entry.metrics.at("CaptchaProvider") ==
          static_cast<int64_t>(captcha_provider)) {
        return true;
      }
    }
    return false;
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
      "PageLoad.Clients.CaptchaFrameLoadVisible", 0);
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

TEST_F(CaptchaMetricsObserverTest, CaptchaProviderSpecificMetrics) {
  // Load the Captcha frames.
  NavigateAndCommit(GURL("https://www.top-level-site.com/"));
  RenderFrameHost* captcha_frame = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "captcha-frame",
      GURL("https://www.captcha.com/subframe.html"));
  RenderFrameHost* recaptcha_frame = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "recaptcha-frame",
      GURL("https://www.google.com/recaptcha/api2/anchor"));
  RenderFrameHost* hcaptcha_frame = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "hcaptcha-frame",
      GURL("https://www.hcaptcha.com/captcha/index.html"));
  RenderFrameHost* cloudflare_frame = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "cloudflare-frame",
      GURL("https://challenges.cloudflare.com/turnstile/index.html"));

  // Check UMA histograms.
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoad", 4);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoad.UnknownCaptchaProvider", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoad.ReCaptcha", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoad.HCaptcha", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoad.CloudflareTurnstile", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameActivation", 0);

  // Simulate the Captcha frames becoming visible with valid sizes.
  tester()->metrics_web_contents_observer()->FrameSizeChanged(
      captcha_frame, gfx::Size(300, 75));
  tester()->metrics_web_contents_observer()->FrameSizeChanged(
      recaptcha_frame, gfx::Size(300, 75));
  tester()->metrics_web_contents_observer()->FrameSizeChanged(
      hcaptcha_frame, gfx::Size(300, 75));
  tester()->metrics_web_contents_observer()->FrameSizeChanged(
      cloudflare_frame, gfx::Size(300, 75));

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible", 4);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible.UnknownCaptchaProvider", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible.ReCaptcha", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible.HCaptcha", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible.CloudflareTurnstile", 1);

  // Check UKM entries.
  auto captcha_frame_load_entries = tester()->test_ukm_recorder().GetEntries(
      "PageLoad.CaptchaFrameLoad", {"AgentContext", "CaptchaProvider"});
  EXPECT_EQ(captcha_frame_load_entries.size(), 4u);
  EXPECT_TRUE(HasUkmEntryForCaptchaProvider(captcha_frame_load_entries,
                                            CaptchaProvider::kUnknown));
  EXPECT_TRUE(HasUkmEntryForCaptchaProvider(captcha_frame_load_entries,
                                            CaptchaProvider::kReCaptcha));
  EXPECT_TRUE(HasUkmEntryForCaptchaProvider(captcha_frame_load_entries,
                                            CaptchaProvider::kHCaptcha));
  EXPECT_TRUE(HasUkmEntryForCaptchaProvider(
      captcha_frame_load_entries, CaptchaProvider::kCloudflareTurnstile));

  // Simulate user activation on the Captcha frames.
  RenderFrameHostTester::For(captcha_frame)->SimulateUserActivation();
  RenderFrameHostTester::For(recaptcha_frame)->SimulateUserActivation();
  RenderFrameHostTester::For(hcaptcha_frame)->SimulateUserActivation();
  RenderFrameHostTester::For(cloudflare_frame)->SimulateUserActivation();

  // Check UMA histograms.
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameActivation", 4);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameActivation.UnknownCaptchaProvider", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameActivation.ReCaptcha", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameActivation.HCaptcha", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameActivation.CloudflareTurnstile", 1);

  // Check UKM entries.
  auto captcha_frame_activation_entries =
      tester()->test_ukm_recorder().GetEntries(
          "PageLoad.CaptchaFrameActivation",
          {"AgentContext", "CaptchaProvider"});
  EXPECT_EQ(captcha_frame_activation_entries.size(), 4u);
  EXPECT_TRUE(HasUkmEntryForCaptchaProvider(captcha_frame_activation_entries,
                                            CaptchaProvider::kUnknown));
  EXPECT_TRUE(HasUkmEntryForCaptchaProvider(captcha_frame_activation_entries,
                                            CaptchaProvider::kReCaptcha));
  EXPECT_TRUE(HasUkmEntryForCaptchaProvider(captcha_frame_activation_entries,
                                            CaptchaProvider::kHCaptcha));
  EXPECT_TRUE(HasUkmEntryForCaptchaProvider(
      captcha_frame_activation_entries, CaptchaProvider::kCloudflareTurnstile));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(CaptchaMetricsObserverTest, CaptchaLoadWithGlicAgent) {
  NavigateAndCommit(GURL("https://www.top-level-site.com/"));

  // Associate a mock TabInterface with the WebContents.
  actor::TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);

  // Create an active ActorTask.
  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());

  // Force the task into an "acting" state, and add the tab to the task.
  auto* task = actor_service->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  task->AddTab(tab_state.tab.GetHandle(), /*stop_task_on_detach=*/false,
               base::DoNothing());

  AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "captcha-frame",
      GURL("https://www.captcha.com/subframe.html"));

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.CaptchaFrameLoad",
      CaptchaFrameAgentContext::kGlicAgentActiveOnTab, 1);

  auto entries = tester()->test_ukm_recorder().GetEntries(
      "PageLoad.CaptchaFrameLoad", {"AgentContext"});
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(
      entries[0].metrics.at("AgentContext"),
      static_cast<int64_t>(CaptchaFrameAgentContext::kGlicAgentActiveOnTab));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(CaptchaMetricsObserverTest, CaptchaLoadWithDevToolsAgent) {
  // Attach a DevTools client to the WebContents.
  content::TestDevToolsProtocolClient devtools_client;
  scoped_refptr<content::DevToolsAgentHost> devtools_host =
      content::DevToolsAgentHost::GetOrCreateForTab(web_contents());
  devtools_host->AttachClient(&devtools_client);

  NavigateAndCommit(GURL("https://www.top-level-site.com/"));
  AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "captcha-frame",
      GURL("https://www.captcha.com/subframe.html"));

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.CaptchaFrameLoad",
      CaptchaFrameAgentContext::kDevToolsAgentActiveOnTab, 1);

  auto entries = tester()->test_ukm_recorder().GetEntries(
      "PageLoad.CaptchaFrameLoad", {"AgentContext"});
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].metrics.at("AgentContext"),
            static_cast<int64_t>(
                CaptchaFrameAgentContext::kDevToolsAgentActiveOnTab));

  devtools_host->DetachClient(&devtools_client);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(CaptchaMetricsObserverTest, CaptchaLoadWithMultipleAgents) {
  NavigateAndCommit(GURL("https://www.top-level-site.com/"));

  // Attach a DevTools client to the WebContents.
  content::TestDevToolsProtocolClient devtools_client;
  scoped_refptr<content::DevToolsAgentHost> devtools_host =
      content::DevToolsAgentHost::GetOrCreateForTab(web_contents());
  devtools_host->AttachClient(&devtools_client);

  // Associate a mock TabInterface with the WebContents.
  actor::TestTabState tab_state(web_contents());
  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                       &tab_state.tab);

  // Create an active ActorTask.
  auto* actor_service = actor::ActorKeyedService::Get(browser_context());
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());

  // Force the task into an "acting" state, and add the tab to the task.
  auto* task = actor_service->GetTask(task_id);
  task->SetState(actor::ActorTask::State::kActing);
  task->AddTab(tab_state.tab.GetHandle(), /*stop_task_on_detach=*/false,
               base::DoNothing());

  AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "captcha-frame",
      GURL("https://www.captcha.com/subframe.html"));

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.CaptchaFrameLoad",
      CaptchaFrameAgentContext::kMultipleAgentsActiveOnTab, 1);

  auto entries = tester()->test_ukm_recorder().GetEntries(
      "PageLoad.CaptchaFrameLoad", {"AgentContext"});
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].metrics.at("AgentContext"),
            static_cast<int64_t>(
                CaptchaFrameAgentContext::kMultipleAgentsActiveOnTab));

  devtools_host->DetachClient(&devtools_client);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(CaptchaMetricsObserverTest, CaptchaFrameVisibility) {
  NavigateAndCommit(GURL("https://www.top-level-site.com/"));
  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL("https://www.google.com/recaptcha/api2/anchor"));

  // Frame load is recorded immediately, but visible load is not recorded until
  // the frame has a valid size (>= 25 px^2), is not display:none, and is
  // rendered in the viewport.
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoad", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible.ReCaptcha", 0);

  // Small frame size (4x4 = 16 < 25 px^2) should not trigger visible metric.
  tester()->metrics_web_contents_observer()->FrameSizeChanged(subframe,
                                                              gfx::Size(4, 4));
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible", 0);

  // If the frame is display:none, resizing to >= 25 px^2 should still not
  // record the visible metric.
  tester()->metrics_web_contents_observer()->FrameDisplayStateChanged(
      subframe, /*is_display_none=*/true);
  tester()->metrics_web_contents_observer()->FrameSizeChanged(
      subframe, gfx::Size(300, 75));
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible", 0);

  // If the frame is not rendered in the viewport, un-setting display:none
  // should still not record the visible metric.
  tester()->metrics_web_contents_observer()->OnFrameVisibilityChanged(
      subframe, blink::mojom::FrameVisibility::kRenderedOutOfViewport);
  tester()->metrics_web_contents_observer()->FrameDisplayStateChanged(
      subframe, /*is_display_none=*/false);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible", 0);

  // Once the frame enters the viewport (while not display:none and >= 25 px^2),
  // the visible metric is recorded.
  tester()->metrics_web_contents_observer()->OnFrameVisibilityChanged(
      subframe, blink::mojom::FrameVisibility::kRenderedInViewport);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.CaptchaFrameLoadVisible",
      CaptchaFrameAgentContext::kNoAgentActiveOnTab, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.CaptchaFrameLoadVisible.ReCaptcha",
      CaptchaFrameAgentContext::kNoAgentActiveOnTab, 1);

  // Subsequent size or visibility changes on the same frame navigation should
  // not record the metric again.
  tester()->metrics_web_contents_observer()->FrameSizeChanged(
      subframe, gfx::Size(400, 100));
  tester()->metrics_web_contents_observer()->OnFrameVisibilityChanged(
      subframe, blink::mojom::FrameVisibility::kRenderedOutOfViewport);
  tester()->metrics_web_contents_observer()->OnFrameVisibilityChanged(
      subframe, blink::mojom::FrameVisibility::kRenderedInViewport);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible.ReCaptcha", 1);
}

TEST_F(CaptchaMetricsObserverTest, CaptchaFrameVisibilityWithZoom) {
  NavigateAndCommit(GURL("https://www.top-level-site.com/"));
  content::HostZoomMap::SetZoomLevel(web_contents(),
                                     blink::ZoomFactorToZoomLevel(2.0));

  RenderFrameHost* subframe = AppendChildFrameAndNavigateAndCommit(
      web_contents()->GetPrimaryMainFrame(), "subframe",
      GURL("https://www.google.com/recaptcha/api2/anchor"));

  // At 2x browser zoom, an 8x8 physical pixel frame is 4x4 CSS pixels
  // (16 < 25 CSS px^2), so it should not be recorded as visible.
  tester()->metrics_web_contents_observer()->FrameSizeChanged(subframe,
                                                              gfx::Size(8, 8));
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.CaptchaFrameLoadVisible", 0);

  // At 2x browser zoom, a 10x10 physical pixel frame is 5x5 CSS pixels
  // (25 >= 25 CSS px^2), so it should be recorded as visible.
  tester()->metrics_web_contents_observer()->FrameSizeChanged(
      subframe, gfx::Size(10, 10));
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.CaptchaFrameLoadVisible",
      CaptchaFrameAgentContext::kNoAgentActiveOnTab, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.CaptchaFrameLoadVisible.ReCaptcha",
      CaptchaFrameAgentContext::kNoAgentActiveOnTab, 1);
}
