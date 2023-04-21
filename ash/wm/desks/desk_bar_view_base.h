// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_
#define ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "base/allocator/partition_allocator/pointers/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Base class for desk bar views, including desk bar view within overview and
// desk bar view for the desk button.
class ASH_EXPORT DeskBarViewBase : public views::View,
                                   public DesksController::Observer {
 public:
  enum class Type {
    kOverview,
    kDeskButton,
  };

  enum class State {
    kZero,
    kExpanded,
  };

  DeskBarViewBase(aura::Window* root, Type type);
  DeskBarViewBase(const DeskBarViewBase&) = delete;
  DeskBarViewBase& operator=(const DeskBarViewBase&) = delete;
  ~DeskBarViewBase() override;

  // Returns the preferred height of the desk bar that exists on `root` with
  // `state`.
  static int GetPreferredBarHeight(aura::Window* root, Type type, State state);

  // Returns the preferred state for the desk bar given `type`.
  static State GetPerferredState(Type type);

  // Creates and returns the widget that contains the desk bar view of `type`.
  // The returned widget has no contents view yet, and hasn't been shown yet.
  static std::unique_ptr<views::Widget>
  CreateDeskWidget(aura::Window* root, const gfx::Rect& bounds, Type type);

  Type type() const { return type_; }

  State state() const { return state_; }

  void set_is_bounds_animation_on_going(bool value) {
    is_bounds_animation_on_going_ = value;
  }

  const gfx::Point& last_dragged_item_screen_location() const {
    return last_dragged_item_screen_location_;
  }

  bool dragged_item_over_bar() const { return dragged_item_over_bar_; }

  OverviewGrid* overview_grid() const { return overview_grid_; }
  void set_overview_grid(OverviewGrid* overview_grid) {
    overview_grid_ = overview_grid;
  }

  const std::vector<DeskMiniView*>& mini_views() const { return mini_views_; }

  const views::View* scroll_view_contents() const {
    return scroll_view_contents_;
  }

  // TODO(yongshun): Migrate the following virtual functions to base class.
  virtual void UpdateNewMiniViews(bool initializing_bar_view,
                                  bool expanding_bar_view);
  virtual void UpdateDeskButtonsVisibility();
  virtual void SwitchToExpandedState();
  virtual void NudgeDeskName(int desk_index);
  virtual void HandlePressEvent(DeskMiniView* mini_view,
                                const ui::LocatedEvent& event);
  virtual void HandleLongPressEvent(DeskMiniView* mini_view,
                                    const ui::LocatedEvent& event);
  virtual void HandleDragEvent(DeskMiniView* mini_view,
                               const ui::LocatedEvent& event);
  virtual bool HandleReleaseEvent(DeskMiniView* mini_view,
                                  const ui::LocatedEvent& event);

  // Returns true if it is currently in zero state.
  bool IsZeroState() const;

  // If a desk is in a drag & drop cycle.
  bool IsDraggingDesk() const;

  // If the focused `view` is outside of the scroll view's visible bounds,
  // scrolls the bar to make sure it can always be seen. Please note, `view`
  // must be a child of `scroll_view_contents_`.
  void ScrollToShowViewIfNecessary(const views::View* view);

  // Returns the mini_view associated with `desk` or nullptr if no mini_view
  // has been created for it yet.
  DeskMiniView* FindMiniViewForDesk(const Desk* desk) const;

  // Get the index of a desk mini view in the `mini_views`.
  int GetMiniViewIndex(const DeskMiniView* mini_view) const;

 protected:
  // TODO(yongshun): Migrate the following virtual functions to base class.
  virtual void UpdateScrollButtonsVisibility();
  virtual void UpdateGradientMask();

  // Returns the X offset of the first mini_view on the left (if there's one),
  // or the X offset of this view's center point when there are no mini_views.
  // This offset is used to calculate the amount by which the mini_views should
  // be moved when performing the mini_view creation or deletion animations.
  int GetFirstMiniViewXOffset() const;

  const Type type_ = Type::kOverview;

  State state_ = State::kZero;

  // True if the `DesksBarBoundsAnimation` is started and hasn't finished yet.
  // It will be used to hold `Layout` until the bounds animation is completed.
  // `Layout` is expensive and will be called on bounds changes, which means it
  // will be called lots of times during the bounds changes animation. This is
  // done to eliminate the unnecessary `Layout` calls during the animation.
  bool is_bounds_animation_on_going_ = false;

  // Mini view whose preview is being dragged.
  DeskMiniView* drag_view_ = nullptr;

  // The screen location of the most recent drag position. This value is valid
  // only when the below `dragged_item_over_bar_` is true.
  gfx::Point last_dragged_item_screen_location_;

  // True when the drag location of the overview item is intersecting with this
  // view.
  bool dragged_item_over_bar_ = false;

  // The `OverviewGrid` that contains this object if this is a `Type::kOverview`
  // bar, nullptr otherwise.
  OverviewGrid* overview_grid_;

  // The views representing desks mini_views. They're owned by views hierarchy.
  std::vector<DeskMiniView*> mini_views_;

  // Puts the contents in a ScrollView to support scrollable desks.
  views::ScrollView* scroll_view_ = nullptr;

  // Contents of `scroll_view_`, which includes `mini_views_`,
  // `expanded_state_new_desk_button_` and optionally
  // `expanded_state_library_button_` currently.
  views::View* scroll_view_contents_ = nullptr;

 private:
  // TODO(yongshun): Migrate the following virtual functions to base class.
  virtual void UpdateDeskButtonsVisibilityCrOSNext();
  virtual void UpdateLibraryButtonVisibilityCrOSNext();

  raw_ptr<aura::Window> root_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_
