// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
#define ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/projector/projector_metrics.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class ProjectorControllerImpl;

// The controller in charge of UI.
class ASH_EXPORT ProjectorUiController : public ProjectorSessionObserver {
 public:
  // Shows a notification informing the user that a Projector error has
  // occurred.
  static void ShowFailureNotification(
      int message_id,
      int title_id = IDS_ASH_PROJECTOR_FAILURE_TITLE);

  // Shows a notification informing the user that a Projector save error has
  // occurred.
  static void ShowSaveFailureNotification();

  explicit ProjectorUiController(ProjectorControllerImpl* projector_controller);
  ProjectorUiController(const ProjectorUiController&) = delete;
  ProjectorUiController& operator=(const ProjectorUiController&) = delete;
  ~ProjectorUiController() override;

  // Show Projector annotation tray for `current_root`. Virtual for testing.
  virtual void ShowAnnotationTray(aura::Window* current_root);
  // Hide Projector annotation tray. Virtual for testing.
  virtual void HideAnnotationTray();

  void OnRecordedWindowChangingRoot(aura::Window* new_root);

 private:
  // ProjectorSessionObserver:
  void OnProjectorSessionActiveStateChanged(bool active) override;

  // The current root window in which the video recording is happening.
  raw_ptr<aura::Window, DanglingUntriaged> current_root_ = nullptr;

  base::ScopedObservation<ProjectorSession, ProjectorSessionObserver>
      projector_session_observation_{this};
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_UI_CONTROLLER_H_
