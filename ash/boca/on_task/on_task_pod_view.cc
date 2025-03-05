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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// Border radius for the OnTask pod.
constexpr int kPodBorderRadius = 12;

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

}  // namespace

OnTaskPodView::OnTaskPodView(OnTaskPodController* pod_controller)
    : pod_controller_(pod_controller) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
  SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kPodBorderRadius));

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
  left_separator_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  left_separator_->SetPreferredLength(GetPreferredSize().height());

  reload_tab_button_ = AddChildView(CreateIconButton(
      base::BindRepeating(&OnTaskPodController::ReloadCurrentPage,
                          base::Unretained(pod_controller_)),
      &kKsvReloadIcon, IDS_ON_TASK_POD_RELOAD_ACCESSIBLE_NAME,
      /*is_togglable=*/false));
}

void OnTaskPodView::ToggleSnapLocation() {
  snap_pod_button_->SetToggled(!snap_pod_button_->toggled());
  if (snap_pod_button_->toggled()) {
    pod_controller_->SetSnapLocation(OnTaskPodSnapLocation::kTopRight);
  } else {
    pod_controller_->SetSnapLocation(OnTaskPodSnapLocation::kTopLeft);
  }
}

BEGIN_METADATA(OnTaskPodView)
END_METADATA

}  // namespace ash
