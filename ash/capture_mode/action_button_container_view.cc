// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/action_button_container_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/capture_mode/action_button_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The horizontal distance between action buttons in a row.
constexpr int kActionButtonSpacing = 10;

// The animation duration for fading out old action buttons after the smart
// actions button is pressed.
constexpr base::TimeDelta kSmartActionsButtonTransitionFadeOutDuration =
    base::Milliseconds(100);

// The animation duration for fading in new icon buttons after the smart actions
// button is pressed.
constexpr base::TimeDelta kSmartActionsButtonTransitionFadeInDuration =
    base::Milliseconds(50);

// The animation duration for sliding in new icon buttons after the smart
// actions button is pressed.
constexpr base::TimeDelta kSmartActionsButtonTransitionSlideInDuration =
    base::Milliseconds(250);

}  // namespace

ActionButtonContainerView::ActionButtonContainerView() {
  SetUseDefaultFillLayout(true);
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .CopyAddressTo(&action_button_row_)
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(kActionButtonSpacing)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch)
          .Build());
}

ActionButtonContainerView::~ActionButtonContainerView() = default;

ActionButtonView* ActionButtonContainerView::AddActionButton(
    views::Button::PressedCallback callback,
    std::u16string text,
    const gfx::VectorIcon* icon,
    ActionButtonRank rank,
    ActionButtonViewID id) {
  // Collect the existing buttons and newly requested button, and sort them by
  // rank.
  std::vector<std::unique_ptr<ActionButtonView>> action_buttons;

  // Populate `action_buttons` with the existing action buttons, if any. We need
  // to copy the old action buttons vector since we will be removing buttons
  // from the original vector.
  views::View::Views old_action_buttons = GetActionButtons();
  for (views::View* action_button : old_action_buttons) {
    CHECK(action_button);
    action_buttons.push_back(action_button_row_->RemoveChildViewT(
        views::AsViewClass<ActionButtonView>(action_button)));
  }

  CHECK(GetActionButtons().empty());

  // Add the new action button to the vector so it can also be sorted.
  auto new_action_button =
      std::make_unique<ActionButtonView>(std::move(callback), text, icon, rank);
  new_action_button->SetID(id);
  ActionButtonView* new_action_button_ptr = new_action_button.get();
  action_buttons.push_back(std::move(new_action_button));

  // Sort the buttons by rank.
  auto rank_sort = [](const std::unique_ptr<ActionButtonView>& lhs,
                      const std::unique_ptr<ActionButtonView>& rhs) {
    return lhs->rank() < rhs->rank();
  };
  sort(action_buttons.begin(), action_buttons.end(), rank_sort);

  // Re-insert the buttons into the container view in sorted order from highest
  // to lowest. Higher ranked buttons should appear to the right of lower ranked
  // buttons, so insert new buttons on the left.
  for (std::unique_ptr<ActionButtonView>& action_button : action_buttons) {
    action_button_row_->AddChildView(std::move(action_button));
  }

  return new_action_button_ptr;
}

void ActionButtonContainerView::RemoveAllActionButtons() {
  action_button_row_->RemoveAllChildViews();
}

const views::View::Views& ActionButtonContainerView::GetActionButtons() const {
  return action_button_row_->children();
}

void ActionButtonContainerView::StartSmartActionsButtonTransition() {
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }

  SetWidgetEventsEnabled(false);
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(
          &ActionButtonContainerView::OnSmartActionsButtonFadedOut,
          weak_ptr_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kSmartActionsButtonTransitionFadeOutDuration)
      .SetOpacity(widget->GetLayer(), 0.0f, gfx::Tween::LINEAR);
}

void ActionButtonContainerView::OnSmartActionsButtonFadedOut() {
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }

  // Remove Scanner action buttons and keep other buttons. We need to copy the
  // old action buttons vector since we will be removing buttons from the
  // original vector.
  std::vector<std::unique_ptr<ActionButtonView>> action_buttons_to_keep;
  views::View::Views old_action_buttons = GetActionButtons();
  for (views::View* view : old_action_buttons) {
    auto action_button = action_button_row_->RemoveChildViewT(
        views::AsViewClass<ActionButtonView>(view));
    if (action_button->rank().type != ActionButtonType::kScanner) {
      action_buttons_to_keep.push_back(std::move(action_button));
    }
  }
  CHECK(GetActionButtons().empty());

  // Add the buttons to keep back into the action button container and
  // collapse them into icon buttons.
  for (std::unique_ptr<ActionButtonView>& action_button :
       action_buttons_to_keep) {
    action_button->CollapseToIconButton();
    action_button_row_->AddChildView(std::move(action_button));
  }

  // Compute bounds required to slide in the new icon buttons from the left edge
  // of the old action container bounds to the right edge.
  const gfx::Rect old_action_container_bounds =
      widget->GetWindowBoundsInScreen();
  const gfx::Size new_preferred_size = GetPreferredSize();
  const gfx::Vector2d slide_offset(
      old_action_container_bounds.width() - new_preferred_size.width(), 0);

  // Set the target bounds at the right edge.
  widget->SetBounds(gfx::Rect(
      old_action_container_bounds.origin() + slide_offset, new_preferred_size));

  // Set an initial translation so that the new icon buttons start sliding from
  // the left edge.
  gfx::Transform initial_translation;
  initial_translation.Translate(-slide_offset);
  ui::Layer* layer = widget->GetLayer();
  layer->SetTransform(initial_translation);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(
          base::BindOnce(&ActionButtonContainerView::SetWidgetEventsEnabled,
                         weak_ptr_factory_.GetWeakPtr(), true))
      .Once()
      .SetDuration(kSmartActionsButtonTransitionFadeInDuration)
      .SetOpacity(layer, 1.0f, gfx::Tween::LINEAR)
      .At(base::TimeDelta())
      .SetDuration(kSmartActionsButtonTransitionSlideInDuration)
      .SetTransform(layer, gfx::Transform(), gfx::Tween::ACCEL_LIN_DECEL_100);
}

void ActionButtonContainerView::SetWidgetEventsEnabled(bool enabled) {
  views::Widget* widget = GetWidget();
  if (!widget) {
    return;
  }
  widget->GetContentsView()->SetCanProcessEventsWithinSubtree(enabled);
  widget->GetNativeWindow()->SetEventTargetingPolicy(
      enabled ? aura::EventTargetingPolicy::kTargetAndDescendants
              : aura::EventTargetingPolicy::kNone);
}

BEGIN_METADATA(ActionButtonContainerView)
END_METADATA

}  // namespace ash
