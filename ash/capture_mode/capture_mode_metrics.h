// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_

#include <stdint.h>
#include <string>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"

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
  kMaxValue = kLowDriveFsQuota,
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
  kMaxValue = kWindowRecording,
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
  kMaxValue = kProjector,
};

// Enumeration of quick actions on screenshot notification. Note that these
// values are persisted to histograms so existing values should remain
// unchanged and new values should be added to the end.
enum class CaptureQuickAction {
  kBacklight,
  kFiles,
  kDelete,
  kMaxValue = kDelete,
};

// Enumeration of user's selection on save-to locations. Note that these values
// are persisted to histograms so existing values should remain unchanged and
// new values should be added to the end.
enum class CaptureModeSaveToLocation {
  kDefault,
  kDrive,
  kDriveFolder,
  kCustomizedFolder,
  kMaxValue = kCustomizedFolder,
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
                                    bool audio_on,
                                    bool is_in_projector_mode);

// Records the method the user enters capture mode given by |entry_type|.
void RecordCaptureModeEntryType(CaptureModeEntryType entry_type);

// Records the length in seconds of a recording taken by capture mode.
void RecordCaptureModeRecordTime(int64_t length_in_seconds,
                                 bool is_in_projector_mode);

// Records if the user has switched modes during a capture session.
void RecordCaptureModeSwitchesFromInitialMode(bool switched);

// Records the number of times a user adjusts a capture region. This includes
// moving and resizing. The count is started when a user sets the capture source
// as a region. The count is recorded and reset when a user performs a capture.
// The count is just reset when a user selects a new region or the user switches
// capture sources.
void RecordNumberOfCaptureRegionAdjustments(int num_adjustments,
                                            bool is_in_projector_mode);

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
void RecordSaveToLocation(CaptureModeSaveToLocation save_location);

// Records the `reason` for which the capture folder is switched to default
// downloads folder.
void RecordSwitchToDefaultFolderReason(CaptureModeSwitchToDefaultReason reason);

// Maps given `type` and `source` to CaptureModeConfiguration enum.
ASH_EXPORT CaptureModeConfiguration GetConfiguration(CaptureModeType type,
                                                     CaptureModeSource source);
// Records how often recording starts with a camera on.
void RecordRecordingStartsWithCamera(bool starts_with_camera,
                                     bool is_in_projector_mode);

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
                                        bool is_in_projector_mode);

// Appends the proper suffix to `prefix` based on whether the user is in tablet
// mode or not.
ASH_EXPORT std::string GetCaptureModeHistogramName(std::string prefix);

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_
