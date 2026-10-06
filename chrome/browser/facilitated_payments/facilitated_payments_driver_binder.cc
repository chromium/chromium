// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/facilitated_payments_driver_binder.h"

#include <utility>

#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"
#include "components/facilitated_payments/content/browser/content_facilitated_payments_driver.h"
#include "components/facilitated_payments/core/mojom/facilitated_payments_agent.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

void BindFacilitatedPaymentsDriver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingAssociatedReceiver<
        payments::facilitated::mojom::FacilitatedPaymentsDriver> receiver) {
  // Abandoning `receiver` closes the pipe. The agent is only created for the
  // main frame, and an inactive frame is about to go away.
  if (!render_frame_host->IsActive() ||
      render_frame_host->GetParentOrOuterDocument()) {
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    return;
  }

  // Only tabs have a client, so non-tab `WebContents` are ignored.
  ChromeFacilitatedPaymentsClient* client =
      ChromeFacilitatedPaymentsClient::From(
          tabs::TabInterface::MaybeGetFromContents(web_contents));
  if (!client) {
    return;
  }

  payments::facilitated::ContentFacilitatedPaymentsDriver* driver =
      client->GetFacilitatedPaymentsDriverForFrame(render_frame_host);
  if (!driver) {
    return;
  }

  driver->SetFacilitatedPaymentsDriverReceiver(std::move(receiver));
}
