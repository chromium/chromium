// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_test_util.h"

#include "ash/shell.h"
#include "ash/wm/desks/desks_bar_view.h"
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

views::Button* GetZeroStateDesksTemplatesButton() {
  const auto* overview_grid = GetPrimaryOverviewGrid();
  if (!overview_grid)
    return nullptr;

  // May be null in tablet mode.
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  return desks_bar_view ? desks_bar_view->zero_state_desks_templates_button()
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

}  // namespace ash
