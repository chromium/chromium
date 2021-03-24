// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_
#define ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_

#include "ash/projector/model/projector_ui_model.h"
#include "ash/projector/ui/projector_color_button.h"
#include "ash/projector/ui/projector_image_button.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class UniqueWidgetPtr;
}  // namespace views

namespace ash {

class ProjectorControllerImpl;

class ProjectorBarView : public views::View {
 public:
  METADATA_HEADER(ProjectorBarView);

  explicit ProjectorBarView(ProjectorControllerImpl* projector_controller);
  ProjectorBarView(const ProjectorBarView&) = delete;
  ProjectorBarView& operator=(const ProjectorBarView&) = delete;
  ~ProjectorBarView() override;

  static views::UniqueWidgetPtr Create(
      ProjectorControllerImpl* projector_controller);

  // Invoke when laser pointer activation state changed.
  void OnLaserPointerStateChanged(bool enabled);
  // Invoke when marker activation state changed.
  void OnMarkerStateChanged(bool enabled);

  // views::View:
  void OnThemeChanged() override;

 private:
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
};

}  // namespace ash

#endif  // ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_
