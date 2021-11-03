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

// Callback ran after creating and activating a new desk for launching a
// template. Launches apps into the active desk.
void OnNewDeskCreatedForTemplate(std::unique_ptr<DeskTemplate> desk_template,
                                 bool on_create_activate_success) {
  if (!on_create_activate_success)
    return;

  Shell::Get()->shell_delegate()->LaunchAppsFromTemplate(
      std::move(desk_template));
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

size_t DesksTemplatesPresenter::GetEntryCount() const {
  return GetDeskModel()->GetEntryCount();
}

size_t DesksTemplatesPresenter::GetMaxEntryCount() const {
  return GetDeskModel()->GetMaxEntryCount();
}

void DesksTemplatesPresenter::UpdateDesksTemplatesUI() {
  // The save as desk template button is hidden in tablet mode. The desks
  // templates button on the desk bar view and the desks templates grid are
  // hidden in tablet mode and if there no templates to view.
  const bool in_tablet_mode =
      Shell::Get()->tablet_mode_controller()->InTabletMode();
  should_show_templates_ui_ = !in_tablet_mode && GetEntryCount() > 0u;
  for (auto& overview_grid : overview_session_->grid_list()) {
    if (DesksBarView* desks_bar_view =
            const_cast<DesksBarView*>(overview_grid->desks_bar_view())) {
      // When templates is enabled but templates haven't loaded, the templates
      // button may be visible but have a size of 0x0 so we have to make a
      // Layout() call here.
      desks_bar_view->UpdateDesksTemplatesButtonVisibility();
      desks_bar_view->UpdateButtonsForDesksTemplatesGrid();
      desks_bar_view->Layout();
    }

    overview_grid->UpdateSaveDeskAsTemplateButton();

    if (!overview_grid->IsShowingDesksTemplatesGrid())
      continue;

    if (!should_show_templates_ui_) {
      // When deleting, it is possible to delete the last template. In this
      // case, close the template grid and go back to overview.
      overview_grid->HideDesksTemplatesGrid(/*exit_overview=*/false);
      continue;
    }
  }
}

void DesksTemplatesPresenter::GetAllEntries() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetDeskModel()->GetAllEntries(
      base::BindOnce(&DesksTemplatesPresenter::OnGetAllEntries,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DesksTemplatesPresenter::DeleteEntry(const std::string& template_uuid) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetDeskModel()->DeleteEntry(
      template_uuid, base::BindOnce(&DesksTemplatesPresenter::OnDeleteEntry,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void DesksTemplatesPresenter::LaunchDeskTemplate(
    const std::string& template_uuid) {
  // TODO(richui): If we are at the max desk limit (currently is 8), a new desk
  // cannot be created, so we need to display a toast to the user.
  if (!DesksController::Get()->CanCreateDesks())
    return;

  weak_ptr_factory_.InvalidateWeakPtrs();

  GetDeskModel()->GetEntryByUUID(
      template_uuid,
      base::BindOnce(&DesksTemplatesPresenter::OnGetTemplateForDeskLaunch,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DesksTemplatesPresenter::DeskModelLoaded() {}

void DesksTemplatesPresenter::OnDeskModelDestroying() {
  desk_model_observation_.Reset();
}

void DesksTemplatesPresenter::SaveActiveDeskAsTemplate() {
  std::unique_ptr<DeskTemplate> desk_template =
      DesksController::Get()->CaptureActiveDeskAsTemplate();
  auto desk_template_clone = desk_template->Clone();

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Save `desk_template_clone` as an entry in DeskModel.
  GetDeskModel()->AddOrUpdateEntry(
      std::move(desk_template_clone),
      base::BindOnce(&DesksTemplatesPresenter::OnAddOrUpdateEntry,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DesksTemplatesPresenter::OnGetAllEntries(
    desks_storage::DeskModel::GetAllEntriesStatus status,
    std::vector<DeskTemplate*> entries) {
  if (status != desks_storage::DeskModel::GetAllEntriesStatus::kOk)
    return;

  DCHECK_EQ(GetEntryCount(), entries.size());

  // This updates `should_show_templates_ui_`.
  UpdateDesksTemplatesUI();

  for (auto& overview_grid : overview_session_->grid_list()) {
    // Populate `DesksTemplatesGridView` with the desk template entries.
    if (views::Widget* grid_widget =
            overview_grid->desks_templates_grid_widget()) {
      static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView())
          ->UpdateGridUI(entries, overview_grid->GetGridEffectiveBounds());
    }
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

void DesksTemplatesPresenter::OnGetTemplateForDeskLaunch(
    desks_storage::DeskModel::GetEntryByUuidStatus status,
    std::unique_ptr<DeskTemplate> entry) {
  if (status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk)
    return;

  // Launch the windows as specified in the template to a new desk.
  // Calling `CreateAndActivateNewDeskForTemplate` results in exiting overview
  // mode, which means the presenter doesn't exist anymore on callback (since it
  // is owned by `OverviewSession`). Because of this, we bind a non-member
  // function in the anonymous namespace.
  const auto template_name = entry->template_name();
  DesksController::Get()->CreateAndActivateNewDeskForTemplate(
      template_name,
      base::BindOnce(&OnNewDeskCreatedForTemplate, std::move(entry)));

  if (on_update_ui_closure_for_testing_)
    std::move(on_update_ui_closure_for_testing_).Run();
}

void DesksTemplatesPresenter::OnAddOrUpdateEntry(
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  // TODO: Display dialog for unsupported apps using
  // `overview_session_->desks_templates_dialog_controller()`, and update the UI
  // to display the Templates grid after a template has been added.

  // Update the button here in case it has been disabled.
  for (auto& overview_grid : overview_session_->grid_list()) {
    overview_grid->UpdateSaveDeskAsTemplateButton();
  }
}

}  // namespace ash
