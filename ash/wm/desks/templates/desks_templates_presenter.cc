// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_presenter.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "base/bind.h"

namespace ash {

namespace {

// Helper to get the desk model from the shell delegate. Should always return a
// usable desk model, either from chrome sync, or a local storage.
// TODO(sammiequon): Investigate if we can cache this.
desks_storage::DeskModel* GetDeskModel() {
  auto* desk_model = Shell::Get()->shell_delegate()->GetDeskModel();
  DCHECK(desk_model);
  return desk_model;
}

}  // namespace

DesksTemplatesPresenter::DesksTemplatesPresenter(
    OverviewSession* overview_session)
    : overview_session_(overview_session) {
  DCHECK(overview_session_);

  auto* desk_model = GetDeskModel();
  desk_model_observation_.Observe(desk_model);

  desk_model->GetAllEntries(
      base::BindOnce(&DesksTemplatesPresenter::OnGetAllEntries,
                     weak_ptr_factory_.GetWeakPtr()));
}

DesksTemplatesPresenter::~DesksTemplatesPresenter() = default;

void DesksTemplatesPresenter::OnGetAllEntries(
    desks_storage::DeskModel::GetAllEntriesStatus status,
    std::vector<DeskTemplate*> entries) {
  if (status != desks_storage::DeskModel::GetAllEntriesStatus::kOk)
    return;

  // The desks templates button is invisible if there are no entries to view.
  const bool visible = !entries.empty();
  for (auto& grid : overview_session_->grid_list()) {
    const DesksBarView* desks_bar_view = grid->desks_bar_view();
    if (desks_bar_view) {
      const bool is_zero_state = desks_bar_view->IsZeroState();
      desks_bar_view->zero_state_desks_templates_button()->SetVisible(
          is_zero_state && visible);
      desks_bar_view->expanded_state_desks_templates_button()->SetVisible(
          !is_zero_state && visible);
    }
  }

  if (on_update_ui_closure_for_testing_)
    std::move(on_update_ui_closure_for_testing_).Run();
}

}  // namespace ash
