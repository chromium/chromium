// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/anchor_element_preloader.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/prefetch/prefetch_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "url/scheme_host_port.h"

const char kPreloadingAnchorElementPreloaderPreloadingTriggered[] =
    "Preloading.AnchorElementPreloader.PreloadingTriggered";

AnchorElementPreloader::~AnchorElementPreloader() = default;

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
  if (!prefetch::IsSomePreloadingEnabled(
          *Profile::FromBrowserContext(render_frame_host()->GetBrowserContext())
               ->GetPrefs())) {
    return;
  }
  url::SchemeHostPort scheme_host_port(target);
  if (preconnected_targets_.find(scheme_host_port) !=
      preconnected_targets_.end()) {
    // We've already preconnected to that origin.
    return;
  }
  preconnected_targets_.insert(scheme_host_port);

  RecordUmaPreloadedTriggered(AnchorElementPreloaderType::kPreconnect);

  RecordUkmPreloadType(AnchorElementPreloaderType::kPreconnect);

  if (base::GetFieldTrialParamByFeatureAsBool(
          blink::features::kAnchorElementInteraction, "preconnect_holdback",
          false)) {
    return;
  }

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

void AnchorElementPreloader::RecordUmaPreloadedTriggered(
    AnchorElementPreloaderType preload) {
  base::UmaHistogramEnumeration(
      kPreloadingAnchorElementPreloaderPreloadingTriggered, preload);
}

void AnchorElementPreloader::RecordUkmPreloadType(
    AnchorElementPreloaderType type) {
  ukm::SourceId source_id = render_frame_host()->GetPageUkmSourceId();

  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::Preloading_AnchorInteraction(source_id)
      .SetAnchorElementPreloaderType(static_cast<int64_t>(type))
      .Record(ukm_recorder);
}
