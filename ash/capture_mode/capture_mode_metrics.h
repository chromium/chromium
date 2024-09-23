// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_

#include <stdint.h>
#include <string>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_behavior.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/time/time.h"

namespace ash {

// Enumeration of the reasons that lead to ending the screen recording.
// Note that these values are persisted to histograms so existing values should
// remain unchanged and new values should be added to the end.
enum class EndRecordingReason {
  kStopRecordingButton,
  kDisplayOrWindowClosing,
  kActiveUserChange,
  kSessionBlocked,
  kShuttingDown,
  kImminentSuspend,
  kRecordingServiceDisconnected,
  kFileIoError,
  kDlpInterruption,
  kLowDiskSpace,
  kHdcpInterruption,
  kServiceClosing,
  kVizVideoCaptureDisconnected,
  kAudioEncoderInitializationFailure,
  kVideoEncoderInitializationFailure,
  kAudioEncodingError,
  kVideoEncodingError,
  kProjectorTranscriptionError,
  kLowDriveFsQuota,
  kVideoEncoderReconfigurationFailure,
  kKeyboardShortcut,
  kGameDashboardStopRecordingButton,
  kGameToolbarStopRecordingButton,
  kGameDashboardTabletMode,
  kMaxValue = kGameDashboardTabletMode,
};

// Enumeration of capture bar buttons that can be pressed while in capture mode.
// Note that these values are persisted to histograms so existing values should
// remain unchanged and new values should be added to the end.
enum class CaptureModeBarButtonType {
  kScreenCapture,
  kScreenRecord,
  kFull,
  kRegion,
  kWindow,
  kExit,
  kMaxValue = kExit,
};

// Enumeration of the various configurations a user can have while in capture
// mode. Note that these values are persisted to histograms so existing values
// should remain unchanged and new values should be added to the end.
enum class CaptureModeConfiguration {
  kFullscreenScreenshot,
  kRegionScreenshot,
  kWindowScreenshot,
  kFullscreenRecording,
  kRegionRecording,
  kWindowRecording,
  kRegionGifRecording,
  kMaxValue = kRegionGifRecording,
};

// Enumeration of actions that can be taken to enter capture mode. Note that
// these values are persisted to histograms so existing values should remain
// unchanged and new values should be added to the end.
enum class CaptureModeEntryType {
  kAccelTakePartialScreenshot,
  kAccelTakeWindowScreenshot,
  kQuickSettings,
  kStylusPalette,
  kPowerMenu,
  kSnipKey,
  kCaptureAllDisplays,
  kProjector,
  kCaptureGivenWindow,
  kGameDashboard,
  kSunfish,
  kMaxValue = kSunfish,
};

// Enumeration of quick actions on screenshot notification. Note that these
// values are persisted to histograms so existing values should remain
// unchanged and new values should be added to the end.
enum class CaptureQuickAction {
  kBacklight,
  kFiles,
  kDelete,
  kOpenDefault,
  kMaxValue = kOpenDefault,
};

// Enumeration of user's selection on save-to locations. Note that these values
// are persisted to histograms so existing values should remain unchanged and
// new values should be added to the end.
enum class CaptureModeSaveToLocation {
  kDefault,
  kDrive,
  kDriveFolder,
  kCustomizedFolder,
  kOneDrive,
  kOneDriveFolder,
  kMaxValue = kOneDriveFolder,
};

// Enumeration of reasons for which the capture folder is switched to default
// downloads folder. Note that these values are persisted to histograms so
// existing values should remain unchanged and new values should be added to the
// end.
enum class CaptureModeSwitchToDefaultReason {
  kFolderUnavailable,
  kUserSelectedFromFolderSelectionDialog,
  kUserSelectedFromSettingsMenu,
  kMaxValue = kUserSelectedFromSettingsMenu,
};

// Enumeration of the camera preview size. Note that these values are persisted
// to histograms so existing values should remain unchanged and new values
// should be added to the end.
enum class CaptureModeCameraSize {
  kExpanded,
  kCollapsed,
  kMaxValue = kCollapsed,
};

// Records the `reason` for which screen recording was ended.
void RecordEndRecordingReason(EndRecordingReason reason);

// Records capture mode bar button presses given by `button_type`.
void RecordCaptureModeBarButtonType(CaptureModeBarButtonType button_type);

// Records a user's configuration when they perform a capture.
void RecordCaptureModeConfiguration(CaptureModeType type,
                                    CaptureModeSource source,
                                    RecordingType recording_type,
                                    AudioRecordingMode audio_mode,
                                    const CaptureModeBehavior* behavior);

// Records the percent ratio between the area of the user selected region to be
// recorded as GIF to the area of the entire screen.
void RecordGifRegionToScreenRatio(float ratio_percent);

// Records the method the user enters capture mode given by `entry_type`.
void RecordCaptureModeEntryType(CaptureModeEntryType entry_type);

// Records the duration of a recording taken by capture mode.
void RecordCaptureModeRecordingDuration(base::TimeDelta recording_duration,
                                        const CaptureModeBehavior* behavior,
                                        bool is_gif);

// Records the given video file `size_in_kb`. The used histogram will depend on
// whether this video file was GIF or WebM.
void RecordVideoFileSizeKB(bool is_gif,
                           const CaptureModeBehavior* behavior,
                           int size_in_kb);

// Records if the user has switched modes during a capture session.
void RecordCaptureModeSwitchesFromInitialMode(bool switched);

// Records the number of times a user adjusts a capture region. This includes
// moving and resizing. The count is started when a user sets the capture source
// as a region. The count is recorded and reset when a user performs a capture.
// The count is just reset when a user selects a new region or the user switches
// capture sources.
void RecordNumberOfCaptureRegionAdjustments(
    int num_adjustments,
    const CaptureModeBehavior* behavior);

// Records the number of times a user consecutively screenshots. Only records a
// sample if `num_consecutive_screenshots` is greater than 1.
void RecordNumberOfConsecutiveScreenshots(int num_consecutive_screenshots);

// Records the number of screenshots taken. This metric is meant to be a rough
// approximation so its counts are not persisted across crashes, restarts or
// sessions.
void RecordNumberOfScreenshotsTakenInLastDay(
    int num_screenshots_taken_in_last_day);
void RecordNumberOfScreenshotsTakenInLastWeek(
    int num_screenshots_taken_in_last_week);

// Records the action taken on screen notification.
void RecordScreenshotNotificationQuickAction(CaptureQuickAction action);

// Records the location where screen capture is saved.
void RecordSaveToLocation(CaptureModeSaveToLocation save_location,
                          const CaptureModeBehavior* behavior);

// Records the `reason` for which the capture folder is switched to default
// downloads folder.
void RecordSwitchToDefaultFolderReason(CaptureModeSwitchToDefaultReason reason);

// Maps given `type`, `source` and `recording_type` to CaptureModeConfiguration
// enum.
ASH_EXPORT CaptureModeConfiguration
GetConfiguration(CaptureModeType type,
                 CaptureModeSource source,
                 RecordingType recording_type);
// Records how often recording starts with a camera on.
void RecordRecordingStartsWithCamera(bool starts_with_camera,
                                     const CaptureModeBehavior* behavior);

// Records the number of camera disconnections during recording.
void RecordCameraDisconnectionsDuringRecordings(int num_camera_disconnections);

// Records the given `num_camera_connected`.
void RecordNumberOfConnectedCameras(int num_camera_connected);

// Records the duration of camera becoming available again after camera
// disconnection.
void RecordCameraReconnectDuration(int length_in_seconds,
                                   int grace_period_in_seconds);

// Records the camera size when recording starts.
void RecordCameraSizeOnStart(CaptureModeCameraSize camera_size);

// Records the camera position when recording starts.
void RecordCameraPositionOnStart(CameraPreviewSnapPosition camera_position);

// Records how often recording starts with demo tools feature enabled.
void RecordRecordingStartsWithDemoTools(bool demo_tools_enabled,
                                        const CaptureModeBehavior* behavior);

// Prepends the common prefix to the `root_word` and optionally inserts the
// client's metric component (as specified by the given `behavior`) or appends
// the ui mode suffix to build the full histogram name.
ASH_EXPORT std::string BuildHistogramName(const char* const root_word,
                                          const CaptureModeBehavior* behavior,
                                          bool append_ui_mode_suffix);

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_
