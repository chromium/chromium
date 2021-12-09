// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_test_util.h"

#include "ash/shell.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/templates/desks_templates_item_view.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_test_util.h"

namespace ash {

namespace {

// Gets the overview grid associated with the primary root window. Returns null
// if we aren't in overview.
OverviewGrid* GetPrimaryOverviewGrid() {
  auto* overview_session = GetOverviewSession();
  return overview_session ? overview_session->GetGridWithRootWindow(
                                Shell::GetPrimaryRootWindow())
                          : nullptr;
}

}  // namespace

DesksTemplatesPresenterTestApi::DesksTemplatesPresenterTestApi(
    DesksTemplatesPresenter* presenter)
    : presenter_(presenter) {
  DCHECK(presenter_);
}

DesksTemplatesPresenterTestApi::~DesksTemplatesPresenterTestApi() = default;

void DesksTemplatesPresenterTestApi::SetOnUpdateUiClosure(
    base::OnceClosure closure) {
  DCHECK(!presenter_->on_update_ui_closure_for_testing_);
  presenter_->on_update_ui_closure_for_testing_ = std::move(closure);
}

DesksTemplatesGridViewTestApi::DesksTemplatesGridViewTestApi(
    const DesksTemplatesGridView* grid_view)
    : grid_view_(grid_view) {
  DCHECK(grid_view_);
}

DesksTemplatesGridViewTestApi::~DesksTemplatesGridViewTestApi() = default;

DesksTemplatesItemViewTestApi::DesksTemplatesItemViewTestApi(
    const DesksTemplatesItemView* item_view)
    : item_view_(item_view) {
  DCHECK(item_view_);
}

DesksTemplatesItemViewTestApi::~DesksTemplatesItemViewTestApi() = default;

DesksTemplatesIconViewTestApi::DesksTemplatesIconViewTestApi(
    const DesksTemplatesIconView* desks_templates_icon_view)
    : desks_templates_icon_view_(desks_templates_icon_view) {
  DCHECK(desks_templates_icon_view_);
}

DesksTemplatesIconViewTestApi::~DesksTemplatesIconViewTestApi() = default;

DesksTemplatesNameViewTestApi::DesksTemplatesNameViewTestApi(
    const DesksTemplatesNameView* desks_templates_name_view)
    : desks_templates_name_view_(desks_templates_name_view) {
  DCHECK(desks_templates_name_view_);
}

DesksTemplatesNameViewTestApi::~DesksTemplatesNameViewTestApi() = default;

DesksTemplatesItemView* GetItemViewFromTemplatesGrid(int grid_item_index) {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;

  views::Widget* grid_widget = overview_grid->desks_templates_grid_widget();
  DCHECK(grid_widget);

  const DesksTemplatesGridView* templates_grid_view =
      static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView());
  DCHECK(templates_grid_view);

  std::vector<DesksTemplatesItemView*> grid_items =
      DesksTemplatesGridViewTestApi(templates_grid_view).grid_items();
  DesksTemplatesItemView* item_view = grid_items.at(grid_item_index);
  DCHECK(item_view);
  return item_view;
}

views::Button* GetZeroStateDesksTemplatesButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;

  // May be null in tablet mode.
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  return desks_bar_view ? desks_bar_view->zero_state_desks_templates_button()
                        : nullptr;
}

views::Button* GetExpandedStateDesksTemplatesButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;

  // May be null in tablet mode.
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  return desks_bar_view
             ? desks_bar_view->expanded_state_desks_templates_button()
                   ->inner_button()
             : nullptr;
}

views::Button* GetSaveDeskAsTemplateButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;
  views::Widget* widget =
      overview_grid->save_desk_as_template_widget_for_testing();
  return widget ? static_cast<views::Button*>(widget->GetContentsView())
                : nullptr;
}

views::Button* GetTemplateItemButton(int index) {
  auto* item = GetItemViewFromTemplatesGrid(index);
  return item ? static_cast<views::Button*>(item) : nullptr;
}

void WaitForDesksTemplatesUI() {
  auto* overview_session = GetOverviewSession();
  DCHECK(overview_session);

  base::RunLoop run_loop;
  DesksTemplatesPresenterTestApi(overview_session->desks_templates_presenter())
      .SetOnUpdateUiClosure(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace ash
