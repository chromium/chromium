// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/captcha_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/page_load_metrics/observers/captcha_provider_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

using page_load_metrics::CaptchaProviderManager;

namespace {

bool IsActorActingOnWebContents(content::WebContents* web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  auto* actor_service =
      actor::ActorKeyedService::Get(web_contents->GetBrowserContext());
  if (!actor_service) {
    return false;
  }

  const auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  return tab_interface && actor_service->IsActiveOnTab(*tab_interface);
#else
  return false;
#endif
}

}  // namespace

CaptchaMetricsObserver::CaptchaMetricsObserver() = default;
CaptchaMetricsObserver::~CaptchaMetricsObserver() = default;

const char* CaptchaMetricsObserver::GetObserverName() const {
  static const char kName[] = "CaptchaMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CaptchaMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CaptchaMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return FORWARD_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CaptchaMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

void CaptchaMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() || IsInPrerendering()) {
    return;
  }

  if (CaptchaProviderManager::GetInstance()->IsCaptchaUrl(
          navigation_handle->GetURL())) {
    const CaptchaFrameAgentContext agent_context =
        IsActorActingOnWebContents(GetDelegate().GetWebContents())
            ? CaptchaFrameAgentContext::kGlicAgentActiveOnTab
            : CaptchaFrameAgentContext::kNoAgentActiveOnTab;
    base::UmaHistogramEnumeration("PageLoad.Clients.CaptchaFrameLoad",
                                  agent_context);
    ukm::builders::PageLoad_CaptchaFrameLoad(GetDelegate().GetPageUkmSourceId())
        .SetAgentContext(static_cast<int64_t>(agent_context))
        .Record(ukm::UkmRecorder::Get());
  }
}

void CaptchaMetricsObserver::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  if (IsInPrerendering()) {
    return;
  }

  if (CaptchaProviderManager::GetInstance()->IsCaptchaUrl(
          render_frame_host->GetLastCommittedURL())) {
    const CaptchaFrameAgentContext agent_context =
        IsActorActingOnWebContents(GetDelegate().GetWebContents())
            ? CaptchaFrameAgentContext::kGlicAgentActiveOnTab
            : CaptchaFrameAgentContext::kNoAgentActiveOnTab;
    base::UmaHistogramEnumeration("PageLoad.Clients.CaptchaFrameActivation",
                                  agent_context);
    ukm::builders::PageLoad_CaptchaFrameActivation(
        GetDelegate().GetPageUkmSourceId())
        .SetAgentContext(static_cast<int64_t>(agent_context))
        .Record(ukm::UkmRecorder::Get());
  }
}

bool CaptchaMetricsObserver::IsInPrerendering() const {
  return (GetDelegate().GetPrerenderingState() ==
          page_load_metrics::PrerenderingState::kInPrerendering);
}
