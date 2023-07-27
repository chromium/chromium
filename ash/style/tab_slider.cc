// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/tab_slider.h"

#include <cstddef>

#include "ash/style/style_util.h"
#include "ash/style/tab_slider_button.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr ui::ColorId kSliderBackgroundColorId =
    cros_tokens::kCrosSysSystemOnBase;
constexpr ui::ColorId kSelectorBackgroundColorId =
    cros_tokens::kCrosSysSystemPrimaryContainer;

constexpr base::TimeDelta kSelectorAnimationDuration = base::Milliseconds(150);

}  // namespace

//------------------------------------------------------------------------------
// TabSlider::SelectorView:

// The selector shows behind the selected slider button. When a button is
// selected, it moves from the previously selected button to the currently
// selected button.
class TabSlider::SelectorView : public views::View {
 public:
  explicit SelectorView(bool has_animation) : has_animation_(has_animation) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetBackground(StyleUtil::CreateThemedFullyRoundedRectBackground(
        kSelectorBackgroundColorId));
  }

  SelectorView(const SelectorView&) = delete;
  SelectorView& operator=(const SelectorView&) = delete;
  ~SelectorView() override = default;

  // Moves the selector to the selected button. Performs animation if
  // `has_animation_` is true.
  void MoveToSelectedButton(TabSliderButton* button) {
    DCHECK(button);
    DCHECK(button->selected());

    if (button_ == button) {
      return;
    }

    TabSliderButton* previous_button = button_;
    button_ = button;

    // Update selector's bounds with the selected button's bounds.
    SetBoundsRect(button_->bounds());

    // Performs an animation of the selector moving from the position of last
    // selected button to the position of currently selected button, if needed.
    if (!previous_button || !has_animation_) {
      return;
    }

    auto* view_layer = layer();

    gfx::Transform reverse_transform = gfx::TransformBetweenRects(
        gfx::RectF(button_->GetMirroredBounds()),
        gfx::RectF(previous_button->GetMirroredBounds()));
    view_layer->SetTransform(reverse_transform);
    ui::ScopedLayerAnimationSettings settings(view_layer->GetAnimator());
    settings.SetTransitionDuration(kSelectorAnimationDuration);
    view_layer->SetTransform(gfx::Transform());
  }

 private:
  // Indicates if there is a movement animation.
  const bool has_animation_;
  // Now owned.
  raw_ptr<TabSliderButton, ExperimentalAsh> button_ = nullptr;
};

//------------------------------------------------------------------------------
// TabSlider:

TabSlider::TabSlider(bool has_background,
                     bool has_selector_animation,
                     bool distribute_space_evenly)
    : selector_view_(
          AddChildView(std::make_unique<SelectorView>(has_selector_animation))),
      distribute_space_evenly_(distribute_space_evenly) {
  // Add a fully rounded rect background if needed.
  if (has_background) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetBackground(StyleUtil::CreateThemedFullyRoundedRectBackground(
        kSliderBackgroundColorId));
  }

  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      &TabSlider::OnEnabledStateChanged, base::Unretained(this)));
}

TabSlider::~TabSlider() = default;

views::View* TabSlider::GetSelectorView() {
  return selector_view_;
}

TabSliderButton* TabSlider::GetButtonAtIndex(size_t index) {
  CHECK(index < buttons_.size());
  return buttons_[index];
}

void TabSlider::SetCustomLayout(const LayoutParams& layout_params) {
  use_button_recommended_layout_ = false;

  // Configure the layout with the custom layout parameters.
  custom_layout_params_ = layout_params;
  UpdateLayout();
}

void TabSlider::OnButtonSelected(TabSliderButton* button) {
  DCHECK(button);
  DCHECK(base::Contains(buttons_, button));
  DCHECK(button->selected());

  // Deselect all the other buttons.
  for (auto* b : buttons_) {
    b->SetSelected(b == button);
  }

  // Move the selector to the selected button.
  selector_view_->MoveToSelectedButton(button);
}

void TabSlider::Layout() {
  views::View::Layout();

  // Synchronize the selector bounds with selected button's bounds.
  auto it =
      std::find_if(buttons_.begin(), buttons_.end(),
                   [](TabSliderButton* button) { return button->selected(); });
  if (it == buttons_.end()) {
    return;
  }
  selector_view_->SetBoundsRect((*it)->bounds());
}

void TabSlider::AddButtonInternal(TabSliderButton* button) {
  DCHECK(button);
  // Add the button as a child of the tab slider and insert it in the
  // `buttons_` list.
  AddChildView(button);
  buttons_.emplace_back(button);
  button->AddedToSlider(this);
  OnButtonAdded(button);
}

void TabSlider::OnButtonAdded(TabSliderButton* button) {
  DCHECK(button);

  // Always update the layout, at a minimum a new column will need to be added.
  base::ScopedClosureRunner scoped_runner(
      base::BindOnce(&TabSlider::UpdateLayout, base::Unretained(this)));

  // `SetCustomLayout()` results in child button's requested layout being
  // ignored.
  if (!use_button_recommended_layout_) {
    return;
  }

  auto recommended_layout = button->GetRecommendedSliderLayout();
  if (!recommended_layout) {
    return;
  }

  custom_layout_params_.internal_border_padding =
      std::max(recommended_layout->internal_border_padding,
               custom_layout_params_.internal_border_padding);
  custom_layout_params_.between_buttons_spacing =
      std::max(recommended_layout->between_buttons_spacing,
               custom_layout_params_.between_buttons_spacing);
}

void TabSlider::UpdateLayout() {
  // Update the layout based on how many buttons exist, `custom_layout_params_`,
  // and `distribute_space_evenly_`.
  auto* table_layout = SetLayoutManager(std::make_unique<views::TableLayout>());

  // Explicitly mark this view as ignored because
  // `views::kViewIgnoredByLayoutKey` is not supported by `views::TableLayout`.
  table_layout->SetChildViewIgnoredByLayout(selector_view_, /*ignored=*/true);

  size_t column_index = 0;
  table_layout
      ->AddPaddingRow(views::TableLayout::kFixedSize,
                      custom_layout_params_.internal_border_padding)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        custom_layout_params_.internal_border_padding);
  column_index++;

  // Keep track of columns with buttons so their sizes can be linked if
  // necessary.
  std::vector<size_t> columns_containing_buttons;
  for (size_t i = 0; i < buttons_.size(); ++i) {
    columns_containing_buttons.push_back(column_index);
    table_layout->AddColumn(
        views::LayoutAlignment::kStretch, views::LayoutAlignment::kCenter, 1.0f,
        views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
    column_index++;
    if (i < buttons_.size() - 1) {
      table_layout->AddPaddingColumn(
          views::TableLayout::kFixedSize,
          custom_layout_params_.between_buttons_spacing);
      column_index++;
    }
  }

  // Add the row buttons will live in.
  table_layout->AddRows(1, views::TableLayout::kFixedSize);
  table_layout
      ->AddPaddingRow(views::TableLayout::kFixedSize,
                      custom_layout_params_.internal_border_padding)
      .AddPaddingColumn(views::TableLayout::kFixedSize,
                        custom_layout_params_.internal_border_padding);
  if (distribute_space_evenly_) {
    // Ensure extra space is spread evenly between the button containing
    // columns.
    table_layout->LinkColumnSizes(columns_containing_buttons);
  }
}

void TabSlider::OnEnabledStateChanged() {
  // Propagate the enabled state to all slider buttons and the selector view.
  const bool enabled = GetEnabled();

  for (auto* b : buttons_) {
    b->SetEnabled(enabled);
  }

  selector_view_->SetEnabled(enabled);
  SchedulePaint();
}

BEGIN_METADATA(TabSlider, views::View)
END_METADATA

}  // namespace ash
