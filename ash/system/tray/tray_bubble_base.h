// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BUBBLE_BASE_H_
#define ASH_SYSTEM_TRAY_TRAY_BUBBLE_BASE_H_

#include "ash/ash_export.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class TrayBackgroundView;
class TrayBubbleView;

// Base class for tray bubbles registered to TrayEventFilter.
class ASH_EXPORT TrayBubbleBase {
 public:
  virtual ~TrayBubbleBase() {}

  // Returns the tray button instance.
  virtual TrayBackgroundView* GetTray() const = 0;

  // Returns the TrayBubbleView instance of the bubble.
  virtual TrayBubbleView* GetBubbleView() const = 0;

  // Returns the widget of the bubble.
  virtual views::Widget* GetBubbleWidget() const = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BUBBLE_BASE_H_
