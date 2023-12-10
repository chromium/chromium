// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/cursor_setter.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/shell.h"

namespace ash {

CursorSetter::CursorSetter()
    : cursor_manager_(Shell::Get()->cursor_manager()),
      original_cursor_(cursor_manager_->GetCursor()),
      original_cursor_visible_(cursor_manager_->IsCursorVisible()),
      original_cursor_locked_(cursor_manager_->IsCursorLocked()) {
  if (!cursor_manager_->IsMouseEventsEnabled()) {
    cursor_manager_->EnableMouseEvents();
  }
}

CursorSetter::~CursorSetter() {
  custom_cursor_params_.reset();
  ResetCursor();
}

void CursorSetter::UpdateCursor(aura::Window* root_window,
                                const ui::Cursor& cursor,
                                std::optional<int> custom_type_id) {
  if (original_cursor_locked_) {
    return;
  }

  if (in_cursor_update_) {
    return;
  }

  base::AutoReset<bool> auto_reset_in_cursor_update(&in_cursor_update_, true);
  const ui::mojom::CursorType new_cursor_type = cursor.type();
  const float device_scale_factor = display::Screen::GetScreen()
                                        ->GetDisplayNearestWindow(root_window)
                                        .device_scale_factor();
  const chromeos::OrientationType orientation = GetCurrentScreenOrientation();
  if (!DidCursorChange(new_cursor_type, device_scale_factor, orientation,
                       custom_type_id)) {
    return;
  }

  if (new_cursor_type == ui::mojom::CursorType::kCustom) {
    custom_cursor_params_ = CustomCursorParams{
        custom_type_id.value(), device_scale_factor, orientation};
  }

  if (cursor_manager_->IsCursorLocked()) {
    cursor_manager_->UnlockCursor();
  }
  if (new_cursor_type == ui::mojom::CursorType::kNone) {
    cursor_manager_->HideCursor();
  } else {
    cursor_manager_->SetCursor(cursor);
    cursor_manager_->ShowCursor();
  }
  cursor_manager_->LockCursor();
  was_cursor_reset_to_original_ = false;
}

void CursorSetter::HideCursor() {
  if (original_cursor_locked_ || !IsCursorVisible()) {
    return;
  }

  if (cursor_manager_->IsCursorLocked()) {
    cursor_manager_->UnlockCursor();
  }

  cursor_manager_->HideCursor();
  cursor_manager_->LockCursor();
  was_cursor_reset_to_original_ = false;
}

void CursorSetter::ResetCursor() {
  // Only unlock the cursor if it wasn't locked before.
  if (original_cursor_locked_) {
    return;
  }

  // Only reset cursor if it hasn't been reset before.
  if (was_cursor_reset_to_original_) {
    return;
  }

  if (cursor_manager_->IsCursorLocked()) {
    cursor_manager_->UnlockCursor();
  }
  cursor_manager_->SetCursor(original_cursor_);
  if (original_cursor_visible_) {
    cursor_manager_->ShowCursor();
  } else {
    cursor_manager_->HideCursor();
  }
  was_cursor_reset_to_original_ = true;
}

bool CursorSetter::IsCursorVisible() const {
  return cursor_manager_->IsCursorVisible();
}

bool CursorSetter::IsUsingCustomCursor(int custom_type_id) const {
  return cursor_manager_->GetCursor().type() ==
             ui::mojom::CursorType::kCustom &&
         custom_cursor_params_->id == custom_type_id;
}

bool CursorSetter::DidCursorChange(
    ui::mojom::CursorType new_cursor_type,
    float device_scale_factor,
    chromeos::OrientationType orientation,
    const std::optional<int> custom_type_id) const {
  const ui::mojom::CursorType current_cursor_type =
      cursor_manager_->GetCursor().type();

  if (current_cursor_type != new_cursor_type) {
    return true;
  }

  // We use `kNone` as a signal to hide the cursor. See `UpdateCursor()`.
  bool visibility_changed = cursor_manager_->IsCursorVisible() !=
                            (new_cursor_type != ui::mojom::CursorType::kNone);

  if (visibility_changed) {
    return true;
  }

  if (current_cursor_type == ui::mojom::CursorType::kCustom) {
    CHECK(custom_type_id);
    return (custom_cursor_params_ != CustomCursorParams{custom_type_id.value(),
                                                        device_scale_factor,
                                                        orientation});
  }

  return false;
}

}  // namespace ash
