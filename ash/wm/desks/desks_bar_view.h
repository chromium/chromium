// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_BAR_VIEW_H_
#define ASH_WM_DESKS_DESKS_BAR_VIEW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class DeskMiniView;
class NewDeskButton;
class DeskBarHoverObserver;
class OverviewGrid;

// A bar that resides at the top portion of the overview mode's ShieldView,
// which contains the virtual desks mini_views, as well as the new desk button.
class ASH_EXPORT DesksBarView : public views::View,
                                public views::ButtonListener,
                                public DesksController::Observer {
 public:
  DesksBarView(OverviewGrid* overview_grid);
  ~DesksBarView() override;

  // Returns the height of the desk bar view which is based on the given |width|
  // and |desks_bar_view|'s content.
  // If |desks_bar_view| is nullptr, the height returned will be solely based on
  // the |width|.
  static int GetBarHeightForWidth(const DesksBarView* desks_bar_view,
                                  int width);

  // Creates and returns the widget that contains the DeskBarView in overview
  // mode. The returned widget has no content view yet, and hasn't been shown
  // yet.
  static std::unique_ptr<views::Widget> CreateDesksWidget(
      aura::Window* root,
      const gfx::Rect& bounds);

  views::View* background_view() const { return background_view_; }

  NewDeskButton* new_desk_button() const { return new_desk_button_; }

  const std::vector<std::unique_ptr<DeskMiniView>>& mini_views() const {
    return mini_views_;
  }

  const gfx::Point& last_dragged_item_screen_location() const {
    return last_dragged_item_screen_location_;
  }

  bool dragged_item_over_bar() const { return dragged_item_over_bar_; }

  // Initializes and creates mini_views for any pre-existing desks, before the
  // bar was created. This should only be called after this view has been added
  // to a widget, as it needs to call `GetWidget()` when it's performing a
  // layout.
  void Init();

  // Updates the visibility state of the close buttons on all the mini_views as
  // a result of mouse and gesture events.
  void OnHoverStateMayHaveChanged();
  void OnGestureTap(const gfx::Rect& screen_rect, bool is_long_gesture);

  // Called when an item is being dragged in overview mode to update whether it
  // is currently intersecting with this view, and the |screen_location| of the
  // current drag position.
  void SetDragDetails(const gfx::Point& screen_location,
                      bool dragged_item_over_bar);

  // views::View:
  const char* GetClassName() const override;
  void Layout() override;

  // Returns true if the width of the DesksBarView is below a defined
  // threshold or the contents no longer fit within this object's bounds in
  // default mode, suggesting a compact small screens layout should be used for
  // both itself and its children.
  bool UsesCompactLayout() const;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override;
  void OnDeskRemoved(const Desk* desk) override;
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;
  void OnDeskSwitchAnimationLaunching() override;
  void OnDeskSwitchAnimationFinished() override;

 private:
  // This is called on initialization or when a new desk is created to create
  // the needed new mini_views. If |animate| is true, the mini_views will be
  // animated to their final positions.
  void UpdateNewMiniViews(bool animate);

  // Returns the mini_view associated with |desk| or nullptr if no mini_view
  // has been created for it yet.
  DeskMiniView* FindMiniViewForDesk(const Desk* desk) const;

  // Updates the text labels of the existing mini_views. This is called after a
  // mini_view has been removed.
  void UpdateMiniViewsLabels();

  // Returns the X offset of the first mini_view on the left (if there's one),
  // or the X offset of this view's center point when there are no mini_views.
  // This offset is used to calculate the amount by which the mini_views should
  // be moved when performing the mini_view creation or deletion animations.
  int GetFirstMiniViewXOffset() const;

  // Updates the cached minimum width required to fit all contents.
  void UpdateMinimumWidthToFitContents();

  // A view that shows a dark gary transparent background that can be animated
  // when the very first mini_views are created.
  views::View* background_view_;

  NewDeskButton* new_desk_button_;

  // The views representing desks mini_views. They're owned by this DeskBarView
  // (i.e. `owned_by_client_` is true).
  std::vector<std::unique_ptr<DeskMiniView>> mini_views_;

  // Observes mouse events on the desks bar widget and updates the states of the
  // mini_views accordingly.
  std::unique_ptr<DeskBarHoverObserver> hover_observer_;

  // The screen location of the most recent drag position. This value is valid
  // only when the below `dragged_item_on_bar_` is true.
  gfx::Point last_dragged_item_screen_location_;

  // True when the drag location of the overview item is intersecting with this
  // view.
  bool dragged_item_over_bar_ = false;

  // The OverviewGrid that contains this object.
  OverviewGrid* overview_grid_;

  // Caches the calculated minimum width to fit contents.
  int min_width_to_fit_contents_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DesksBarView);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_BAR_VIEW_H_
