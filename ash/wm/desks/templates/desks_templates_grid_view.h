// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_

#include <vector>

#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class DesksTemplatesEventHandler;
class DesksTemplatesItemView;
class DeskTemplate;
class PillButton;

// A view that acts as the content view of the desks templates widget.
// TODO(richui): Add details and ASCII.
class DesksTemplatesGridView : public views::View, public aura::WindowObserver {
 public:
  METADATA_HEADER(DesksTemplatesGridView);

  DesksTemplatesGridView();
  DesksTemplatesGridView(const DesksTemplatesGridView&) = delete;
  DesksTemplatesGridView& operator=(const DesksTemplatesGridView&) = delete;
  ~DesksTemplatesGridView() override;

  // Creates and returns the widget that contains the DesksTemplatesGridView in
  // overview mode. This does not show the widget.
  // TODO(sammiequon): We might want this view to be part of the DesksWidget
  // depending on the animations.
  static std::unique_ptr<views::Widget> CreateDesksTemplatesGridWidget(
      aura::Window* root);

  const std::vector<DesksTemplatesItemView*>& grid_items() const {
    return grid_items_;
  }

  // Updates the UI by creating a grid layout and populating the grid with the
  // provided list of desk templates.
  void UpdateGridUI(const std::vector<DeskTemplate*>& desk_templates,
                    const gfx::Rect& grid_bounds);

  // Returns true if a template name is being modified using an item view's
  // `DesksTemplatesNameView` in this grid.
  bool IsTemplateNameBeingModified() const;

  // views::View:
  void Layout() override;
  void AddedToWidget() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  friend class DesksTemplatesEventHandler;
  friend class DesksTemplatesGridViewTestApi;

  // Updates the visibility state of the hover buttons on all the `grid_items_`
  // as a result of mouse and gesture events.
  void OnLocatedEvent(ui::LocatedEvent* event, bool is_touch);

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
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_
