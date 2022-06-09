// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_presenter.h"

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
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/time/time.h"
#include "components/desks_storage/core/desk_template_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Toast names.
constexpr char kMaximumDeskLaunchTemplateToastName[] =
    "MaximumDeskLaunchTemplateToast";
constexpr char kTemplateTooLargeToastName[] = "TemplateTooLargeToast";

// Duplicate value format.
constexpr char kDuplicateNumberFormat[] = "(%d)";
// Initial duplicate number value.
constexpr char kInitialDuplicateNumberValue[] = " (1)";
// Regex used in determining if duplicate name should be incremented.
constexpr char kDuplicateNumberRegex[] = "\\(([0-9]+)\\)$";

// Helper to get the desk model from the shell delegate. Should always return a
// usable desk model, either from chrome sync, or a local storage.
desks_storage::DeskModel* GetDeskModel() {
  auto* desk_model = Shell::Get()->desks_templates_delegate()->GetDeskModel();
  DCHECK(desk_model);
  return desk_model;
}

}  // namespace

SavedDeskPresenter::SavedDeskPresenter(OverviewSession* overview_session)
    : overview_session_(overview_session) {
  DCHECK(overview_session_);

  auto* desk_model = GetDeskModel();
  desk_model_observation_.Observe(desk_model);
  GetAllEntries(base::GUID(), Shell::GetPrimaryRootWindow());

  should_show_templates_ui_ =
      !Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      (GetEntryCount(DeskTemplateType::kTemplate) +
       GetEntryCount(DeskTemplateType::kSaveAndRecall)) > 0u;
}

SavedDeskPresenter::~SavedDeskPresenter() = default;

size_t SavedDeskPresenter::GetEntryCount(DeskTemplateType type) const {
  auto* model = GetDeskModel();
  return type == DeskTemplateType::kTemplate
             ? model->GetDeskTemplateEntryCount()
             : model->GetSaveAndRecallDeskEntryCount();
}

size_t SavedDeskPresenter::GetMaxEntryCount(DeskTemplateType type) const {
  auto* model = GetDeskModel();
  return type == DeskTemplateType::kTemplate
             ? model->GetMaxDeskTemplateEntryCount()
             : model->GetMaxSaveAndRecallDeskEntryCount();
}

ash::DeskTemplate* SavedDeskPresenter::FindOtherEntryWithName(
    const std::u16string& name,
    ash::DeskTemplateType type,
    const base::GUID& uuid) const {
  return GetDeskModel()->FindOtherEntryWithName(name, type, uuid);
}

void SavedDeskPresenter::UpdateDesksTemplatesUI() {
  // This function:
  //  1. Figures out whether the library button should be shown in the desk bar.
  //  2. Hides the library if necessary.
  //  3. Triggers save desk buttons in the overview overgrid to update.
  //
  // The library and the library button is always hidden if we enter tablet
  // mode. If not in tablet mode, the library button is visible if there are
  // saved desks in the model, *or* we are already showing the library.
  const bool in_tablet_mode =
      Shell::Get()->tablet_mode_controller()->InTabletMode();

  const bool has_saved_desks =
      (GetEntryCount(DeskTemplateType::kTemplate) +
       GetEntryCount(DeskTemplateType::kSaveAndRecall)) > 0u;

  for (auto& overview_grid : overview_session_->grid_list()) {
    const bool is_showing_library =
        overview_grid->IsShowingDesksTemplatesGrid();

    if (in_tablet_mode && is_showing_library) {
      // This happens when entering tablet mode while the library is visible.
      overview_grid->HideDesksTemplatesGrid(/*exit_overview=*/false);
    }

    // The functions below reach into this class to determine whether the
    // buttons should be shown or not. If we are already showing saved desk
    // library, they should not go away (unless we're in tablet mode).
    should_show_templates_ui_ =
        !in_tablet_mode && (is_showing_library || has_saved_desks);

    if (DesksBarView* desks_bar_view =
            const_cast<DesksBarView*>(overview_grid->desks_bar_view())) {
      desks_bar_view->UpdateDesksTemplatesButtonVisibility();
      desks_bar_view->UpdateButtonsForDesksTemplatesGrid();
      overview_grid->UpdateSaveDeskButtons();
    }
  }
}

void SavedDeskPresenter::GetAllEntries(const base::GUID& item_to_focus,
                                       aura::Window* const root_window) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetDeskModel()->GetAllEntries(base::BindOnce(
      &SavedDeskPresenter::OnGetAllEntries, weak_ptr_factory_.GetWeakPtr(),
      item_to_focus, root_window));
}

void SavedDeskPresenter::DeleteEntry(
    const std::string& uuid,
    absl::optional<DeskTemplateType> record_for_type) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetDeskModel()->DeleteEntry(
      uuid,
      base::BindOnce(&SavedDeskPresenter::OnDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(), uuid, record_for_type));
}

void SavedDeskPresenter::LaunchDeskTemplate(const std::string& template_uuid,
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
      base::BindOnce(&SavedDeskPresenter::OnGetTemplateForDeskLaunch,
                     weak_ptr_factory_.GetWeakPtr(), base::Time::Now(), delay,
                     root_window));
}

void SavedDeskPresenter::MaybeSaveActiveDeskAsTemplate(
    DeskTemplateType template_type,
    aura::Window* root_window_to_show) {
  DesksController::Get()->CaptureActiveDeskAsTemplate(
      base::BindOnce(&SavedDeskPresenter::SaveOrUpdateDeskTemplate,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*is_update=*/false, root_window_to_show),
      template_type, root_window_to_show);
}

void SavedDeskPresenter::SaveOrUpdateDeskTemplate(
    bool is_update,
    aura::Window* const root_window,
    std::unique_ptr<DeskTemplate> desk_template) {
  if (!desk_template)
    return;

  if (is_update)
    desk_template->set_updated_time(base::Time::Now());
  else
    RecordWindowAndTabCountHistogram(*desk_template);

  // While we still find duplicate names iterate the duplicate number. i.e.
  // if there are 4 duplicates of some template name then this iterates until
  // the current template will be named 5.
  while (GetDeskModel()->FindOtherEntryWithName(desk_template->template_name(),
                                                desk_template->type(),
                                                desk_template->uuid())) {
    desk_template->set_template_name(
        AppendDuplicateNumberToDuplicateName(desk_template->template_name()));
  }

  // Clone the desk template so one can be sent to the model, and the other as
  // part of the callback.
  // TODO: Investigate if we can modify the model to send the template as one of
  // the callback args.
  auto desk_template_clone = desk_template->Clone();

  // Save or update `desk_template` as an entry in DeskModel.
  GetDeskModel()->AddOrUpdateEntry(
      std::move(desk_template),
      base::BindOnce(&SavedDeskPresenter::OnAddOrUpdateEntry,
                     weak_ptr_factory_.GetWeakPtr(), is_update, root_window,
                     std::move(desk_template_clone)));
}

void SavedDeskPresenter::OnDeskModelDestroying() {
  desk_model_observation_.Reset();
}

void SavedDeskPresenter::EntriesAddedOrUpdatedRemotely(
    const std::vector<const DeskTemplate*>& new_entries) {
  AddOrUpdateUIEntries(new_entries);
}

void SavedDeskPresenter::EntriesRemovedRemotely(
    const std::vector<std::string>& uuids) {
  RemoveUIEntries(uuids);
}

void SavedDeskPresenter::OnGetAllEntries(
    const base::GUID& item_to_focus,
    aura::Window* const root_window,
    desks_storage::DeskModel::GetAllEntriesStatus status,
    const std::vector<const DeskTemplate*>& entries) {
  if (status != desks_storage::DeskModel::GetAllEntriesStatus::kOk)
    return;

  // This updates `should_show_templates_ui_`.
  UpdateDesksTemplatesUI();

  for (auto& overview_grid : overview_session_->grid_list()) {
    // Populate `SavedDeskLibraryView` with the desk template entries.
    if (SavedDeskLibraryView* library_view =
            overview_grid->GetSavedDeskLibraryView()) {
      library_view->PopulateGridUI(entries,
                                   overview_grid->GetGridEffectiveBounds(),
                                   /*last_saved_desk_uuid=*/item_to_focus);
      SavedDeskItemView* item_view =
          library_view->GetItemForUUID(item_to_focus);
      if (!item_view)
        continue;

      item_view->MaybeRemoveNameNumber();
      if (library_view->GetWidget()->GetNativeWindow()->GetRootWindow() ==
          root_window) {
        item_view->name_view()->RequestFocus();
      }
    }
  }

  if (on_update_ui_closure_for_testing_)
    std::move(on_update_ui_closure_for_testing_).Run();
}

void SavedDeskPresenter::OnDeleteEntry(
    const std::string& uuid,
    absl::optional<DeskTemplateType> record_for_type,
    desks_storage::DeskModel::DeleteEntryStatus status) {
  if (status != desks_storage::DeskModel::DeleteEntryStatus::kOk)
    return;

  if (record_for_type) {
    RecordDeleteSavedDeskHistogram(*record_for_type);
    RecordUserSavedDeskCountHistogram(*record_for_type,
                                      GetEntryCount(*record_for_type),
                                      GetMaxEntryCount(*record_for_type));
  }

  RemoveUIEntries({uuid});
}

void SavedDeskPresenter::OnGetTemplateForDeskLaunch(
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

  const auto saved_desk_name = entry->template_name();
  const auto saved_desk_type = entry->type();
  const bool activate_desk = saved_desk_type == DeskTemplateType::kTemplate;
  DesksController::Get()->CreateNewDeskForTemplate(
      activate_desk,
      base::BindOnce(&SavedDeskPresenter::OnNewDeskCreatedForTemplate,
                     weak_ptr_factory_.GetWeakPtr(), std::move(entry),
                     time_launch_started, delay, root_window),
      saved_desk_name);

  if (on_update_ui_closure_for_testing)
    std::move(on_update_ui_closure_for_testing).Run();

  RecordLaunchSavedDeskHistogram(saved_desk_type);
}

void SavedDeskPresenter::OnNewDeskCreatedForTemplate(
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

  if (delete_template_on_launch) {
    // Passing nullopt as type since this indicates that we don't want to record
    // the `delete` metric for this operation.
    DeleteEntry(template_uuid, /*record_for_type=*/absl::nullopt);
  }
}

void SavedDeskPresenter::OnAddOrUpdateEntry(
    bool was_update,
    aura::Window* const root_window,
    std::unique_ptr<DeskTemplate> desk_template,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status) {
  RecordAddOrUpdateTemplateStatusHistogram(status);

  if (status ==
      desks_storage::DeskModel::AddOrUpdateEntryStatus::kEntryTooLarge) {
    // Show a toast if the template we tried to save was too large to be
    // transported through Chrome Sync.
    int toast_text_id = desk_template->type() == DeskTemplateType::kTemplate
                            ? IDS_ASH_DESKS_TEMPLATES_TEMPLATE_TOO_LARGE_TOAST
                            : IDS_ASH_DESKS_TEMPLATES_DESK_TOO_LARGE_TOAST;
    ToastData toast_data(kTemplateTooLargeToastName,
                         ToastCatalogName::kDeskTemplateTooLarge,
                         l10n_util::GetStringUTF16(toast_text_id));
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

  if (auto* library_view = overview_grid->GetSavedDeskLibraryView()) {
    // TODO(dandersson): Rework literally all of this. This path is only taken
    // if the library has been visible in a session and we then save a desk. We
    // should not need this special case.
    AddOrUpdateUIEntries({desk_template.get()});

    if (!was_update) {
      // Shows the grid if it was hidden. This will not call `GetAllEntries`.
      overview_session_->ShowDesksTemplatesGrids(is_zero_state, base::GUID(),
                                                 root_window);
      if (SavedDeskItemView* item_view =
              library_view->GetItemForUUID(desk_template->uuid())) {
        item_view->MaybeRemoveNameNumber();
        item_view->name_view()->RequestFocus();
      }
    }

    if (on_update_ui_closure_for_testing_)
      std::move(on_update_ui_closure_for_testing_).Run();
  } else {
    // This will update the templates button and save as desks button too. This
    // will call `GetAllEntries`.
    overview_session_->ShowDesksTemplatesGrids(
        is_zero_state, desk_template->uuid(), root_window);
  }

  if (!was_update) {
    const auto saved_desk_type = desk_template->type();
    if (saved_desk_type == DeskTemplateType::kSaveAndRecall) {
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
                                   DeskCloseType::kCloseAllWindows);
    }

    RecordNewSavedDeskHistogram(saved_desk_type);
    RecordUserSavedDeskCountHistogram(saved_desk_type,
                                      GetEntryCount(saved_desk_type),
                                      GetMaxEntryCount(saved_desk_type));
  }

  // Note we do not run `on_update_ui_closure_for_testing` here as we want to
  // wait for the `GetAllEntries` fired in `ShowDesksTemplatesGrids`.
}

void SavedDeskPresenter::AddOrUpdateUIEntries(
    const std::vector<const DeskTemplate*>& new_entries) {
  if (new_entries.empty())
    return;

  // This updates `should_show_templates_ui_`.
  UpdateDesksTemplatesUI();

  for (auto& overview_grid : overview_session_->grid_list()) {
    if (auto* library_view = overview_grid->GetSavedDeskLibraryView()) {
      library_view->AddOrUpdateTemplates(
          new_entries, /*initializing_grid_view=*/false,
          /*last_saved_template_uuid=*/base::GUID());
    }
  }

  if (on_update_ui_closure_for_testing_)
    std::move(on_update_ui_closure_for_testing_).Run();
}

void SavedDeskPresenter::RemoveUIEntries(
    const std::vector<std::string>& uuids) {
  if (uuids.empty())
    return;

  // This updates `should_show_templates_ui_`.
  UpdateDesksTemplatesUI();

  for (auto& overview_grid : overview_session_->grid_list()) {
    // Remove the entries from `SavedDeskLibraryView`.
    if (auto* library_view = overview_grid->GetSavedDeskLibraryView())
      library_view->DeleteTemplates(uuids);
  }

  if (on_update_ui_closure_for_testing_)
    std::move(on_update_ui_closure_for_testing_).Run();
}

std::u16string SavedDeskPresenter::AppendDuplicateNumberToDuplicateName(
    const std::u16string& duplicate_name_u16) {
  std::string duplicate_name = base::UTF16ToUTF8(duplicate_name_u16);
  int found_duplicate_number;

  if (RE2::PartialMatch(duplicate_name, kDuplicateNumberRegex,
                        &found_duplicate_number)) {
    RE2::Replace(
        &duplicate_name, kDuplicateNumberRegex,
        base::StringPrintf(kDuplicateNumberFormat, found_duplicate_number + 1));
  } else {
    duplicate_name.append(kInitialDuplicateNumberValue);
  }

  return base::UTF8ToUTF16(duplicate_name);
}

}  // namespace ash
