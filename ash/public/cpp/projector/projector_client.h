// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CLIENT_H_
#define ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Creates interface to access Browser side functionalities for the
// ProjectorControllerImpl.
class ASH_PUBLIC_EXPORT ProjectorClient {
 public:
  ProjectorClient() = default;
  ProjectorClient(const ProjectorClient&) = delete;
  ProjectorClient& operator=(const ProjectorClient&) = delete;
  virtual ~ProjectorClient() = default;

  virtual void StartSpeechRecognition() = 0;
  virtual void StopSpeechRecognition() = 0;

  // TODO(crbug/1199396): Migrate to IPC after Lacros launch and ash-chrome
  // deprecation.
  virtual void ShowSelfieCam() = 0;
  virtual void CloseSelfieCam() = 0;
  virtual bool IsSelfieCamVisible() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CLIENT_H_
