// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_host.h"

#include "chrome/common/chrome_features.h"
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

}  // namespace predictors
