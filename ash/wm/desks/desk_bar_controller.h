// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BAR_CONTROLLER_H_
#define ASH_WM_DESKS_DESK_BAR_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/window_occlusion_calculator.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

class DeskButtonContainer;

// Controller for the desk bars that is responsible for creating, destroying,
// and managing all desk bars. At this point, it supports only desk button desk
// bar, but eventually, it will support all bars. Please note this controller is
// owned by `DesksController`.
class ASH_EXPORT DeskBarController : public DesksController::Observer,
                                     public ui::EventHandler,
                                     public OverviewObserver,
                                     public ShellObserver,
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

    // Returns the next focusable view based on `starting_view`, which is a
    // focusable view in `bar_view`, and `reverse`. If `reverse` is false, it
    // returns the traversible view after `starting_view`; otherwise, it returns
    // the traversible view before `starting_view`.
    const views::View* GetNextFocusableView(views::View* starting_view,
                                            bool reverse) const;

    // Returns the first focusable view in `bar_view`.
    const views::View* GetFirstFocusableView() const;

    // Returns the last focusable view in `bar_view`.
    const views::View* GetLastFocusableView() const;

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

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // Returns desk bar view in `root`. If there is no such desk bar, nullptr is
  // returned.
  DeskBarViewBase* GetDeskBarView(aura::Window* root);

  // Returns true when there is a visible desk bar.
  bool IsShowingDeskBar() const;

  // Creates and shows the desk bar in 'root'.
  void OpenDeskBar(aura::Window* root);

  // Hides and destroys the desk bar in 'root'.
  void CloseDeskBar(aura::Window* root);

  // Hides and destroys all desk bars.
  void CloseAllDeskBars();

 private:
  // Moves the focus ring to the next traversable view.
  void MoveFocus(const BarWidgetAndView& desk_bar, bool reverse);

  void CloseDeskBarInternal(BarWidgetAndView& desk_bar);

  // Common handling of mouse and touch events.
  void OnLocatedEvent(ui::LocatedEvent& event);

  // When pressing off the bar, it should either commit desk name change, or
  // hide the bar.
  void OnMaybePressOffBar(ui::LocatedEvent& event);

  // Returns desk button container for `root`.
  DeskButtonContainer* GetDeskButtonContainer(aura::Window* root);

  // Updates desk button activation.
  void SetDeskButtonActivation(aura::Window* root, bool is_activated);

  // Bar widgets and bar views for the desk bars. Right now, it supports only
  // desk button desk bar. Support for overview desk bar will be added later.
  std::vector<BarWidgetAndView> desk_bars_;

  // Observes display configuration changes.
  display::ScopedDisplayObserver display_observer_{this};

  // Root window of the desk button that is clicked. This is used to determine
  // which desk button should gain focus back after the desk bar is closed.
  raw_ptr<aura::Window> desk_button_root_ = nullptr;

  // True if the desk button should acquire focus back when hitting esc.
  bool should_desk_button_acquire_focus_ = false;

  // Indicates that shell is destroying.
  bool is_shell_destroying_ = false;

  bool should_ignore_activation_change_ = false;

  std::optional<WindowOcclusionCalculator> window_occlusion_calculator_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_BAR_CONTROLLER_H_
