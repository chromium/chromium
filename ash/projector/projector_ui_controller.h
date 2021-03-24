// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
#define ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/fast_ink/laser/laser_pointer_controller.h"
#include "ash/marker/marker_controller.h"
#include "ash/projector/model/projector_ui_model.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "base/scoped_observation.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

class ProjectorControllerImpl;
class ProjectorBarView;

// The controller in charge of UI.
class ASH_EXPORT ProjectorUiController : public LaserPointerObserver,
                                         public MarkerObserver,
                                         public ProjectorSessionObserver {
 public:
  explicit ProjectorUiController(ProjectorControllerImpl* projector_controller);
  ProjectorUiController(const ProjectorUiController&) = delete;
  ProjectorUiController& operator=(const ProjectorUiController&) = delete;
  ~ProjectorUiController() override;

  // Show Projector toolbar. Virtual for testing.
  virtual void ShowToolbar();
  // Close Projector toolbar. Virtual for testing.
  virtual void CloseToolbar();
  // Invoked when key idea is marked to show a toast. Virtual for testing.
  virtual void OnKeyIdeaMarked();
  // Invoked when laser pointer button is pressed. Virtual for testing.
  virtual void OnLaserPointerPressed();
  // Invoked when marker button is pressed. Virtual for testing.
  virtual void OnMarkerPressed();
  // Invoked when transcription is available for rendering. Virtual for testing.
  virtual void OnTranscription(const std::string& transcription, bool is_final);

  bool IsToolbarVisible() const;

  ProjectorUiModel* model() { return &model_; }

 private:
  // Reset tools, including resetting the state in model, closing the sub
  // widgets, etc.
  void ResetTools();

  // LaserPointerObserver:
  void OnLaserPointerStateChanged(bool enabled) override;

  // MarkerObserver:
  void OnMarkerStateChanged(bool enabled) override;

  // ProjectorSessionObserver:
  void OnProjectorSessionActiveStateChanged(bool active) override;

  ProjectorUiModel model_;
  views::UniqueWidgetPtr projector_bar_widget_;
  ProjectorBarView* projector_bar_view_ = nullptr;

  ProjectorControllerImpl* projector_controller_ = nullptr;

  base::ScopedObservation<LaserPointerController, LaserPointerObserver>
      laser_pointer_controller_observation_{this};

  base::ScopedObservation<MarkerController, MarkerObserver>
      marker_controller_observation_{this};

  base::ScopedObservation<ProjectorSession, ProjectorSessionObserver>
      projector_session_observation_{this};
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
