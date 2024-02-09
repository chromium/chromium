// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_KEYBOARD_SHORTCUT_VIEW_H_
#define ASH_STYLE_KEYBOARD_SHORTCUT_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

// A view used to represent a keyboard shortcut/accelerator as a series of
// bordered text or icon keys.
class ASH_EXPORT KeyboardShortcutView : public views::FlexLayoutView {
  METADATA_HEADER(KeyboardShortcutView, views::FlexLayoutView)

 public:
  explicit KeyboardShortcutView(
      const std::vector<ui::KeyboardCode>& keyboard_codes);
  KeyboardShortcutView(const KeyboardShortcutView&) = delete;
  KeyboardShortcutView& operator=(const KeyboardShortcutView&) = delete;
  ~KeyboardShortcutView() override;
};

}  // namespace ash

#endif  // ASH_STYLE_KEYBOARD_SHORTCUT_VIEW_H_
