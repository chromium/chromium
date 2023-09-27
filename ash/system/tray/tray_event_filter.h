// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
#define ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_

#include "ash/ash_export.h"
#include "ash/bubble/bubble_event_filter.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class LocatedEvent;
}

namespace views {
class Widget;
}  // namespace views

namespace ash {

class TrayBackgroundView;
class TrayBubbleView;

// Handles events for tray bubbles, closing the system tray bubble when the user
// clicks outside of it or when other windows are activated.
class ASH_EXPORT TrayEventFilter : public BubbleEventFilter,
                                   public ::wm::ActivationChangeObserver {
 public:
  TrayEventFilter(views::Widget* bubble_widget,
                  TrayBubbleView* bubble_view,
                  TrayBackgroundView* tray_button);

  TrayEventFilter(const TrayEventFilter&) = delete;
  TrayEventFilter& operator=(const TrayEventFilter&) = delete;

  ~TrayEventFilter() override;

  // BubbleEventFilter:
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool ShouldRunOnClickOutsideCallback(const ui::LocatedEvent& event) override;

 private:
  // ::wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  const raw_ptr<views::Widget> bubble_widget_;
  const raw_ptr<TrayBubbleView> bubble_view_;
  const raw_ptr<TrayBackgroundView> tray_button_;

  base::WeakPtrFactory<TrayEventFilter> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_EVENT_FILTER_H_
