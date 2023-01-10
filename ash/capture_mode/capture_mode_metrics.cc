// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_metrics.h"

#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

namespace {

constexpr char kEndRecordingReasonHistogramName[] =
    "Ash.CaptureModeController.EndRecordingReason";
constexpr char kBarButtonHistogramName[] =
    "Ash.CaptureModeController.BarButtons";
constexpr char kCaptureAudioOnHistogramName[] =
    "Ash.CaptureModeController.CaptureAudioOnMetric";
constexpr char kCaptureConfigurationHistogramName[] =
    "Ash.CaptureModeController.CaptureConfiguration";
constexpr char kCaptureRegionAdjustmentHistogramName[] =
    "Ash.CaptureModeController.CaptureRegionAdjusted";
constexpr char kConsecutiveScreenshotHistogramName[] =
    "Ash.CaptureModeController.ConsecutiveScreenshots";
constexpr char kDemoToolsEnabledOnRecordingStart[] =
    "Ash.CaptureModeController.DemoToolsEnabledOnRecordingStart";
constexpr char kEntryHistogramName[] = "Ash.CaptureModeController.EntryPoint";
constexpr char kQuickActionHistogramName[] =
    "Ash.CaptureModeController.QuickAction";
constexpr char kRecordTimeHistogramName[] =
    "Ash.CaptureModeController.ScreenRecordingLength";
constexpr char kSaveToLocationHistogramName[] =
    "Ash.CaptureModeController.SaveLocation";
constexpr char kScreenshotsPerDayHistogramName[] =
    "Ash.CaptureModeController.ScreenshotsPerDay";
constexpr char kScreenshotsPerWeekHistogramName[] =
    "Ash.CaptureModeController.ScreenshotsPerWeek";
constexpr char kSwitchesFromInitialModeHistogramName[] =
    "Ash.CaptureModeController.SwitchesFromInitialCaptureMode";
constexpr char kSwitchToDefaultFolderReasonHistogramName[] =
    "Ash.CaptureModeController.SwitchToDefaultReason";
constexpr char kProjectorCaptureConfigurationHistogramName[] =
    "Ash.CaptureModeController.Projector.CaptureConfiguration";
constexpr char kProjectorCaptureRegionAdjustmentHistogramName[] =
    "Ash.CaptureModeController.Projector.CaptureRegionAdjusted";
constexpr char kProjectorRecordTimeHistogramName[] =
    "Ash.CaptureModeController.Projector.ScreenRecordingLength";
constexpr char kRecordingStartsWithCamera[] =
    "Ash.CaptureModeController.RecordingStartsWithCamera";
constexpr char kProjectorDemoToolsEnabledOnRecordingStart[] =
    "Ash.CaptureModeController.Projector.DemoToolsEnabledOnRecordingStart";
constexpr char kProjectorRecordingStartsWithCamera[] =
    "Ash.CaptureModeController.Projector.RecordingStartsWithCamera";
constexpr char kCameraDisconnectionsDuringRecordings[] =
    "Ash.CaptureModeController.CameraDisconnectionsDuringRecordings";
constexpr char kCameraReconnectDuration[] =
    "Ash.CaptureModeController.CameraReconnectDuration";
constexpr char kRecordingCameraSizeOnStart[] =
    "Ash.CaptureModeController.RecordingCameraSizeOnStart";
constexpr char kRecordingCameraPositionOnStart[] =
    "Ash.CaptureModeController.RecordingCameraPositionOnStart";
constexpr char kNumberOfConnectedCameras[] =
    "Ash.CaptureModeController.NumberOfConnectedCameras";

}  // namespace

void RecordEndRecordingReason(EndRecordingReason reason) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kEndRecordingReasonHistogramName), reason);
}

void RecordCaptureModeBarButtonType(CaptureModeBarButtonType button_type) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kBarButtonHistogramName), button_type);
}

void RecordCaptureModeConfiguration(CaptureModeType type,
                                    CaptureModeSource source,
                                    bool audio_on,
                                    bool is_in_projector_mode) {
  const std::string histogram_name = GetCaptureModeHistogramName(
      is_in_projector_mode ? kProjectorCaptureConfigurationHistogramName
                           : kCaptureConfigurationHistogramName);

  base::UmaHistogramEnumeration(histogram_name, GetConfiguration(type, source));
  if (type == CaptureModeType::kVideo) {
    base::UmaHistogramBoolean(
        GetCaptureModeHistogramName(kCaptureAudioOnHistogramName), audio_on);
  }
}

void RecordCaptureModeEntryType(CaptureModeEntryType entry_type) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kEntryHistogramName), entry_type);
}

void RecordCaptureModeRecordTime(int64_t length_in_seconds,
                                 bool is_in_projector_mode) {
  const std::string histogram_name = GetCaptureModeHistogramName(
      is_in_projector_mode ? kProjectorRecordTimeHistogramName
                           : kRecordTimeHistogramName);

  // Use custom counts macro instead of custom times so we can record in
  // seconds instead of milliseconds. The max bucket is 3 hours.
  base::UmaHistogramCustomCounts(histogram_name, length_in_seconds,
                                 /*min=*/1,
                                 /*max=*/base::Hours(3).InSeconds(),
                                 /*bucket_count=*/50);
}

void RecordCaptureModeSwitchesFromInitialMode(bool switched) {
  base::UmaHistogramBoolean(kSwitchesFromInitialModeHistogramName, switched);
}

void RecordNumberOfCaptureRegionAdjustments(int num_adjustments,
                                            bool is_in_projector_mode) {
  const std::string histogram_name = GetCaptureModeHistogramName(
      is_in_projector_mode ? kProjectorCaptureRegionAdjustmentHistogramName
                           : kCaptureRegionAdjustmentHistogramName);

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
      GetCaptureModeHistogramName(kSaveToLocationHistogramName), save_location);
}

void RecordSwitchToDefaultFolderReason(
    CaptureModeSwitchToDefaultReason reason) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kSwitchToDefaultFolderReasonHistogramName),
      reason);
}

CaptureModeConfiguration GetConfiguration(CaptureModeType type,
                                          CaptureModeSource source) {
  switch (source) {
    case CaptureModeSource::kFullscreen:
      return type == CaptureModeType::kImage
                 ? CaptureModeConfiguration::kFullscreenScreenshot
                 : CaptureModeConfiguration::kFullscreenRecording;
    case CaptureModeSource::kRegion:
      return type == CaptureModeType::kImage
                 ? CaptureModeConfiguration::kRegionScreenshot
                 : CaptureModeConfiguration::kRegionRecording;
    case CaptureModeSource::kWindow:
      return type == CaptureModeType::kImage
                 ? CaptureModeConfiguration::kWindowScreenshot
                 : CaptureModeConfiguration::kWindowRecording;
  }
}

void RecordRecordingStartsWithCamera(bool starts_with_camera,
                                     bool is_in_projector_mode) {
  const std::string histogram_name = is_in_projector_mode
                                         ? kProjectorRecordingStartsWithCamera
                                         : kRecordingStartsWithCamera;
  base::UmaHistogramBoolean(GetCaptureModeHistogramName(histogram_name),
                            starts_with_camera);
}

void RecordCameraDisconnectionsDuringRecordings(int num_camera_disconnections) {
  base::UmaHistogramCounts100(
      GetCaptureModeHistogramName(kCameraDisconnectionsDuringRecordings),
      num_camera_disconnections);
}

void RecordNumberOfConnectedCameras(int num_camera_connected) {
  base::UmaHistogramCounts100(kNumberOfConnectedCameras, num_camera_connected);
}

void RecordCameraReconnectDuration(int length_in_seconds,
                                   int grace_period_in_seconds) {
  base::UmaHistogramCustomCounts(
      GetCaptureModeHistogramName(kCameraReconnectDuration), length_in_seconds,
      0, grace_period_in_seconds, grace_period_in_seconds);
}

void RecordCameraSizeOnStart(CaptureModeCameraSize camera_size) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kRecordingCameraSizeOnStart), camera_size);
}

void RecordCameraPositionOnStart(CameraPreviewSnapPosition camera_position) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kRecordingCameraPositionOnStart),
      camera_position);
}

void RecordRecordingStartsWithDemoTools(bool demo_tools_enabled,
                                        bool is_in_projector_mode) {
  const std::string histogram_name =
      is_in_projector_mode ? kProjectorDemoToolsEnabledOnRecordingStart
                           : kDemoToolsEnabledOnRecordingStart;
  base::UmaHistogramBoolean(GetCaptureModeHistogramName(histogram_name),
                            demo_tools_enabled);
}

std::string GetCaptureModeHistogramName(std::string prefix) {
  prefix.append(Shell::Get()->IsInTabletMode() ? ".TabletMode"
                                               : ".ClamshellMode");
  return prefix;
}

}  // namespace ash
