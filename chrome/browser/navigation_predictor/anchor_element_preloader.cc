// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/anchor_element_preloader.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "content/public/browser/browser_context.h"

AnchorElementPreloader::AnchorElementPreloader(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AnchorElementInteractionHost> receiver)
    : content::DocumentService<blink::mojom::AnchorElementInteractionHost>(
          render_frame_host,
          std::move(receiver)) {}

void AnchorElementPreloader::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AnchorElementInteractionHost>
        receiver) {
  // The object is bound to the lifetime of the |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new AnchorElementPreloader(render_frame_host, std::move(receiver));
}

void AnchorElementPreloader::OnPointerDown(const GURL& target) {
  auto* loading_predictor = predictors::LoadingPredictorFactory::GetForProfile(
      Profile::FromBrowserContext(render_frame_host()->GetBrowserContext()));

  if (!loading_predictor) {
    return;
  }

  net::SchemefulSite schemeful_site(target);
  net::NetworkIsolationKey network_isolation_key(schemeful_site,
                                                 schemeful_site);
  loading_predictor->PreconnectURLIfAllowed(target, /*allow_credentials=*/true,
                                            network_isolation_key);
}
