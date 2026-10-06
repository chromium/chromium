// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FACILITATED_PAYMENTS_FACILITATED_PAYMENTS_DRIVER_BINDER_H_
#define CHROME_BROWSER_FACILITATED_PAYMENTS_FACILITATED_PAYMENTS_DRIVER_BINDER_H_

#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

// Binds `receiver` to the `ContentFacilitatedPaymentsDriver` of
// `render_frame_host`, so that the renderer-side `FacilitatedPaymentsAgent` can
// report its heuristic signals. Drops `receiver`, which closes the pipe, if the
// frame is inactive, is not the main frame, or the tab has no
// `ChromeFacilitatedPaymentsClient`.
void BindFacilitatedPaymentsDriver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingAssociatedReceiver<
        payments::facilitated::mojom::FacilitatedPaymentsDriver> receiver);

#endif  // CHROME_BROWSER_FACILITATED_PAYMENTS_FACILITATED_PAYMENTS_DRIVER_BINDER_H_
