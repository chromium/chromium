// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"

#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/version.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace media_router {

namespace {

constexpr char kHistogramProviderRouteControllerCreationOutcome[] =
    "MediaRouter.Provider.RouteControllerCreationOutcome";
constexpr char kHistogramProviderVersion[] = "MediaRouter.Provider.Version";
constexpr char kHistogramProviderWakeReason[] =
    "MediaRouter.Provider.WakeReason";
constexpr char kHistogramProviderWakeup[] = "MediaRouter.Provider.Wakeup";

}  // namespace

// static
void MediaRouterMojoMetrics::RecordMediaRouteProviderWakeReason(
    MediaRouteProviderWakeReason reason) {
  DCHECK_LT(static_cast<int>(reason),
            static_cast<int>(MediaRouteProviderWakeReason::TOTAL_COUNT));
  base::UmaHistogramEnumeration(kHistogramProviderWakeReason, reason,
                                MediaRouteProviderWakeReason::TOTAL_COUNT);
}

// static
void MediaRouterMojoMetrics::RecordMediaRouteProviderVersion(
    const extensions::Extension& extension) {
  MediaRouteProviderVersion version = MediaRouteProviderVersion::UNKNOWN;
  version = GetMediaRouteProviderVersion(extension.version(),
                                         version_info::GetVersion());

  DCHECK_LT(static_cast<int>(version),
            static_cast<int>(MediaRouteProviderVersion::TOTAL_COUNT));
  base::UmaHistogramEnumeration(kHistogramProviderVersion, version,
                                MediaRouteProviderVersion::TOTAL_COUNT);
}

// static
void MediaRouterMojoMetrics::RecordMediaRouteProviderWakeup(
    MediaRouteProviderWakeup wakeup) {
  DCHECK_LT(static_cast<int>(wakeup),
            static_cast<int>(MediaRouteProviderWakeup::TOTAL_COUNT));
  base::UmaHistogramEnumeration(kHistogramProviderWakeup, wakeup,
                                MediaRouteProviderWakeup::TOTAL_COUNT);
}

// static
void MediaRouterMojoMetrics::RecordMediaRouteControllerCreationResult(
    bool success) {
  base::UmaHistogramBoolean(kHistogramProviderRouteControllerCreationOutcome,
                            success);
}

// static
void MediaRouterMojoMetrics::RecordTabMirroringMetrics(
    content::WebContents* web_contents) {
  ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents);
  WebContentsAudioState audio_state = WebContentsAudioState::kWasNeverAudible;
  if (web_contents->IsCurrentlyAudible()) {
    audio_state = WebContentsAudioState::kIsCurrentlyAudible;
  } else if (web_contents->WasEverAudible()) {
    audio_state = WebContentsAudioState::kWasPreviouslyAudible;
  }

  ukm::builders::MediaRouter_TabMirroringStarted(source_id)
      .SetAudioState(static_cast<int>(audio_state))
      .Record(ukm::UkmRecorder::Get());
}

// static
void MediaRouterMojoMetrics::RecordSiteInitiatedMirroringStarted(
    content::WebContents* web_contents,
    const MediaSource& media_source) {
  ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents);
  auto cast_source = CastMediaSource::FromMediaSource(media_source);
  if (cast_source) {
    ukm::builders::MediaRouter_SiteInitiatedMirroringStarted(source_id)
        .SetAllowAudioCapture(cast_source->site_requested_audio_capture())
        .Record(ukm::UkmRecorder::Get());
  }
}

// static
MediaRouteProviderVersion MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
    const base::Version& extension_version,
    const base::Version& browser_version) {
  if (!extension_version.IsValid() || extension_version.components().empty() ||
      !browser_version.IsValid() || browser_version.components().empty()) {
    return MediaRouteProviderVersion::UNKNOWN;
  }

  uint32_t extension_major = extension_version.components()[0];
  uint32_t browser_major = browser_version.components()[0];
  // Sanity check.
  if (extension_major == 0 || browser_major == 0) {
    return MediaRouteProviderVersion::UNKNOWN;
  } else if (extension_major >= browser_major) {
    return MediaRouteProviderVersion::SAME_VERSION_AS_CHROME;
  } else if (browser_major - extension_major == 1) {
    return MediaRouteProviderVersion::ONE_VERSION_BEHIND_CHROME;
  } else {
    return MediaRouteProviderVersion::MULTIPLE_VERSIONS_BEHIND_CHROME;
  }
}

}  // namespace media_router
