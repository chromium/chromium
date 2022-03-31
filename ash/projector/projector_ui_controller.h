// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
#define ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "base/scoped_observation.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

class ProjectorControllerImpl;
struct AnnotatorTool;

// The controller in charge of UI.
class ASH_EXPORT ProjectorUiController : public ProjectorSessionObserver {
 public:
  // Shows a notification informing the user that a Projector error has
  // occurred.
  static void ShowFailureNotification(int message_id);

  // Shows a notification informing the user that a Projector save error has
  // occurred.
  static void ShowSaveFailureNotification();

  explicit ProjectorUiController(ProjectorControllerImpl* projector_controller);
  ProjectorUiController(const ProjectorUiController&) = delete;
  ProjectorUiController& operator=(const ProjectorUiController&) = delete;
  ~ProjectorUiController() override;

  // Show Projector toolbar. Virtual for testing.
  virtual void ShowToolbar();
  // Close Projector toolbar. Virtual for testing.
  virtual void CloseToolbar();
  // Invoked when marker button is pressed. Virtual for testing.
  virtual void OnMarkerPressed();
  // Sets the annotator tool.
  virtual void SetAnnotatorTool(const AnnotatorTool& tool);
  // Resets and disables the annotator tools and clears the canvas.
  void ResetTools();

  bool is_annotator_enabled() { return annotator_enabled_; }

 private:
  // ProjectorSessionObserver:
  void OnProjectorSessionActiveStateChanged(bool active) override;

  ProjectorMarkerColor GetMarkerColorForMetrics(SkColor color);

  bool annotator_enabled_ = false;

  base::ScopedObservation<ProjectorSession, ProjectorSessionObserver>
      projector_session_observation_{this};
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
