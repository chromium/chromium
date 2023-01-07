// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_POPUP_INK_DROP_STYLE_H_
#define ASH_SYSTEM_TRAY_TRAY_POPUP_INK_DROP_STYLE_H_

namespace ash {

// The different styles of ink drops applied to the system menu.
enum class TrayPopupInkDropStyle {
  // Used for targets where the user doesn't need to know the exact targetable
  // area and they are expected to target an icon centered in the targetable
  // space. Highlight and ripple are drawn as a circle.
  HOST_CENTERED,
  // Used for targets where the user should know the targetable bounds but
  // where the ink drop shouldn't fill the entire bounds. e.g. row of buttons
  // separated with separators.
  INSET_BOUNDS,
  // Used for targets that should indicate to the user what the actual
  // targetable bounds are. e.g. a full system menu row.
  FILL_BOUNDS
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_POPUP_INK_DROP_STYLE_H_
