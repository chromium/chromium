// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/boca/on_task/on_task_pod_view.h"

#include <memory>
#include <string>

#include "ash/boca/on_task/on_task_pod_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/icon_button.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// Parameters for the OnTask pod.
constexpr int kPodBorderRadius = 26;
constexpr int kPodVerticalPadding = 10;
constexpr int kPodHorizontalPadding = 12;
constexpr int kPodElementSpace = 8;

// Parameters for the separator in the OnTask pod.
constexpr int kSeparatorVerticalPadding = 0;
constexpr int kSeparatorHorizontalPadding = 4;

// Parameters for the label button in the OnTask pod.
constexpr int kLabelButtonRadius = 16;
constexpr int kLabelButtonTopPadding = 0;
constexpr int kLabelButtonLeftPadding = 12;
constexpr int kLabelButtonButtomPadding = 0;
constexpr int kLabelButtonRightPadding = 16;
constexpr int kLabelButtonHeight = 32;
constexpr int kLabelButtonWidth = 120;
constexpr int kLabelButtonIconTextSpace = 8;

std::unique_ptr<IconButton> CreateIconButton(base::RepeatingClosure callback,
                                             const gfx::VectorIcon* icon,
                                             int accessible_name_id,
                                             bool is_togglable) {
  auto button = std::make_unique<IconButton>(
      std::move(callback), IconButton::Type::kMedium, icon, accessible_name_id,
      is_togglable, /*has_border=*/false);
  button->SetIconColor(cros_tokens::kCrosSysOnSurface);
  button->SetBackgroundColor(SK_ColorTRANSPARENT);
  return button;
}

std::unique_ptr<views::LabelButton> CreateLabelButton(
    base::RepeatingClosure callback,
    const std::u16string& text,
    const gfx::VectorIcon* icon) {
  auto button = std::make_unique<views::LabelButton>(std::move(callback), text);
  button->SetImageModel(views::Button::STATE_NORMAL,
                        ui::ImageModel::FromVectorIcon(*icon, ui::kColorIcon));
  button->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kLabelButtonTopPadding, kLabelButtonLeftPadding,
                        kLabelButtonButtomPadding, kLabelButtonRightPadding)));
  button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  button->SetImageLabelSpacing(kLabelButtonIconTextSpace);
  button->SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBaseOpaque, kLabelButtonRadius));
  return button;
}

}  // namespace

OnTaskPodView::OnTaskPodView(OnTaskPodController* pod_controller)
    : pod_controller_(pod_controller) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
  SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kPodBorderRadius));
  SetInsideBorderInsets(
      gfx::Insets::VH(kPodVerticalPadding, kPodHorizontalPadding));
  SetBetweenChildSpacing(kPodElementSpace);

  AddShortcutButtons();
}

OnTaskPodView::~OnTaskPodView() = default;

void OnTaskPodView::AddShortcutButtons() {
  snap_pod_button_ = AddChildView(CreateIconButton(
      base::BindRepeating(&OnTaskPodView::ToggleSnapLocation,
                          weak_ptr_factory_.GetWeakPtr()),
      &kKsvArrowRightIcon, IDS_ON_TASK_POD_TOGGLE_SNAP_LOCATION_ACCESSIBLE_NAME,
      /*is_togglable=*/true));
  snap_pod_button_->SetToggledVectorIcon(kKsvArrowLeftIcon);
  snap_pod_button_->SetIconToggledColor(
      cros_tokens::kCrosSysSystemOnPrimaryContainer);
  snap_pod_button_->SetBackgroundToggledColor(
      cros_tokens::kCrosSysSystemPrimaryContainer);

  left_separator_ = AddChildView(std::make_unique<views::Separator>());
  left_separator_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kSeparatorVerticalPadding, kSeparatorHorizontalPadding)));
  left_separator_->SetColorId(cros_tokens::kCrosSysSeparator);
  left_separator_->SetPreferredLength(kLabelButtonHeight);

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
  right_separator_->SetPreferredLength(kLabelButtonHeight);

  pin_tab_strip_button_ = AddChildView(CreateLabelButton(
      base::BindRepeating(&OnTaskPodView::UpdatePinTabStripButton,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_ON_TASK_POD_PIN_TAP_STRIP_ACCESSIBLE_NAME),
      &kUnpinnedIcon));
  pin_tab_strip_button_->SetPreferredSize(
      gfx::Size(kLabelButtonWidth, kLabelButtonHeight));
  pin_tab_strip_button_->SetEnabled(
      pod_controller_->CanToggleTabStripVisibility());
}

void OnTaskPodView::UpdatePinTabStripButton() {
  should_show_tab_strip_ = !should_show_tab_strip_;
  pod_controller_->ToggleTabStripVisibility(should_show_tab_strip_);

  if (should_show_tab_strip_) {
    // Button is "Hide tabs" when the tab strip is already shown.
    pin_tab_strip_button_->SetText(l10n_util::GetStringUTF16(
        IDS_ON_TASK_POD_UNPIN_TAP_STRIP_ACCESSIBLE_NAME));
    pin_tab_strip_button_->SetBackground(views::CreateRoundedRectBackground(
        cros_tokens::kCrosSysSystemPrimaryContainer, kLabelButtonRadius));
    pin_tab_strip_button_->SetEnabledTextColors(
        cros_tokens::kCrosSysSystemOnPrimaryContainer);
  } else {
    // Otherwise, button is "Show tabs" when the tab strip is already hidden.
    pin_tab_strip_button_->SetText(l10n_util::GetStringUTF16(
        IDS_ON_TASK_POD_PIN_TAP_STRIP_ACCESSIBLE_NAME));
    pin_tab_strip_button_->SetBackground(views::CreateRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBaseOpaque, kLabelButtonRadius));
    pin_tab_strip_button_->SetEnabledTextColors(cros_tokens::kCrosSysOnSurface);
  }
}

void OnTaskPodView::ToggleSnapLocation() {
  snap_pod_button_->SetToggled(!snap_pod_button_->toggled());
  if (snap_pod_button_->toggled()) {
    pod_controller_->SetSnapLocation(OnTaskPodSnapLocation::kTopRight);
  } else {
    pod_controller_->SetSnapLocation(OnTaskPodSnapLocation::kTopLeft);
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
  pin_tab_strip_button_->SetEnabled(
      pod_controller_->CanToggleTabStripVisibility());
}

BEGIN_METADATA(OnTaskPodView)
END_METADATA

}  // namespace ash
