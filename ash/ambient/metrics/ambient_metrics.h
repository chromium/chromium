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
#include "base/time/time.h"

namespace ash {

struct AmbientSettings;
class AmbientUiSettings;
class AshWebView;

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

ASH_EXPORT void RecordAmbientModeTimeElapsed(base::TimeDelta time_delta,
                                             bool tablet_mode);

ASH_EXPORT void RecordAmbientModeTopicSource(
    ash::personalization_app::mojom::TopicSource topic_source);

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

}  // namespace ambient
}  // namespace ash

#endif  // ASH_AMBIENT_METRICS_AMBIENT_METRICS_H_
