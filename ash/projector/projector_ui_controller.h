// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
#define ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"

namespace ash {

// The controller in charge of UI.
class ASH_EXPORT ProjectorUiController {
 public:
  ProjectorUiController();
  ProjectorUiController(const ProjectorUiController&) = delete;
  ProjectorUiController& operator=(const ProjectorUiController&) = delete;
  virtual ~ProjectorUiController();

  // Virtual for testing.
  virtual void ShowToolbar();
  // Invoked when key idea is marked to show a toast. Virtual for testing.
  virtual void OnKeyIdeaMarked();
  // Invoked when transcription is available for rendering. Virtual for testing.
  virtual void OnTranscription(const std::string& transcription, bool is_final);
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
