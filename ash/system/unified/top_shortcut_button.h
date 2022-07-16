// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_TOP_SHORTCUT_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_TOP_SHORTCUT_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// A button used in top shortcuts. Top shortcuts are small circular buttons
// shown on top of the UnifiedSystemTrayView that allows quick access to
// frequently used features e.g. lock screen, settings, and shutdown.
class TopShortcutButton : public views::ImageButton {
 public:
  METADATA_HEADER(TopShortcutButton);

  TopShortcutButton(PressedCallback callback,
                    const gfx::VectorIcon& icon,
                    int accessible_name_id);

  TopShortcutButton(const TopShortcutButton&) = delete;
  TopShortcutButton& operator=(const TopShortcutButton&) = delete;

  ~TopShortcutButton() override;

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  const gfx::VectorIcon& icon_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_TOP_SHORTCUT_BUTTON_H_
