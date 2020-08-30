// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/payment_credential_factory.h"

#include "chrome/browser/payments/chrome_payment_request_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace payments {

void CreatePaymentCredential(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::PaymentCredential> receiver) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  content::GlobalFrameRoutingId initiator_frame_routing_id =
      render_frame_host->GetProcess()
          ? content::GlobalFrameRoutingId(
                render_frame_host->GetProcess()->GetID(),
                render_frame_host->GetRoutingID())
          : content::GlobalFrameRoutingId();
  PaymentRequestWebContentsManager::GetOrCreateForWebContents(web_contents)
      ->CreatePaymentCredential(
          initiator_frame_routing_id,
          WebDataServiceFactory::GetPaymentManifestWebDataForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()),
              ServiceAccessType::EXPLICIT_ACCESS),
          std::move(receiver));
}

}  // namespace payments
