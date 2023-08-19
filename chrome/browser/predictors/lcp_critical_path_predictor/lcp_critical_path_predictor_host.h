// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_HOST_H_
#define CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_HOST_H_

#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"

namespace predictors {

// LCPCriticalPathPredictorHost provides an implementation of
// blink::mojom::LCPCriticalPathPredictorHost in the renderer process. We'll
// have one instance per document. The interface serves two purposes: recording
// the useful signals from the page load to the database, and providing the hint
// data for the subsequent navigations.
class LCPCriticalPathPredictorHost
    : public content::DocumentService<
          blink::mojom::LCPCriticalPathPredictorHost> {
 public:
  LCPCriticalPathPredictorHost(const LCPCriticalPathPredictorHost&) = delete;
  LCPCriticalPathPredictorHost& operator=(const LCPCriticalPathPredictorHost&) =
      delete;

  // Create and bind LCPCriticalPathPredictorHost
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::LCPCriticalPathPredictorHost>
          receiver);

 private:
  LCPCriticalPathPredictorHost(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::LCPCriticalPathPredictorHost>
          receiver);

  ~LCPCriticalPathPredictorHost() override;

  // Implements blink::mojom::LCPCriticalPathPredictorHost.
  void SetLcpElementLocator(const std::string& lcp_element_locator) override;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_IMPL_H_
