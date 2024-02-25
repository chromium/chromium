// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_CURSOR_SETTER_H_
#define ASH_UTILITY_CURSOR_SETTER_H_

#include "chromeos/ui/base/display_util.h"
#include "ui/base/cursor/cursor.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

// A class to update or reset the cursor.
class CursorSetter {
 public:
  CursorSetter();
  CursorSetter(const CursorSetter&) = delete;
  CursorSetter& operator=(const CursorSetter&) = delete;
  ~CursorSetter();

  // Updates the cursor on the given `root_window` to the provided `cursor`. If
  // the given `cursor`'s type is `kCustom`, `custom_type_id` will be used as a
  // unique ID to identify this custom cursor, and must be provided. Otherwise
  // it will be ignored.
  void UpdateCursor(aura::Window* root_window,
                    const ui::Cursor& cursor,
                    std::optional<int> custom_type_id = std::nullopt);

  void HideCursor();

  // Resets to its original cursor.
  void ResetCursor();

  bool IsCursorVisible() const;
  bool IsUsingCustomCursor(int custom_type) const;

 private:
  // An encapsulation of elements that are needed to check for custom cursor.
  struct CustomCursorParams {
    int id;
    float device_scale_factor;
    chromeos::OrientationType orientation;

    bool operator==(const CustomCursorParams& rhs) const {
      return id == rhs.id && device_scale_factor == rhs.device_scale_factor &&
             orientation == rhs.orientation;
    }

    bool operator!=(const CustomCursorParams& rhs) const {
      return !(*this == rhs);
    }
  };

  // Returns true if the `new_cursor_type` is different from the current, its
  // visibility changed, or if it's a custom cursor and its parameters
  // (`device_scale_factor`, `orientation` or `custom_type_id`) changed.
  bool DidCursorChange(ui::mojom::CursorType new_cursor_type,
                       float device_scale_factor,
                       chromeos::OrientationType orientation,
                       std::optional<int> custom_type_id) const;

  const raw_ptr<wm::CursorManager> cursor_manager_;
  const gfx::NativeCursor original_cursor_;
  const bool original_cursor_visible_;

  // If the original cursor is already locked, don't make any changes to it.
  const bool original_cursor_locked_;

  std::optional<CustomCursorParams> custom_cursor_params_;

  // True if the cursor has reset back to its original cursor. It's to prevent
  // `ResetCursor()` from setting the cursor to `original_cursor_` more than
  // once.
  bool was_cursor_reset_to_original_ = true;

  // True if the cursor is currently being updated. This is to prevent
  // `UpdateCursor()` is called nestly more than once and the mouse is locked
  // multiple times.
  bool in_cursor_update_ = false;
};

}  // namespace ash

#endif  // ASH_UTILITY_CURSOR_SETTER_H_
