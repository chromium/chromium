// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/ui/projector_bar_view.h"

#include "ash/projector/projector_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr float kBarRadius = 20.f;
constexpr int kBarAlpha_ = 230;  // 90% opacity

constexpr int kBarWidth = 50;
constexpr gfx::Insets kBarPadding{/*vertical=*/15, /*horizontal=*/5};
constexpr int kBarLeftMargin = 10;

// The spacing used by the BoxLayout manager to space out child views in the
// |ProjectorBarView|.
constexpr int kBetweenChildSpacing = 8;

// Recording buttons.
constexpr int kRecordingButtonColorViewSize = 12;
constexpr int kStartRecordingButtonColorViewRadius = 6;
constexpr int kStopRecordingButtonColorViewRadius = 2;

}  // namespace

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
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.context = Shell::Get()->GetRootWindowForNewWindows();

  auto screen_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  int height = bar_view->GetHeightForWidth(kBarWidth);
  auto origin =
      gfx::Point(kBarLeftMargin, (screen_bounds.height() - height) / 2);
  auto size = gfx::Size(kBarWidth, height);
  params.bounds = gfx::Rect(origin, size);

  auto widget = views::UniqueWidgetPtr(
      std::make_unique<views::Widget>(std::move(params)));
  widget->SetContentsView(std::move(bar_view));
  return widget;
}

void ProjectorBarView::OnLaserPointerStateChanged(bool enabled) {
  laser_pointer_button_->SetToggled(enabled);
}

void ProjectorBarView::OnMarkerStateChanged(bool enabled) {
  marker_button_->SetToggled(enabled);

  // TODO(llin): shows the marker submenu if marker is enabled.
}

void ProjectorBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateVectorIcon();
}

void ProjectorBarView::InitLayout() {
  // Set up layout manager.
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBarPadding,
      kBetweenChildSpacing));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Set background color.
  SetBackground(views::CreateBackgroundFromPainter(
      views::Painter::CreateSolidRoundRectPainter(
          SkColorSetA(gfx::kGoogleGrey900, kBarAlpha_), kBarRadius)));

  // Add Drag handle.
  drag_handle_ = AddChildView(std::make_unique<views::ImageView>());
  UpdateVectorIcon();

  // Add recording buttons.
  record_button_ = AddChildView(std::make_unique<ProjectorColorButton>(
      base::BindRepeating(&ProjectorBarView::OnRecordButtonPressed,
                          base::Unretained(this)),
      SK_ColorWHITE, kRecordingButtonColorViewSize,
      kStartRecordingButtonColorViewRadius));
  stop_button_ = AddChildView(std::make_unique<ProjectorColorButton>(
      base::BindRepeating(&ProjectorBarView::OnStopButtonPressed,
                          base::Unretained(this)),
      SK_ColorRED, kRecordingButtonColorViewSize,
      kStopRecordingButtonColorViewRadius)),
  stop_button_->SetVisible(false);

  // Add key idea button.
  key_idea_button_ = AddChildView(std::make_unique<ProjectorImageButton>(
      base::BindRepeating(&ProjectorBarView::OnKeyIdeaButtonPressed,
                          base::Unretained(this)),
      kProjectorKeyIdeaIcon));

  // Add laser pointer button.
  laser_pointer_button_ = AddChildView(std::make_unique<ProjectorImageButton>(
      base::BindRepeating(&ProjectorBarView::OnLaserPointerPressed,
                          base::Unretained(this)),
      kPaletteTrayIconLaserPointerIcon));

  // Add marker button.
  marker_button_ = AddChildView(std::make_unique<ProjectorImageButton>(
      base::BindRepeating(&ProjectorBarView::OnMarkerPressed,
                          base::Unretained(this)),
      kProjectorMarkerIcon));
}

void ProjectorBarView::UpdateVectorIcon() {
  auto* color_provider = AshColorProvider::Get();
  const SkColor normal_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  drag_handle_->SetImage(
      gfx::CreateVectorIcon(kProjectorDragHandleIcon, normal_color));
}

void ProjectorBarView::OnRecordButtonPressed() {
  // TODO(crbug.com/1165435): Start the recording session and update the button
  // visibility based on recording state after integrating with capture mode and
  // recording service.
  record_button_->SetVisible(false);
  stop_button_->SetVisible(true);
}

void ProjectorBarView::OnStopButtonPressed() {
  // TODO(crbug.com/1165435): Stop the recording session and update the button
  // visibility based on recording state after integrating with capture mode and
  // recording service.
  record_button_->SetVisible(true);
  stop_button_->SetVisible(false);
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

BEGIN_METADATA(ProjectorBarView, views::View)
END_METADATA

}  // namespace ash
