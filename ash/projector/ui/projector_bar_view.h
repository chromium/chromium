// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_
#define ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_

#include "ash/projector/model/projector_ui_model.h"
#include "ash/projector/projector_ui_controller.h"
#include "ash/projector/ui/projector_color_button.h"
#include "ash/projector/ui/projector_image_button.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {

class ProjectorBarView : public views::View {
 public:
  METADATA_HEADER(ProjectorBarView);

  explicit ProjectorBarView(ProjectorUiController* ui_controller);
  ProjectorBarView(const ProjectorBarView&) = delete;
  ProjectorBarView& operator=(const ProjectorBarView&) = delete;
  ~ProjectorBarView() override;

  static views::UniqueWidgetPtr Create(ProjectorUiController* ui_controller);

  // views::View:
  void OnThemeChanged() override;

 private:
  void InitLayout();
  void InitWidget();

  void UpdateVectorIcon();

  void OnRecordButtonPressed();
  void OnStopButtonPressed();
  void OnKeyIdeaButtonPressed();

  views::ImageView* drag_handle_ = nullptr;
  ProjectorColorButton* record_button_ = nullptr;
  ProjectorColorButton* stop_button_ = nullptr;
  ProjectorButton* key_idea_button_ = nullptr;

  ProjectorUiController* ui_controller_ = nullptr;
};

}  // namespace ash
#endif  // ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_
