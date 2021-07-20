// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_
#define ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/time/time.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"

namespace ash {

class ProjectorClient;

// Interface to control projector in ash.
class ASH_PUBLIC_EXPORT ProjectorController {
 public:
  class ScopedInstanceResetterForTest {
   public:
    ScopedInstanceResetterForTest();
    ScopedInstanceResetterForTest(const ScopedInstanceResetterForTest&) =
        delete;
    ScopedInstanceResetterForTest& operator=(
        const ScopedInstanceResetterForTest&) = delete;
    ~ScopedInstanceResetterForTest();

   private:
    ProjectorController* const controller_;
  };

  ProjectorController();
  ProjectorController(const ProjectorController&) = delete;
  ProjectorController& operator=(const ProjectorController&) = delete;
  virtual ~ProjectorController();

  static ProjectorController* Get();

  // Make sure the client is set before attempting to use to the
  // ProjectorController.
  virtual void SetClient(ProjectorClient* client) = 0;

  // Called when speech recognition using SODA is available.
  virtual void OnSpeechRecognitionAvailable(bool available) = 0;

  // Called when transcription result from mic input is ready.
  virtual void OnTranscription(
      const media::SpeechRecognitionResult& result) = 0;

  // Called when there is an error in transcription.
  virtual void OnTranscriptionError() = 0;

  // Sets Projector toolbar visibility.
  virtual void SetProjectorToolsVisible(bool is_visible) = 0;

  // Returns true if Projector is eligible to start a new session.
  // TODO(yilkal): Rename to something more descriptive, like CanStart().
  virtual bool IsEligible() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_
