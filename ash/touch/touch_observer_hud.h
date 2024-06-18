// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
#define ASH_TOUCH_TOUCH_OBSERVER_HUD_H_

#include <stdint.h>

#include <string>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

namespace ash {

// An event filter which handles system level gesture events. Objects of this
// class manage their own lifetime. TODO(estade): find a more descriptive name
// for this class that aligns with its subclasses (or consider using composition
// over inheritance).
class ASH_EXPORT TouchObserverHud
    : public ui::EventHandler,
      public views::WidgetObserver,
      public display::DisplayObserver,
      public display::DisplayConfigurator::Observer,
      public display::DisplayManagerObserver {
 public:
  TouchObserverHud(const TouchObserverHud&) = delete;
  TouchObserverHud& operator=(const TouchObserverHud&) = delete;

  // Called to clear touch points and traces from the screen.
  virtual void Clear() = 0;

  // Removes the HUD from the screen.
  void Remove();

  int64_t display_id() const { return display_id_; }

 protected:
  // |widget_name| is set on Widget::InitParams::name, and is used purely for
  // debugging.
  TouchObserverHud(aura::Window* initial_root, const std::string& widget_name);

  ~TouchObserverHud() override;

  virtual void SetHudForRootWindowController(
      RootWindowController* controller) = 0;
  virtual void UnsetHudForRootWindowController(
      RootWindowController* controller) = 0;

  views::Widget* widget() { return widget_; }

  // ui::EventHandler:
  void OnTouchEvent(ui::TouchEvent* event) override = 0;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // display::DisplayObserver:
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // display::DisplayConfigurator::Observer:
  void OnDisplayConfigurationChanged(
      const display::DisplayConfigurator::DisplayStateList& outputs) override;

  // display::DisplayManagerObserver
  void OnDisplaysInitialized() override;
  void OnWillApplyDisplayChanges() override;
  void OnDidApplyDisplayChanges() override;

 private:
  friend class TouchHudTestBase;

  const int64_t display_id_;
  raw_ptr<aura::Window> root_window_;

  raw_ptr<views::Widget> widget_;

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_OBSERVER_HUD_H_
