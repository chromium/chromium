// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/metrics/ambient_metrics.h"

#include <string>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "ash/public/cpp/ash_web_view.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {
namespace ambient {

namespace {

// Histograms default to exponential bucketing, so the smallest bucket occupies
// 24 hours / (2 ^ (144 - 1)) milliseconds. Exponential bucketing is desirable
// for engagement time because most users exit screensaver on the order of
// several minutes, while a small fraction of users exit screensaver after
// many hours. So the histogram's highest resolution should occupy the smaller
// engagement times.
constexpr int kAmbientModeElapsedTimeHistogramBuckets = 144;

// Fields of the JSON dictionary that the ambient video HTML sends to C++ to
// communicate playback metrics. They reflect the VideoPlaybackQuality JS API:
// https://developer.mozilla.org/en-US/docs/Web/API/VideoPlaybackQuality
//
// Total number of video frames dropped since playback started.
constexpr base::StringPiece kVideoFieldDroppedFrames = "dropped_frames";
// Total number of video frames expected since playback started (frames
// created + frames dropped).
constexpr base::StringPiece kVideoFieldTotalFrames = "total_frames";

std::string GetHistogramName(const char* prefix, bool tablet_mode) {
  std::string histogram = prefix;
  if (tablet_mode) {
    histogram += ".TabletMode";
  } else {
    histogram += ".ClamshellMode";
  }

  return histogram;
}

void RecordEngagementTime(base::StringPiece histogram_name,
                          base::TimeDelta engagement_time) {
  base::UmaHistogramCustomTimes(
      histogram_name.data(),
      /*sample=*/engagement_time,
      // There is no value in bucketing engagement times that are on the order
      // of milliseconds. A 1 second minimum is imposed here but not in the
      // metric above for legacy reasons (the metric above was already pushed
      // to the field and established before this change was made).
      /*min=*/base::Seconds(1),
      /*max=*/base::Hours(24),
      /*buckets=*/kAmbientModeElapsedTimeHistogramBuckets);
}

}  // namespace

AmbientModePhotoSource AmbientSettingsToPhotoSource(
    const AmbientSettings& settings) {
  if (settings.topic_source == ash::AmbientModeTopicSource::kArtGallery) {
    return AmbientModePhotoSource::kArtGallery;
  }

  if (settings.selected_album_ids.size() == 0) {
    return AmbientModePhotoSource::kGooglePhotosEmpty;
  }

  bool has_recent_highlights = base::Contains(
      settings.selected_album_ids, ash::kAmbientModeRecentHighlightsAlbumId);

  if (has_recent_highlights && settings.selected_album_ids.size() == 1) {
    return AmbientModePhotoSource::kGooglePhotosRecentHighlights;
  }

  if (has_recent_highlights && settings.selected_album_ids.size() > 1) {
    return AmbientModePhotoSource::kGooglePhotosBoth;
  }

  return AmbientModePhotoSource::kGooglePhotosPersonalAlbum;
}

void RecordAmbientModeActivation(AmbientUiMode ui_mode, bool tablet_mode) {
  base::UmaHistogramEnumeration(
      GetHistogramName("Ash.AmbientMode.Activation", tablet_mode), ui_mode);
}

void RecordAmbientModeTimeElapsed(base::TimeDelta time_delta,
                                  bool tablet_mode,
                                  const AmbientUiSettings& ui_settings) {
  base::UmaHistogramCustomTimes(
      /*name=*/GetHistogramName("Ash.AmbientMode.EngagementTime", tablet_mode),
      /*sample=*/time_delta,
      /*min=*/base::Hours(0),
      /*max=*/base::Hours(24),
      /*buckets=*/kAmbientModeElapsedTimeHistogramBuckets);

  RecordEngagementTime(
      base::StrCat({"Ash.AmbientMode.EngagementTime.", ui_settings.ToString()}),
      time_delta);
}

void RecordAmbientModeTotalNumberOfAlbums(int num_albums) {
  base::UmaHistogramCounts100("Ash.AmbientMode.TotalNumberOfAlbums",
                              num_albums);
}

void RecordAmbientModeSelectedNumberOfAlbums(int num_albums) {
  base::UmaHistogramCounts100("Ash.AmbientMode.SelectedNumberOfAlbums",
                              num_albums);
}

void RecordAmbientModeAnimationSmoothness(
    int smoothness,
    const AmbientUiSettings& ui_settings) {
  base::UmaHistogramPercentage(
      base::StrCat({"Ash.AmbientMode.LottieAnimationSmoothness.",
                    ui_settings.ToString()}),
      smoothness);
}

void RecordAmbientModePhotoOrientationMatch(
    int percentage_match,
    const AmbientUiSettings& ui_settings) {
  base::UmaHistogramPercentage(
      base::StrCat(
          {"Ash.AmbientMode.PhotoOrientationMatch.", ui_settings.ToString()}),
      percentage_match);
}

void RecordAmbientModeStartupTime(base::TimeDelta startup_time,
                                  const AmbientUiSettings& ui_settings) {
  base::UmaHistogramCustomTimes(
      /*name=*/base::StrCat(
          {"Ash.AmbientMode.StartupTime.", ui_settings.ToString()}),
      /*sample=*/startup_time,
      /*min=*/base::Seconds(0),
      /*max=*/kMetricsStartupTimeMax,
      /*buckets=*/50);
}

void RecordAmbientModeVideoSmoothness(AshWebView* web_view,
                                      const AmbientUiSettings& ui_settings) {
  CHECK(web_view);
  CHECK_EQ(ui_settings.theme(), AmbientTheme::kVideo);
  // The URL fragment identifier is used as a way of communicating the playback
  // metrics data without using any elaborate frameworks or permissions
  // (ex: a WebUI).
  std::string serialized_playback_metrics =
      net::UnescapePercentEncodedUrl(web_view->GetVisibleURL().ref());
  if (serialized_playback_metrics.empty()) {
    // This can legitimately happen if the ambient session was too short (just a
    // couple seconds) and not statistically significant enough to record.
    DVLOG(2) << "Ambient video session not long enough to record smoothness";
    return;
  }
  absl::optional<base::Value::Dict> playback_metrics =
      base::JSONReader::ReadDict(serialized_playback_metrics);
  if (!playback_metrics) {
    LOG(ERROR) << "Received non-json metrics: " << serialized_playback_metrics;
    return;
  }
  absl::optional<int> dropped_frames =
      playback_metrics->FindInt(kVideoFieldDroppedFrames);
  // Assuming 24 fps, the ambient session would have to last ~2.83 years before
  // the int overflows. For all intensive purposes, this should not happen.
  absl::optional<int> expected_frames =
      playback_metrics->FindInt(kVideoFieldTotalFrames);
  if (!dropped_frames || !expected_frames) {
    LOG(ERROR) << "Received invalid metrics dictionary: " << *playback_metrics;
    return;
  }
  if (*dropped_frames < 0 || *expected_frames <= 0 ||
      *dropped_frames > *expected_frames) {
    LOG(ERROR) << "Frame statistics are invalid: " << *playback_metrics;
    return;
  }
  int created_frames = *expected_frames - *dropped_frames;
  int smoothness = base::ClampRound(
      100.f * (static_cast<float>(created_frames) / *expected_frames));
  base::UmaHistogramPercentage(base::StrCat({"Ash.AmbientMode.VideoSmoothness.",
                                             ui_settings.ToString()}),
                               smoothness);
}

AmbientOrientationMetricsRecorder::AmbientOrientationMetricsRecorder(
    views::View* root_rendering_view,
    const AmbientUiSettings& ui_settings)
    : settings_(ui_settings.ToString()) {
  root_rendering_view_observer_.Observe(root_rendering_view);
  // Capture initial orientation with manual call.
  OnViewBoundsChanged(root_rendering_view);
}

AmbientOrientationMetricsRecorder::~AmbientOrientationMetricsRecorder() {
  SaveCurrentOrientationDuration();
  if (!total_portrait_duration_.is_zero()) {
    RecordEngagementTime(
        base::StringPrintf("Ash.AmbientMode.EngagementTime.%s.Portrait",
                           settings_.data()),
        total_portrait_duration_);
  }
  if (!total_landscape_duration_.is_zero()) {
    RecordEngagementTime(
        base::StringPrintf("Ash.AmbientMode.EngagementTime.%s.Landscape",
                           settings_.data()),
        total_landscape_duration_);
  }
}

void AmbientOrientationMetricsRecorder::OnViewBoundsChanged(
    views::View* observed_view) {
  DCHECK(observed_view);
  gfx::Rect content_bounds = observed_view->GetContentsBounds();
  if (content_bounds.IsEmpty()) {
    DVLOG(4) << "Initial view layout has not occurred yet. Ignoring empty view "
                "bounds";
    return;
  }

  bool new_orientation_is_portrait =
      content_bounds.width() < content_bounds.height();
  if (current_orientation_is_portrait_.has_value() &&
      *current_orientation_is_portrait_ == new_orientation_is_portrait) {
    return;
  }

  SaveCurrentOrientationDuration();
  current_orientation_is_portrait_.emplace(new_orientation_is_portrait);
  // Effectively stops the existing timer and starts new one.
  current_orientation_timer_.emplace();
}

void AmbientOrientationMetricsRecorder::SaveCurrentOrientationDuration() {
  if (!current_orientation_is_portrait_.has_value() ||
      !current_orientation_timer_.has_value()) {
    return;
  }

  if (*current_orientation_is_portrait_) {
    total_portrait_duration_ += current_orientation_timer_->Elapsed();
  } else {
    total_landscape_duration_ += current_orientation_timer_->Elapsed();
  }
}

}  // namespace ambient
}  // namespace ash
