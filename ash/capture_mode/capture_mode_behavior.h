// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_BEHAVIOR_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_BEHAVIOR_H_

#include <vector>

#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Contains the cached capture mode configurations that will be used for
// configurations restoration when initiating the corresponding capture mode
// session.
struct CaptureModeSessionConfigs {
  CaptureModeType type;
  CaptureModeSource source;
  RecordingType recording_type;
  AudioRecordingMode audio_recording_mode;
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

  // Called when this behavior becomes the active behavior of a newly created
  // capture session. Sub classes can choose to do any specific session
  // initialization that they need.
  virtual void AttachToSession();
  // Called when this behavior is no longer attached to an active capture mode
  // session, i.e. when its capture session ends and recording will not start,
  // or when its session ends to start recording right after recording begins.
  virtual void DetachFromSession();

  virtual bool ShouldImageCaptureTypeBeAllowed() const;
  virtual bool ShouldVideoCaptureTypeBeAllowed() const;
  virtual bool ShouldFulscreenCaptureSourceBeAllowed() const;
  virtual bool ShouldRegionCaptureSourceBeAllowed() const;
  virtual bool ShouldWindowCaptureSourceBeAllowed() const;
  // Returns true if the given `mode` is supported by this behavior.
  virtual bool SupportsAudioRecordingMode(AudioRecordingMode mode) const;
  virtual bool ShouldCameraSelectionSettingsBeIncluded() const;
  virtual bool ShouldDemoToolsSettingsBeIncluded() const;
  virtual bool ShouldSaveToSettingsBeIncluded() const;
  virtual bool ShouldGifBeSupported() const;
  virtual bool ShouldShowPreviewNotification() const;
  virtual bool ShouldSkipVideoRecordingCountDown() const;
  virtual bool ShouldCreateRecordingOverlayController() const;
  virtual bool ShouldShowUserNudge() const;
  virtual bool ShouldAutoSelectFirstCamera() const;
  virtual bool RequiresCaptureFolderCreation() const;
  // Returns the full path for the capture file. If the creation of the path
  // failed, the path provided will be empty.
  using OnCaptureFolderCreatedCallback =
      base::OnceCallback<void(const base::FilePath& capture_file_full_path)>;
  virtual void CreateCaptureFolder(OnCaptureFolderCreatedCallback callback);
  virtual std::vector<RecordingType> GetSupportedRecordingTypes() const;

 protected:
  explicit CaptureModeBehavior(const CaptureModeSessionConfigs& configs);

  // Capture mode session configs to be used for the current capture mode
  // session.
  CaptureModeSessionConfigs capture_mode_configs_;

  // Can be used to cache the old capture mode session configs before this
  // behavior is attached to a new session.
  absl::optional<CaptureModeSessionConfigs> cached_configs_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_BEHAVIOR_H_
