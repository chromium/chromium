// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BAR_CONTROLLER_H_
#define ASH_WM_DESKS_DESK_BAR_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_button/desk_button.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Controller for the desk bars that is responsible for creating, destroying,
// and managing all desk bars. At this point, it supports only desk button desk
// bar, but eventually, it will support all bars. Please note this controller is
// owned by `DesksController`.
class ASH_EXPORT DeskBarController : public DesksController::Observer,
                                     public ui::EventHandler,
                                     public OverviewObserver,
                                     public ShellObserver,
                                     public TabletModeObserver,
                                     public wm::ActivationChangeObserver,
                                     public display::DisplayObserver {
 public:
  struct BarWidgetAndView {
    BarWidgetAndView(DeskBarViewBase* bar_view,
                     std::unique_ptr<views::Widget> bar_widget);
    BarWidgetAndView(BarWidgetAndView&& other);
    BarWidgetAndView& operator=(BarWidgetAndView&& other);
    BarWidgetAndView(const BarWidgetAndView&) = delete;
    BarWidgetAndView& operator=(const BarWidgetAndView&) = delete;
    ~BarWidgetAndView();

    std::unique_ptr<views::Widget> bar_widget;
    raw_ptr<DeskBarViewBase> bar_view;
  };

  DeskBarController();

  DeskBarController(const DeskBarController&) = delete;
  DeskBarController& operator=(const DeskBarController&) = delete;

  ~DeskBarController() override;

  // DesksController::Observer:
  void OnDeskSwitchAnimationLaunching() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // OverviewObserver:
  void OnOverviewModeWillStart() override;

  // ShellObserver:
  void OnShellDestroying() override;

  // TabletModeObserver:
  void OnTabletModeStarting() override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Returns desk bar view in `root`. If there is no such desk bar, nullptr is
  // returned.
  DeskBarViewBase* GetDeskBarView(aura::Window* root) const;

  // Returns true when there is a visible desk bar.
  bool IsShowingDeskBar() const;

  // Creates and shows the desk bar in 'root'.
  void OpenDeskBar(aura::Window* root);

  // Hides and destroys the desk bar in 'root'.
  void CloseDeskBar(aura::Window* root);

  // Hides and destroys all desk bars.
  void CloseAllDeskBars();

 private:
  void CloseDeskBarInternal(BarWidgetAndView& desk_bar);

  // Returns bounds for desk bar widget in `root`. Please note, this is the full
  // available bounds and does not change after initialization. Therefore, the
  // desk bar view can adjust its bounds as needed without manipulating the
  // widget. This calculates bounds of `kDeskButton` bar for `kBottom`, `kLeft`,
  // and `kRight` aligned shelf as following.
  //
  // Symbols:
  //   - H: Home button
  //   - D: Desk button
  //   - S: Shelf
  //   - B: Bar widget
  //
  // Charts:
  //   1. `kBottom`
  //     ┌────────────────────────────────┐
  //     │                                │
  //     │                                │
  //     │                                │
  //     │                                │
  //     │                                │
  //     ├────────────────────────────────│
  //     │                B               │
  //     ├───┬─────┬──────────────────────┤
  //     │ H │  D  │           S          │
  //     └───┴─────┴──────────────────────┘
  //   2. `kLeft`
  //     ┌───┬────────────────────────────┐
  //     │ H │                            │
  //     ├───┤ ┌──────────────────────────┤
  //     │ D │ │             B            │
  //     ├───┤ │                          │
  //     │   │ └──────────────────────────┤
  //     │   │                            │
  //     │ S │                            │
  //     │   │                            │
  //     │   │                            │
  //     └───┴────────────────────────────┘
  //   3. `kRight`
  //     ┌────────────────────────────┬───┐
  //     │                            │ H │
  //     ├──────────────────────────┐ ├───┤
  //     │             B            │ │ D │
  //     │                          │ ├───┤
  //     ├──────────────────────────┘ │   │
  //     │                            │   │
  //     │                            │ S │
  //     │                            │   │
  //     │                            │   │
  //     └────────────────────────────┴───┘
  gfx::Rect GetDeskBarWidgetBounds(aura::Window* root) const;

  // When pressing off the bar, it should either commit desk name change, or
  // hide the bar.
  void OnMaybePressOffBar(ui::LocatedEvent& event);

  // Returns desk button for `root`.
  DeskButton* GetDeskButton(aura::Window* root);

  // Updates desk button activation.
  void SetDeskButtonActivation(aura::Window* root, bool is_activated);

  // Bar widgets and bar views for the desk bars. Right now, it supports only
  // desk button desk bar. Support for overview desk bar will be added later.
  std::vector<BarWidgetAndView> desk_bars_;

  // Observes display configuration changes.
  display::ScopedDisplayObserver display_observer_{this};

  // Indicates that shell is destroying.
  bool is_shell_destroying_ = false;

  bool should_ignore_activation_change_ = false;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_BAR_CONTROLLER_H_
