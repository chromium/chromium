// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_presenter.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/templates/desks_templates_grid_view.h"
#include "ash/wm/desks/templates/desks_templates_metrics_util.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

DesksTemplatesPresenter* g_instance = nullptr;

// The amount of time for which the launch template toasts will remain
// displayed.
constexpr int kLaunchTemplateToastDurationMs = 6 * 1000;

// Toast name.
constexpr char kMaximumDeskLaunchTemplateToastName[] =
    "MaximumDeskLaunchTemplateToast";

// Helper to get the desk model from the shell delegate. Should always return a
// usable desk model, either from chrome sync, or a local storage.
desks_storage::DeskModel* GetDeskModel() {
  auto* desk_model = Shell::Get()->desks_templates_delegate()->GetDeskModel();
  DCHECK(desk_model);
  return desk_model;
}

// Callback ran after creating and activating a new desk for launching a
// template. Launches apps into the active desk.
void OnNewDeskCreatedForTemplate(std::unique_ptr<DeskTemplate> desk_template,
                                 bool on_create_activate_success) {
  if (!on_create_activate_success)
    return;

  Shell::Get()->desks_templates_delegate()->LaunchAppsFromTemplate(
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
    if (!should_show_templates_ui_ &&
        overview_grid->IsShowingDesksTemplatesGrid()) {
      // When deleting, it is possible to delete the last template. In this
      // case, close the template grid and go back to overview.
      overview_grid->HideDesksTemplatesGrid(/*exit_overview=*/false);
      continue;
    }

    if (DesksBarView* desks_bar_view =
            const_cast<DesksBarView*>(overview_grid->desks_bar_view())) {
      // When templates is enabled but templates haven't loaded, the templates
      // button may be visible but have a size of 0x0 so we have to make a
      // Layout() call here.
      desks_bar_view->UpdateDesksTemplatesButtonVisibility();
      desks_bar_view->UpdateButtonsForDesksTemplatesGrid();
      desks_bar_view->Layout();
      overview_grid->UpdateSaveDeskAsTemplateButton();
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
  // If we are at the max desk limit (currently is 8), a new desk
  // cannot be created, and a toast will be displayed to the user.
  if (!DesksController::Get()->CanCreateDesks()) {
    ToastData toast_data = {
        /*id=*/kMaximumDeskLaunchTemplateToastName,
        /*text=*/
        l10n_util::GetStringFUTF16(
            IDS_ASH_DESKS_TEMPLATES_REACH_MAXIMUM_DESK_TOAST,
            base::FormatNumber(desks_util::kMaxNumberOfDesks)),
        kLaunchTemplateToastDurationMs,
        /*dismiss_text=*/absl::nullopt};
    ToastManager::Get()->Show(toast_data);
    return;
  }

  weak_ptr_factory_.InvalidateWeakPtrs();

  GetDeskModel()->GetEntryByUUID(
      template_uuid,
      base::BindOnce(&DesksTemplatesPresenter::OnGetTemplateForDeskLaunch,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DesksTemplatesPresenter::MaybeSaveActiveDeskAsTemplate() {
  DesksController::Get()->CaptureActiveDeskAsTemplate(
      base::BindOnce(&DesksTemplatesPresenter::SaveOrUpdateDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*is_update=*/false));
}

void DesksTemplatesPresenter::SaveOrUpdateDeskTemplate(
    bool is_update,
    std::unique_ptr<DeskTemplate> desk_template) {
  if (!desk_template)
    return;

  auto desk_template_clone = desk_template->Clone();

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Save or update `desk_template_clone` as an entry in DeskModel.
  GetDeskModel()->AddOrUpdateEntry(
      std::move(desk_template_clone),
      base::BindOnce(&DesksTemplatesPresenter::OnAddOrUpdateEntry,
                     weak_ptr_factory_.GetWeakPtr(), is_update));
}

void DesksTemplatesPresenter::OnDeskModelDestroying() {
  desk_model_observation_.Reset();
}

void DesksTemplatesPresenter::EntriesAddedOrUpdatedRemotely(
    const std::vector<const DeskTemplate*>& new_entries) {
  if (overview_session_->IsShowingDesksTemplatesGrid())
    GetAllEntries();
}

void DesksTemplatesPresenter::EntriesRemovedRemotely(
    const std::vector<std::string>& uuids) {
  if (overview_session_->IsShowingDesksTemplatesGrid())
    GetAllEntries();
}

void DesksTemplatesPresenter::OnGetAllEntries(
    desks_storage::DeskModel::GetAllEntriesStatus status,
    const std::vector<DeskTemplate*>& entries) {
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

  RecordDeleteTemplateHistogram();
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

  RecordLaunchTemplateHistogram();
}

void DesksTemplatesPresenter::OnAddOrUpdateEntry(
    bool was_update,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  if (status != desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk)
    return;

  // If the templates grid is already shown, just update the entries.
  if (overview_session_->IsShowingDesksTemplatesGrid()) {
    GetAllEntries();
    return;
  }

  // Update the button here in case it has been disabled.
  const auto& grid_list = overview_session_->grid_list();
  DCHECK(!grid_list.empty());
  overview_session_->ShowDesksTemplatesGrids(
      grid_list.front()->desks_bar_view()->IsZeroState());
  for (auto& overview_grid : grid_list)
    overview_grid->UpdateSaveDeskAsTemplateButton();

  if (!was_update)
    RecordNewTemplateHistogram();
}

}  // namespace ash
