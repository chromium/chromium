// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/boca/on_task/on_task_pod_view.h"

#include <memory>
#include <string>

#include "ash/boca/on_task/on_task_pod_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/system_shadow.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

std::unique_ptr<IconButton> CreateIconButton(base::RepeatingClosure callback,
                                             const gfx::VectorIcon* icon,
                                             int accessible_name_id,
                                             bool is_togglable) {
  auto button = std::make_unique<IconButton>(
      std::move(callback), IconButton::Type::kMedium, icon, accessible_name_id,
      is_togglable, /*has_border=*/false);
  button->SetIconColor(cros_tokens::kCrosSysOnSurface);
  button->SetBackgroundColor(SK_ColorTRANSPARENT);
  // Set up highlight for button hover and press.
  StyleUtil::SetUpInkDropForButton(button.get(), gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/false);
  button->SetAnimateOnStateChange(true);
  button->SetHasInkDropActionOnClick(true);
  return button;
}

}  // namespace

OnTaskPodView::OnTaskPodView(OnTaskPodController* pod_controller)
    : pod_controller_(pod_controller),
      // Since this view has fully circular rounded corners, we can't use a
      // nine patch layer for the shadow. We have to use the
      // `ShadowOnTextureLayer`. For more info, see https://crbug.com/1308800.
      shadow_(SystemShadow::CreateShadowOnTextureLayer(
          SystemShadow::Type::kElevation4)) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  shadow_->SetRoundedCornerRadius(kPodBorderRadius);
  SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevatedOpaque, kPodBorderRadius));
  SetInsideBorderInsets(
      gfx::Insets::VH(kPodVerticalPadding, kPodHorizontalPadding));
  SetBetweenChildSpacing(kPodElementSpace);

  AddShortcutButtons();
}

OnTaskPodView::~OnTaskPodView() = default;

void OnTaskPodView::AddShortcutButtons() {
  pod_position_slider_ = AddChildView(std::make_unique<TabSlider>(
      /*max_tab_num=*/2,
      TabSlider::InitParams{/*internal_border_padding=*/0,
                            /*between_child_spacing=*/0,
                            /*has_background=*/true,
                            /*has_selector_animation=*/true,
                            /*distribute_space_evenly=*/true}));
  dock_left_button_ = pod_position_slider_->AddButton<IconSliderButton>(
      base::BindRepeating(&OnTaskPodController::SetSnapLocation,
                          base::Unretained(pod_controller_),
                          OnTaskPodSnapLocation::kTopLeft),
      &kOnTaskPodPositionTopLeftIcon,
      l10n_util::GetStringUTF16(IDS_ON_TASK_MOVE_POD_TOP_LEFT_ACCESSIBLE_NAME));
  dock_right_button_ = pod_position_slider_->AddButton<IconSliderButton>(
      base::BindRepeating(&OnTaskPodController::SetSnapLocation,
                          base::Unretained(pod_controller_),
                          OnTaskPodSnapLocation::kTopRight),
      &kOnTaskPodPositionTopRightIcon,
      l10n_util::GetStringUTF16(
          IDS_ON_TASK_MOVE_POD_TOP_RIGHT_ACCESSIBLE_NAME));
  dock_left_button_->SetSelected(true);

  left_separator_ = AddChildView(std::make_unique<views::Separator>());
  left_separator_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kSeparatorVerticalPadding, kSeparatorHorizontalPadding)));
  left_separator_->SetColorId(cros_tokens::kCrosSysSeparator);
  left_separator_->SetPreferredLength(kSeparatorHeight);

  back_button_ = AddChildView(CreateIconButton(
      base::BindRepeating(&OnTaskPodController::MaybeNavigateToPreviousPage,
                          base::Unretained(pod_controller_)),
      &kKsvBrowserBackIcon, IDS_ON_TASK_POD_NAVIGATE_BACK_ACCESSIBLE_NAME,
      /*is_togglable=*/false));
  forward_button_ = AddChildView(CreateIconButton(
      base::BindRepeating(&OnTaskPodController::MaybeNavigateToNextPage,
                          base::Unretained(pod_controller_)),
      &kKsvBrowserForwardIcon, IDS_ON_TASK_POD_NAVIGATE_FORWARD_ACCESSIBLE_NAME,
      /*is_togglable=*/false));
  reload_tab_button_ = AddChildView(CreateIconButton(
      base::BindRepeating(&OnTaskPodController::ReloadCurrentPage,
                          base::Unretained(pod_controller_)),
      &kKsvReloadIcon, IDS_ON_TASK_POD_RELOAD_ACCESSIBLE_NAME,
      /*is_togglable=*/false));

  right_separator_ = AddChildView(std::make_unique<views::Separator>());
  right_separator_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kSeparatorVerticalPadding, kSeparatorHorizontalPadding)));
  right_separator_->SetColorId(cros_tokens::kCrosSysSeparator);
  right_separator_->SetPreferredLength(kSeparatorHeight);
  right_separator_->SetVisible(pod_controller_->CanToggleTabStripVisibility());

  pin_tab_strip_button_ = AddChildView(CreateIconButton(
      base::BindRepeating(&OnTaskPodView::UpdatePinTabStripButton,
                          base::Unretained(this), true),
      &kOnTaskPodTabsIcon, IDS_ON_TASK_POD_PIN_TAP_STRIP_ACCESSIBLE_NAME,
      /*is_togglable=*/true));
  pin_tab_strip_button_->SetVisible(
      pod_controller_->CanToggleTabStripVisibility());
}

void OnTaskPodView::AddedToWidget() {
  views::BoxLayoutView::AddedToWidget();

  // Since the layer of the shadow has to be added as a sibling to
  // `on_task_pod_view` layer, we need to wait until the view is added to the
  // widget.
  auto* const parent = layer()->parent();
  ui::Layer* const shadow_layer = shadow_->GetLayer();
  parent->Add(shadow_layer);
  parent->StackAtBottom(shadow_layer);

  // Make the shadow observe the color provider source change to update the
  // colors.
  shadow_->ObserveColorProviderSource(GetWidget());
}

void OnTaskPodView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // The shadow layer is a sibling of `on_task_pod_view` layer, and should have
  // the same bounds.
  shadow_->SetContentBounds(layer()->bounds());
}

void OnTaskPodView::UpdatePinTabStripButton(bool user_action) {
  should_show_tab_strip_ = !should_show_tab_strip_;
  pin_tab_strip_button_->SetToggled(should_show_tab_strip_);
  pod_controller_->ToggleTabStripVisibility(should_show_tab_strip_,
                                            user_action);

  if (should_show_tab_strip_) {
    // Button is "Hide tabs" when the tab strip is already shown.
    pin_tab_strip_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ON_TASK_POD_UNPIN_TAP_STRIP_ACCESSIBLE_NAME));
  } else {
    // Otherwise, button is "Show tabs" when the tab strip is already hidden.
    pin_tab_strip_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ON_TASK_POD_PIN_TAP_STRIP_ACCESSIBLE_NAME));
  }
}

void OnTaskPodView::OnPageNavigationContextUpdate() {
  back_button_->SetEnabled(pod_controller_->CanNavigateToPreviousPage());
  if (!back_button_->GetEnabled()) {
    back_button_->SetBackground(nullptr);
  }
  forward_button_->SetEnabled(pod_controller_->CanNavigateToNextPage());
  if (!forward_button_->GetEnabled()) {
    forward_button_->SetBackground(nullptr);
  }
}

void OnTaskPodView::OnLockedModeUpdate() {
  const bool can_toggle = pod_controller_->CanToggleTabStripVisibility();
  right_separator_->SetVisible(can_toggle);
  pin_tab_strip_button_->SetVisible(can_toggle);
  if (can_toggle) {
    // `should_show_tab_strip_` is set to true to ensure it is toggled in
    // `UpdatePinTabStripButton()` to by default hide the tab strip when
    // entering locked mode.
    should_show_tab_strip_ = true;
    UpdatePinTabStripButton(false);
  }
}

BEGIN_METADATA(OnTaskPodView)
END_METADATA

}  // namespace ash
