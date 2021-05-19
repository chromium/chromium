// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_
#define ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/projector/model/projector_ui_model.h"
#include "ash/projector/ui/projector_color_button.h"
#include "ash/projector/ui/projector_image_button.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/view.h"

#include <vector>

namespace views {
class BoxLayoutView;
class UniqueWidgetPtr;
}  // namespace views

namespace ash {

class ProjectorControllerImpl;

class ASH_EXPORT ProjectorBarView : public views::View {
 public:
  METADATA_HEADER(ProjectorBarView);

  static const SkColor kProjectorMarkerDefaultColor;

  explicit ProjectorBarView(ProjectorControllerImpl* projector_controller);
  ProjectorBarView(const ProjectorBarView&) = delete;
  ProjectorBarView& operator=(const ProjectorBarView&) = delete;
  ~ProjectorBarView() override;

  static views::UniqueWidgetPtr Create(
      ProjectorControllerImpl* projector_controller);

  // Invoked when recording state changed.
  void OnRecordingStateChanged(bool started);
  // Invoke when selfie cam activation state changed.
  void OnSelfieCamStateChanged(bool enabled);
  // Invoked when the caption bubble state is changed.
  void OnCaptionBubbleModelStateChanged(bool opened);
  // Invoke when laser pointer activation state changed.
  void OnLaserPointerStateChanged(bool enabled);
  // Invoke when marker activation state changed.
  void OnMarkerStateChanged(bool enabled);
  // Invoked when the magnifier state changed.
  void OnMagnifierStateChanged(bool enabled);

  // views::View:
  void OnThemeChanged() override;

  bool IsKeyIdeaButtonEnabled() const;
  bool IsClosedCaptionEnabled() const;

 private:
  // The location of the bar.
  enum class BarLocation { kUpperLeft, kUpperRight, kLowerRight, kLowerLeft };

  // The state of the marker bar.
  enum class MarkerBarState { kDisabled, kHighlighted, kExpanded };

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  void InitLayout();
  void AddSeparatorViewToView(views::View* view);
  void CreateMarkerOptionsBar();
  void CreateTrailingButtonsBar();

  void OnKeyIdeaButtonPressed();
  void OnLaserPointerPressed();
  void OnMarkerPressed();
  void OnClearAllMarkersPressed();
  void OnSelfieCamPressed(bool enabled);
  void OnMagnifierButtonPressed(bool enabled);
  void OnChangeBarLocationButtonPressed();
  void OnCaretButtonPressed(bool expand);
  void OnUndoButtonPressed();
  void OnChangeMarkerColorPressed(SkColor new_color);
  void OnInkPenButtonPressed();
  void OnMarkerPenButtonPressed();

  void SetCaptionState(bool opened);
  void UpdateToolbarButtonsVisibility();
  gfx::Rect CalculateBoundsInScreen() const;

  ProjectorButton* key_idea_button_ = nullptr;
  ProjectorButton* laser_pointer_button_ = nullptr;
  ProjectorButton* marker_button_ = nullptr;
  ProjectorButton* ink_pen_button_ = nullptr;
  ProjectorButton* marker_pen_button_ = nullptr;
  std::vector<ProjectorColorButton*> marker_color_buttons_;
  ProjectorButton* undo_button_ = nullptr;
  ProjectorButton* clear_all_markers_button_ = nullptr;
  ProjectorButton* magnifier_start_button_ = nullptr;
  ProjectorButton* magnifier_stop_button_ = nullptr;
  ProjectorButton* caret_right_ = nullptr;
  ProjectorButton* caret_left_ = nullptr;
  ProjectorButton* selfie_cam_on_button_ = nullptr;
  ProjectorButton* selfie_cam_off_button_ = nullptr;
  ProjectorButton* closed_caption_show_button_ = nullptr;
  ProjectorButton* closed_caption_hide_button_ = nullptr;
  ProjectorImageButton* bar_location_button_ = nullptr;
  views::BoxLayoutView* marker_bar_ = nullptr;
  views::BoxLayoutView* tools_bar_ = nullptr;

  BarLocation bar_location_ = BarLocation::kLowerLeft;
  MarkerBarState marker_bar_state_ = MarkerBarState::kDisabled;

  ProjectorControllerImpl* projector_controller_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PROJECTOR_UI_PROJECTOR_BAR_VIEW_H_
