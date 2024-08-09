// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/menu_util.h"

#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/scoped_animation_disabler.h"

namespace ash {

void HideActiveContextMenu() {
  auto* menu = views::MenuController::GetActiveInstance();
  if (!menu) {
    return;
  }

  views::MenuItemView* menu_item = menu->GetSelectedMenuItem();
  if (!menu_item) {
    return;
  }

  std::vector<std::unique_ptr<wm::ScopedAnimationDisabler>> disables;
  views::MenuItemView* parent = menu_item;
  while (parent) {
    if (parent->GetSubmenu() && parent->GetSubmenu()->GetWidget()) {
      disables.emplace_back(std::make_unique<wm::ScopedAnimationDisabler>(
          parent->GetSubmenu()->GetWidget()->GetNativeWindow()));
    }
    parent = parent->GetParentMenuItem();
  }

  menu->Cancel(views::MenuController::ExitType::kAll);
}

}  // namespace ash
