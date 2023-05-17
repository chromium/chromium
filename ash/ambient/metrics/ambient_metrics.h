// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_METRICS_AMBIENT_METRICS_H_
#define ASH_AMBIENT_METRICS_AMBIENT_METRICS_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_mode_photo_source.h"
#include "base/scoped_observation.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

struct AmbientSettings;
class AmbientUiSettings;
class AshWebView;
enum class AmbientUiMode;

namespace ambient {

// Duration after which ambient mode is considered to have failed to start.
// See summary in histograms.xml for why 15 seconds is used.
constexpr base::TimeDelta kMetricsStartupTimeMax = base::Seconds(15);

ASH_EXPORT AmbientModePhotoSource
AmbientSettingsToPhotoSource(const AmbientSettings& settings);

ASH_EXPORT void RecordAmbientModeActivation(AmbientUiMode ui_mode,
                                            bool tablet_mode);

ASH_EXPORT void RecordAmbientModeTimeElapsed(
    base::TimeDelta time_delta,
    bool tablet_mode,
    const AmbientUiSettings& ui_settings);

ASH_EXPORT void RecordAmbientModeTotalNumberOfAlbums(int num_albums);

ASH_EXPORT void RecordAmbientModeSelectedNumberOfAlbums(int num_albums);

ASH_EXPORT void RecordAmbientModeAnimationSmoothness(
    int smoothness,
    const AmbientUiSettings& ui_settings);

ASH_EXPORT void RecordAmbientModePhotoOrientationMatch(
    int percentage_match,
    const AmbientUiSettings& ui_settings);

ASH_EXPORT void RecordAmbientModeStartupTime(
    base::TimeDelta startup_time,
    const AmbientUiSettings& ui_settings);

ASH_EXPORT void RecordAmbientModeVideoSmoothness(
    AshWebView* web_view,
    const AmbientUiSettings& ui_settings);

// Records metrics that track the total usage of each orientation in ambient
// mode.
class ASH_EXPORT AmbientOrientationMetricsRecorder
    : public views::ViewObserver {
 public:
  AmbientOrientationMetricsRecorder(views::View* root_rendering_view,
                                    const AmbientUiSettings& ui_settings);
  AmbientOrientationMetricsRecorder(const AmbientOrientationMetricsRecorder&) =
      delete;
  AmbientOrientationMetricsRecorder& operator=(
      const AmbientOrientationMetricsRecorder&) = delete;
  ~AmbientOrientationMetricsRecorder() override;

 private:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void SaveCurrentOrientationDuration();

  const std::string settings_;
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

#endif  // ASH_AMBIENT_METRICS_AMBIENT_METRICS_H_
