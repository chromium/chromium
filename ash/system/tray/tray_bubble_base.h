// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BUBBLE_BASE_H_
#define ASH_SYSTEM_TRAY_TRAY_BUBBLE_BASE_H_

#include "ash/ash_export.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class TrayBackgroundView;
class TrayBubbleView;

// Base class for tray bubbles registered to TrayEventFilter.
// Note: As this class implements `views::WidgetObserver`, the derived classes
// are required to add themselves as a `views::WidgetObserver` to the bubble
// Widgets they make.
class ASH_EXPORT TrayBubbleBase : public views::WidgetObserver {
 public:
  TrayBubbleBase() = default;
  ~TrayBubbleBase() override = default;

  // Returns the tray button instance.
  virtual TrayBackgroundView* GetTray() const = 0;

  // Returns the TrayBubbleView instance of the bubble.
  virtual TrayBubbleView* GetBubbleView() const = 0;

  // Returns the widget of the bubble.
  virtual views::Widget* GetBubbleWidget() const = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BUBBLE_BASE_H_
