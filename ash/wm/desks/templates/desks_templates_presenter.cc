// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_presenter.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/system/toast_catalog.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/templates/desks_templates_grid_view.h"
#include "ash/wm/desks/templates/desks_templates_item_view.h"
#include "ash/wm/desks/templates/desks_templates_metrics_util.h"
#include "ash/wm/desks/templates/desks_templates_name_view.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

DesksTemplatesPresenter* g_instance = nullptr;

// Toast names.
constexpr char kMaximumDeskLaunchTemplateToastName[] =
    "MaximumDeskLaunchTemplateToast";
constexpr char kTemplateTooLargeToastName[] = "TemplateTooLargeToast";

// Helper to get the desk model from the shell delegate. Should always return a
// usable desk model, either from chrome sync, or a local storage.
desks_storage::DeskModel* GetDeskModel() {
  auto* desk_model = Shell::Get()->desks_templates_delegate()->GetDeskModel();
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
  GetAllEntries(base::GUID(), Shell::GetPrimaryRootWindow());
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
    }

    if (DesksBarView* desks_bar_view =
            const_cast<DesksBarView*>(overview_grid->desks_bar_view())) {
      desks_bar_view->UpdateDesksTemplatesButtonVisibility();
      desks_bar_view->UpdateButtonsForDesksTemplatesGrid();
      overview_grid->UpdateSaveDeskButtons();
    }
  }
}

void DesksTemplatesPresenter::GetAllEntries(const base::GUID& item_to_focus,
                                            aura::Window* const root_window) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetDeskModel()->GetAllEntries(base::BindOnce(
      &DesksTemplatesPresenter::OnGetAllEntries, weak_ptr_factory_.GetWeakPtr(),
      item_to_focus, root_window));
}

void DesksTemplatesPresenter::DeleteEntry(const std::string& template_uuid) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetDeskModel()->DeleteEntry(
      template_uuid,
      base::BindOnce(&DesksTemplatesPresenter::OnDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(), template_uuid));
}

void DesksTemplatesPresenter::LaunchDeskTemplate(
    const std::string& template_uuid,
    base::TimeDelta delay,
    aura::Window* root_window) {
  // If we are at the max desk limit (currently is 8), a new desk
  // cannot be created, and a toast will be displayed to the user.
  if (!DesksController::Get()->CanCreateDesks()) {
    ToastData toast_data = {
        /*id=*/kMaximumDeskLaunchTemplateToastName,
        ToastCatalogName::kMaximumDeskLaunchTemplate,
        /*text=*/
        l10n_util::GetStringFUTF16(
            IDS_ASH_DESKS_TEMPLATES_REACH_MAXIMUM_DESK_TOAST,
            base::FormatNumber(desks_util::kMaxNumberOfDesks))};
    ToastManager::Get()->Show(toast_data);
    return;
  }

  weak_ptr_factory_.InvalidateWeakPtrs();

  GetDeskModel()->GetEntryByUUID(
      template_uuid,
      base::BindOnce(&DesksTemplatesPresenter::OnGetTemplateForDeskLaunch,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now(), delay,
                     root_window));
}

void DesksTemplatesPresenter::MaybeSaveActiveDeskAsTemplate(
    DeskTemplateType template_type,
    aura::Window* root_window_to_show) {
  DesksController::Get()->CaptureActiveDeskAsTemplate(
      base::BindOnce(&DesksTemplatesPresenter::SaveOrUpdateDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*is_update=*/false, root_window_to_show),
      template_type, root_window_to_show);
}

void DesksTemplatesPresenter::SaveOrUpdateDeskTemplate(
    bool is_update,
    aura::Window* const root_window,
    std::unique_ptr<DeskTemplate> desk_template) {
  if (!desk_template)
    return;

  if (is_update)
    desk_template->set_updated_time(base::Time::Now());
  else
    RecordWindowAndTabCountHistogram(desk_template.get());

  // Clone the desk template so one can be sent to the model, and the other as
  // part of the callback.
  // TODO: Investigate if we can modify the model to send the template as one of
  // the callback args.
  auto desk_template_clone = desk_template->Clone();

  // Save or update `desk_template` as an entry in DeskModel.
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetDeskModel()->AddOrUpdateEntry(
      std::move(desk_template),
      base::BindOnce(&DesksTemplatesPresenter::OnAddOrUpdateEntry,
                     weak_ptr_factory_.GetWeakPtr(), is_update, root_window,
                     std::move(desk_template_clone)));
}

void DesksTemplatesPresenter::OnDeskModelDestroying() {
  desk_model_observation_.Reset();
}

void DesksTemplatesPresenter::EntriesAddedOrUpdatedRemotely(
    const std::vector<const DeskTemplate*>& new_entries) {
  AddOrUpdateUIEntries(new_entries);
}

void DesksTemplatesPresenter::EntriesRemovedRemotely(
    const std::vector<std::string>& uuids) {
  RemoveUIEntries(uuids);
}

void DesksTemplatesPresenter::OnGetAllEntries(
    const base::GUID& item_to_focus,
    aura::Window* const root_window,
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
      auto* grid_view =
          static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView());
      grid_view->PopulateGridUI(entries,
                                overview_grid->GetGridEffectiveBounds(),
                                /*last_saved_template_uuid=*/item_to_focus);
      DesksTemplatesItemView* item_view =
          grid_view->GetItemForUUID(item_to_focus);
      if (!item_view)
        continue;

      item_view->MaybeRemoveNameNumber();
      if (grid_widget->GetNativeWindow()->GetRootWindow() == root_window)
        item_view->name_view()->RequestFocus();
    }
  }

  if (on_update_ui_closure_for_testing_)
    std::move(on_update_ui_closure_for_testing_).Run();
}

void DesksTemplatesPresenter::OnDeleteEntry(
    const std::string& template_uuid,
    desks_storage::DeskModel::DeleteEntryStatus status) {
  if (status != desks_storage::DeskModel::DeleteEntryStatus::kOk)
    return;

  RecordDeleteTemplateHistogram();
  RecordUserTemplateCountHistogram(GetEntryCount(), GetMaxEntryCount());
  RemoveUIEntries({template_uuid});
}

void DesksTemplatesPresenter::OnGetTemplateForDeskLaunch(
    base::Time time_launch_started,
    base::TimeDelta delay,
    aura::Window* root_window,
    desks_storage::DeskModel::GetEntryByUuidStatus status,
    std::unique_ptr<DeskTemplate> entry) {
  if (status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk)
    return;

  // `CreateAndActivateNewDeskForTemplate` may destroy `this`. Copy the member
  // variables to a local to prevent UAF. See https://crbug.com/1284138.
  base::OnceClosure on_update_ui_closure_for_testing =
      std::move(on_update_ui_closure_for_testing_);

  const auto template_name = entry->template_name();
  const bool activate_desk = entry->type() == DeskTemplateType::kTemplate;
  DesksController::Get()->CreateNewDeskForTemplate(
      template_name, activate_desk,
      base::BindOnce(&DesksTemplatesPresenter::OnNewDeskCreatedForTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(entry),
                     time_launch_started, delay, root_window));

  if (on_update_ui_closure_for_testing)
    std::move(on_update_ui_closure_for_testing).Run();

  RecordLaunchTemplateHistogram();
}

void DesksTemplatesPresenter::OnNewDeskCreatedForTemplate(
    std::unique_ptr<DeskTemplate> desk_template,
    base::Time time_launch_started,
    base::TimeDelta delay,
    aura::Window* root_window,
    const Desk* new_desk) {
  // Desk creation failed.
  if (!new_desk)
    return;

  // For Save & Recall, the underlying desk definition is deleted on launch. We
  // store the template ID here since we're about to move the desk template.
  const bool delete_template_on_launch =
      desk_template->type() == DeskTemplateType::kSaveAndRecall;
  const std::string template_uuid = desk_template->uuid().AsLowercaseString();

  // Copy the index of the newly created desk to the template. This ensures that
  // apps appear on the right desk even if the user switches to another.
  const int desk_index = DesksController::Get()->GetDeskIndex(new_desk);
  desk_template->SetDeskIndex(desk_index);

  Shell::Get()->desks_templates_delegate()->LaunchAppsFromTemplate(
      std::move(desk_template), time_launch_started, delay);

  DesksBarView* desks_bar_view = const_cast<DesksBarView*>(
      overview_session_->GetGridWithRootWindow(root_window)->desks_bar_view());
  desks_bar_view->NudgeDeskName(desk_index);

  if (delete_template_on_launch)
    DeleteEntry(template_uuid);
}

void DesksTemplatesPresenter::OnAddOrUpdateEntry(
    bool was_update,
    aura::Window* const root_window,
    std::unique_ptr<DeskTemplate> desk_template,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  RecordAddOrUpdateTemplateStatusHistogram(status);

  if (status ==
      desks_storage::DeskModel::AddOrUpdateEntryStatus::kEntryTooLarge) {
    // Show a toast if the template we tried to save was too large to be
    // transported through Chrome Sync.
    ToastData toast_data(kTemplateTooLargeToastName,
                         ToastCatalogName::kDeskTemplateTooLarge,
                         l10n_util::GetStringUTF16(
                             IDS_ASH_DESKS_TEMPLATES_TEMPLATE_TOO_LARGE_TOAST));
    ToastManager::Get()->Show(toast_data);
    return;
  }

  if (status != desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk)
    return;

  // If the templates grid has already been shown before, update the entry.
  OverviewGrid* overview_grid =
      overview_session_->GetGridWithRootWindow(root_window);
  DCHECK(overview_grid);
  const bool is_zero_state = overview_grid->desks_bar_view()->IsZeroState();

  views::Widget* grid_widget = overview_grid->desks_templates_grid_widget();
  if (grid_widget) {
    AddOrUpdateUIEntries({desk_template.get()});

    if (!was_update) {
      // Shows the grid if it was hidden. This will not call `GetAllEntries`.
      overview_session_->ShowDesksTemplatesGrids(is_zero_state, base::GUID(),
                                                 root_window);
      auto* grid_view =
          static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView());
      DesksTemplatesItemView* item_view =
          grid_view->GetItemForUUID(desk_template->uuid());
      if (item_view) {
        item_view->MaybeRemoveNameNumber();
        item_view->name_view()->RequestFocus();
      }
    }

    if (on_update_ui_closure_for_testing_)
      std::move(on_update_ui_closure_for_testing_).Run();
    return;
  }

  // This will update the templates button and save as desks button too. This
  // will call `GetAllEntries`.
  overview_session_->ShowDesksTemplatesGrids(
      is_zero_state, desk_template->uuid(), root_window);

  if (!was_update) {
    if (desk_template->type() == DeskTemplateType::kSaveAndRecall) {
      // We have successfully created a *new* desk template for Save & Recall,
      // so we are now going to close all the windows on the active desk and
      // also remove the desk.
      auto* desks_controller = DesksController::Get();
      auto* active_desk = desks_controller->active_desk();

      // If this is the only desk, we have to create a new desk before we can
      // remove the current one.
      if (!desks_controller->CanRemoveDesks())
        desks_controller->NewDesk(DesksCreationRemovalSource::kSaveAndRecall);

      desks_controller->RemoveDesk(active_desk,
                                   DesksCreationRemovalSource::kSaveAndRecall,
                                   /*close_windows=*/true);
    }

    RecordNewTemplateHistogram();
    RecordUserTemplateCountHistogram(GetEntryCount(), GetMaxEntryCount());
  }

  // Note we do not run `on_update_ui_closure_for_testing` here as we want to
  // wait for the `GetAllEntries` fired in `ShowDesksTemplatesGrids`.
}

void DesksTemplatesPresenter::AddOrUpdateUIEntries(
    const std::vector<const DeskTemplate*>& new_entries) {
  if (new_entries.empty())
    return;

  // This updates `should_show_templates_ui_`.
  UpdateDesksTemplatesUI();

  for (auto& overview_grid : overview_session_->grid_list()) {
    // Update `DesksTemplatesGridView` with the new or added desk template
    // entries.
    if (views::Widget* grid_widget =
            overview_grid->desks_templates_grid_widget()) {
      static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView())
          ->AddOrUpdateTemplates(new_entries, /*initializing_grid_view=*/false,
                                 /*last_saved_template_uuid=*/base::GUID());
    }
  }

  if (on_update_ui_closure_for_testing_)
    std::move(on_update_ui_closure_for_testing_).Run();
}

void DesksTemplatesPresenter::RemoveUIEntries(
    const std::vector<std::string>& uuids) {
  if (uuids.empty())
    return;

  // This updates `should_show_templates_ui_`.
  UpdateDesksTemplatesUI();

  for (auto& overview_grid : overview_session_->grid_list()) {
    // Remove the entries from `DesksTemplatesGridView`.
    if (views::Widget* grid_widget =
            overview_grid->desks_templates_grid_widget()) {
      static_cast<DesksTemplatesGridView*>(grid_widget->GetContentsView())
          ->DeleteTemplates(uuids);
    }
  }

  if (on_update_ui_closure_for_testing_)
    std::move(on_update_ui_closure_for_testing_).Run();
}

}  // namespace ash
