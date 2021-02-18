// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
#define ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/projector/model/projector_ui_model.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// The controller in charge of UI.
class ASH_EXPORT ProjectorUiController {
 public:
  ProjectorUiController();
  ProjectorUiController(const ProjectorUiController&) = delete;
  ProjectorUiController& operator=(const ProjectorUiController&) = delete;
  virtual ~ProjectorUiController();

  // Show Projector toolbar. Virtual for testing.
  virtual void ShowToolbar();
  // Close Projector toolbar. Virtual for testing.
  virtual void CloseToolbar();
  // Toggle Projector toolbar based on the toolbar visibility state.
  // Virtual for testing.
  virtual void ToggleToolbar();
  // Invoked when key idea is marked to show a toast. Virtual for testing.
  virtual void OnKeyIdeaMarked();
  // Invoked when transcription is available for rendering. Virtual for testing.
  virtual void OnTranscription(const std::string& transcription, bool is_final);

  ProjectorUiModel* model() { return &model_; }

 private:
  ProjectorUiModel model_;
  views::UniqueWidgetPtr projector_bar_widget_;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
