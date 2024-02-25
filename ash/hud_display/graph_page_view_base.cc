// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/hud_display/graph_page_view_base.h"

#include <utility>

#include "ash/hud_display/hud_constants.h"
#include "ash/hud_display/hud_properties.h"
#include "ash/hud_display/legend.h"
#include "ash/hud_display/reference_lines.h"
#include "ash/hud_display/solid_source_background.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
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
  METADATA_HEADER(MinMaxButton, views::ImageButton)

 public:
  explicit MinMaxButton(views::Button::PressedCallback callback)
      : views::ImageButton(std::move(callback)) {
    SetBorder(views::CreateEmptyBorder(kMinMaxButtonBorder));
    SetBackground(std::make_unique<SolidSourceBackground>(kHUDLegendBackground,
                                                          /*radius=*/0));
    SetProperty(kHUDClickHandler, HTCLIENT);

    SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
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

BEGIN_METADATA(MinMaxButton)
END_METADATA

void SetMinimizeIconToButton(views::ImageButton* button) {
  button->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(views::kWindowControlMinimizeIcon,
                                     kHUDDefaultColor, kMinMaxButtonIconSize));
}

void SetRestoreIconToButton(views::ImageButton* button) {
  button->SetImageModel(
      views::Button::ButtonState::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(views::kWindowControlRestoreIcon,
                                     kHUDDefaultColor, kMinMaxButtonIconSize));
}

}  // namespace

BEGIN_METADATA(GraphPageViewBase)
END_METADATA

GraphPageViewBase::GraphPageViewBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // There are two overlaid children: reference lines and legend container.
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // |ReferenceLines| object is added after this object is fully initialized,
  // but it should be located under control elements (or they will never receive
  // events). This way we need to create a separate container for it.
  reference_lines_container_ = AddChildView(std::make_unique<views::View>());
  reference_lines_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());

  // Legend is floating in its own container. Invisible border of
  // kLegendPositionOffset makes it float on top of the graph.
  constexpr int kLegendPositionOffset = 20;
  legend_container_ = AddChildView(std::make_unique<views::View>());
  legend_container_
      ->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical))
      ->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kStart);
  legend_container_->SetBorder(views::CreateEmptyBorder(kLegendPositionOffset));
  legend_container_->SetVisible(false);

  legend_min_max_button_ = legend_container_->AddChildView(
      std::make_unique<MinMaxButton>(base::BindRepeating(
          &GraphPageViewBase::OnButtonPressed, base::Unretained(this))));

  legend_min_max_button_->SetTooltipText(u"Trigger graph legend");
  SetMinimizeIconToButton(legend_min_max_button_);
}

GraphPageViewBase::~GraphPageViewBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

void GraphPageViewBase::OnButtonPressed() {
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

ReferenceLines* GraphPageViewBase::CreateReferenceLines(
    float left,
    float top,
    float right,
    float bottom,
    const std::u16string& x_unit,
    const std::u16string& y_unit,
    int horizontal_points_number,
    int horizontal_ticks_interval,
    float vertical_ticks_interval) {
  DCHECK(reference_lines_container_->children().empty());
  return reference_lines_container_->AddChildView(
      std::make_unique<ReferenceLines>(
          left, top, right, bottom, x_unit, y_unit, horizontal_points_number,
          horizontal_ticks_interval, vertical_ticks_interval));
}

void GraphPageViewBase::RefreshLegendValues() {
  if (legend_)
    legend_->RefreshValues();
}

}  // namespace hud_display
}  // namespace ash
