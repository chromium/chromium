// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FACILITATED_PAYMENTS_PAYMENT_LINK_HANDLER_BINDER_H_
#define CHROME_BROWSER_FACILITATED_PAYMENTS_PAYMENT_LINK_HANDLER_BINDER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/facilitated_payments/payment_link_handler.mojom-forward.h"

namespace content {
class RenderFrameHost;
}

void BindPaymentLinkHandler(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<payments::facilitated::mojom::PaymentLinkHandler>
        receiver);

#endif  // CHROME_BROWSER_FACILITATED_PAYMENTS_PAYMENT_LINK_HANDLER_BINDER_H_
