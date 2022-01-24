// Copyright 2020 The Chromium Authors. All rights reserved.
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

// Appends the proper suffix to `prefix` based on whether the user is in tablet
// mode or not.
std::string GetCaptureModeHistogramName(std::string prefix) {
  prefix.append(Shell::Get()->IsInTabletMode() ? ".TabletMode"
                                               : ".ClamshellMode");
  return prefix;
}

// Maps given |type| and |source| to CaptureModeConfiguration enum.
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
    default:
      NOTREACHED();
      return CaptureModeConfiguration::kFullscreenScreenshot;
  }
}

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
                                    bool audio_on) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kCaptureConfigurationHistogramName),
      GetConfiguration(type, source));
  if (type == CaptureModeType::kVideo) {
    base::UmaHistogramBoolean(
        GetCaptureModeHistogramName(kCaptureAudioOnHistogramName), audio_on);
  }
}

void RecordCaptureModeEntryType(CaptureModeEntryType entry_type) {
  base::UmaHistogramEnumeration(
      GetCaptureModeHistogramName(kEntryHistogramName), entry_type);
}

void RecordCaptureModeRecordTime(int64_t length_in_seconds) {
  // Use custom counts macro instead of custom times so we can record in
  // seconds instead of milliseconds. The max bucket is 3 hours.
  base::UmaHistogramCustomCounts(kRecordTimeHistogramName, length_in_seconds,
                                 /*min=*/1,
                                 /*max=*/base::Hours(3).InSeconds(),
                                 /*bucket_count=*/50);
}

void RecordCaptureModeSwitchesFromInitialMode(bool switched) {
  base::UmaHistogramBoolean(kSwitchesFromInitialModeHistogramName, switched);
}

void RecordNumberOfCaptureRegionAdjustments(int num_adjustments) {
  base::UmaHistogramCounts100(
      GetCaptureModeHistogramName(kCaptureRegionAdjustmentHistogramName),
      num_adjustments);
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

}  // namespace ash
