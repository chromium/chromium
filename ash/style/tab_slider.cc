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
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/focus/focus_manager.h"
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
  METADATA_HEADER(SelectorView, views::View)

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
  raw_ptr<TabSliderButton> button_ = nullptr;
};

BEGIN_METADATA(TabSlider, SelectorView)
END_METADATA

//------------------------------------------------------------------------------
// TabSlider:

TabSlider::TabSlider(size_t max_tab_num, const InitParams& params)
    : max_tab_num_(max_tab_num),
      params_(params),
      selector_view_(AddChildView(
          std::make_unique<SelectorView>(params.has_selector_animation))) {
  // Add a fully rounded rect background if needed.
  if (params_.has_background) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetBackground(StyleUtil::CreateThemedFullyRoundedRectBackground(
        kSliderBackgroundColorId));
  }

  Init();

  selector_view_->SetProperty(views::kViewIgnoredByLayoutKey, true);

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

void TabSlider::OnButtonSelected(TabSliderButton* button) {
  DCHECK(button);
  DCHECK(base::Contains(buttons_, button));
  DCHECK(button->selected());

  // Deselect all the other buttons and check if the tab slider has focus.
  bool has_focus = false;
  for (ash::TabSliderButton* b : buttons_) {
    b->SetSelected(b == button);
    has_focus |= b->HasFocus();
  }

  // Move the selector to the selected button.
  selector_view_->MoveToSelectedButton(button);

  // Move the focus to the selected button.
  if (has_focus) {
    GetFocusManager()->SetFocusedView(button);
  }
}

void TabSlider::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // Synchronize the selector bounds with selected button's bounds.
  auto it =
      std::find_if(buttons_.begin(), buttons_.end(),
                   [](TabSliderButton* button) { return button->selected(); });
  if (it == buttons_.end()) {
    return;
  }
  selector_view_->SetBoundsRect((*it)->bounds());
}

void TabSlider::Init() {
  const int internal_border_padding = params_.internal_border_padding;

  // Create rows:
  // Add top border padding row.
  AddPaddingRow(views::TableLayout::kFixedSize, internal_border_padding);
  // Add middle buttons row.
  AddRows(1, views::TableLayout::kFixedSize);
  // Add bottom border padding row.
  AddPaddingRow(views::TableLayout::kFixedSize, internal_border_padding);

  // Create columns:
  // Add left border padding column.
  AddPaddingColumn(views::TableLayout::kFixedSize, internal_border_padding);
  // Alternatively add button column and padding column.
  std::vector<size_t> columns_containing_buttons;
  for (size_t i = 0; i < max_tab_num_; ++i) {
    AddColumn(views::LayoutAlignment::kStretch, views::LayoutAlignment::kCenter,
              1.0f, views::TableLayout::ColumnSize::kUsePreferred, 0, 0);
    columns_containing_buttons.push_back(2 * i + 1);
    if (i != max_tab_num_ - 1) {
      AddPaddingColumn(views::TableLayout::kFixedSize,
                       params_.between_buttons_spacing);
    }
  }
  // Add right border padding column.
  AddPaddingColumn(views::TableLayout::kFixedSize, internal_border_padding);

  if (params_.distribute_space_evenly) {
    // Ensure extra space is spread evenly between the button containing
    // columns.
    LinkColumnSizes(columns_containing_buttons);
  }
}

void TabSlider::AddButtonInternal(TabSliderButton* button) {
  CHECK(button);
  CHECK_LT(buttons_.size(), max_tab_num_)
      << "Number of buttons reaches the limit";

  // Add the button as a child of the tab slider and insert it in the
  // `buttons_` list.
  AddChildView(button);
  buttons_.emplace_back(button);
  button->AddedToSlider(this);
}

void TabSlider::OnEnabledStateChanged() {
  // Propagate the enabled state to all slider buttons and the selector view.
  const bool enabled = GetEnabled();

  for (ash::TabSliderButton* b : buttons_) {
    b->SetEnabled(enabled);
  }

  selector_view_->SetEnabled(enabled);
  SchedulePaint();
}

BEGIN_METADATA(TabSlider)
END_METADATA

}  // namespace ash
