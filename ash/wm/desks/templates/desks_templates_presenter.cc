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
#include "ash/wm/desks/desks_bar_view.h"
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

// Callback ran after creating and activating a new desk for launching a
// template. Launches apps into the active desk.
void OnNewDeskCreatedForTemplate(std::unique_ptr<DeskTemplate> desk_template,
                                 base::TimeDelta delay,
                                 aura::Window* root_window,
                                 bool on_create_activate_success) {
  if (!on_create_activate_success)
    return;

  Shell::Get()->desks_templates_delegate()->LaunchAppsFromTemplate(
      std::move(desk_template), delay);

  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  DesksBarView* desks_bar_view = const_cast<DesksBarView*>(
      overview_session->GetGridWithRootWindow(root_window)->desks_bar_view());
  desks_bar_view->set_should_name_nudge(true);
  desks_bar_view->UpdateNewMiniViews(
      /*initializing_bar_view=*/false,
      /*is_expanding_bar_view*/ true);
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
      template_uuid,
      base::BindOnce(&DesksTemplatesPresenter::OnDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(), template_uuid));
  cached_saved_template_uuid_.reset();
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
                     weak_ptr_factory_.GetWeakPtr(), delay, root_window));
}

void DesksTemplatesPresenter::MaybeSaveActiveDeskAsTemplate(
    aura::Window* root_window_to_show) {
  DesksController::Get()->CaptureActiveDeskAsTemplate(
      base::BindOnce(&DesksTemplatesPresenter::SaveOrUpdateDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*is_update=*/false),
      root_window_to_show);
}

void DesksTemplatesPresenter::SaveOrUpdateDeskTemplate(
    bool is_update,
    std::unique_ptr<DeskTemplate> desk_template) {
  if (!desk_template)
    return;

  if (is_update)
    desk_template->set_updated_time(base::Time::Now());

  weak_ptr_factory_.InvalidateWeakPtrs();

  if (!is_update) {
    RecordWindowAndTabCountHistogram(desk_template.get());
    cached_saved_template_uuid_ = desk_template->uuid();
  } else {
    cached_saved_template_uuid_.reset();
  }

  // TODO(richui): Look into passing the entire template and not just the
  // UUID.
  const std::string template_uuid = desk_template->uuid().AsLowercaseString();

  // Save or update `desk_template` as an entry in DeskModel.
  GetDeskModel()->AddOrUpdateEntry(
      std::move(desk_template),
      base::BindOnce(&DesksTemplatesPresenter::OnAddOrUpdateEntry,
                     weak_ptr_factory_.GetWeakPtr(), is_update, template_uuid));
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
                                overview_grid->GetGridEffectiveBounds());
      if (cached_saved_template_uuid_) {
        for (auto* item_view : grid_view->grid_items()) {
          if (cached_saved_template_uuid_ ==
              item_view->desk_template()->uuid()) {
            // If a template was newly added, set focus on that template item's
            // name view.
            DCHECK(!item_view->name_view()->GetReadOnly());
            item_view->name_view()->RequestFocus();
            cached_saved_template_uuid_.reset();
            break;
          }
        }
      }
    }
  }

  if (on_update_ui_closure_for_testing_)
    std::move(on_update_ui_closure_for_testing_).Run();
}

void DesksTemplatesPresenter::GetEntryByUUID(const std::string& template_uuid) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetDeskModel()->GetEntryByUUID(
      template_uuid, base::BindOnce(&DesksTemplatesPresenter::OnGetEntryByUUID,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void DesksTemplatesPresenter::OnGetEntryByUUID(
    desks_storage::DeskModel::GetEntryByUuidStatus status,
    std::unique_ptr<ash::DeskTemplate> entry) {
  if (status != desks_storage::DeskModel::GetEntryByUuidStatus::kOk)
    return;

  if (!entry)
    return;

  AddOrUpdateUIEntries({entry.get()});
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

  // Launch the windows as specified in the template to a new desk.
  // Calling `CreateAndActivateNewDeskForTemplate` results in exiting overview
  // mode, which means the presenter doesn't exist anymore on callback (since it
  // is owned by `OverviewSession`). Because of this, we bind a non-member
  // function in the anonymous namespace.
  const auto template_name = entry->template_name();
  DesksController::Get()->CreateAndActivateNewDeskForTemplate(
      template_name, base::BindOnce(&OnNewDeskCreatedForTemplate,
                                    std::move(entry), delay, root_window));

  if (on_update_ui_closure_for_testing)
    std::move(on_update_ui_closure_for_testing).Run();

  RecordLaunchTemplateHistogram();
}

void DesksTemplatesPresenter::OnAddOrUpdateEntry(
    bool was_update,
    const std::string& template_uuid,
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

  // If the templates grid is already shown, just update the entry.
  if (overview_session_->IsShowingDesksTemplatesGrid()) {
    GetEntryByUUID(template_uuid);
    return;
  }

  // Update the button here in case it has been disabled.
  const auto& grid_list = overview_session_->grid_list();
  DCHECK(!grid_list.empty());
  overview_session_->ShowDesksTemplatesGrids(
      grid_list.front()->desks_bar_view()->IsZeroState());

  if (!was_update) {
    RecordNewTemplateHistogram();
    RecordUserTemplateCountHistogram(GetEntryCount(), GetMaxEntryCount());
  }
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
          ->AddOrUpdateTemplates(new_entries, /*initializing_grid_view=*/false);
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
