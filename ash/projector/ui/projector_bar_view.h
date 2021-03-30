// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_
#define ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_

#include "ash/fast_ink/laser/laser_pointer_controller.h"
#include "ash/marker/marker_controller.h"
#include "ash/projector/model/projector_ui_model.h"
#include "ash/projector/ui/projector_color_button.h"
#include "ash/projector/ui/projector_image_button.h"
#include "base/scoped_observation.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class UniqueWidgetPtr;
}  // namespace views

namespace ash {

class ProjectorControllerImpl;

class ProjectorBarView : public views::View,
                         public LaserPointerObserver,
                         public MarkerObserver {
 public:
  METADATA_HEADER(ProjectorBarView);

  explicit ProjectorBarView(ProjectorControllerImpl* projector_controller);
  ProjectorBarView(const ProjectorBarView&) = delete;
  ProjectorBarView& operator=(const ProjectorBarView&) = delete;
  ~ProjectorBarView() override;

  static views::UniqueWidgetPtr Create(
      ProjectorControllerImpl* projector_controller);

  // views::View:
  void OnThemeChanged() override;

 private:
  // LaserPointerObserver:
  void OnLaserPointerStateChanged(bool enabled) override;

  // MarkerObserver:
  void OnMarkerStateChanged(bool enabled) override;

  void InitLayout();
  void InitWidget();

  void UpdateVectorIcon();

  void OnRecordButtonPressed();
  void OnStopButtonPressed();
  void OnKeyIdeaButtonPressed();
  void OnLaserPointerPressed();
  void OnMarkerPressed();

  views::ImageView* drag_handle_ = nullptr;
  ProjectorColorButton* record_button_ = nullptr;
  ProjectorColorButton* stop_button_ = nullptr;
  ProjectorButton* key_idea_button_ = nullptr;
  ProjectorButton* laser_pointer_button_ = nullptr;
  ProjectorButton* marker_button_ = nullptr;

  ProjectorControllerImpl* projector_controller_ = nullptr;

  base::ScopedObservation<LaserPointerController, LaserPointerObserver>
      laser_pointer_controller_observation_{this};

  base::ScopedObservation<MarkerController, MarkerObserver>
      marker_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_
