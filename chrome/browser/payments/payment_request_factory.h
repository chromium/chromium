// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_FACTORY_H_
#define CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_FACTORY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom-forward.h"

namespace content {
class RenderFrameHost;
}

namespace payments {

// Will create a PaymentRequest based on the contents of |receiver|. The
// |receiver| was initiated by the frame hosted by |render_frame_host|, which is
// inside of |web_contents|. This function is called every time a new instance
// of PaymentRequest is created in the renderer.
void CreatePaymentRequest(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::PaymentRequest> receiver);

// Unit tests should clean up this factory override by giving a null callback so
// as to not interfere with other tests.
void SetPaymentRequestFactoryForTesting(
    base::RepeatingCallback<
        void(mojo::PendingReceiver<mojom::PaymentRequest> receiver,
             content::RenderFrameHost* render_frame_host)> factory_callback);

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_PAYMENT_REQUEST_FACTORY_H_
