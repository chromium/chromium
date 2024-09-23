// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_METRICS_AMBIENT_METRICS_H_
#define ASH_AMBIENT_METRICS_AMBIENT_METRICS_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_mode_photo_source.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

struct AmbientSettings;
class AmbientUiSettings;
class AshWebView;
enum class AmbientUiMode;

namespace ambient {

// These values are persisted to UMA logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AmbientVideoSessionStatus {
  // Confirmed playback started successfully.
  kSuccess = 0,
  // Confirmed playback failed with a hard error.
  kFailed = 1,
  // User terminated ambient session while video was still loading. Unknown
  // whether playback would have ultimately succeeded or not. This should be
  // rare.
  kLoading = 2,
  kMaxValue = kLoading,
};

// Duration after which ambient mode is considered to have failed to start.
// See summary in histograms.xml for why 15 seconds is used.
constexpr base::TimeDelta kMetricsStartupTimeMax = base::Seconds(15);

// Must be kept in sync with the `AmbientVideoDlcInstallLabels` variants
// in tool/metrics/histograms/metadata/ash/histograms.xml.
//
// Install that happens when it's time to launch one of the video screen savers
// (on demand). If a "Background" installation succeeded in the past, the
// foreground installation will succeed and be a trivial operation.
inline constexpr char kAmbientVideoDlcForegroundLabel[] = "Foreground";

// Install that happens shortly after login. In most cases, this should occur
// before the screen saver is first launched into the foreground. If the
// background install fails, it's not user-facing and another attempt will be
// made with the "Foreground".
inline constexpr char kAmbientVideoDlcBackgroundLabel[] = "Background";

ASH_EXPORT AmbientModePhotoSource
AmbientSettingsToPhotoSource(const AmbientSettings& settings);

ASH_EXPORT void RecordAmbientModeActivation(AmbientUiMode ui_mode,
                                            bool tablet_mode);

ASH_EXPORT void RecordAmbientModeTimeElapsed(
    base::TimeDelta time_delta,
    bool tablet_mode,
    const AmbientUiSettings& ui_settings);

ASH_EXPORT void RecordAmbientModeTopicSource(
    ash::personalization_app::mojom::TopicSource topic_source);

ASH_EXPORT void RecordAmbientModeTotalNumberOfAlbums(int num_albums);

ASH_EXPORT void RecordAmbientModeSelectedNumberOfAlbums(int num_albums);

ASH_EXPORT void RecordAmbientModeAnimationSmoothness(
    int smoothness,
    const AmbientUiSettings& ui_settings);

ASH_EXPORT void RecordAmbientModeStartupTime(
    base::TimeDelta startup_time,
    const AmbientUiSettings& ui_settings);

ASH_EXPORT void GetAmbientModeVideoSessionStatus(
    AshWebView* web_view,
    base::OnceCallback<void(AmbientVideoSessionStatus)> completion_cb);

ASH_EXPORT void RecordAmbientModeVideoSessionStatus(
    AshWebView* web_view,
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
  std::optional<bool> current_orientation_is_portrait_;
  std::optional<base::ElapsedTimer> current_orientation_timer_;
  base::TimeDelta total_portrait_duration_;
  base::TimeDelta total_landscape_duration_;
};

}  // namespace ambient
}  // namespace ash

#endif  // ASH_AMBIENT_METRICS_AMBIENT_METRICS_H_
