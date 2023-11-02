// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_METRICS_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_METRICS_H_

#include "ash/constants/ambient_animation_theme.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/scoped_observation.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

struct AmbientSettings;
enum class AmbientUiMode;

namespace ambient {

// These values are persisted in UMA logs, so they should never be renumbered or
// reused. Used for reporting the histogram |Ash.AmbientMode.PhotoSource|.
enum class ASH_PUBLIC_EXPORT AmbientModePhotoSource {
  kUnset = 0,
  kArtGallery = 1,
  kGooglePhotosRecentHighlights = 2,
  kGooglePhotosPersonalAlbum = 3,
  kGooglePhotosBoth = 4,
  kGooglePhotosEmpty = 5,
  kMaxValue = kGooglePhotosEmpty,
};

// Duration after which ambient mode is considered to have failed to start.
// See summary in histograms.xml for why 15 seconds is used.
constexpr base::TimeDelta kMetricsStartupTimeMax = base::Seconds(15);

ASH_PUBLIC_EXPORT AmbientModePhotoSource
AmbientSettingsToPhotoSource(const AmbientSettings& settings);

ASH_PUBLIC_EXPORT void RecordAmbientModeActivation(AmbientUiMode ui_mode,
                                                   bool tablet_mode);

ASH_PUBLIC_EXPORT void RecordAmbientModeTimeElapsed(
    base::TimeDelta time_delta,
    bool tablet_mode,
    AmbientAnimationTheme theme);

ASH_PUBLIC_EXPORT void RecordAmbientModeTotalNumberOfAlbums(int num_albums);

ASH_PUBLIC_EXPORT void RecordAmbientModeSelectedNumberOfAlbums(int num_albums);

ASH_PUBLIC_EXPORT void RecordAmbientModeAnimationSmoothness(
    int smoothness,
    AmbientAnimationTheme theme);

ASH_PUBLIC_EXPORT void RecordAmbientModePhotoOrientationMatch(
    int percentage_match,
    AmbientAnimationTheme theme);

ASH_PUBLIC_EXPORT void RecordAmbientModeStartupTime(
    base::TimeDelta startup_time,
    AmbientAnimationTheme theme);

// Records metrics that track the total usage of each orientation in ambient
// mode.
class ASH_PUBLIC_EXPORT AmbientOrientationMetricsRecorder
    : public views::ViewObserver {
 public:
  AmbientOrientationMetricsRecorder(views::View* root_rendering_view,
                                    AmbientAnimationTheme theme);
  AmbientOrientationMetricsRecorder(const AmbientOrientationMetricsRecorder&) =
      delete;
  AmbientOrientationMetricsRecorder& operator=(
      const AmbientOrientationMetricsRecorder&) = delete;
  ~AmbientOrientationMetricsRecorder() override;

 private:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void SaveCurrentOrientationDuration();

  const base::StringPiece theme_;
  base::ScopedObservation<views::View, ViewObserver>
      root_rendering_view_observer_{this};
  // Null until a non-empty view boundary is provided (i.e. the initial view
  // layout occurs).
  absl::optional<bool> current_orientation_is_portrait_;
  absl::optional<base::ElapsedTimer> current_orientation_timer_;
  base::TimeDelta total_portrait_duration_;
  base::TimeDelta total_landscape_duration_;
};

}  // namespace ambient
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_METRICS_H_
