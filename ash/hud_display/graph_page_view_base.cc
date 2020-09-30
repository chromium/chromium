// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/graph_page_view_base.h"

#include "ash/hud_display/grid.h"
#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/hud_properties.h"
#include "ash/hud_display/legend.h"
#include "ash/hud_display/solid_source_background.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/window/vector_icons/vector_icons.h"

namespace ash {
namespace hud_display {

namespace {

constexpr int kMinMaxButtonIconSize = 10;
constexpr int kMinMaxButtonBorder = 5;

// ImageButton with underline
class MinMaxButton : public views::ImageButton {
 public:
  METADATA_HEADER(MinMaxButton);

  explicit MinMaxButton(views::ButtonListener* listener)
      : views::ImageButton(listener) {
    SetBorder(views::CreateEmptyBorder(gfx::Insets(kMinMaxButtonBorder)));
    SetBackground(std::make_unique<SolidSourceBackground>(kHUDLegendBackground,
                                                          /*radius=*/0));
    SetProperty(kHUDClickHandler, HTCLIENT);
  }

  MinMaxButton(const MinMaxButton&) = delete;
  MinMaxButton& operator=(const MinMaxButton&) = delete;

  ~MinMaxButton() override = default;

 protected:
  // ImageButton
  void PaintButtonContents(gfx::Canvas* canvas) override {
    views::ImageButton::PaintButtonContents(canvas);

    SkPath path;
    path.moveTo(0, height());
    path.lineTo(height(), width());

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setBlendMode(SkBlendMode::kSrc);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(1);
    flags.setColor(kHUDDefaultColor);
    canvas->DrawPath(path, flags);
  }
};

BEGIN_METADATA(MinMaxButton, views::ImageButton)
END_METADATA

void SetMinimizeIconToButton(views::ImageButton* button) {
  button->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kWindowControlMinimizeIcon,
                            kMinMaxButtonIconSize, kHUDDefaultColor));
}

void SetRestoreIconToButton(views::ImageButton* button) {
  button->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(views::kWindowControlRestoreIcon,
                            kMinMaxButtonIconSize, kHUDDefaultColor));
}

}  // namespace

BEGIN_METADATA(GraphPageViewBase, views::View)
END_METADATA

GraphPageViewBase::GraphPageViewBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // There are two overlaid children: grid and container for the legend.
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Grid is added after this object is fully initialized, but it should be
  // located under control elements (or they will never receive events). This
  // way we need to create a separate container for it.
  grid_container_ = AddChildView(std::make_unique<views::View>());
  grid_container_->SetLayoutManager(std::make_unique<views::FillLayout>());

  // Legend is floating in its own container. Invisible border of
  // kLegendPositionOffset makes it float on top of the graph.
  constexpr int kLegendPositionOffset = 20;
  legend_container_ = AddChildView(std::make_unique<views::View>());
  legend_container_
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);
  legend_container_->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kLegendPositionOffset)));
  legend_container_->SetVisible(false);

  legend_min_max_button_ =
      legend_container_->AddChildView(std::make_unique<MinMaxButton>(this));
  SetMinimizeIconToButton(legend_min_max_button_);
}

GraphPageViewBase::~GraphPageViewBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

void GraphPageViewBase::ButtonPressed(views::Button*, ui::Event const&) {
  if (legend_->GetVisible()) {
    legend_->SetVisible(false);
    SetRestoreIconToButton(legend_min_max_button_);
  } else {
    legend_->SetVisible(true);
    SetMinimizeIconToButton(legend_min_max_button_);
  }
}

void GraphPageViewBase::CreateLegend(
    const std::vector<Legend::Entry>& entries) {
  DCHECK(!legend_);
  legend_ = legend_container_->AddChildView(std::make_unique<Legend>(entries));
  legend_container_->SetVisible(true);
}

// Put grid in its dedicated container.
Grid* GraphPageViewBase::CreateGrid(float left,
                                    float top,
                                    float right,
                                    float bottom,
                                    const base::string16& x_unit,
                                    const base::string16& y_unit,
                                    int horizontal_points_number,
                                    int horizontal_ticks_interval) {
  DCHECK(grid_container_->children().empty());
  return grid_container_->AddChildView(std::make_unique<Grid>(
      left, top, right, bottom, x_unit, y_unit, horizontal_points_number,
      horizontal_ticks_interval));
}

void GraphPageViewBase::RefreshLegendValues() {
  if (legend_)
    legend_->RefreshValues();
}

}  // namespace hud_display
}  // namespace ash
