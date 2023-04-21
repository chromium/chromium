// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_BEHAVIOR_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_BEHAVIOR_H_

#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_types.h"

namespace ash {

// Contains the cached capture mode configurations that will be used
// for configurations restoration when initiating the corresponding capture
// mode session.
struct CaptureModeSessionConfigs {
  CaptureModeType type;
  CaptureModeSource source;
  RecordingType recording_type;
  bool audio_on;
  bool demo_tools_enabled;
};

// Defines the interface for the capture mode behavior which will be implemented
// by `DefaultBehavior` and `ProjectorBehavior`. The `CaptureModeController`
// owns the instance of this interface.
class CaptureModeBehavior {
 public:
  CaptureModeBehavior(const CaptureModeBehavior&) = delete;
  CaptureModeBehavior& operator=(const CaptureModeBehavior&) = delete;
  virtual ~CaptureModeBehavior() = default;

  // Creates an instance of the `CaptureModeBehavior` given the `behavior_type`.
  static std::unique_ptr<CaptureModeBehavior> Create(
      BehaviorType behavior_type);

  const CaptureModeSessionConfigs& capture_mode_configs() const {
    return capture_mode_configs_;
  }

  virtual bool ShouldImageCaptureTypeBeAllowed() const;
  virtual bool ShouldVideoCaptureTypeBeAllowed() const;
  virtual bool ShouldFulscreenCaptureSourceBeAllowed() const;
  virtual bool ShouldRegionCaptureSourceBeAllowed() const;
  virtual bool ShouldWindowCaptureSourceBeAllowed() const;
  virtual bool ShouldAudioInputSettingsBeIncluded() const;
  virtual bool ShouldCameraSelectionSettingsBeIncluded() const;
  virtual bool ShouldDemoToolsSettingsBeIncluded() const;
  virtual bool ShouldSaveToSettingsBeIncluded() const;
  virtual bool ShouldGifBeSupported() const;
  virtual bool ShouldShowPreviewNotification() const;
  virtual bool ShouldSkipVideoRecordingCountDown() const;
  virtual bool ShouldCreateRecordingOverlayController() const;
  virtual bool ShouldShowUserNudge() const;
  virtual bool ShouldAutoSelectFirstCamera() const;

 protected:
  explicit CaptureModeBehavior(const CaptureModeSessionConfigs& configs);

  CaptureModeSessionConfigs capture_mode_configs_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_BEHAVIOR_H_
