// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CAPTCHA_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CAPTCHA_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
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
    kMaxValue = kGlicAgentActiveOnTab,
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

 private:
  bool IsInPrerendering() const;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CAPTCHA_METRICS_OBSERVER_H_
