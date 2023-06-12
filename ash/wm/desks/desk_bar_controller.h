// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BAR_CONTROLLER_H_
#define ASH_WM_DESKS_DESK_BAR_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_button/desk_button.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/observer_list.h"
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
                                     public TabletModeObserver,
                                     public wm::ActivationChangeObserver {
 public:
  DeskBarController();

  DeskBarController(const DeskBarController&) = delete;
  DeskBarController& operator=(const DeskBarController&) = delete;

  ~DeskBarController() override;

  // DesksController::Observer:
  void OnDeskSwitchAnimationLaunching() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // OverviewObserver:
  void OnOverviewModeWillStart() override;

  // TabletModeObserver:
  void OnTabletModeStarting() override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // Returns desk bar view in `root`. If there is no such desk bar, nullptr is
  // returned.
  DeskBarViewBase* GetDeskBarView(aura::Window* root) const;

  // Opens the desk bar in 'root'.
  void OpenDeskBar(aura::Window* root);

  // Closes the desk bar in 'root'. Please note, the destruction of the desk bar
  // is triggered when the bar loses activation via `OnWindowActivated()`.
  void CloseDeskBar(aura::Window* root);

  // Closes all desk bars. Please note, the destruction of the desk bars is
  // triggered when the bars lose activation via `OnWindowActivated()`.
  void CloseAllDeskBars();

 private:
  // Creates desk bar(both bar widget and bar view) in `root`. If there is
  // another bar in `root`, it will get rid of the existing one and then
  // create a new one.
  void CreateDeskBar(aura::Window* root);

  // Destroys all desk bars. This is where the real destruction happens.
  void DestroyAllDeskBars();

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
  void OnMaybePressOffBar(const ui::LocatedEvent& event);

  // Returns desk button for `root`.
  DeskButton* GetDeskButton(aura::Window* root);

  // Updates desk button activation.
  void SetDeskButtonActivation(aura::Window* root, bool is_activated);

  // Bar widgets and bar views for the desk bars. Right now, it supports only
  // desk button desk bar. Support for overview desk bar will be added later.
  std::vector<std::unique_ptr<views::Widget>> desk_bar_widgets_;
  std::vector<DeskBarViewBase*> desk_bar_views_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_BAR_CONTROLLER_H_
