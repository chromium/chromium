// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
#define ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_bubble_base.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class TrayBackgroundView;
class TrayBubbleView;
class TrayEventFilter;

// Creates and manages the Widget components of a bubble.
class ASH_EXPORT TrayBubbleWrapper : public TrayBubbleBase {
 public:
  // `event_handling` When set to false disables the tray's event filtering
  // and also ignores the activation events. Eche window is an example of a use
  // case in which we do not want the keyboard events (both inside and outside
  // of the bubble) be filtered and also we do not want activaion of other
  // windows closes the bubble.
  explicit TrayBubbleWrapper(TrayBackgroundView* tray,
                             bool event_handling = true);

  TrayBubbleWrapper(const TrayBubbleWrapper&) = delete;
  TrayBubbleWrapper& operator=(const TrayBubbleWrapper&) = delete;

  ~TrayBubbleWrapper() override;

  void ShowBubble(std::unique_ptr<TrayBubbleView> bubble_view);

  // TrayBubbleBase overrides:
  TrayBackgroundView* GetTray() const override;
  TrayBubbleView* GetBubbleView() const override;
  views::Widget* GetBubbleWidget() const override;

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;

  TrayBackgroundView* tray() { return tray_; }
  TrayBubbleView* bubble_view() { return bubble_view_; }
  views::Widget* bubble_widget() { return bubble_widget_; }

 private:
  raw_ptr<TrayBackgroundView> tray_;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;

  // Owned by `bubble_widget_`
  raw_ptr<TrayBubbleView> bubble_view_ = nullptr;

  std::unique_ptr<TrayEventFilter> tray_event_filter_;

  // When set to false disables the tray's event filtering
  // and also ignores the activation events. Eche window is an example of a use
  // case in which we do not want the keyboard events (both inside and outside
  // of the bubble) be filtered and also we do not want activaion of other
  // windows closes the bubble.
  const bool event_handling_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
