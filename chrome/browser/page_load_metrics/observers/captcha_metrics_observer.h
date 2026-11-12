// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CAPTCHA_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CAPTCHA_METRICS_OBSERVER_H_

#include <map>
#include <optional>

#include "chrome/browser/page_load_metrics/observers/captcha_provider_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

// Records metrics about captcha challenges shown on a page.
class CaptchaMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(CaptchaFrameAgentContext)
  enum class CaptchaFrameAgentContext {
    kUnknown = 0,
    kNoAgentActiveOnTab = 1,
    kGlicAgentActiveOnTab = 2,
    kDevToolsAgentActiveOnTab = 3,
    kMultipleAgentsActiveOnTab = 4,
    kMaxValue = kMultipleAgentsActiveOnTab,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:CaptchaFrameAgentContext)

  CaptchaMetricsObserver();

  CaptchaMetricsObserver(const CaptchaMetricsObserver&) = delete;
  CaptchaMetricsObserver& operator=(const CaptchaMetricsObserver&) = delete;

  ~CaptchaMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  void FrameDisplayStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_display_none) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;
  void OnFrameVisibilityChanged(
      content::RenderFrameHost* render_frame_host,
      blink::mojom::FrameVisibility visibility) override;
  void OnSubFrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;

 private:
  struct SubFrameData {
    std::optional<page_load_metrics::CaptchaProvider> captcha_provider;
    bool is_display_none = false;
    std::optional<gfx::Size> frame_size;
    // Defaults to kRenderedInViewport to match RenderFrameHostImpl::visibility_
    // and CrossProcessFrameConnector::SetView(), which only forwards
    // OnVisibilityChanged() on view attachment when visibility !=
    // kRenderedInViewport.
    blink::mojom::FrameVisibility visibility =
        blink::mojom::FrameVisibility::kRenderedInViewport;
    bool recorded_visible = false;
  };

  void MaybeRecordCaptchaFrameLoadVisible(SubFrameData& subframe_data);
  bool IsInPrerendering() const;

  std::map<content::FrameTreeNodeId, SubFrameData> subframe_data_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CAPTCHA_METRICS_OBSERVER_H_
