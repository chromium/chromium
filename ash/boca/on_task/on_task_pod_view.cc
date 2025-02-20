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
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// Border radius for the OnTask pod.
constexpr int kPodBorderRadius = 4;

std::unique_ptr<IconButton> CreateIconButton(base::RepeatingClosure callback,
                                             const gfx::VectorIcon* icon,
                                             int accessible_name_id) {
  auto button = std::make_unique<IconButton>(
      std::move(callback), IconButton::Type::kMedium, icon, accessible_name_id);
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
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysPrimaryContainer, kPodBorderRadius));

  AddShortcutButtons();
}

OnTaskPodView::~OnTaskPodView() = default;

void OnTaskPodView::AddShortcutButtons() {
  reload_tab_button_ = AddChildView(CreateIconButton(
      base::BindRepeating(&OnTaskPodController::ReloadCurrentPage,
                          base::Unretained(pod_controller_)),
      &kKsvReloadIcon, IDS_ON_TASK_POD_RELOAD_ACCESSIBLE_NAME));
}

BEGIN_METADATA(OnTaskPodView)
END_METADATA

}  // namespace ash
