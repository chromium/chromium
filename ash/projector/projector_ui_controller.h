// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
#define ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/fast_ink/laser/laser_pointer_controller.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "base/scoped_observation.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

class ProjectorControllerImpl;

// The controller in charge of UI.
class ASH_EXPORT ProjectorUiController : public ProjectorSessionObserver,
                                         public LaserPointerObserver {
 public:
  // Shows a notification informing the user that a Projector error has
  // occurred.
  static void ShowFailureNotification(int message_id);

  explicit ProjectorUiController(ProjectorControllerImpl* projector_controller);
  ProjectorUiController(const ProjectorUiController&) = delete;
  ProjectorUiController& operator=(const ProjectorUiController&) = delete;
  ~ProjectorUiController() override;

  // Show Projector toolbar. Virtual for testing.
  virtual void ShowToolbar();
  // Close Projector toolbar. Virtual for testing.
  virtual void CloseToolbar();
  // Invoked when laser pointer button is pressed. Virtual for testing.
  virtual void OnLaserPointerPressed();
  // Invoked when marker button is pressed. Virtual for testing.
  virtual void OnMarkerPressed();
  // Reset and disable the laser pointer and the annotator tools.
  void ResetTools();

  bool IsLaserPointerEnabled();
  bool is_annotator_enabled() { return annotator_enabled_; }

 private:
  // ProjectorSessionObserver:
  void OnProjectorSessionActiveStateChanged(bool active) override;

  // LaserPointerObserver:
  void OnLaserPointerStateChanged(bool enabled) override;

  bool annotator_enabled_ = false;

  base::ScopedObservation<LaserPointerController, LaserPointerObserver>
      laser_pointer_controller_observation_{this};

  base::ScopedObservation<ProjectorSession, ProjectorSessionObserver>
      projector_session_observation_{this};
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
