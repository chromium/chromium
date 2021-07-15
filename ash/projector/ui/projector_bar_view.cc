// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/ui/projector_bar_view.h"

#include "ash/projector/projector_controller_impl.h"
#include "ash/projector/projector_metrics.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/work_area_insets.h"
#include "base/bind.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr float kBarRadius = 20.f;
constexpr int kBarAlpha_ = 230;  // 90% opacity
constexpr int kBarHeight = 68;
constexpr int kSeparatorHeight = 16;
constexpr int kBarMargin = 15;

constexpr float kInnerBarRadius = 50.f;

// The spacing used by the BoxLayout manager to space out child views in the
// |ProjectorBarView|.
constexpr int kBetweenChildSpacing = 16;

// Color selection buttons.
constexpr int kColorButtonColorViewSize = 24;
constexpr int kColorButtonViewRadius = 12;

constexpr gfx::Insets kProjectorBarViewPadding(12, 16, 12, 16);
constexpr gfx::Insets kMarkerBarViewPadding(4, 15, 4, 15);

constexpr SkColor kProjectorColors[] = {SK_ColorBLACK, SK_ColorWHITE,
                                        SK_ColorBLUE};

std::u16string GetColorName(SkColor color) {
  int name = IDS_UNKNOWN_COLOR_BUTTON;
  switch (color) {
    case SK_ColorBLACK:
      name = IDS_BLACK_COLOR_BUTTON;
      break;
    case SK_ColorWHITE:
      name = IDS_WHITE_COLOR_BUTTON;
      break;
    case SK_ColorBLUE:
      name = IDS_BLUE_COLOR_BUTTON;
      break;
  }
  DCHECK_NE(name, IDS_UNKNOWN_COLOR_BUTTON);
  return l10n_util::GetStringUTF16(name);
}

}  // namespace

// static
const SkColor ProjectorBarView::kProjectorMarkerDefaultColor =
    kProjectorColors[0];

ProjectorBarView::ProjectorBarView(
    ProjectorControllerImpl* projector_controller)
    : projector_controller_(projector_controller) {
  InitLayout();
}

ProjectorBarView::~ProjectorBarView() = default;

views::UniqueWidgetPtr ProjectorBarView::Create(
    ProjectorControllerImpl* projector_controller) {
  auto bar_view = std::make_unique<ProjectorBarView>(projector_controller);

  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.context = Shell::Get()->GetRootWindowForNewWindows();

  params.bounds = bar_view->CalculateBoundsInScreen();

  auto widget = views::UniqueWidgetPtr(
      std::make_unique<views::Widget>(std::move(params)));
  widget->SetContentsView(std::move(bar_view));

  return widget;
}

void ProjectorBarView::OnRecordingStateChanged(bool started) {
  // Closed caption and key idea buttons states are dependent on the recording
  // state.
  auto recording_related_buttons_state =
      started ? views::Button::ButtonState::STATE_NORMAL
              : views::Button::ButtonState::STATE_DISABLED;

  closed_caption_hide_button_->SetState(recording_related_buttons_state);
  closed_caption_show_button_->SetState(recording_related_buttons_state);
  key_idea_button_->SetState(recording_related_buttons_state);

  // If recording just got turned off, make the `closed_caption_show_button`
  // visible.
  if (!started) {
    closed_caption_show_button_->SetVisible(true);
    closed_caption_hide_button_->SetVisible(false);
  }
}

void ProjectorBarView::OnSelfieCamStateChanged(bool enabled) {
  selfie_cam_on_button_->SetVisible(!enabled);
  selfie_cam_off_button_->SetVisible(enabled);
}

void ProjectorBarView::OnCaptionBubbleModelStateChanged(bool opened) {
  closed_caption_show_button_->SetVisible(!opened);
  closed_caption_hide_button_->SetVisible(opened);
}

void ProjectorBarView::OnLaserPointerStateChanged(bool enabled) {
  laser_pointer_button_->SetToggled(enabled);
}

void ProjectorBarView::OnMarkerStateChanged(bool enabled) {
  marker_button_->SetToggled(enabled);
  undo_button_->SetEnabled(enabled);
  clear_all_markers_button_->SetEnabled(enabled);

  marker_bar_state_ =
      enabled ? MarkerBarState::kHighlighted : MarkerBarState::kDisabled;
  UpdateToolbarButtonsVisibility();

  if (!enabled)
    projector_controller_->OnClearAllMarkersPressed();
}

void ProjectorBarView::OnMagnifierStateChanged(bool enabled) {
  magnifier_start_button_->SetVisible(!enabled);
  magnifier_stop_button_->SetVisible(enabled);
}

void ProjectorBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
}

bool ProjectorBarView::IsKeyIdeaButtonEnabled() const {
  return key_idea_button_->GetState() ==
         views::Button::ButtonState::STATE_NORMAL;
}

bool ProjectorBarView::IsClosedCaptionEnabled() const {
  return closed_caption_show_button_->GetState() ==
         views::Button::ButtonState::STATE_NORMAL;
}

gfx::Size ProjectorBarView::CalculatePreferredSize() const {
  int width = views::View::CalculatePreferredSize().width();
  return gfx::Size(width, kBarHeight);
}

void ProjectorBarView::InitLayout() {
  // Set up layout manager.
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kProjectorBarViewPadding,
      kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Set background color.
  SetBackground(views::CreateBackgroundFromPainter(
      views::Painter::CreateSolidRoundRectPainter(
          SkColorSetA(gfx::kGoogleGrey900, kBarAlpha_), kBarRadius)));

  // Add key idea button.
  key_idea_button_ = AddChildView(std::make_unique<ProjectorImageButton>(
      base::BindRepeating(&ProjectorBarView::OnKeyIdeaButtonPressed,
                          base::Unretained(this)),
      kProjectorKeyIdeaIcon, l10n_util::GetStringUTF16(IDS_KEY_IDEA_BUTTON)));
  key_idea_button_->SetState(views::Button::ButtonState::STATE_DISABLED);

  // Add separator view
  AddSeparatorViewToView(this);

  // Add laser pointer button.
  laser_pointer_button_ = AddChildView(std::make_unique<ProjectorImageButton>(
      base::BindRepeating(&ProjectorBarView::OnLaserPointerPressed,
                          base::Unretained(this)),
      kPaletteTrayIconLaserPointerIcon,
      l10n_util::GetStringUTF16(IDS_LASER_POINTER_BUTTON)));

  // Add marker button.
  marker_button_ = AddChildView(std::make_unique<ProjectorImageButton>(
      base::BindRepeating(&ProjectorBarView::OnMarkerPressed,
                          base::Unretained(this)),
      kProjectorMarkerIcon,
      l10n_util::GetStringUTF16(IDS_MARKER_TOOLS_BUTTON)));

  CreateMarkerOptionsBar();

  CreateTrailingButtonsBar();
}

void ProjectorBarView::AddSeparatorViewToView(views::View* view) {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor));
  separator->SetPreferredHeight(kSeparatorHeight);
  view->AddChildView(std::move(separator));
}

void ProjectorBarView::CreateMarkerOptionsBar() {
  auto box_layout = std::make_unique<views::BoxLayoutView>();
  box_layout->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  box_layout->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  box_layout->SetBetweenChildSpacing(kBetweenChildSpacing);
  box_layout->SetInsideBorderInsets(kMarkerBarViewPadding);
  box_layout->SetBackground(views::CreateBackgroundFromPainter(
      views::Painter::CreateSolidRoundRectPainter(
          SkColorSetA(gfx::kGoogleGrey800, kBarAlpha_ / 2), kInnerBarRadius)));

  ink_pen_button_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(&ProjectorBarView::OnInkPenButtonPressed,
                              base::Unretained(this)),
          kInkPenIcon, l10n_util::GetStringUTF16(IDS_INK_PEN_BUTTON)));
  marker_pen_button_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(&ProjectorBarView::OnMarkerPenButtonPressed,
                              base::Unretained(this)),
          kMarkerIcon, l10n_util::GetStringUTF16(IDS_MARKER_PEN_BUTTON)));

  for (const auto& color : kProjectorColors) {
    std::u16string button_name = l10n_util::GetStringFUTF16(
        IDS_MARKER_COLOR_BUTTON, GetColorName(color));
    marker_color_buttons_.push_back(
        box_layout->AddChildView(std::make_unique<ProjectorColorButton>(
            base::BindRepeating(&ProjectorBarView::OnChangeMarkerColorPressed,
                                base::Unretained(this), color),
            color, kColorButtonColorViewSize, kColorButtonViewRadius,
            button_name)));
  }

  undo_button_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(&ProjectorBarView::OnUndoButtonPressed,
                              base::Unretained(this)),
          kUndoIcon, l10n_util::GetStringUTF16(IDS_UNDO_BUTTON)));

  // This button is disabled by default until marker mode activated.
  undo_button_->SetEnabled(marker_button_->GetToggled());

  // Add clear all markers button.
  clear_all_markers_button_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(&ProjectorBarView::OnClearAllMarkersPressed,
                              base::Unretained(this)),
          kTrashCanIcon,
          l10n_util::GetStringUTF16(IDS_CLEAR_ALL_MARKERS_BUTTON)));

  // This button is disabled by default until marker mode activated.
  clear_all_markers_button_->SetEnabled(marker_button_->GetToggled());

  caret_right_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(&ProjectorBarView::OnCaretButtonPressed,
                              base::Unretained(this), /* expand =*/true),
          kCaretRightIcon,
          l10n_util::GetStringUTF16(IDS_EXPAND_MARKER_TOOLS_BUTTON)));
  caret_left_ = box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
      base::BindRepeating(&ProjectorBarView::OnCaretButtonPressed,
                          base::Unretained(this), /* expand =*/false),
      kCaretLeftIcon,
      l10n_util::GetStringUTF16(IDS_COLLAPSE_MARKER_TOOLS_BUTTON)));

  marker_bar_ = AddChildView(std::move(box_layout));
  marker_bar_->SetVisible(false);
}

void ProjectorBarView::CreateTrailingButtonsBar() {
  auto box_layout = std::make_unique<views::BoxLayoutView>();
  box_layout->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  box_layout->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  box_layout->SetBetweenChildSpacing(kBetweenChildSpacing);

  // Add magnifier buttons.
  magnifier_start_button_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(&ProjectorBarView::OnMagnifierButtonPressed,
                              base::Unretained(this), /* enabled =*/true),
          kZoomInIcon, l10n_util::GetStringUTF16(IDS_START_MAGNIFIER_BUTTON)));
  magnifier_start_button_->SetVisible(true);
  magnifier_stop_button_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(&ProjectorBarView::OnMagnifierButtonPressed,
                              base::Unretained(this), /* enabled =*/false),
          kZoomOutIcon, l10n_util::GetStringUTF16(IDS_STOP_MAGNIFIER_BUTTON)));
  magnifier_stop_button_->SetVisible(false);

  AddSeparatorViewToView(box_layout.get());

  // Add selfie cam button.
  selfie_cam_on_button_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(&ProjectorBarView::OnSelfieCamPressed,
                              base::Unretained(this), /*enabled=*/true),
          kProjectorSelfieCamOnIcon,
          l10n_util::GetStringUTF16(IDS_START_SELFIE_CAMERA_BUTTON)));
  selfie_cam_on_button_->SetVisible(true);

  selfie_cam_off_button_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(&ProjectorBarView::OnSelfieCamPressed,
                              base::Unretained(this), /*enabled=*/false),
          kProjectorSelfieCamOffIcon,
          l10n_util::GetStringUTF16(IDS_STOP_SELFIE_CAMERA_BUTTON)));
  selfie_cam_off_button_->SetVisible(false);

  // Add closed caption show/hide buttons.
  closed_caption_hide_button_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(&ProjectorBarView::SetCaptionState,
                              base::Unretained(this), false),
          kHideClosedCaptionIcon,
          l10n_util::GetStringUTF16(IDS_STOP_CLOSED_CAPTIONS_BUTTON)));
  closed_caption_hide_button_->SetVisible(false);
  closed_caption_hide_button_->SetState(
      views::Button::ButtonState::STATE_DISABLED);

  closed_caption_show_button_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(&ProjectorBarView::SetCaptionState,
                              base::Unretained(this), true),
          kShowClosedCaptionIcon,
          l10n_util::GetStringUTF16(IDS_START_CLOSED_CAPTIONS_BUTTON)));
  closed_caption_show_button_->SetVisible(true);
  closed_caption_show_button_->SetState(
      views::Button::ButtonState::STATE_DISABLED);

  AddSeparatorViewToView(box_layout.get());

  bar_location_button_ =
      box_layout->AddChildView(std::make_unique<ProjectorImageButton>(
          base::BindRepeating(
              &ProjectorBarView::OnChangeBarLocationButtonPressed,
              base::Unretained(this)),
          kToolbarPositionBottomCenterIcon,
          l10n_util::GetStringUTF16(IDS_TOOLBAR_LOCATION_BUTTON)));
  bar_location_button_->SetVisible(true);
  tools_bar_ = AddChildView(std::move(box_layout));
}

void ProjectorBarView::OnKeyIdeaButtonPressed() {
  DCHECK(projector_controller_);
  projector_controller_->MarkKeyIdea();
}

void ProjectorBarView::OnLaserPointerPressed() {
  projector_controller_->OnLaserPointerPressed();
}

void ProjectorBarView::OnMarkerPressed() {
  projector_controller_->OnMarkerPressed();
}

void ProjectorBarView::OnClearAllMarkersPressed() {
  projector_controller_->OnClearAllMarkersPressed();
}

void ProjectorBarView::OnSelfieCamPressed(bool enabled) {
  projector_controller_->OnSelfieCamPressed(enabled);
}

void ProjectorBarView::OnMagnifierButtonPressed(bool enabled) {
  projector_controller_->OnMagnifierButtonPressed(enabled);
}

void ProjectorBarView::OnChangeBarLocationButtonPressed() {
  switch (bar_location_) {
    case BarLocation::kUpperLeft:
      bar_location_ = BarLocation::kUpperCenter;
      bar_location_button_->SetVectorIcon(kToolbarPositionTopCenterIcon);
      RecordToolbarMetrics(ProjectorToolbar::kToolbarLocationTopCenter);
      break;
    case BarLocation::kUpperCenter:
      bar_location_ = BarLocation::kUpperRight;
      bar_location_button_->SetVectorIcon(kAutoclickPositionTopRightIcon);
      RecordToolbarMetrics(ProjectorToolbar::kToolbarLocationTopRight);
      break;
    case BarLocation::kUpperRight:
      bar_location_ = BarLocation::kLowerRight;
      bar_location_button_->SetVectorIcon(kAutoclickPositionBottomRightIcon);
      RecordToolbarMetrics(ProjectorToolbar::kToolbarLocationBottomRight);
      break;
    case BarLocation::kLowerRight:
      bar_location_ = BarLocation::kLowerCenter;
      bar_location_button_->SetVectorIcon(kToolbarPositionBottomCenterIcon);
      RecordToolbarMetrics(ProjectorToolbar::kToolbarLocationBottomCenter);
      break;
    case BarLocation::kLowerCenter:
      bar_location_ = BarLocation::kLowerLeft;
      bar_location_button_->SetVectorIcon(kAutoclickPositionBottomLeftIcon);
      RecordToolbarMetrics(ProjectorToolbar::kToolbarLocationBottomLeft);
      break;
    case BarLocation::kLowerLeft:
      bar_location_ = BarLocation::kUpperLeft;
      bar_location_button_->SetVectorIcon(kAutoclickPositionTopLeftIcon);
      RecordToolbarMetrics(ProjectorToolbar::kToolbarLocationTopLeft);
      break;
  }

  GetWidget()->SetBounds(CalculateBoundsInScreen());
}

void ProjectorBarView::OnCaretButtonPressed(bool expand) {
  marker_bar_state_ =
      expand ? MarkerBarState::kExpanded : MarkerBarState::kHighlighted;
  UpdateToolbarButtonsVisibility();
  RecordToolbarMetrics(expand ? ProjectorToolbar::kExpandMarkerTools
                              : ProjectorToolbar::kCollapseMarkerTools);
}

void ProjectorBarView::OnUndoButtonPressed() {
  projector_controller_->OnUndoPressed();
}

void ProjectorBarView::OnChangeMarkerColorPressed(SkColor new_color) {
  projector_controller_->OnChangeMarkerColorPressed(new_color);
}

void ProjectorBarView::OnInkPenButtonPressed() {
  // TODO(crbug/1203444) Implement change between marker.
}

void ProjectorBarView::OnMarkerPenButtonPressed() {
  // TODO(crbug/1203444) Implement change between marker.
}

void ProjectorBarView::SetCaptionState(bool opened) {
  projector_controller_->SetCaptionBubbleState(opened);
}

void ProjectorBarView::UpdateToolbarButtonsVisibility() {
  switch (marker_bar_state_) {
    case MarkerBarState::kDisabled:
      tools_bar_->SetVisible(true);
      marker_bar_->SetVisible(false);
      break;
    case MarkerBarState::kHighlighted:
      tools_bar_->SetVisible(true);
      marker_bar_->SetVisible(true);
      ink_pen_button_->SetVisible(false);
      marker_pen_button_->SetVisible(false);
      undo_button_->SetVisible(true);
      clear_all_markers_button_->SetVisible(false);
      caret_left_->SetVisible(false);
      caret_right_->SetVisible(true);
      for (auto* color_button : marker_color_buttons_)
        color_button->SetVisible(false);
      break;
    case MarkerBarState::kExpanded:
      tools_bar_->SetVisible(false);
      marker_bar_->SetVisible(true);
      ink_pen_button_->SetVisible(true);
      marker_pen_button_->SetVisible(true);
      undo_button_->SetVisible(true);
      clear_all_markers_button_->SetVisible(true);
      caret_left_->SetVisible(true);
      caret_right_->SetVisible(false);
      for (auto* color_button : marker_color_buttons_)
        color_button->SetVisible(true);
      break;
  }
  GetWidget()->SetBounds(CalculateBoundsInScreen());
}

gfx::Rect ProjectorBarView::CalculateBoundsInScreen() const {
  auto preferred_size = CalculatePreferredSize();
  aura::Window* window = Shell::GetPrimaryRootWindow();
  gfx::Rect work_area =
      WorkAreaInsets::ForWindow(window)->user_work_area_bounds();

  gfx::Point origin;
  switch (bar_location_) {
    case BarLocation::kUpperLeft:
      origin =
          gfx::Point(work_area.x() + kBarMargin, work_area.y() + kBarMargin);
      break;
    case BarLocation::kUpperCenter:
      origin = gfx::Point(
          work_area.x() + (work_area.width() - preferred_size.width()) / 2,
          work_area.y() + kBarMargin);
      break;
    case BarLocation::kUpperRight:
      origin =
          gfx::Point(work_area.right() - preferred_size.width() - kBarMargin,
                     work_area.y() + kBarMargin);

      break;
    case BarLocation::kLowerRight:
      origin =
          gfx::Point(work_area.right() - preferred_size.width() - kBarMargin,
                     work_area.bottom() - preferred_size.height() - kBarMargin);
      break;
    case BarLocation::kLowerCenter:
      origin = gfx::Point(
          work_area.x() + (work_area.width() - preferred_size.width()) / 2,
          work_area.bottom() - preferred_size.height() - kBarMargin);
      break;
    case BarLocation::kLowerLeft:
      origin =
          gfx::Point(work_area.x() + kBarMargin,
                     work_area.bottom() - preferred_size.height() - kBarMargin);
      break;
  }

  return gfx::Rect(origin, preferred_size);
}

BEGIN_METADATA(ProjectorBarView, views::View)
END_METADATA

}  // namespace ash
