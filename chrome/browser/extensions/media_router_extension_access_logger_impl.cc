// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/media_router_extension_access_logger_impl.h"

#include <string>

#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/origin.h"

namespace extensions {
namespace {

void DidGetBackgroundSourceIdForCastExtensionMetric(
    absl::optional<ukm::SourceId> source_id) {
  if (!source_id.has_value())
    return;  // Can't record as it wasn't found in the history.

  // Log UMA.
  base::UmaHistogramBoolean("MediaRouter.CastWebSenderExtensionLoaded", true);

  // Use the newly generated source ID to log UMK.
  ukm::builders::MediaRouter_CastWebSenderExtensionLoadUrl(source_id.value())
      .SetHasOccurred(true)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace

MediaRouterExtensionAccessLoggerImpl::~MediaRouterExtensionAccessLoggerImpl() =
    default;

void MediaRouterExtensionAccessLoggerImpl::LogMediaRouterComponentExtensionUse(
    const url::Origin& origin,
    content::BrowserContext* context) const {
  // Log metrics if |origin| exists in |context|'s history.
  Profile* profile = Profile::FromBrowserContext(context);
  auto* ukm_background_service =
      ukm::UkmBackgroundRecorderFactory::GetForProfile(profile);
  DCHECK(ukm_background_service);
  ukm_background_service->GetBackgroundSourceIdIfAllowed(
      origin, base::BindOnce(&DidGetBackgroundSourceIdForCastExtensionMetric));
}

}  // namespace extensions
