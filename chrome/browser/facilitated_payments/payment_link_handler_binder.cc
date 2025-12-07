// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/payment_link_handler_binder.h"

#include <utility>

#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"
#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

void BindPaymentLinkHandler(
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

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return;
  }

  ChromeFacilitatedPaymentsClient* client =
      ChromeFacilitatedPaymentsClient::FromWebContents(web_contents);
  if (!client) {
    return;
  }

  payments::facilitated::ContentFacilitatedPaymentsDriver* driver =
      client->GetFacilitatedPaymentsDriverForFrame(render_frame_host);
  if (!driver) {
    return;
  }

  driver->SetPaymentLinkHandlerReceiver(std::move(receiver));
}
