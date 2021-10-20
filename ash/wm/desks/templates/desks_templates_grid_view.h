// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class GridLayout;
class UniqueWidgetPtr;
}  // namespace views

namespace ash {

class DesksTemplatesItemView;
class DeskTemplate;

// A view that acts as the content view of the desks templates widget.
// TODO(richui): Add details and ASCII.
class DesksTemplatesGridView : public views::View {
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
  static views::UniqueWidgetPtr CreateDesksTemplatesGridWidget(
      aura::Window* root);

  // Updates the UI by creating a grid layout and populating the grid with the
  // provided list of desk templates.
  void UpdateGridUI(const std::vector<DeskTemplate*>& desk_templates,
                    const gfx::Rect& grid_bounds);

  // views::View:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

 private:
  friend class DesksTemplatesGridViewTestApi;

  // Helper to unify mouse/touch events.
  void OnLocatedEvent(ui::LocatedEvent* event, bool is_touch);

  // Owned by the views hierarchy.
  views::GridLayout* layout_ = nullptr;

  // The views representing templates. They're owned by views hierarchy.
  std::vector<DesksTemplatesItemView*> grid_items_;

  // The underlying window of the templates grid widget.
  aura::Window* widget_window_ = nullptr;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_GRID_VIEW_H_
