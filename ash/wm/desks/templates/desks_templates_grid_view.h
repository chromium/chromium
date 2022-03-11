// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_

#include <vector>

#include "base/guid.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/view.h"

namespace ash {

class DesksTemplatesEventHandler;
class DesksTemplatesItemView;
class DeskTemplate;
class PillButton;

// A view that acts as the content view of the desks templates widget. Displays
// each desk template as a DesksTemplatesItemView.
class DesksTemplatesGridView : public views::View, public aura::WindowObserver {
 public:
  METADATA_HEADER(DesksTemplatesGridView);

  DesksTemplatesGridView();
  DesksTemplatesGridView(const DesksTemplatesGridView&) = delete;
  DesksTemplatesGridView& operator=(const DesksTemplatesGridView&) = delete;
  ~DesksTemplatesGridView() override;

  // Creates and returns the widget that contains the DesksTemplatesGridView in
  // overview mode. This does not show the widget.
  static std::unique_ptr<views::Widget> CreateDesksTemplatesGridWidget(
      aura::Window* root);

  const std::vector<DesksTemplatesItemView*>& grid_items() const {
    return grid_items_;
  }

  // Updates the UI by creating a grid layout and populating the grid with the
  // provided list of desk templates.
  void PopulateGridUI(const std::vector<DeskTemplate*>& desk_templates,
                      const gfx::Rect& grid_bounds);

  // Updates existing templates and adds new templates to the grid. Also sorts
  // `grid_items_` in alphabetical order. This will animate the `grid_items_` to
  // their final positions if `initializing_grid_view` is false. Currently only
  // allows a maximum of 6 templates to be shown in the grid.
  void AddOrUpdateTemplates(const std::vector<const DeskTemplate*>& entries,
                            bool initializing_grid_view);

  // Removes templates from the grid by UUID. Will trigger an animation to
  // shuffle `grid_items_` to their final positions.
  void DeleteTemplates(const std::vector<std::string>& uuids);

  // Returns the grid item view if there is a template name is being modified,
  // otherwise returns `nullptr`.
  DesksTemplatesItemView* GridItemBeingModified();

  // Returns whether the given `point_in_screen` intersects with the feedback
  // button.
  bool IntersectsWithFeedbackButton(const gfx::Point& point_in_screen);

  // Returns whether the given `point_in_screen` intersect with any grid item.
  bool IntersectsWithGridItem(const gfx::Point& point_in_screen);

  // Returns the item view associated with `uuid`.
  DesksTemplatesItemView* GetItemForUUID(const base::GUID& uuid);

  // views::View:
  void Layout() override;
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  friend class DesksTemplatesEventHandler;
  friend class DesksTemplatesGridViewTestApi;

  // Updates the visibility state of the hover buttons on all the `grid_items_`
  // as a result of mouse and gesture events.
  void OnLocatedEvent(ui::LocatedEvent* event, bool is_touch);

  // Calculates the bounds for each grid item within the templates grid. The
  // indices of the returned vector directly correlate to those of `grid_items_`
  // (i.e. the Rect at index 1 of the returned vector should be applied to the
  // `DesksTemplatesItemView` found at index 1 of `grid_items_`).
  std::vector<gfx::Rect> CalculateGridItemPositions() const;

  // Calculates the bounds to be applied to the feedback button based off of the
  // grid item positions.
  gfx::Rect CalculateFeedbackButtonPosition() const;

  // Animates the bounds for all the `grid_items_` (using `bounds_animator_`) to
  // their calculated position. `new_grid_items` contains a list of the
  // newly-created desk template items and will be animated differently than
  // the existing views that are being shifted around.
  void AnimateGridItems(
      const std::vector<DesksTemplatesItemView*>& new_grid_items);

  // Called when the feedback button is pressed. Shows the feedback dialog with
  // desks templates information.
  void OnFeedbackButtonPressed();

  // The views representing templates. They're owned by views hierarchy.
  std::vector<DesksTemplatesItemView*> grid_items_;

  // Owned by views hierarchy. Temporary button to help users give feedback.
  // TODO(crbug.com/1289880): Remove this button when it is no longer needed.
  PillButton* feedback_button_ = nullptr;

  // The underlying window of the templates grid widget.
  aura::Window* widget_window_ = nullptr;

  // Handles mouse/touch events on the desk templates grid widget.
  std::unique_ptr<DesksTemplatesEventHandler> event_handler_;

  // Used to animate individual view positions.
  views::BoundsAnimator bounds_animator_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_
