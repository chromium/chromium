// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/media_router/common/providers/cast/cast_media_source.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace media_router {

namespace {

constexpr char kHistogramProviderRouteControllerCreationOutcome[] =
    "MediaRouter.Provider.RouteControllerCreationOutcome";

}  // namespace

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
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
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
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  auto cast_source = CastMediaSource::FromMediaSource(media_source);
  if (cast_source) {
    ukm::builders::MediaRouter_SiteInitiatedMirroringStarted(source_id)
        .SetAllowAudioCapture(cast_source->site_requested_audio_capture())
        .Record(ukm::UkmRecorder::Get());
  }
}

}  // namespace media_router
