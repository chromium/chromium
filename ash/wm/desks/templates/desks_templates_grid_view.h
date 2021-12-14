// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_

#include <vector>

#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class TableLayout;
}  // namespace views

namespace ash {

class DesksTemplatesEventHandler;
class DesksTemplatesItemView;
class DeskTemplate;

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
  void AddedToWidget() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  friend class DesksTemplatesEventHandler;
  friend class DesksTemplatesGridViewTestApi;

  // Updates the visibility state of the hover buttons on all the `grid_items_`
  // as a result of mouse and gesture events.
  void OnLocatedEvent(ui::LocatedEvent* event, bool is_touch);

  // Owned by the views hierarchy.
  // TODO(richui): This is temporary until we decide what is the best way to
  // layout the grid which may be 2x3 or 3x2 depending on the display size and
  // needs to support animations when items are added or removed.
  views::TableLayout* layout_ = nullptr;

  // The views representing templates. They're owned by views hierarchy.
  std::vector<DesksTemplatesItemView*> grid_items_;

  // The underlying window of the templates grid widget.
  aura::Window* widget_window_ = nullptr;

  // Handles mouse/touch events on the desk templates grid widget.
  std::unique_ptr<DesksTemplatesEventHandler> event_handler_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_
