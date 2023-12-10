// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/folder_background_view.h"

#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

FolderBackgroundView::FolderBackgroundView(AppListFolderView* folder_view)
    : folder_view_(folder_view) {}

FolderBackgroundView::~FolderBackgroundView() = default;

bool FolderBackgroundView::OnMousePressed(const ui::MouseEvent& event) {
  HandleClickOrTap();
  return true;
}

void FolderBackgroundView::OnGestureEvent(ui::GestureEvent* event) {
  // A fix for the current folder close animation should be implemented to allow
  // for a folder to close while pages are changing. Until this, we should
  // always close the folder before movement.
  // https://crbug.com/875133
  HandleClickOrTap();
  event->SetHandled();
}

void FolderBackgroundView::HandleClickOrTap() {
  // If the virtual keyboard is visible, dismiss the keyboard and return early
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsKeyboardVisible()) {
    keyboard_controller->HideKeyboardByUser();
    return;
  }
  // TODO(ginko): make the first tap close the keyboard only, and the second tap
  // close the folder. Bug: https://crbug.com/879329
  folder_view_->CloseFolderPage();
}

BEGIN_METADATA(FolderBackgroundView)
END_METADATA

}  // namespace ash
