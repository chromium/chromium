// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/action_button_container_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/capture_mode/action_button_view.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Horizontal spacing between the error view and action buttons.
constexpr int kErrorViewActionButtonSpacing = 6;

constexpr auto kErrorViewBorderInsets = gfx::Insets::TLBR(8, 8, 8, 12);

constexpr int kErrorViewCornerRadius = 18;

constexpr int kErrorViewLeadingIconSize = 20;

// Padding to the right of the error view's leading icon, to separate the icon
// from the error message label.
constexpr auto kErrorViewLeadingIconRightPadding = 4;

// Padding around the try again link in the error view.
constexpr auto kErrorViewTryAgainLinkPadding = gfx::Insets::TLBR(0, 8, 0, 4);

// The horizontal distance between action buttons in a row.
constexpr int kActionButtonSpacing = 6;

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

ActionButtonContainerView::ErrorView::ErrorView()
    : shadow_(SystemShadow::CreateShadowOnTextureLayer(
          SystemShadow::Type::kElevation12)) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetInsideBorderInsets(kErrorViewBorderInsets);

  SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated,
      gfx::RoundedCornersF(kErrorViewCornerRadius)));
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  shadow_->SetRoundedCornerRadius(kErrorViewCornerRadius);
  capture_mode_util::SetHighlightBorder(
      this, kErrorViewCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderNoShadow);

  AddChildView(
      views::Builder<views::ImageView>()
          .SetPreferredSize(
              gfx::Size(kErrorViewLeadingIconSize, kErrorViewLeadingIconSize))
          .SetImage(ui::ImageModel::FromVectorIcon(
              kCaptureModeActionErrorIcon, cros_tokens::kCrosSysSecondary))
          .SetProperty(
              views::kMarginsKey,
              gfx::Insets::TLBR(0, 0, 0, kErrorViewLeadingIconRightPadding))
          .Build());

  AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&error_label_)
          .SetEnabledColor(cros_tokens::kCrosSysSecondary)
          .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosAnnotation1))
          .Build());

  AddChildView(
      views::Builder<views::Link>()
          .CopyAddressTo(&try_again_link_)
          .SetText(l10n_util::GetStringUTF16(
              IDS_ASH_SCANNER_ERROR_TRY_AGAIN_LINK_TEXT))
          .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosButton2))
          .SetEnabledColor(cros_tokens::kCrosSysPrimary)
          .SetForceUnderline(false)
          .SetProperty(views::kMarginsKey, kErrorViewTryAgainLinkPadding)
          .SetVisible(false)
          .Build());
  CaptureModeSessionFocusCycler::HighlightHelper::Install(try_again_link_);
}

ActionButtonContainerView::ErrorView::~ErrorView() = default;

void ActionButtonContainerView::ErrorView::SetVisible(bool visible) {
  views::BoxLayoutView::SetVisible(visible);
  shadow_->GetLayer()->SetVisible(visible);
}

void ActionButtonContainerView::ErrorView::AddedToWidget() {
  views::BoxLayoutView::AddedToWidget();

  // Since the layer of the shadow has to be added as a sibling to this view's
  // layer, we need to wait until the view is added to the widget.
  auto* parent = layer()->parent();
  ui::Layer* shadow_layer = shadow_->GetLayer();
  parent->Add(shadow_layer);
  parent->StackAtBottom(shadow_layer);

  // Make the shadow observe the color provider source change to update the
  // colors.
  shadow_->ObserveColorProviderSource(GetWidget());
}

void ActionButtonContainerView::ErrorView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  // The shadow layer is a sibling of this view's layer, and should have the
  // same bounds.
  shadow_->SetContentBounds(layer()->bounds());
}

void ActionButtonContainerView::ErrorView::SetErrorMessage(
    const std::u16string& error_message) {
  error_label_->SetText(error_message);
}

void ActionButtonContainerView::ErrorView::SetTryAgainCallback(
    base::RepeatingClosure try_again_callback) {
  try_again_link_->SetVisible(!try_again_callback.is_null());
  try_again_link_->SetCallback(std::move(try_again_callback));
}

std::u16string_view
ActionButtonContainerView::ErrorView::GetErrorMessageForTesting() const {
  return error_label_->GetText();
}

ActionButtonContainerView::ActionButtonContainerView() {
  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  box_layout->set_between_child_spacing(kErrorViewActionButtonSpacing);

  error_view_ = AddChildView(std::make_unique<ErrorView>());
  error_view_->SetVisible(false);

  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .CopyAddressTo(&action_button_row_)
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(kActionButtonSpacing)
          .SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch)
          // We use the `action_button_row_` layer to parent the action buttons'
          // shadows. This is to ensure the action button shadows are correctly
          // updated when `action_button_row_` bounds are updated.
          .SetPaintToLayer()
          .Build());
  action_button_row_->layer()->SetFillsBoundsOpaquely(false);
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

void ActionButtonContainerView::ClearContainer() {
  action_button_row_->RemoveAllChildViews();
  HideErrorView();
}

const views::View::Views& ActionButtonContainerView::GetActionButtons() const {
  return action_button_row_->children();
}

std::vector<views::View*> ActionButtonContainerView::GetFocusableViews() {
  std::vector<views::View*> focusable_views;
  views::View* try_again_link = error_view_->try_again_link();
  if (error_view_->GetVisible() && try_again_link->GetVisible()) {
    focusable_views.push_back(try_again_link);
  }
  for (auto action_button : GetActionButtons()) {
    if (action_button->GetEnabled()) {
      focusable_views.push_back(action_button);
    }
  }
  return focusable_views;
}

void ActionButtonContainerView::ShowErrorView(
    const std::u16string& error_message,
    base::RepeatingClosure try_again_callback) {
  error_view_->SetErrorMessage(error_message);
  error_view_->SetTryAgainCallback(std::move(try_again_callback));
  error_view_->SetVisible(true);
}

void ActionButtonContainerView::HideErrorView() {
  error_view_->SetVisible(false);
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

  RemoveSmartActionsButton();

  // Collapse the remaining buttons into icon buttons.
  for (views::View* view : GetActionButtons()) {
    auto* action_button = views::AsViewClass<ActionButtonView>(view);
    if (action_button->rank().type != ActionButtonType::kScanner) {
      action_button->CollapseToIconButton();
    }
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

void ActionButtonContainerView::RemoveSmartActionsButton() {
  // Note that `views::View::GetViewByID` recursively a view's children, so if
  // `ActionButtonView` contains any children with ID `kSmartActionsButton = 1`,
  // this will fail.
  // As of writing, `ActionButtonView`'s children all have an ID of 0.
  if (views::View* smart_actions_button = action_button_row_->GetViewByID(
          ActionButtonViewID::kSmartActionsButton)) {
    action_button_row_->RemoveChildViewT(smart_actions_button);
  }
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

BEGIN_METADATA(ActionButtonContainerView, ErrorView)
END_METADATA

BEGIN_METADATA(ActionButtonContainerView)
END_METADATA

}  // namespace ash
