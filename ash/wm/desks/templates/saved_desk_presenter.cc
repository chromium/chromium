// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_presenter.h"

#include <vector>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"

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
  auto* desk_model = Shell::Get()->saved_desk_delegate()->GetDeskModel();
  DCHECK(desk_model);
  return desk_model;
}

// Shows the saved desk library view and remove the active desk if `remove_desk`
// is true.
void ShowLibrary(aura::Window* const root_window,
                 const std::u16string& saved_desk_name,
                 const base::Uuid& uuid,
                 bool remove_desk) {
  OverviewController* overview_controller = Shell::Get()->overview_controller();

  OverviewSession* overview_session = overview_controller->overview_session();
  if (!overview_session) {
    if (!overview_controller->StartOverview(
            OverviewStartAction::kOverviewButton,
            OverviewEnterExitType::kImmediateEnterWithoutFocus)) {
      // If for whatever reason we didn't enter overview mode, bail.
      return;
    }

    overview_session = overview_controller->overview_session();
    DCHECK(overview_session);
  }

  // Show the library, this should focus the newly saved item.
  overview_session->ShowSavedDeskLibrary(uuid, saved_desk_name, root_window);

  // Remove the current desk, this will be done without animation.
  if (remove_desk) {
    auto* desks_controller = DesksController::Get();
    const Desk* desk_to_remove = desks_controller->active_desk();
    if (desks_controller->HasDesk(desk_to_remove)) {
      if (!desks_controller->CanRemoveDesks()) {
        desks_controller->NewDesk(
            DesksCreationRemovalSource::kEnsureDefaultDesk);
      }

      desks_controller->RemoveDesk(desk_to_remove,
                                   DesksCreationRemovalSource::kSaveAndRecall,
                                   DeskCloseType::kCloseAllWindows);
    }
  }
}

// The WindowCloseObserver helper is used in the Save & Recall save flow. When
// the user saves a desk, we will try to close all windows on that desk, and
// then finally remove the desk. This is an asynchronous task that may trigger
// confirmation dialogs to pop up.
//
// The flow is as follows:
//  1. The user initiates Save & Recall by clicking the save button.
//  2. Windows are enumerated and saved to a saved desk definition.
//  3. We start a `WindowCloseObserver` and call `Close()` on all windows.
// The observer then deals with three different cases:
//  a. All windows close automatically.
//  b. A confirmation dialog appears and the user decides to close the window.
//  c. A confirmation dialog appears and the user decides to keep the window.
//
// For cases a & b, we remove the desk and transition the user into the saved
// desk library. For case c, we leave the desk alone. Also note that for cases b
// & c, the confirmation dialog will take the user out of overview mode.

class WindowCloseObserver;

// This is a global raw pointer since the lifetime of the watcher cannot be tied
// to the presenter. The presenter's lifetime is indirectly tied to the overview
// session, and the watcher must survive going out of overview mode.
WindowCloseObserver* g_window_close_observer = nullptr;

class WindowCloseObserver : public aura::WindowObserver {
 public:
  WindowCloseObserver(
      aura::Window* root_window,
      const base::Uuid& saved_desk_uuid,
      const std::u16string& saved_desk_name,
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows)
      : root_window_(root_window),
        saved_desk_uuid_(saved_desk_uuid),
        saved_desk_name_(saved_desk_name) {
    DCHECK(g_window_close_observer == nullptr);

    auto* desks_controller = DesksController::Get();
    desk_to_remove_ = desks_controller->active_desk();

    desk_container_ = desks_controller->GetDeskContainer(
        root_window, desks_controller->GetDeskIndex(desk_to_remove_));
    DCHECK(desk_container_);
    window_observer_.AddObservation(desk_container_.get());

    // If any of the observed windows belong to an ARC app, we need to handle
    // things a bit differently.
    has_arc_app_ = base::ranges::any_of(windows, &IsArcWindow);

    // Observe the windows that we are going to close. Since `windows` here are
    // all non-all-desks windows or non-transient windows, we can observe all
    // of them.
    for (aura::Window* window : windows) {
      window_observer_.AddObservation(window);
    }

    OverviewController* overview_controller =
        Shell::Get()->overview_controller();
    if (OverviewSession* overview_session =
            overview_controller->overview_session()) {
      overview_session->set_allow_empty_desk_without_exiting(true);
    }

    system_modal_container_ = Shell::Get()->GetContainer(
        root_window, kShellWindowId_SystemModalContainer);
    window_observer_.AddObservation(system_modal_container_.get());
  }

  ~WindowCloseObserver() override {
    OverviewController* overview_controller =
        Shell::Get()->overview_controller();
    if (OverviewSession* overview_session =
            overview_controller->overview_session()) {
      overview_session->set_allow_empty_desk_without_exiting(false);
    }
    g_window_close_observer = nullptr;
  }

  void SetModalDialogCallbackForTesting(base::OnceClosure closure) {
    modal_dialog_closure_for_testing_ = std::move(closure);
  }

  void FireWindowWatcherTimerForTesting() {
    DCHECK(auto_transition_timer_.IsRunning());
    auto_transition_timer_.FireNow();
  }

 private:
  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override {
    if (new_window->parent() == desk_container_) {
      if (waiting_for_resurrects_ && IsArcWindow(new_window))
        window_observer_.AddObservation(new_window);
    }

    if (new_window->parent() != system_modal_container_)
      return;

    if (modal_dialog_closure_for_testing_)
      std::move(modal_dialog_closure_for_testing_).Run();

    modal_dialog_showed_ = true;
  }

  void OnWindowRemoved(aura::Window* removed_window) override {
    if (!modal_dialog_showed_ || !system_modal_container_->children().empty())
      return;

    // The last modal dialog has been dismissed. At this point we don't know if
    // the user has dismissed the windows or decided to keep them. We will now
    // allow a short time for windows to close. If, after this time, windows
    // have not been closed, we'll proceed to the library but leave the desk.
    //
    // If all windows *do* close before the timer hits, then this will be picked
    // up by `OnWindowDestroyed` and we will show the saved desk library.
    if (!auto_transition_timer_.IsRunning()) {
      auto_transition_timer_.Start(
          FROM_HERE, base::Seconds(1), this,
          &WindowCloseObserver::ShowLibraryWithoutRemovingDesk);
    }
  }

  void OnWindowDestroyed(aura::Window* window) override {
    window_observer_.RemoveObservation(window);

    // In the unexpected case that the system modal container or current desk
    // container is destroyed, we will bail.
    if (window == system_modal_container_ || window == desk_container_) {
      Terminate();
      return;
    }

    // The observer is used for the windows on the desk that we are closing, as
    // well as the system modal container. When there's only two windows left
    // (the system modal container and the desk container) in the observing set,
    // we're done.
    if (window_observer_.GetSourcesCount() == 2) {
      if (!has_arc_app_) {
        // We're ready to transition into the saved desk library and highlight
        // the item. After this has been done, we'll remove ourselves.
        ShowLibraryAndRemoveDesk();
        return;
      }

      // If we had an ARC app, then we're going to wait for a small amount of
      // time in case ARC decides to spawn a new window. If, during this time, a
      // new ARC window appears, then we will detect this, and put it into the
      // observing set and wait for it to disappear.
      // See crbug.com/1350297.
      has_arc_app_ = false;
      waiting_for_resurrects_ = true;
      auto_transition_timer_.Start(
          FROM_HERE, base::Milliseconds(500), this,
          &WindowCloseObserver::ShowLibraryAndRemoveDesk);
    }
  }

  void ShowLibraryWithoutRemovingDesk() {
    ShowLibrary(/*remove_desk=*/false);
    Terminate();
  }

  void ShowLibraryAndRemoveDesk() {
    ShowLibrary(/*remove_desk=*/true);
    Terminate();
  }

  void ShowLibrary(bool remove_desk) {
    window_observer_.RemoveAllObservations();
    ::ash::ShowLibrary(root_window_, saved_desk_name_, saved_desk_uuid_,
                       remove_desk);
  }

  void Terminate() { delete this; }

  raw_ptr<aura::Window> root_window_;

  raw_ptr<aura::Window> system_modal_container_ = nullptr;

  // Current desk container. Will be used when monitoring for new windows.
  raw_ptr<aura::Window> desk_container_ = nullptr;

  // Tracks whether a modal "confirm close" dialog has been showed.
  bool modal_dialog_showed_ = false;

  // True if at least one monitored window belongs to an ARC app.
  bool has_arc_app_ = false;
  // True if we're in the phase of waiting for an ARC window to resurrect.
  bool waiting_for_resurrects_ = false;

  // Used to automatically transition the user to the library after a modal
  // dialog has been dismissed.
  base::OneShotTimer auto_transition_timer_{
      base::DefaultTickClock::GetInstance()};

  // The desk that the user has saved and that we will remove once windows have
  // been removed.
  raw_ptr<const Desk, DanglingUntriaged> desk_to_remove_ = nullptr;

  // UUID and name of the saved desk.
  const base::Uuid saved_desk_uuid_;
  const std::u16string saved_desk_name_;

  // Used by unit tests to wait for modal dialogs.
  base::OnceClosure modal_dialog_closure_for_testing_;

  // Used to watch windows on the desk to remove, as well as the system modal
  // container.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observer_{this};
};

}  // namespace

SavedDeskPresenter::SavedDeskPresenter(OverviewSession* overview_session)
    : overview_session_(overview_session) {
  DCHECK(overview_session_);

  auto* desk_model = GetDeskModel();
  desk_model_observation_.Observe(desk_model);
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
    const base::Uuid& uuid) const {
  return GetDeskModel()->FindOtherEntryWithName(name, type, uuid);
}

void SavedDeskPresenter::UpdateUIForSavedDeskLibrary() {
  // This function:
  //  1. Figures out whether the library button should be shown in the desk bar.
  //  2. Hides the library if necessary.
  //  3. Triggers save desk buttons in the overview overgrid to update.
  //
  // The library and the library button is always hidden if we enter tablet
  // mode. If not in tablet mode, the library button is visible if there are
  // saved desks in the model, *or* we are already showing the library.
  const bool in_tablet_mode = display::Screen::GetScreen()->InTabletMode();

  for (auto& overview_grid : overview_session_->grid_list()) {
    const bool is_showing_library = overview_grid->IsShowingSavedDeskLibrary();

    if (in_tablet_mode && is_showing_library) {
      // This happens when entering tablet mode while the library is visible.
      overview_grid->HideSavedDeskLibrary(/*exit_overview=*/false);
    }

    if (OverviewDeskBarView* desks_bar_view = overview_grid->desks_bar_view()) {
      // Library UI needs an update. If it's currently in the library page, keep
      // the UI visible.
      desks_bar_view->set_library_ui_visibility(
          (!in_tablet_mode && is_showing_library)
              ? DeskBarViewBase::LibraryUiVisibility::kVisible
              : DeskBarViewBase::LibraryUiVisibility::kToBeChecked);
      desks_bar_view->UpdateLibraryButtonVisibility();
      desks_bar_view->UpdateButtonsForSavedDeskGrid();
      overview_grid->UpdateSaveDeskButtons();
    }
  }
}

void SavedDeskPresenter::DeleteEntry(
    const base::Uuid& uuid,
    std::optional<DeskTemplateType> record_for_type) {
  weak_ptr_factory_.InvalidateWeakPtrs();
  GetDeskModel()->DeleteEntry(
      uuid,
      base::BindOnce(&SavedDeskPresenter::OnDeleteEntry,
                     weak_ptr_factory_.GetWeakPtr(), uuid, record_for_type));
}

void SavedDeskPresenter::LaunchSavedDesk(
    std::unique_ptr<DeskTemplate> saved_desk,
    aura::Window* root_window) {
  DesksController* desks_controller = DesksController::Get();

  // If we are at the max desk limit (currently is 8), a new desk
  // cannot be created, and a toast will be displayed to the user.
  if (!desks_controller->CanCreateDesks()) {
    ToastData toast_data = {
        /*id=*/kMaximumDeskLaunchTemplateToastName,
        ToastCatalogName::kMaximumDeskLaunchTemplate,
        /*text=*/
        l10n_util::GetStringFUTF16(
            IDS_ASH_DESKS_TEMPLATES_REACH_MAXIMUM_DESK_TOAST,
            base::FormatNumber(desks_util::GetMaxNumberOfDesks()))};
    ToastManager::Get()->Show(std::move(toast_data));
    return;
  }

  // Copy fields we need from `desk_template` since we're about to move it.
  const auto saved_desk_type = saved_desk->type();
  Desk* new_desk = desks_controller->CreateNewDeskForSavedDesk(
      saved_desk_type, saved_desk->template_name());

  // Set the lacros profile ID for the newly created desk. This is effectively a
  // no-op if `lacros_profile_id` returns zero.
  new_desk->SetLacrosProfileId(saved_desk->lacros_profile_id(),
                               /*source=*/std::nullopt);

  LaunchSavedDeskIntoNewDesk(std::move(saved_desk), root_window, new_desk);

  // Note: `LaunchSavedDeskIntoNewDesk` *may* cause overview mode to exit. This
  // means that the saved desk presenter may have been destroyed at this point.
  // Do not add any code below this point that depend on `this`.

  RecordLaunchSavedDeskHistogram(saved_desk_type);
}

void SavedDeskPresenter::MaybeSaveActiveDeskAsSavedDesk(
    DeskTemplateType template_type,
    aura::Window* root_window_to_show) {
  DesksController::Get()->CaptureActiveDeskAsSavedDesk(
      base::BindOnce(&SavedDeskPresenter::SaveOrUpdateSavedDesk,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*is_update=*/false, root_window_to_show),
      template_type, root_window_to_show);
}

void SavedDeskPresenter::SaveOrUpdateSavedDesk(
    bool is_update,
    aura::Window* const root_window,
    std::unique_ptr<DeskTemplate> saved_desk) {
  if (!saved_desk) {
    return;
  }

  if (is_update)
    saved_desk->set_updated_time(base::Time::Now());
  else
    RecordWindowAndTabCountHistogram(*saved_desk);

  const auto saved_desk_name = saved_desk->template_name();

  // While we still find duplicate names iterate the duplicate number. i.e.
  // if there are 4 duplicates of some template name then this iterates until
  // the current template will be named 5.
  while (GetDeskModel()->FindOtherEntryWithName(
      saved_desk->template_name(), saved_desk->type(), saved_desk->uuid())) {
    saved_desk->set_template_name(
        AppendDuplicateNumberToDuplicateName(saved_desk->template_name()));
  }

  // Save or update `desk_template` as an entry in DeskModel.
  GetDeskModel()->AddOrUpdateEntry(
      std::move(saved_desk),
      base::BindOnce(&SavedDeskPresenter::OnAddOrUpdateEntry,
                     weak_ptr_factory_.GetWeakPtr(), is_update, root_window,
                     saved_desk_name));
}

void SavedDeskPresenter::OnDeskModelDestroying() {
  desk_model_observation_.Reset();
}

void SavedDeskPresenter::EntriesAddedOrUpdatedRemotely(
    const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>&
        new_entries) {
  AddOrUpdateUIEntries(new_entries);
}

void SavedDeskPresenter::EntriesRemovedRemotely(
    const std::vector<base::Uuid>& uuids) {
  RemoveUIEntries(uuids);
}

void SavedDeskPresenter::GetAllEntries(const base::Uuid& item_to_focus,
                                       const std::u16string& saved_desk_name,
                                       aura::Window* const root_window) {
  auto result = GetDeskModel()->GetAllEntries();

  if (result.status != desks_storage::DeskModel::GetAllEntriesStatus::kOk)
    return;

  // This updates UI for saved desk library.
  UpdateUIForSavedDeskLibrary();

  for (auto& overview_grid : overview_session_->grid_list()) {
    // Populate `SavedDeskLibraryView` with the saved desk entries.
    if (SavedDeskLibraryView* library_view =
            overview_grid->GetSavedDeskLibraryView()) {
      library_view->AddOrUpdateEntries(result.entries, item_to_focus,
                                       /*animate=*/false);

      SavedDeskItemView* item_view =
          library_view->GetItemForUUID(item_to_focus);
      if (!item_view)
        continue;

      if (FindOtherEntryWithName(saved_desk_name,
                                 item_view->saved_desk().type(),
                                 item_view->uuid())) {
        // When we are here, the item view will contain "{saved_desk_name} (n)",
        // so what we are doing here is just setting the contained name view to
        // exactly "{saved_desk_name}" before activating the entry.
        item_view->SetDisplayName(saved_desk_name);
      }
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
    const base::Uuid& uuid,
    std::optional<DeskTemplateType> record_for_type,
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

void SavedDeskPresenter::LaunchSavedDeskIntoNewDesk(
    std::unique_ptr<DeskTemplate> saved_desk,
    aura::Window* root_window,
    const Desk* new_desk) {
  DCHECK(new_desk);

  // For Save & Recall, the underlying desk definition is deleted on launch. We
  // store the template ID here since we're about to move the desk template.
  const auto saved_desk_type = saved_desk->type();
  const auto saved_desk_creation_time = saved_desk->created_time();
  const base::Uuid uuid = saved_desk->uuid();

  auto* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession()) {
    if (saved_desk_type == DeskTemplateType::kSaveAndRecall) {
      auto* overview_session = overview_controller->overview_session();
      OverviewGrid* overview_grid =
          overview_session->GetGridWithRootWindow(root_window);

      const OverviewDeskBarView* desks_bar_view =
          overview_grid->desks_bar_view();
      DCHECK(desks_bar_view);
      DeskMiniView* mini_view = desks_bar_view->FindMiniViewForDesk(new_desk);
      DCHECK(mini_view);

      SavedDeskLibraryView* library = overview_grid->GetSavedDeskLibraryView();
      library->AnimateDeskLaunch(uuid, mini_view);
    } else if (saved_desk_type == DeskTemplateType::kTemplate) {
      // For a desk template launch, we will stay in overview mode and hide the
      // library. The overview grid will show and get populated with launched
      // apps.
      for (auto& overview_grid : overview_session_->grid_list()) {
        overview_grid->HideSavedDeskLibrary(/*exit_overview=*/false);
      }
    }
  }

  // Copy the uuid of the newly created desk to the saved desk. This ensures
  // that apps appear on the right desk even if the user switches to another.
  saved_desk->SetDeskUuid(new_desk->uuid());

  Shell::Get()->saved_desk_delegate()->LaunchAppsFromSavedDesk(
      std::move(saved_desk));

  if (!overview_controller->InOverviewSession()) {
    // Note: it is the intention that we don't leave overview mode when
    // launching a saved desk. However, if something goes wrong when launching a
    // window and the correct properties aren't applied, then we may find that
    // we have left overview mode.
    //
    // The `SavedDeskPresenter` is indirectly owned by the overview session, so
    // if we get here, `this` is gone and we must not access any member
    // functions or variables.

    // Bare minimum code to remove save & recall desks.
    if (saved_desk_type == DeskTemplateType::kSaveAndRecall) {
      auto* desk_model = Shell::Get()->saved_desk_delegate()->GetDeskModel();
      desk_model->DeleteEntry(uuid, base::DoNothing());
    }

    return;
  }

  overview_session_->GetGridWithRootWindow(root_window)
      ->desks_bar_view()
      ->NudgeDeskName(DesksController::Get()->GetDeskIndex(new_desk));

  if (saved_desk_type == DeskTemplateType::kSaveAndRecall) {
    // Passing nullopt as type since this indicates that we don't want to record
    // the `delete` metric for this operation.
    DeleteEntry(uuid, /*record_for_type=*/std::nullopt);
    RecordTimeBetweenSaveAndRecall(base::Time::Now() -
                                   saved_desk_creation_time);
  }
}

void SavedDeskPresenter::OnAddOrUpdateEntry(
    bool was_update,
    aura::Window* const root_window,
    const std::u16string& saved_desk_name,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status,
    std::unique_ptr<DeskTemplate> saved_desk) {
  RecordAddOrUpdateTemplateStatusHistogram(status);

  if (status ==
      desks_storage::DeskModel::AddOrUpdateEntryStatus::kEntryTooLarge) {
    // Show a toast if the template we tried to save was too large to be
    // transported through Chrome Sync.
    int toast_text_id = saved_desk->type() == DeskTemplateType::kTemplate
                            ? IDS_ASH_DESKS_TEMPLATES_TEMPLATE_TOO_LARGE_TOAST
                            : IDS_ASH_DESKS_TEMPLATES_DESK_TOO_LARGE_TOAST;
    ToastData toast_data(kTemplateTooLargeToastName,
                         ToastCatalogName::kDeskTemplateTooLarge,
                         l10n_util::GetStringUTF16(toast_text_id));
    ToastManager::Get()->Show(std::move(toast_data));
    return;
  }

  if (status != desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk)
    return;

  // If the saved desks grid has already been shown before, update the entry.
  OverviewGrid* overview_grid =
      overview_session_->GetGridWithRootWindow(root_window);
  DCHECK(overview_grid);

  if (auto* library_view = overview_grid->GetSavedDeskLibraryView()) {
    // TODO(dandersson): Rework literally all of this. This path is only taken
    // if the library has been visible in a session and we then save a desk. We
    // should not need this special case.
    AddOrUpdateUIEntries({saved_desk.get()});

    if (!was_update) {
      // Shows the library if it was hidden. This will not call `GetAllEntries`.
      overview_session_->ShowSavedDeskLibrary(base::Uuid(),
                                              /*saved_desk_name=*/u"",
                                              root_window);
      if (SavedDeskItemView* item_view =
              library_view->GetItemForUUID(saved_desk->uuid())) {
        if (FindOtherEntryWithName(saved_desk_name, saved_desk->type(),
                                   saved_desk->uuid())) {
          item_view->SetDisplayName(saved_desk_name);
        }
        item_view->name_view()->RequestFocus();
      }
    }

    if (on_update_ui_closure_for_testing_)
      std::move(on_update_ui_closure_for_testing_).Run();
  } else if (saved_desk->type() != DeskTemplateType::kSaveAndRecall) {
    // This will update the library button and save desk button too. This will
    // call `GetAllEntries`.
    overview_session_->ShowSavedDeskLibrary(saved_desk->uuid(), saved_desk_name,
                                            root_window);
  }

  if (!was_update) {
    const auto saved_desk_type = saved_desk->type();
    RecordNewSavedDeskHistogram(saved_desk_type);
    RecordUserSavedDeskCountHistogram(saved_desk_type,
                                      GetEntryCount(saved_desk_type),
                                      GetMaxEntryCount(saved_desk_type));

    if (saved_desk_type == DeskTemplateType::kSaveAndRecall) {
      std::vector<raw_ptr<aura::Window, VectorExperimental>> windows =
          Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);

      // Get rid of transient windows and all-desks windows.
      std::erase_if(windows, [](aura::Window* window) {
        return wm::GetTransientParent(window) != nullptr ||
               desks_util::IsWindowVisibleOnAllWorkspaces(window);
      });

      if (g_window_close_observer)
        delete g_window_close_observer;

      if (windows.empty()) {
        // Show library and remove desk when the windows are all all-desks.
        ShowLibrary(root_window, saved_desk_name, saved_desk->uuid(),
                    /*remove_desk=*/true);
      } else {
        // Only create the observer when we have closing windows.
        g_window_close_observer = new WindowCloseObserver(
            root_window, saved_desk->uuid(), saved_desk_name, windows);

        // Go through windows and attempt to close them.
        for (aura::Window* window : windows) {
          if (views::Widget* widget =
                  views::Widget::GetWidgetForNativeView(window)) {
            widget->Close();
          }
        }
      }

      if (on_update_ui_closure_for_testing_)
        std::move(on_update_ui_closure_for_testing_).Run();
    }
  }

  // Note we do not run `on_update_ui_closure_for_testing` here as we want to
  // wait for the `GetAllEntries()` fired in `ShowSavedDeskLibrary()`.
}

void SavedDeskPresenter::AddOrUpdateUIEntries(
    const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>&
        new_entries) {
  if (new_entries.empty())
    return;

  // This updates UI for saved desk library.
  UpdateUIForSavedDeskLibrary();

  for (auto& overview_grid : overview_session_->grid_list()) {
    if (auto* library_view = overview_grid->GetSavedDeskLibraryView()) {
      library_view->AddOrUpdateEntries(new_entries, /*order_first_uuid=*/{},
                                       /*animate=*/true);
    }
  }

  if (on_update_ui_closure_for_testing_)
    std::move(on_update_ui_closure_for_testing_).Run();
}

void SavedDeskPresenter::RemoveUIEntries(const std::vector<base::Uuid>& uuids) {
  if (uuids.empty())
    return;

  // This updates UI for saved desk library.
  UpdateUIForSavedDeskLibrary();

  for (auto& overview_grid : overview_session_->grid_list()) {
    // Remove the entries from `SavedDeskLibraryView`.
    if (auto* library_view = overview_grid->GetSavedDeskLibraryView())
      library_view->DeleteEntries(uuids, /*delete_animation=*/true);
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

// static
void SavedDeskPresenter::SetModalDialogCallbackForTesting(
    base::OnceClosure closure) {
  DCHECK(g_window_close_observer);
  g_window_close_observer->SetModalDialogCallbackForTesting(  // IN-TEST
      std::move(closure));
}

// static
void SavedDeskPresenter::FireWindowWatcherTimerForTesting() {
  DCHECK(g_window_close_observer);
  g_window_close_observer->FireWindowWatcherTimerForTesting();  // IN-TEST
}

}  // namespace ash
