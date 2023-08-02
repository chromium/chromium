// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_host.h"

#include "chrome/browser/page_load_metrics/observers/lcp_critical_path_predictor_page_load_metrics_observer.h"
#include "content/public/browser/render_frame_host.h"

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

}  // namespace predictors
