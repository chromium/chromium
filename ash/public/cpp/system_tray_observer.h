// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TRAY_OBSERVER_H_
#define ASH_PUBLIC_CPP_SYSTEM_TRAY_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// A class that observes system tray related focus events.
class ASH_PUBLIC_EXPORT SystemTrayObserver {
 public:
  // Called when focus is about to leave system tray.
  virtual void OnFocusLeavingSystemTray(bool reverse) = 0;

  // Called when the UnifiedSystemTrayBubble is shown.
  virtual void OnSystemTrayBubbleShown() {}

 protected:
  virtual ~SystemTrayObserver() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TRAY_OBSERVER_H_
