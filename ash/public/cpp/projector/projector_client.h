// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CLIENT_H_
#define ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CLIENT_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"

namespace base {
class FilePath;
}

namespace ash {

struct NewScreencastPrecondition;
struct SpeechRecognitionAvailability;

// Creates interface to access Browser side functionalities for the
// ProjectorControllerImpl.
class ASH_PUBLIC_EXPORT ProjectorClient {
 public:
  static ProjectorClient* Get();

  ProjectorClient();
  ProjectorClient(const ProjectorClient&) = delete;
  ProjectorClient& operator=(const ProjectorClient&) = delete;
  virtual ~ProjectorClient();

  virtual SpeechRecognitionAvailability GetSpeechRecognitionAvailability()
      const = 0;
  virtual void StartSpeechRecognition() = 0;
  virtual void StopSpeechRecognition() = 0;
  virtual void ForceEndSpeechRecognition() = 0;
  // Returns false if base storage path is not available. Normally the base path
  // is the DriveFS mounted folder. It is download folder when extended feature
  // command line flag is disabled.
  virtual bool GetBaseStoragePath(base::FilePath* result) const = 0;
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
  // Notifies the Projector SWA if it can trigger a new Projector session.
  virtual void OnNewScreencastPreconditionChanged(
      const NewScreencastPrecondition& precondition) const = 0;
  // Toggles to suppress/resume the system notification for `screencast_paths`.
  virtual void ToggleFileSyncingNotificationForPaths(
      const std::vector<base::FilePath>& screencast_paths,
      bool suppress) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CLIENT_H_
