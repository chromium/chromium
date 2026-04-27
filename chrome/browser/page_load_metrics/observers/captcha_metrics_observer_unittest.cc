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
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_devtools_protocol_client.h"

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
      "PageLoad.Clients.CaptchaFrameActivation", 0);

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

  NavigateAndCommit(GURL("https://www.top-level-site.com/"));
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

  NavigateAndCommit(GURL("https://www.top-level-site.com/"));
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
