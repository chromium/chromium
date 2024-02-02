// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_KEY_COMBO_VIEW_H_
#define ASH_CAPTURE_MODE_KEY_COMBO_VIEW_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/capture_mode/key_item_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/view.h"

namespace ash {

class ModifiersContainerView;

// A view that contains the key item widgets that constitute the keyboard
// shortcuts combo view. The modifier key will always show before the
// non-modifier key, which will be hosted in the `modifiers_container_view_`.
class ASH_EXPORT KeyComboView : public views::View {
  METADATA_HEADER(KeyComboView, views::View)

 public:
  KeyComboView();
  KeyComboView(const KeyComboView&) = delete;
  KeyComboView& operator=(const KeyComboView&) = delete;
  ~KeyComboView() override;

  // Appends or removes a key item view based on the given `modifiers` and
  // `last_non_modifier_key`.
  void RefreshView(int modifiers, ui::KeyboardCode last_non_modifier_key);

 private:
  friend class CaptureModeDemoToolsTestApi;

  // Returns the key code vector in the `ModifiersContainerView`, this function
  // is needed for testing.
  std::vector<ui::KeyboardCode> GetModifierKeycodeVector() const;

  int modifiers_ = 0;
  ui::KeyboardCode last_non_modifier_key_ = ui::VKEY_UNKNOWN;
  raw_ptr<ModifiersContainerView> modifiers_container_view_ = nullptr;
  raw_ptr<KeyItemView, DanglingUntriaged> non_modifier_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_KEY_COMBO_VIEW_H_
