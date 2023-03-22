// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_metrics.h"

#include "ash/capture_mode/capture_mode_types.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace ash {

namespace {

constexpr char kEndRecordingReasonHistogramPrefix[] =
    "Ash.CaptureModeController.EndRecordingReason";
constexpr char kBarButtonHistogramPrefix[] =
    "Ash.CaptureModeController.BarButtons";
constexpr char kCaptureAudioOnHistogramPrefix[] =
    "Ash.CaptureModeController.CaptureAudioOnMetric";
constexpr char kCaptureConfigurationHistogramPrefix[] =
    "Ash.CaptureModeController.CaptureConfiguration";
constexpr char kCaptureRegionAdjustmentHistogramPrefix[] =
    "Ash.CaptureModeController.CaptureRegionAdjusted";
constexpr char kDemoToolsEnabledOnRecordingStartPrefix[] =
    "Ash.CaptureModeController.DemoToolsEnabledOnRecordingStart";
constexpr char kEntryHistogramPrefix[] = "Ash.CaptureModeController.EntryPoint";
constexpr char kRecordTimeHistogramPrefix[] =
    "Ash.CaptureModeController.ScreenRecordingLength";
constexpr char kGifRecordingTimeHistogramPrefix[] =
    "Ash.CaptureModeController.GIFRecordingLength";
constexpr char kGifRecordingRegionToScreenRatioHistogramPrefix[] =
    "Ash.CaptureModeController.GIFRecordingRegionToScreenRatio";
constexpr char kSaveToLocationHistogramPrefix[] =
    "Ash.CaptureModeController.SaveLocation";
constexpr char kSwitchToDefaultFolderReasonHistogramPrefix[] =
    "Ash.CaptureModeController.SwitchToDefaultReason";
constexpr char kProjectorCaptureConfigurationHistogramPrefix[] =
    "Ash.CaptureModeController.Projector.CaptureConfiguration";
constexpr char kProjectorCaptureRegionAdjustmentHistogramPrefix[] =
    "Ash.CaptureModeController.Projector.CaptureRegionAdjusted";
constexpr char kProjectorRecordTimeHistogramPrefix[] =
    "Ash.CaptureModeController.Projector.ScreenRecordingLength";
constexpr char kRecordingStartsWithCameraPrefix[] =
    "Ash.CaptureModeController.RecordingStartsWithCamera";
constexpr char kProjectorDemoToolsEnabledOnRecordingStartPrefix[] =
    "Ash.CaptureModeController.Projector.DemoToolsEnabledOnRecordingStart";
constexpr char kProjectorRecordingStartsWithCameraPrefix[] =
    "Ash.CaptureModeController.Projector.RecordingStartsWithCamera";
constexpr char kCameraDisconnectionsDuringRecordingsPrefix[] =
    "Ash.CaptureModeController.CameraDisconnectionsDuringRecordings";
constexpr char kCameraReconnectDurationPrefix[] =
    "Ash.CaptureModeController.CameraReconnectDuration";
constexpr char kRecordingCameraSizeOnStartPrefix[] =
    "Ash.CaptureModeController.RecordingCameraSizeOnStart";
constexpr char kRecordingCameraPositionOnStartPrefix[] =
    "Ash.CaptureModeController.RecordingCameraPositionOnStart";
constexpr char kGifRecordingFileSizePrefix[] =
    "Ash.CaptureModeController.GIFRecordingFileSize";
constexpr char kScreenRecordingFileSizePrefix[] =
    "Ash.CaptureModeController.ScreenRecordingFileSize";
constexpr char kNumberOfConnectedCameras[] =
    "Ash.CaptureModeController.NumberOfConnectedCameras";
constexpr char kConsecutiveScreenshotHistogramName[] =
    "Ash.CaptureModeController.ConsecutiveScreenshots";
constexpr char kQuickActionHistogramName[] =
    "Ash.CaptureModeController.QuickAction";
constexpr char kScreenshotsPerDayHistogramName[] =
    "Ash.CaptureModeController.ScreenshotsPerDay";
constexpr char kScreenshotsPerWeekHistogramName[] =
    "Ash.CaptureModeController.ScreenshotsPerWeek";
constexpr char kSwitchesFromInitialModeHistogramName[] =
    "Ash.CaptureModeController.SwitchesFromInitialCaptureMode";

void RecordCaptureModeRecordTimeInternal(const std::string& histogram_prefix,
                                         base::TimeDelta recording_duration) {
  // Use the custom counts function instead of custom times so we can record in
  // seconds instead of milliseconds. The max bucket is 3 hours.
  base::UmaHistogramCustomCounts(GetCaptureModeHistogramName(histogram_prefix),
                                 recording_duration.InSeconds(),
                                 /*min=*/1,
                                 /*exclusive_max=*/base::Hours(3).InSeconds(),
                                 /*buckets=*/50);
}

}  // namespace

void RecordEndRecordingReason(EndRecordingReason reason) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kEndRecordingReasonHistogramPrefix), reason);
}

void RecordCaptureModeBarButtonType(CaptureModeBarButtonType button_type) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kBarButtonHistogramPrefix), button_type);
}

void RecordCaptureModeConfiguration(CaptureModeType type,
                                    CaptureModeSource source,
                                    RecordingType recording_type,
                                    bool audio_on,
                                    bool is_in_projector_mode) {
  const std::string histogram_name = GetCaptureModeHistogramName(
      is_in_projector_mode ? kProjectorCaptureConfigurationHistogramPrefix
                           : kCaptureConfigurationHistogramPrefix);

  base::UmaHistogramEnumeration(histogram_name,
                                GetConfiguration(type, source, recording_type));
  if (type == CaptureModeType::kVideo &&
      recording_type != RecordingType::kGif) {
    base::UmaHistogramBoolean(
        GetCaptureModeHistogramName(kCaptureAudioOnHistogramPrefix), audio_on);
  }
}

void RecordGifRegionToScreenRatio(float ratio_percent) {
  base::UmaHistogramPercentage(
      GetCaptureModeHistogramName(
          kGifRecordingRegionToScreenRatioHistogramPrefix),
      ratio_percent);
}

void RecordCaptureModeEntryType(CaptureModeEntryType entry_type) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kEntryHistogramPrefix), entry_type);
}

void RecordCaptureModeRecordTime(base::TimeDelta recording_duration,
                                 bool is_in_projector_mode,
                                 bool is_gif) {
  if (is_in_projector_mode) {
    DCHECK(!is_gif);
    RecordCaptureModeRecordTimeInternal(kProjectorRecordTimeHistogramPrefix,
                                        recording_duration);
    return;
  }

  RecordCaptureModeRecordTimeInternal(
      is_gif ? kGifRecordingTimeHistogramPrefix : kRecordTimeHistogramPrefix,
      recording_duration);
}

void RecordVideoFileSizeKB(bool is_gif, int size_in_kb) {
  if (size_in_kb < 0) {
    LOG(ERROR) << "Failed to calculate the video file size. Is GIF: " << is_gif;
    return;
  }

  base::UmaHistogramMemoryKB(
      GetCaptureModeHistogramName(is_gif ? kGifRecordingFileSizePrefix
                                         : kScreenRecordingFileSizePrefix),
      size_in_kb);
}

void RecordCaptureModeSwitchesFromInitialMode(bool switched) {
  base::UmaHistogramBoolean(kSwitchesFromInitialModeHistogramName, switched);
}

void RecordNumberOfCaptureRegionAdjustments(int num_adjustments,
                                            bool is_in_projector_mode) {
  const std::string histogram_name = GetCaptureModeHistogramName(
      is_in_projector_mode ? kProjectorCaptureRegionAdjustmentHistogramPrefix
                           : kCaptureRegionAdjustmentHistogramPrefix);

  base::UmaHistogramCounts100(histogram_name, num_adjustments);
}

void RecordNumberOfConsecutiveScreenshots(int num_consecutive_screenshots) {
  if (num_consecutive_screenshots > 1) {
    base::UmaHistogramCounts100(kConsecutiveScreenshotHistogramName,
                                num_consecutive_screenshots);
  }
}

void RecordNumberOfScreenshotsTakenInLastDay(
    int num_screenshots_taken_in_last_day) {
  base::UmaHistogramCounts100(kScreenshotsPerDayHistogramName,
                              num_screenshots_taken_in_last_day);
}

void RecordNumberOfScreenshotsTakenInLastWeek(
    int num_screenshots_taken_in_last_week) {
  base::UmaHistogramCounts100(kScreenshotsPerWeekHistogramName,
                              num_screenshots_taken_in_last_week);
}

void RecordScreenshotNotificationQuickAction(CaptureQuickAction action) {
  base::UmaHistogramEnumeration(kQuickActionHistogramName, action);
}

void RecordSaveToLocation(CaptureModeSaveToLocation save_location) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kSaveToLocationHistogramPrefix),
      save_location);
}

void RecordSwitchToDefaultFolderReason(
    CaptureModeSwitchToDefaultReason reason) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kSwitchToDefaultFolderReasonHistogramPrefix),
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
                                     bool is_in_projector_mode) {
  const std::string histogram_prefix =
      is_in_projector_mode ? kProjectorRecordingStartsWithCameraPrefix
                           : kRecordingStartsWithCameraPrefix;
  base::UmaHistogramBoolean(GetCaptureModeHistogramName(histogram_prefix),
                            starts_with_camera);
}

void RecordCameraDisconnectionsDuringRecordings(int num_camera_disconnections) {
  base::UmaHistogramCounts100(
      GetCaptureModeHistogramName(kCameraDisconnectionsDuringRecordingsPrefix),
      num_camera_disconnections);
}

void RecordNumberOfConnectedCameras(int num_camera_connected) {
  base::UmaHistogramCounts100(kNumberOfConnectedCameras, num_camera_connected);
}

void RecordCameraReconnectDuration(int length_in_seconds,
                                   int grace_period_in_seconds) {
  base::UmaHistogramCustomCounts(
      GetCaptureModeHistogramName(kCameraReconnectDurationPrefix),
      length_in_seconds, 0, grace_period_in_seconds, grace_period_in_seconds);
}

void RecordCameraSizeOnStart(CaptureModeCameraSize camera_size) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kRecordingCameraSizeOnStartPrefix),
      camera_size);
}

void RecordCameraPositionOnStart(CameraPreviewSnapPosition camera_position) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kRecordingCameraPositionOnStartPrefix),
      camera_position);
}

void RecordRecordingStartsWithDemoTools(bool demo_tools_enabled,
                                        bool is_in_projector_mode) {
  const std::string histogram_prefix =
      is_in_projector_mode ? kProjectorDemoToolsEnabledOnRecordingStartPrefix
                           : kDemoToolsEnabledOnRecordingStartPrefix;
  base::UmaHistogramBoolean(GetCaptureModeHistogramName(histogram_prefix),
                            demo_tools_enabled);
}

std::string GetCaptureModeHistogramName(std::string prefix) {
  prefix.append(Shell::Get()->IsInTabletMode() ? ".TabletMode"
                                               : ".ClamshellMode");
  return prefix;
}

}  // namespace ash
