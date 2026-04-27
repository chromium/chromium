// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/captcha_metrics_observer.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/actor/actor_util.h"
#include "chrome/browser/page_load_metrics/observers/captcha_provider_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using page_load_metrics::CaptchaProviderManager;
using CaptchaFrameAgentContext =
    CaptchaMetricsObserver::CaptchaFrameAgentContext;

namespace {

bool IsDevToolsAgentAttached(content::WebContents* web_contents) {
  auto devtools_host = content::DevToolsAgentHost::GetForTab(web_contents);
  return devtools_host && devtools_host->IsAttached();
}

CaptchaFrameAgentContext GetCaptchaFrameAgentContext(
    content::WebContents* web_contents) {
  bool glic_agent_active = actor::HaveActiveTaskForContents(web_contents);
  bool devtools_agent_active = IsDevToolsAgentAttached(web_contents);
  if (glic_agent_active && devtools_agent_active) {
    return CaptchaFrameAgentContext::kMultipleAgentsActiveOnTab;
  }
  if (glic_agent_active) {
    return CaptchaFrameAgentContext::kGlicAgentActiveOnTab;
  }
  if (devtools_agent_active) {
    return CaptchaFrameAgentContext::kDevToolsAgentActiveOnTab;
  }
  return CaptchaFrameAgentContext::kNoAgentActiveOnTab;
}

// Keep the histogram names in sync with the "CaptchaProvider" variant defined
// in tools/metrics/histograms/metadata/page/histograms.xml.
// LINT.IfChange(CaptchaProviderHistogramName)
std::string GetCaptchaProviderHistogramName(
    page_load_metrics::CaptchaProvider captcha_provider) {
  switch (captcha_provider) {
    case page_load_metrics::CaptchaProvider::kUnknown:
      return ".UnknownCaptchaProvider";
    case page_load_metrics::CaptchaProvider::kReCaptcha:
      return ".ReCaptcha";
    case page_load_metrics::CaptchaProvider::kHCaptcha:
      return ".HCaptcha";
    case page_load_metrics::CaptchaProvider::kCloudflareTurnstile:
      return ".CloudflareTurnstile";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:CaptchaProvider)

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

  std::optional<page_load_metrics::CaptchaProvider> captcha_provider =
      CaptchaProviderManager::GetInstance()->GetCaptchaProviderForUrl(
          navigation_handle->GetURL());
  if (captcha_provider.has_value()) {
    const CaptchaFrameAgentContext agent_context =
        GetCaptchaFrameAgentContext(GetDelegate().GetWebContents());
    std::string base_histogram_name = "PageLoad.Clients.CaptchaFrameLoad";
    base::UmaHistogramEnumeration(base_histogram_name, agent_context);
    std::string provider_specific_histogram_name =
        base_histogram_name +
        GetCaptchaProviderHistogramName(captcha_provider.value());
    base::UmaHistogramEnumeration(provider_specific_histogram_name,
                                  agent_context);

    ukm::builders::PageLoad_CaptchaFrameLoad(GetDelegate().GetPageUkmSourceId())
        .SetAgentContext(static_cast<int64_t>(agent_context))
        .SetCaptchaProvider(static_cast<int64_t>(captcha_provider.value()))
        .Record(ukm::UkmRecorder::Get());
  }
}

void CaptchaMetricsObserver::FrameReceivedUserActivation(
    content::RenderFrameHost* render_frame_host) {
  if (IsInPrerendering()) {
    return;
  }

  std::optional<page_load_metrics::CaptchaProvider> captcha_provider =
      CaptchaProviderManager::GetInstance()->GetCaptchaProviderForUrl(
          render_frame_host->GetLastCommittedURL());
  if (captcha_provider.has_value()) {
    const CaptchaFrameAgentContext agent_context =
        GetCaptchaFrameAgentContext(GetDelegate().GetWebContents());
    std::string base_histogram_name = "PageLoad.Clients.CaptchaFrameActivation";
    base::UmaHistogramEnumeration(base_histogram_name, agent_context);
    std::string provider_specific_histogram_name =
        base_histogram_name +
        GetCaptchaProviderHistogramName(captcha_provider.value());
    base::UmaHistogramEnumeration(provider_specific_histogram_name,
                                  agent_context);

    ukm::builders::PageLoad_CaptchaFrameActivation(
        GetDelegate().GetPageUkmSourceId())
        .SetAgentContext(static_cast<int64_t>(agent_context))
        .SetCaptchaProvider(static_cast<int64_t>(captcha_provider.value()))
        .Record(ukm::UkmRecorder::Get());
  }
}

bool CaptchaMetricsObserver::IsInPrerendering() const {
  return (GetDelegate().GetPrerenderingState() ==
          page_load_metrics::PrerenderingState::kInPrerendering);
}
