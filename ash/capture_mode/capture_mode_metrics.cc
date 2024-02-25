// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_metrics.h"

#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace ash {

namespace {

constexpr char kCaptureModeMetricCommonPrefix[] = "Ash.CaptureModeController.";

constexpr char kEndRecordingReasonHistogramRootWord[] = "EndRecordingReason";
constexpr char kBarButtonHistogramRootWord[] = "BarButtons";
constexpr char kCaptureAudioRecordingModeHistogramRootWord[] =
    "AudioRecordingMode";
constexpr char kCaptureConfigurationHistogramRootWord[] =
    "CaptureConfiguration";
constexpr char kCaptureRegionAdjustmentHistogramRootWord[] =
    "CaptureRegionAdjusted";
constexpr char kDemoToolsEnabledOnRecordingStartRootWord[] =
    "DemoToolsEnabledOnRecordingStart";
constexpr char kEntryPointHistogramRootWord[] = "EntryPoint";
constexpr char kRecordingDurationHistogramRootWord[] = "ScreenRecordingLength";
constexpr char kGifRecordingDurationHistogramRootWord[] = "GIFRecordingLength";
constexpr char kGifRecordingRegionToScreenRatioHistogramRootWord[] =
    "GIFRecordingRegionToScreenRatio";
constexpr char kSaveToLocationHistogramRootWord[] = "SaveLocation";
constexpr char kSwitchToDefaultFolderReasonHistogramRootWord[] =
    "SwitchToDefaultReason";
constexpr char kRecordingStartsWithCameraRootWord[] =
    "RecordingStartsWithCamera";
constexpr char kCameraDisconnectionsDuringRecordingsRootWord[] =
    "CameraDisconnectionsDuringRecordings";
constexpr char kCameraReconnectDurationRootWord[] = "CameraReconnectDuration";
constexpr char kRecordingCameraSizeOnStartRootWord[] =
    "RecordingCameraSizeOnStart";
constexpr char kRecordingCameraPositionOnStartRootWord[] =
    "RecordingCameraPositionOnStart";
constexpr char kGifRecordingFileSizeRootWord[] = "GIFRecordingFileSize";
constexpr char kScreenRecordingFileSizeRootWord[] = "ScreenRecordingFileSize";
constexpr char kNumberOfConnectedCamerasRootWord[] = "NumberOfConnectedCameras";
constexpr char kConsecutiveScreenshotRootWord[] = "ConsecutiveScreenshots";
constexpr char kQuickActionRootWord[] = "QuickAction";
constexpr char kScreenshotsPerDayRootWord[] = "ScreenshotsPerDay";
constexpr char kScreenshotsPerWeekRootWord[] = "ScreenshotsPerWeek";
constexpr char kSwitchesFromInitialModeRootWord[] =
    "SwitchesFromInitialCaptureMode";

void RecordCaptureModeRecordingDurationInternal(
    const std::string& histogram_name,
    base::TimeDelta recording_duration) {
  // Use the custom counts function instead of custom times so we can record in
  // seconds instead of milliseconds. The max bucket is 3 hours.
  base::UmaHistogramCustomCounts(histogram_name, recording_duration.InSeconds(),
                                 /*min=*/1,
                                 /*exclusive_max=*/base::Hours(3).InSeconds(),
                                 /*buckets=*/50);
}

// Records capture mode education nudge actions, if the corresponding nudges
// were shown within a particular timeframe or the current session.
void MaybeRecordCaptureModeEducationNudgeActions() {
  // Nudge action metrics are only recorded if the corresponding nudge was
  // shown, so we can trigger all three arms here.
  auto* nudge_manager = AnchoredNudgeManager::Get();
  nudge_manager->MaybeRecordNudgeAction(
      NudgeCatalogName::kCaptureModeEducationShortcutNudge);
  nudge_manager->MaybeRecordNudgeAction(
      NudgeCatalogName::kCaptureModeEducationShortcutTutorial);
  nudge_manager->MaybeRecordNudgeAction(
      NudgeCatalogName::kCaptureModeEducationQuickSettingsNudge);
}

}  // namespace

void RecordEndRecordingReason(EndRecordingReason reason) {
  base::UmaHistogramEnumeration(
      BuildHistogramName(kEndRecordingReasonHistogramRootWord,
                         /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/true),
      reason);
}

void RecordCaptureModeBarButtonType(CaptureModeBarButtonType button_type) {
  base::UmaHistogramEnumeration(
      BuildHistogramName(kBarButtonHistogramRootWord, /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/true),
      button_type);
}

void RecordCaptureModeConfiguration(CaptureModeType type,
                                    CaptureModeSource source,
                                    RecordingType recording_type,
                                    AudioRecordingMode audio_mode,
                                    const CaptureModeBehavior* behavior) {
  std::string configuration_histogram_name =
      BuildHistogramName(kCaptureConfigurationHistogramRootWord, behavior,
                         /*append_ui_mode_suffix=*/true);
  base::UmaHistogramEnumeration(configuration_histogram_name,
                                GetConfiguration(type, source, recording_type));
  if (type == CaptureModeType::kVideo &&
      recording_type != RecordingType::kGif) {
    base::UmaHistogramEnumeration(
        BuildHistogramName(kCaptureAudioRecordingModeHistogramRootWord,
                           behavior,
                           /*append_ui_mode_suffix=*/true),
        audio_mode);
  }
}

void RecordGifRegionToScreenRatio(float ratio_percent) {
  base::UmaHistogramPercentage(
      BuildHistogramName(kGifRecordingRegionToScreenRatioHistogramRootWord,
                         /*behavior=*/nullptr, /*append_ui_mode_suffix=*/true),
      ratio_percent);
}

void RecordCaptureModeEntryType(CaptureModeEntryType entry_type) {
  base::UmaHistogramEnumeration(
      BuildHistogramName(kEntryPointHistogramRootWord, /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/true),
      entry_type);

  MaybeRecordCaptureModeEducationNudgeActions();
}

void RecordCaptureModeRecordingDuration(base::TimeDelta recording_duration,
                                        const CaptureModeBehavior* behavior,
                                        bool is_gif) {
  RecordCaptureModeRecordingDurationInternal(
      BuildHistogramName(!behavior->ShouldGifBeSupported() || !is_gif
                             ? kRecordingDurationHistogramRootWord
                             : kGifRecordingDurationHistogramRootWord,
                         behavior,
                         /*append_ui_mode_suffix=*/true),
      recording_duration);
}

void RecordVideoFileSizeKB(bool is_gif,
                           const CaptureModeBehavior* behavior,
                           int size_in_kb) {
  if (!Shell::HasInstance()) {
    // This function can be called asynchronously after the `Shell` instance had
    // already been destroyed.
    return;
  }

  if (size_in_kb < 0) {
    LOG(ERROR) << "Failed to calculate the video file size. Is GIF: " << is_gif;
    return;
  }

  base::UmaHistogramMemoryKB(
      BuildHistogramName(is_gif ? kGifRecordingFileSizeRootWord
                                : kScreenRecordingFileSizeRootWord,
                         behavior, /*append_ui_mode_suffix=*/true),
      size_in_kb);
}

void RecordCaptureModeSwitchesFromInitialMode(bool switched) {
  base::UmaHistogramBoolean(
      BuildHistogramName(kSwitchesFromInitialModeRootWord, /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/false),
      switched);
}

void RecordNumberOfCaptureRegionAdjustments(
    int num_adjustments,
    const CaptureModeBehavior* behavior) {
  base::UmaHistogramCounts100(
      BuildHistogramName(kCaptureRegionAdjustmentHistogramRootWord, behavior,
                         /*append_ui_mode_suffix=*/true),
      num_adjustments);
}

void RecordNumberOfConsecutiveScreenshots(int num_consecutive_screenshots) {
  if (num_consecutive_screenshots > 1) {
    base::UmaHistogramCounts100(
        BuildHistogramName(kConsecutiveScreenshotRootWord, /*behavior=*/nullptr,
                           /*append_ui_mode_suffix=*/false),
        num_consecutive_screenshots);
  }
}

void RecordNumberOfScreenshotsTakenInLastDay(
    int num_screenshots_taken_in_last_day) {
  base::UmaHistogramCounts100(
      BuildHistogramName(kScreenshotsPerDayRootWord, /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/false),
      num_screenshots_taken_in_last_day);
}

void RecordNumberOfScreenshotsTakenInLastWeek(
    int num_screenshots_taken_in_last_week) {
  base::UmaHistogramCounts100(
      BuildHistogramName(kScreenshotsPerWeekRootWord, /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/false),
      num_screenshots_taken_in_last_week);
}

void RecordScreenshotNotificationQuickAction(CaptureQuickAction action) {
  base::UmaHistogramEnumeration(
      BuildHistogramName(kQuickActionRootWord, /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/false),
      action);
}

void RecordSaveToLocation(CaptureModeSaveToLocation save_location,
                          const CaptureModeBehavior* behavior) {
  // Save-to location metrics should not be recorded for the
  // projector-inititated capture mode session.
  const CaptureModeBehavior* modified_behavior =
      behavior->behavior_type() == BehaviorType::kProjector ? nullptr
                                                            : behavior;
  base::UmaHistogramEnumeration(
      BuildHistogramName(kSaveToLocationHistogramRootWord, modified_behavior,
                         /*append_ui_mode_suffix=*/true),
      save_location);
}

void RecordSwitchToDefaultFolderReason(
    CaptureModeSwitchToDefaultReason reason) {
  base::UmaHistogramEnumeration(
      BuildHistogramName(kSwitchToDefaultFolderReasonHistogramRootWord,
                         /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/true),
      reason);
}

CaptureModeConfiguration GetConfiguration(CaptureModeType type,
                                          CaptureModeSource source,
                                          RecordingType recording_type) {
  switch (source) {
    case CaptureModeSource::kFullscreen:
      return type == CaptureModeType::kImage
                 ? CaptureModeConfiguration::kFullscreenScreenshot
                 : CaptureModeConfiguration::kFullscreenRecording;
    case CaptureModeSource::kRegion:
      if (type == CaptureModeType::kImage) {
        return CaptureModeConfiguration::kRegionScreenshot;
      }

      return recording_type == RecordingType::kGif
                 ? CaptureModeConfiguration::kRegionGifRecording
                 : CaptureModeConfiguration::kRegionRecording;
    case CaptureModeSource::kWindow:
      return type == CaptureModeType::kImage
                 ? CaptureModeConfiguration::kWindowScreenshot
                 : CaptureModeConfiguration::kWindowRecording;
  }
}

void RecordRecordingStartsWithCamera(bool starts_with_camera,
                                     const CaptureModeBehavior* behavior) {
  base::UmaHistogramBoolean(
      BuildHistogramName(kRecordingStartsWithCameraRootWord, behavior,
                         /*append_ui_mode_suffix=*/true),
      starts_with_camera);
}

void RecordCameraDisconnectionsDuringRecordings(int num_camera_disconnections) {
  base::UmaHistogramCounts100(
      BuildHistogramName(kCameraDisconnectionsDuringRecordingsRootWord,
                         /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/true),
      num_camera_disconnections);
}

void RecordNumberOfConnectedCameras(int num_camera_connected) {
  base::UmaHistogramCounts100(
      BuildHistogramName(kNumberOfConnectedCamerasRootWord,
                         /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/false),
      num_camera_connected);
}

void RecordCameraReconnectDuration(int length_in_seconds,
                                   int grace_period_in_seconds) {
  base::UmaHistogramCustomCounts(
      BuildHistogramName(kCameraReconnectDurationRootWord, nullptr,
                         /*append_ui_mode_suffix=*/true),
      length_in_seconds, 0, grace_period_in_seconds, grace_period_in_seconds);
}

void RecordCameraSizeOnStart(CaptureModeCameraSize camera_size) {
  base::UmaHistogramEnumeration(
      BuildHistogramName(kRecordingCameraSizeOnStartRootWord,
                         /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/true),
      camera_size);
}

void RecordCameraPositionOnStart(CameraPreviewSnapPosition camera_position) {
  base::UmaHistogramEnumeration(
      BuildHistogramName(kRecordingCameraPositionOnStartRootWord,
                         /*behavior=*/nullptr,
                         /*append_ui_mode_suffix=*/true),
      camera_position);
}

void RecordRecordingStartsWithDemoTools(bool demo_tools_enabled,
                                        const CaptureModeBehavior* behavior) {
  base::UmaHistogramBoolean(
      BuildHistogramName(kDemoToolsEnabledOnRecordingStartRootWord, behavior,
                         /*append_ui_mode_suffix=*/true),
      demo_tools_enabled);
}

std::string BuildHistogramName(const char* const root_word,
                               const CaptureModeBehavior* behavior,
                               bool append_ui_mode_suffix) {
  std::string histogram_name(kCaptureModeMetricCommonPrefix);
  if (behavior) {
    histogram_name.append(behavior->GetClientMetricComponent());
  }
  histogram_name.append(root_word);
  if (append_ui_mode_suffix) {
    histogram_name.append(Shell::Get()->IsInTabletMode() ? ".TabletMode"
                                                         : ".ClamshellMode");
  }
  return histogram_name;
}

}  // namespace ash
