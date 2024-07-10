// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/payment_link_handler_factory.h"

#include <utility>

#include "components/facilitated_payments/content/browser/content_payment_link_handler_impl.h"
#include "content/public/browser/render_frame_host.h"

void CreatePaymentLinkHandler(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<payments::facilitated::mojom::PaymentLinkHandler>
        receiver) {
  if (!render_frame_host->IsActive()) {
    // This happens when the page has navigated away, which would cause the
    // blink PaymentLinkHandler to be released shortly, or when the iframe is
    // being removed from the page, which is not a use case that we support.
    // Abandoning the `receiver` will close the mojo connection.
    return;
  }

  // Only valid for the main frame.
  if (render_frame_host->GetParentOrOuterDocument()) {
    return;
  }

  // ContentPaymentLinkHandlerImpl is a DocumentService, whose lifetime is
  // managed by the RenderFrameHost passed in here.
  new payments::facilitated::ContentPaymentLinkHandlerImpl(*render_frame_host,
                                                           std::move(receiver));
}
