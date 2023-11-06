// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_host.h"

#include "chrome/browser/page_load_metrics/observers/lcp_critical_path_predictor_page_load_metrics_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/features.h"

namespace {

size_t GetLCPPFontURLPredictorMaxUrlLength() {
  static size_t max_length = base::checked_cast<size_t>(
      blink::features::kLCPPFontURLPredictorMaxUrlLength.Get());
  return max_length;
}

}  // namespace

namespace predictors {

LCPCriticalPathPredictorHost::LCPCriticalPathPredictorHost(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::LCPCriticalPathPredictorHost> receiver)
    : content::DocumentService<blink::mojom::LCPCriticalPathPredictorHost>(
          render_frame_host,
          std::move(receiver)) {}

void LCPCriticalPathPredictorHost::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::LCPCriticalPathPredictorHost>
        receiver) {
  // The object is bound to the lifetime of the |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new LCPCriticalPathPredictorHost(*render_frame_host, std::move(receiver));
}

LCPCriticalPathPredictorHost::~LCPCriticalPathPredictorHost() = default;

void LCPCriticalPathPredictorHost::SetLcpElementLocator(
    const std::string& lcp_element_locator) {
  // `LcpCriticalPathPredictorPageLoadMetricsObserver::OnCommit()` stores
  // `LcpCriticalPathPredictorPageLoadMetricsObserver` in `PageData` as a weak
  // pointer. This weak pointer can be deleted at any time.
  if (auto* page_data =
          LcpCriticalPathPredictorPageLoadMetricsObserver::PageData::GetForPage(
              render_frame_host().GetPage())) {
    if (auto* plmo =
            page_data->GetLcpCriticalPathPredictorPageLoadMetricsObserver()) {
      plmo->SetLcpElementLocator(lcp_element_locator);
    }
  }
}

void LCPCriticalPathPredictorHost::SetLcpInfluencerScriptUrls(
    const std::vector<GURL>& lcp_influencer_scripts) {
  if (!base::FeatureList::IsEnabled(blink::features::kLCPScriptObserver)) {
    return;
  }
  if (auto* page_data =
          LcpCriticalPathPredictorPageLoadMetricsObserver::PageData::GetForPage(
              render_frame_host().GetPage())) {
    if (auto* plmo =
            page_data->GetLcpCriticalPathPredictorPageLoadMetricsObserver()) {
      plmo->SetLcpInfluencerScriptUrls(lcp_influencer_scripts);
    }
  }
}

void LCPCriticalPathPredictorHost::NotifyFetchedFont(const GURL& font_url) {
  if (!base::FeatureList::IsEnabled(blink::features::kLCPPFontURLPredictor)) {
    ReportBadMessageAndDeleteThis(
        "NotifyFetchedFont can be called only if kLCPPFontURLPredictor is "
        "enabled.");
    return;
  }
  if (!font_url.SchemeIsHTTPOrHTTPS()) {
    ReportBadMessageAndDeleteThis("url format must be checked in the caller.");
    return;
  }
  if (font_url.spec().length() > GetLCPPFontURLPredictorMaxUrlLength()) {
    // The size can be different between KURL and GURL, not reporting
    // bad message.
    return;
  }
  auto* page_data =
      LcpCriticalPathPredictorPageLoadMetricsObserver::PageData::GetForPage(
          render_frame_host().GetPage());
  if (!page_data) {
    return;
  }
  auto* plmo = page_data->GetLcpCriticalPathPredictorPageLoadMetricsObserver();
  if (!plmo) {
    return;
  }

  plmo->AppendFetchedFontUrl(font_url);
}

}  // namespace predictors
