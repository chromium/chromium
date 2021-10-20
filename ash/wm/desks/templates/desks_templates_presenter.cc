// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_presenter.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/templates/desks_templates_grid_view.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "base/bind.h"

namespace ash {

namespace {

DesksTemplatesPresenter* g_instance = nullptr;

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

  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;

  auto* desk_model = GetDeskModel();
  desk_model_observation_.Observe(desk_model);
  if (desk_model->IsReady())
    GetAllEntries();
}

DesksTemplatesPresenter::~DesksTemplatesPresenter() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
DesksTemplatesPresenter* DesksTemplatesPresenter::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void DesksTemplatesPresenter::GetAllEntries() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetDeskModel()->GetAllEntries(
      base::BindOnce(&DesksTemplatesPresenter::OnGetAllEntries,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DesksTemplatesPresenter::DeskModelLoaded() {
  GetAllEntries();
}

void DesksTemplatesPresenter::DeleteEntry(const std::string& template_uuid) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetDeskModel()->DeleteEntry(
      template_uuid, base::BindOnce(&DesksTemplatesPresenter::OnDeleteEntry,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void DesksTemplatesPresenter::OnDeskModelDestroying() {
  desk_model_observation_.Reset();
}

void DesksTemplatesPresenter::OnGetAllEntries(
    desks_storage::DeskModel::GetAllEntriesStatus status,
    std::vector<DeskTemplate*> entries) {
  if (status != desks_storage::DeskModel::GetAllEntriesStatus::kOk)
    return;

  // The desks templates button is invisible if there are no entries to view.
  const bool visible = !entries.empty();
  for (auto& overview_grid : overview_session_->grid_list()) {
    const DesksBarView* desks_bar_view = overview_grid->desks_bar_view();
    if (desks_bar_view) {
      const bool is_zero_state = desks_bar_view->IsZeroState();
      desks_bar_view->zero_state_desks_templates_button()->SetVisible(
          is_zero_state && visible);
      desks_bar_view->expanded_state_desks_templates_button()->SetVisible(
          !is_zero_state && visible);
    }

    if (!overview_grid->IsShowingDesksTemplatesGrid())
      continue;

    if (!visible) {
      // When deleting, it is possible to delete the last template. In this
      // case, close the template grid and go back to overview.
      overview_grid->HideDesksTemplatesGrid();
      continue;
    }

    // Populate `DesksTemplatesGridView` with the desk template entries.
    views::Widget* grid_widget = overview_grid->desks_templates_grid_widget();
    DCHECK(grid_widget);
    static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView())
        ->UpdateGridUI(entries, overview_grid->GetGridEffectiveBounds());
  }

  if (on_update_ui_closure_for_testing_)
    std::move(on_update_ui_closure_for_testing_).Run();
}

void DesksTemplatesPresenter::OnDeleteEntry(
    desks_storage::DeskModel::DeleteEntryStatus status) {
  if (status != desks_storage::DeskModel::DeleteEntryStatus::kOk)
    return;

  GetAllEntries();
}

}  // namespace ash
