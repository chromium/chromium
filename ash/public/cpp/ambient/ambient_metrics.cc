// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_metrics.h"

#include <string>

#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/ambient/common/ambient_settings.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

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

std::string GetHistogramName(const char* prefix, bool tablet_mode) {
  std::string histogram = prefix;
  if (tablet_mode)
    histogram += ".TabletMode";
  else
    histogram += ".ClamshellMode";

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
  if (settings.topic_source == ash::AmbientModeTopicSource::kArtGallery)
    return AmbientModePhotoSource::kArtGallery;

  if (settings.selected_album_ids.size() == 0)
    return AmbientModePhotoSource::kGooglePhotosEmpty;

  bool has_recent_highlights = base::Contains(
      settings.selected_album_ids, ash::kAmbientModeRecentHighlightsAlbumId);

  if (has_recent_highlights && settings.selected_album_ids.size() == 1)
    return AmbientModePhotoSource::kGooglePhotosRecentHighlights;

  if (has_recent_highlights && settings.selected_album_ids.size() > 1)
    return AmbientModePhotoSource::kGooglePhotosBoth;

  return AmbientModePhotoSource::kGooglePhotosPersonalAlbum;
}

void RecordAmbientModeActivation(AmbientUiMode ui_mode, bool tablet_mode) {
  base::UmaHistogramEnumeration(
      GetHistogramName("Ash.AmbientMode.Activation", tablet_mode), ui_mode);
}

void RecordAmbientModeTimeElapsed(base::TimeDelta time_delta,
                                  bool tablet_mode,
                                  AmbientAnimationTheme theme) {
  base::UmaHistogramCustomTimes(
      /*name=*/GetHistogramName("Ash.AmbientMode.EngagementTime", tablet_mode),
      /*sample=*/time_delta,
      /*min=*/base::Hours(0),
      /*max=*/base::Hours(24),
      /*buckets=*/kAmbientModeElapsedTimeHistogramBuckets);

  RecordEngagementTime(
      base::StrCat({"Ash.AmbientMode.EngagementTime.", ToString(theme)}),
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

void RecordAmbientModeAnimationSmoothness(int smoothness,
                                          AmbientAnimationTheme theme) {
  base::UmaHistogramPercentage(
      base::StrCat(
          {"Ash.AmbientMode.LottieAnimationSmoothness.", ToString(theme)}),
      smoothness);
}

void RecordAmbientModePhotoOrientationMatch(int percentage_match,
                                            AmbientAnimationTheme theme) {
  base::UmaHistogramPercentage(
      base::StrCat({"Ash.AmbientMode.PhotoOrientationMatch.", ToString(theme)}),
      percentage_match);
}

void RecordAmbientModeStartupTime(base::TimeDelta startup_time,
                                  AmbientAnimationTheme theme) {
  base::UmaHistogramCustomTimes(
      /*name=*/base::StrCat({"Ash.AmbientMode.StartupTime.", ToString(theme)}),
      /*sample=*/startup_time,
      /*min=*/base::Seconds(0),
      /*max=*/kMetricsStartupTimeMax,
      /*buckets=*/50);
}

AmbientOrientationMetricsRecorder::AmbientOrientationMetricsRecorder(
    views::View* root_rendering_view,
    AmbientAnimationTheme theme)
    : theme_(ToString(theme)) {
  root_rendering_view_observer_.Observe(root_rendering_view);
  // Capture initial orientation with manual call.
  OnViewBoundsChanged(root_rendering_view);
}

AmbientOrientationMetricsRecorder::~AmbientOrientationMetricsRecorder() {
  SaveCurrentOrientationDuration();
  if (!total_portrait_duration_.is_zero()) {
    RecordEngagementTime(
        base::StringPrintf("Ash.AmbientMode.EngagementTime.%s.Portrait",
                           theme_.data()),
        total_portrait_duration_);
  }
  if (!total_landscape_duration_.is_zero()) {
    RecordEngagementTime(
        base::StringPrintf("Ash.AmbientMode.EngagementTime.%s.Landscape",
                           theme_.data()),
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
