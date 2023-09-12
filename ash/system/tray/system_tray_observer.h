// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_OBSERVER_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class TrayBubbleView;

// A class that observes system tray related focus events.
class ASH_EXPORT SystemTrayObserver {
 public:
  // Called when focus is about to leave system tray.
  virtual void OnFocusLeavingSystemTray(bool reverse) = 0;

  // Called when the UnifiedSystemTrayBubble is shown.
  virtual void OnSystemTrayBubbleShown() {}

  // Called when a status area anchored bubble change its visibility.
  virtual void OnStatusAreaAnchoredBubbleVisibilityChanged(
      TrayBubbleView* tray_bubble,
      bool visible) {}

  // Called when a tray bubble changes its bounds. Some bubbles trigger this
  // function when they are shown.
  virtual void OnTrayBubbleBoundsChanged(TrayBubbleView* tray_bubble) {}

  // Called when the IME Menu Tray Bubble is shown.
  virtual void OnImeMenuTrayBubbleShown() {}

 protected:
  virtual ~SystemTrayObserver() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_OBSERVER_H_
