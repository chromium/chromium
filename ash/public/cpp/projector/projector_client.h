// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CLIENT_H_
#define ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"

namespace base {
class FilePath;
}

namespace ash {

class AnnotatorMessageHandler;
struct NewScreencastPrecondition;

// Creates interface to access Browser side functionalities for the
// ProjectorControllerImpl.
class ASH_PUBLIC_EXPORT ProjectorClient {
 public:
  static ProjectorClient* Get();

  ProjectorClient();
  ProjectorClient(const ProjectorClient&) = delete;
  ProjectorClient& operator=(const ProjectorClient&) = delete;
  virtual ~ProjectorClient();

  virtual void StartSpeechRecognition() = 0;
  virtual void StopSpeechRecognition() = 0;
  // Returns false if Drive is not enabled.
  virtual bool GetDriveFsMountPointPath(base::FilePath* result) const = 0;
  virtual bool IsDriveFsMounted() const = 0;
  // Return true if Drive mount failed. Drive will not automatically retry to
  // mount.
  virtual bool IsDriveFsMountFailed() const = 0;
  // Opens Projector SWA. The app by default showing the Projector Gallery view.
  virtual void OpenProjectorApp() const = 0;
  // Minimizes Projector SWA.
  virtual void MinimizeProjectorApp() const = 0;
  // Closes Projector SWA.
  virtual void CloseProjectorApp() const = 0;

  // Registers the AnnotatorMessageHandler that is owned by the WebUI that
  // contains the Projector annotator.
  virtual void SetAnnotatorMessageHandler(AnnotatorMessageHandler* handler) = 0;
  // Resets the stored AnnotatorMessageHandler if it matches the one that is
  // passed in.
  virtual void ResetAnnotatorMessageHandler(
      AnnotatorMessageHandler* handler) = 0;

  // Notifies the Projector SWA if it can trigger a new Projector session.
  virtual void OnNewScreencastPreconditionChanged(
      const NewScreencastPrecondition& precondition) const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CLIENT_H_
