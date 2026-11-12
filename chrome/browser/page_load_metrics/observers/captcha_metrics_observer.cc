// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/captcha_metrics_observer.h"

#include <limits>
#include <string>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/actor/actor_util.h"
#include "chrome/browser/page_load_metrics/observers/captcha_provider_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/page/page_zoom.h"

using page_load_metrics::CaptchaProviderManager;
using CaptchaFrameAgentContext =
    CaptchaMetricsObserver::CaptchaFrameAgentContext;

namespace {

// Minimum area in pixels for a frame to be considered visible. This threshold
// filters out tiny (e.g. 0x0 or 1x1) background/invisible CAPTCHA frames.
constexpr int kMinimumVisibleFrameArea = 25;

gfx::Size GetUnscaledFrameSize(content::WebContents* web_contents,
                               const gfx::Size& raw_frame_size) {
  float device_scale_factor = 1.0f;
  if (content::RenderWidgetHostView* view =
          web_contents->GetRenderWidgetHostView()) {
    device_scale_factor = view->GetDeviceScaleFactor();
  }
  double zoom_factor = blink::ZoomLevelToZoomFactor(
      content::HostZoomMap::GetZoomLevel(web_contents));
  if (device_scale_factor <= 0.0 || zoom_factor <= 0.0) {
    return raw_frame_size;
  }

  float scale_factor =
      1.0f / (device_scale_factor * static_cast<float>(zoom_factor));
  return gfx::ScaleToRoundedSize(raw_frame_size, scale_factor);
}

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
  SubFrameData& subframe_data =
      subframe_data_[navigation_handle->GetFrameTreeNodeId()];
  subframe_data.captcha_provider = captcha_provider;
  subframe_data.recorded_visible = false;
  if (content::RenderFrameHost* render_frame_host =
          navigation_handle->GetRenderFrameHost()) {
    subframe_data.is_display_none = render_frame_host->IsFrameDisplayNone();
    subframe_data.frame_size = render_frame_host->GetFrameSize();
  }

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

    MaybeRecordCaptchaFrameLoadVisible(subframe_data);
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

void CaptchaMetricsObserver::FrameDisplayStateChanged(
    content::RenderFrameHost* render_frame_host,
    bool is_display_none) {
  if (IsInPrerendering() || !render_frame_host ||
      !render_frame_host->GetParentOrOuterDocument()) {
    return;
  }

  SubFrameData& subframe_data =
      subframe_data_[render_frame_host->GetFrameTreeNodeId()];
  subframe_data.is_display_none = is_display_none;
  MaybeRecordCaptchaFrameLoadVisible(subframe_data);
}

void CaptchaMetricsObserver::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  if (IsInPrerendering() || !render_frame_host ||
      !render_frame_host->GetParentOrOuterDocument()) {
    return;
  }

  SubFrameData& subframe_data =
      subframe_data_[render_frame_host->GetFrameTreeNodeId()];
  subframe_data.frame_size = frame_size;
  MaybeRecordCaptchaFrameLoadVisible(subframe_data);
}

void CaptchaMetricsObserver::OnFrameVisibilityChanged(
    content::RenderFrameHost* render_frame_host,
    blink::mojom::FrameVisibility visibility) {
  if (IsInPrerendering() || !render_frame_host ||
      !render_frame_host->GetParentOrOuterDocument()) {
    return;
  }

  SubFrameData& subframe_data =
      subframe_data_[render_frame_host->GetFrameTreeNodeId()];
  subframe_data.visibility = visibility;
  MaybeRecordCaptchaFrameLoadVisible(subframe_data);
}

void CaptchaMetricsObserver::OnSubFrameDeleted(
    content::FrameTreeNodeId frame_tree_node_id) {
  subframe_data_.erase(frame_tree_node_id);
}

void CaptchaMetricsObserver::MaybeRecordCaptchaFrameLoadVisible(
    SubFrameData& subframe_data) {
  if (IsInPrerendering() || subframe_data.recorded_visible ||
      !subframe_data.captcha_provider.has_value() ||
      subframe_data.is_display_none ||
      subframe_data.visibility !=
          blink::mojom::FrameVisibility::kRenderedInViewport ||
      !subframe_data.frame_size.has_value()) {
    return;
  }

  content::WebContents* web_contents = GetDelegate().GetWebContents();
  gfx::Size unscaled_frame_size =
      GetUnscaledFrameSize(web_contents, *subframe_data.frame_size);
  if (unscaled_frame_size.GetCheckedArea().ValueOrDefault(
          std::numeric_limits<int>::max()) < kMinimumVisibleFrameArea) {
    return;
  }

  subframe_data.recorded_visible = true;
  const CaptchaFrameAgentContext agent_context =
      GetCaptchaFrameAgentContext(web_contents);
  std::string base_histogram_name = "PageLoad.Clients.CaptchaFrameLoadVisible";
  base::UmaHistogramEnumeration(base_histogram_name, agent_context);
  std::string provider_specific_histogram_name =
      base_histogram_name +
      GetCaptchaProviderHistogramName(subframe_data.captcha_provider.value());
  base::UmaHistogramEnumeration(provider_specific_histogram_name,
                                agent_context);
}

bool CaptchaMetricsObserver::IsInPrerendering() const {
  return (GetDelegate().GetPrerenderingState() ==
          page_load_metrics::PrerenderingState::kInPrerendering);
}
