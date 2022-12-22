// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/tab_slider.h"

#include "ash/style/style_util.h"
#include "ash/style/tab_slider_button.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform_util.h"
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
        gfx::RectF(button_->bounds()), gfx::RectF(previous_button->bounds()));
    view_layer->SetTransform(reverse_transform);
    ui::ScopedLayerAnimationSettings settings(view_layer->GetAnimator());
    settings.SetTransitionDuration(kSelectorAnimationDuration);
    view_layer->SetTransform(gfx::Transform());
  }

 private:
  // Indicates if there is a movement animation.
  const bool has_animation_;
  // Now owned.
  TabSliderButton* button_ = nullptr;
};

//------------------------------------------------------------------------------
// TabSlider:

TabSlider::TabSlider(bool has_background, bool has_selector_animation)
    : selector_view_(AddChildView(
          std::make_unique<SelectorView>(has_selector_animation))) {
  // Add a fully rounded rect background if needed.
  if (has_background) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetBackground(StyleUtil::CreateThemedFullyRoundedRectBackground(
        kSliderBackgroundColorId));
  }

  // Make the selector view ignored by layout, since the selector bounds should
  // be in sync with the selected button's bounds.
  selector_view_->SetProperty(views::kViewIgnoredByLayoutKey, true);

  // Center the slider buttons within the container's box layout.
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);

  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      &TabSlider::OnEnabledStateChanged, base::Unretained(this)));
}

TabSlider::~TabSlider() = default;

void TabSlider::SetCustomLayout(const LayoutParams& layout_params) {
  // Configure the layout with the custom layout parameters.
  custom_layout_params_ = layout_params;
  SetInsideBorderInsets(
      gfx::Insets(custom_layout_params_->internal_border_padding));
  SetBetweenChildSpacing(custom_layout_params_->between_buttons_spacing);
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
  BoxLayoutView::Layout();

  // Synchronize the selector bounds with selected button's bounds.
  for (auto* b : buttons_) {
    if (b->selected()) {
      selector_view_->SetBoundsRect(b->bounds());
      return;
    }
  }
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

  // When adding a button, the slider's layout should be updated according to
  // the button's recommended slider layout if the custom layout is not set.
  if (custom_layout_params_) {
    return;
  }

  auto recommended_layout = button->GetRecommendedSliderLayout();
  if (!recommended_layout) {
    return;
  }

  // Update the inside border, if the spacing between the button and the slider
  // is less than the recommended inner border padding.
  const int current_internal_border_padding =
      (GetPreferredSize().height() - button->GetPreferredSize().height()) / 2;
  if (current_internal_border_padding <
      recommended_layout->internal_border_padding) {
    SetInsideBorderInsets(
        gfx::Insets(recommended_layout->internal_border_padding));
  }

  // Update the between buttons spacing, if it is less than the recommended one.
  SetBetweenChildSpacing(std::max(recommended_layout->between_buttons_spacing,
                                  GetBetweenChildSpacing()));
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
