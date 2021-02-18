// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_
#define ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "chromeos/services/machine_learning/public/mojom/soda.mojom.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

class ProjectorClient;
class ProjectorUiController;
class ProjectorMetadataController;

// A controller to handle projector functionalities.
class ASH_EXPORT ProjectorControllerImpl : public ProjectorController {
 public:
  ProjectorControllerImpl();
  ProjectorControllerImpl(const ProjectorControllerImpl&) = delete;
  ProjectorControllerImpl& operator=(const ProjectorControllerImpl&) = delete;
  ~ProjectorControllerImpl() override;

  // ProjectorController:
  void SetClient(ash::ProjectorClient* client) override;

  // Shows projector toolbar.
  void ShowToolbar();

  // Set caption on/off state.
  void SetCaptionState(bool is_on);
  // Mark a key idea.
  void MarkKeyIdea();

  // TODO(crbug.com/1165435): Consider updating to be delegate of recording
  // service after finalizing on the integration plan with recording mode.
  // Invoked when recording is started to start a screencast session.
  void OnRecordingStarted();

  // Saves the screencast including metadata.
  void SaveScreencast(const base::FilePath& saved_video_path);

  // TODO(crbug.com/1165437): Update the interface once SODA integration is
  // available.
  // Invoked when transcription result is available to record the transcript
  // and maybe update the UI.
  // TODO(yilkal): Make this method an inherited method from
  // ProjectorController.
  void OnTranscription(
      chromeos::machine_learning::mojom::SpeechRecognizerEventPtr
          speech_recognizer_event);

  void SetProjectorUiControllerForTest(
      std::unique_ptr<ProjectorUiController> ui_controller);
  void SetProjectorMetadataControllerForTest(
      std::unique_ptr<ProjectorMetadataController> metadata_controller);

  ProjectorUiController* ui_controller() { return ui_controller_.get(); }

 private:
  // Starts or stops the speech recognition session.
  void StartSpeechRecognition();
  void StopSpeechRecognition();

  ProjectorClient* client_ = nullptr;
  std::unique_ptr<ProjectorUiController> ui_controller_;
  std::unique_ptr<ProjectorMetadataController> metadata_controller_;

  bool is_caption_on_ = false;
  bool is_speech_recognition_on_ = false;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_CONTROLLER_IMPL_H_
