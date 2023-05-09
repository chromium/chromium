// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_behavior.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/constants/ash_features.h"
#include "ash/projector/projector_controller_impl.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"

namespace ash {

namespace {

// Returns the current configs before been overwritten by the client-initiated
// capture mode session
CaptureModeSessionConfigs GetCaptureModeSessionConfigs() {
  CaptureModeController* controller = CaptureModeController::Get();
  CaptureModeSessionConfigs configs = CaptureModeSessionConfigs{
      controller->type(), controller->source(), controller->recording_type(),
      controller->audio_recording_mode(), controller->enable_demo_tools()};
  return configs;
}

void SetCaptureModeSessionConfigs(const CaptureModeSessionConfigs& configs) {
  CaptureModeController* controller = CaptureModeController::Get();
  controller->SetType(configs.type);
  controller->SetSource(configs.source);
  controller->SetRecordingType(configs.recording_type);
  controller->SetAudioRecordingMode(configs.audio_recording_mode);
  controller->EnableDemoTools(configs.demo_tools_enabled);
}

// -----------------------------------------------------------------------------
// DefaultBehavior:
// Implements the `CaptureModeBehavior` interface with behavior defined for the
// default capture mode.
class DefaultBehavior : public CaptureModeBehavior {
 public:
  DefaultBehavior()
      : CaptureModeBehavior({CaptureModeType::kImage,
                             CaptureModeSource::kRegion, RecordingType::kWebM,
                             AudioRecordingMode::kOff,
                             /*demo_tools_enabled=*/false}) {}

  DefaultBehavior(const DefaultBehavior&) = delete;
  DefaultBehavior& operator=(const DefaultBehavior&) = delete;
  ~DefaultBehavior() override = default;
};

// -----------------------------------------------------------------------------
// ProjectorBehavior:
// Implements the `CaptureModeBehavior` interface with behavior defined for the
// projector-initiated capture mode.
class ProjectorBehavior : public CaptureModeBehavior {
 public:
  ProjectorBehavior()
      : CaptureModeBehavior(
            {CaptureModeType::kVideo, CaptureModeSource::kFullscreen,
             RecordingType::kWebM, AudioRecordingMode::kMicrophone,
             /*demo_tools_enabled=*/true}) {}

  ProjectorBehavior(const ProjectorBehavior&) = delete;
  ProjectorBehavior& operator=(const ProjectorBehavior&) = delete;
  ~ProjectorBehavior() override = default;

  // CaptureModeBehavior:
  void AttachToSession() override {
    cached_configs_ = GetCaptureModeSessionConfigs();

    // Overwrite the current capture mode session with the projector
    // configurations.
    SetCaptureModeSessionConfigs(capture_mode_configs_);
  }

  void DetachFromSession() override {
    CHECK(cached_configs_);

    // Restore the capture mode configurations after being overwritten with the
    // projector-specific configurations.
    SetCaptureModeSessionConfigs(cached_configs_.value());
    cached_configs_.reset();
  }

  bool ShouldImageCaptureTypeBeAllowed() const override { return false; }
  bool ShouldSaveToSettingsBeIncluded() const override { return false; }
  bool ShouldGifBeSupported() const override { return false; }
  bool ShouldShowPreviewNotification() const override { return false; }
  bool SupportsAudioRecordingMode(AudioRecordingMode mode) const override {
    switch (mode) {
      case AudioRecordingMode::kOff:
      case AudioRecordingMode::kSystem:
        return false;
      case AudioRecordingMode::kMicrophone:
        return true;
      case AudioRecordingMode::kSystemAndMicrophone:
        return features::IsCaptureModeAudioMixingEnabled();
    }
  }
  bool ShouldCreateRecordingOverlayController() const override { return true; }
  bool ShouldShowUserNudge() const override { return false; }
  bool ShouldAutoSelectFirstCamera() const override { return true; }
  bool RequiresCaptureFolderCreation() const override { return true; }
  void CreateCaptureFolder(OnCaptureFolderCreatedCallback callback) override {
    ProjectorControllerImpl::Get()->CreateScreencastContainerFolder(
        base::BindOnce(&ProjectorBehavior::OnScreencastContainerFolderCreated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }
  std::vector<RecordingType> GetSupportedRecordingTypes() const override {
    return std::vector<RecordingType>{RecordingType::kWebM};
  }

 private:
  // Called when the Projector controller creates the DriveFS folder that will
  // host the video file along with the associated metadata file created by the
  // Projector session.
  void OnScreencastContainerFolderCreated(
      OnCaptureFolderCreatedCallback callback,
      const base::FilePath& capture_file_full_path) {
    base::FilePath path;
    // An empty path is sent to indicate an error.
    if (!capture_file_full_path.empty()) {
      path = capture_file_full_path.AddExtension("webm");
    }
    std::move(callback).Run(path);
  }

  base::WeakPtrFactory<ProjectorBehavior> weak_ptr_factory_{this};
};

// -----------------------------------------------------------------------------
// GameDashboardBehavior:
// Implements the `CaptureModeBehavior` interface with behaviors defined by the
// game dashboard-initiated capture mode.
class GameDashboardBehavior : public CaptureModeBehavior {
 public:
  GameDashboardBehavior()
      : CaptureModeBehavior({CaptureModeType::kVideo,
                             CaptureModeSource::kWindow, RecordingType::kWebM,
                             features::IsCaptureModeAudioMixingEnabled()
                                 ? AudioRecordingMode::kSystemAndMicrophone
                                 : AudioRecordingMode::kMicrophone,
                             /*demo_tools_enabled=*/false}) {}

  GameDashboardBehavior(const GameDashboardBehavior&) = delete;
  GameDashboardBehavior operator=(const GameDashboardBehavior&) = delete;
  ~GameDashboardBehavior() override = default;

  // CaptureModeBehavior:
  bool ShouldImageCaptureTypeBeAllowed() const override { return false; }
  bool ShouldFulscreenCaptureSourceBeAllowed() const override { return false; }
  bool ShouldRegionCaptureSourceBeAllowed() const override { return false; }
  bool ShouldDemoToolsSettingsBeIncluded() const override { return false; }
  bool ShouldGifBeSupported() const override { return false; }
  bool ShouldShowUserNudge() const override { return false; }
  bool ShouldAutoSelectFirstCamera() const override { return true; }
};

}  // namespace

// -----------------------------------------------------------------------------
// CaptureModeBehavior:

CaptureModeBehavior::CaptureModeBehavior(
    const CaptureModeSessionConfigs& configs)
    : capture_mode_configs_(configs) {}

// static
std::unique_ptr<CaptureModeBehavior> CaptureModeBehavior::Create(
    BehaviorType behavior_type) {
  switch (behavior_type) {
    case BehaviorType::kProjector:
      return std::make_unique<ProjectorBehavior>();
    case BehaviorType::kGameDashboard:
      return std::make_unique<GameDashboardBehavior>();
    case BehaviorType::kDefault:
      return std::make_unique<DefaultBehavior>();
  }
}

void CaptureModeBehavior::AttachToSession() {}

void CaptureModeBehavior::DetachFromSession() {}

bool CaptureModeBehavior::ShouldImageCaptureTypeBeAllowed() const {
  return true;
}

bool CaptureModeBehavior::ShouldVideoCaptureTypeBeAllowed() const {
  return true;
}

bool CaptureModeBehavior::ShouldFulscreenCaptureSourceBeAllowed() const {
  return true;
}

bool CaptureModeBehavior::ShouldRegionCaptureSourceBeAllowed() const {
  return true;
}

bool CaptureModeBehavior::ShouldWindowCaptureSourceBeAllowed() const {
  return true;
}

bool CaptureModeBehavior::SupportsAudioRecordingMode(
    AudioRecordingMode mode) const {
  switch (mode) {
    case AudioRecordingMode::kOff:
    case AudioRecordingMode::kMicrophone:
      return true;
    case AudioRecordingMode::kSystem:
    case AudioRecordingMode::kSystemAndMicrophone:
      return features::IsCaptureModeAudioMixingEnabled();
  }
}

bool CaptureModeBehavior::ShouldCameraSelectionSettingsBeIncluded() const {
  return true;
}

bool CaptureModeBehavior::ShouldDemoToolsSettingsBeIncluded() const {
  return true;
}

bool CaptureModeBehavior::ShouldSaveToSettingsBeIncluded() const {
  return true;
}

bool CaptureModeBehavior::ShouldGifBeSupported() const {
  return true;
}

bool CaptureModeBehavior::ShouldShowPreviewNotification() const {
  return true;
}

bool CaptureModeBehavior::ShouldSkipVideoRecordingCountDown() const {
  return false;
}

bool CaptureModeBehavior::ShouldCreateRecordingOverlayController() const {
  return false;
}

bool CaptureModeBehavior::ShouldShowUserNudge() const {
  return true;
}

bool CaptureModeBehavior::ShouldAutoSelectFirstCamera() const {
  return false;
}

bool CaptureModeBehavior::RequiresCaptureFolderCreation() const {
  return false;
}

void CaptureModeBehavior::CreateCaptureFolder(
    OnCaptureFolderCreatedCallback callback) {
  NOTREACHED();
}

std::vector<RecordingType> CaptureModeBehavior::GetSupportedRecordingTypes()
    const {
  std::vector<RecordingType> supported_recording_types;
  supported_recording_types.push_back(RecordingType::kWebM);
  if (features::IsGifRecordingEnabled()) {
    supported_recording_types.push_back(RecordingType::kGif);
  }
  return supported_recording_types;
}

}  // namespace ash
