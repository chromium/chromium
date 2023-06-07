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
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/observer_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Controller for the desk bars that is responsible for creating, destroying,
// and managing all desk bars. At this point, it supports only desk button desk
// bar, but eventually, it will support all bars. Please note this controller is
// owned by `DesksController`.
class ASH_EXPORT DeskBarController : public DesksController::Observer,
                                     public OverviewObserver,
                                     public TabletModeObserver {
 public:
  DeskBarController();

  DeskBarController(const DeskBarController&) = delete;
  DeskBarController& operator=(const DeskBarController&) = delete;

  ~DeskBarController() override;

  // DesksController::Observer:
  void OnDeskSwitchAnimationLaunching() override;

  // OverviewObserver:
  void OnOverviewModeWillStart() override;

  // TabletModeObserver:
  void OnTabletModeStarting() override;

  // Returns desk bar view in `root`. If there is no such desk bar, nullptr is
  // returned.
  DeskBarViewBase* GetDeskBarView(aura::Window* root) const;

  // Creates desk bar(both bar widget and bar view) in `root`. If there is
  // another bar in `root`, it will get rid of the existing one and then
  // create a new one.
  void CreateDeskBar(aura::Window* root);

  // Destroys desk bar in `root`. Please note, this assumes a valid bar always
  // exists.
  void DestroyDeskBar(aura::Window* root);

  // Destroys all desk bars.
  void DestroyAllDeskBars();

  // Shows the desk bar in 'root'. Please note, this assumes a valid bar
  // always exists.
  void ShowDeskBar(aura::Window* root);

  // Hides the desk bar in 'root'. Please note, this assumes a valid bar
  // always exists.
  void HideDeskBar(aura::Window* root);

 private:
  // Returns bounds for desk bar widget in `root`.
  gfx::Rect GetDeskBarWidgetBounds(aura::Window* root) const;

  // Bar widgets and bar views for the desk bars. Right now, it supports only
  // desk button desk bar. Support for overview desk bar will be added later.
  std::vector<std::unique_ptr<views::Widget>> desk_bar_widgets_;
  std::vector<DeskBarViewBase*> desk_bar_views_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_BAR_CONTROLLER_H_
