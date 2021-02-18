// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_
#define ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class ProjectorClient;

// Interface to control projector in ash.
class ASH_PUBLIC_EXPORT ProjectorController {
 public:
  ProjectorController();
  ProjectorController(const ProjectorController&) = delete;
  ProjectorController& operator=(const ProjectorController&) = delete;
  virtual ~ProjectorController();

  static ProjectorController* Get();

  // Make sure the client is set before attempting to use to the
  // ProjectorController.
  virtual void SetClient(ProjectorClient* client) = 0;

  // TODO(ylkal): Add OnTranscriptionResult method to receive transcription
  // results here.
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_
