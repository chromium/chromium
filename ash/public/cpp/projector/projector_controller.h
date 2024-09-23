// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_
#define ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/files/safe_base_name.h"
#include "base/time/time.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

namespace ash {

struct NewScreencastPrecondition;

class ProjectorClient;

// Interface to control projector in ash.
class ASH_PUBLIC_EXPORT ProjectorController {
 public:
  ProjectorController();
  ProjectorController(const ProjectorController&) = delete;
  ProjectorController& operator=(const ProjectorController&) = delete;
  virtual ~ProjectorController();

  static ProjectorController* Get();

  // Returns whether the extended features for projector are enabled. This is
  // decided based on a command line switch.
  static bool AreExtendedProjectorFeaturesDisabled();

  // Starts a capture mode session for the projector workflow if no video
  // recording is currently in progress. `storage_dir` is the container
  // directory name for screencasts and will be used to create the storage path.
  virtual void StartProjectorSession(const base::SafeBaseName& storage_dir) = 0;

  // Make sure the client is set before attempting to use to the
  // ProjectorController.
  virtual void SetClient(ProjectorClient* client) = 0;

  // Called when speech recognition availability changes.
  virtual void OnSpeechRecognitionAvailabilityChanged() = 0;

  // Called when transcription result from mic input is ready.
  virtual void OnTranscription(
      const media::SpeechRecognitionResult& result) = 0;

  // Called when there is an error in transcription.
  virtual void OnTranscriptionError() = 0;

  // Called when speech recognition stopped. `forced` is set to true
  // if the recognition session was forced to stop before it finishes
  // processing.
  virtual void OnSpeechRecognitionStopped(bool forced) = 0;

  // Returns true if we can start a new Projector session.
  virtual NewScreencastPrecondition GetNewScreencastPrecondition() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_
