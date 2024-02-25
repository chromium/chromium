// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_SAVED_DESK_PRESENTER_H_
#define ASH_WM_DESKS_TEMPLATES_SAVED_DESK_PRESENTER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_model_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class Desk;
class DeskTemplate;
class OverviewSession;
enum class DeskTemplateType;

// SavedDeskPresenter is the presenter for the saved desk UI. It handles all
// calls to the model, and lets the UI know what to show or update.
// OverviewSession will create and own an instance of this object. It will be
// created after the desks bar is visible and destroyed once we receive an
// overview shutdown signal, to prevent calls to the model during shutdown.
class ASH_EXPORT SavedDeskPresenter : desks_storage::DeskModelObserver {
 public:
  explicit SavedDeskPresenter(OverviewSession* overview_session);
  SavedDeskPresenter(const SavedDeskPresenter&) = delete;
  SavedDeskPresenter& operator=(const SavedDeskPresenter&) = delete;
  ~SavedDeskPresenter() override;

  // Retrieve the current and max count for a given saved desk type. Note that
  // these are snapshots of the model state, which may not match the current UI
  // state.
  size_t GetEntryCount(DeskTemplateType type) const;
  size_t GetMaxEntryCount(DeskTemplateType type) const;

  // Finds an entry of type `type` with the name `name` that is not the entry
  // identified by `uuid`. Returns nullptr if not found.
  ash::DeskTemplate* FindOtherEntryWithName(const std::u16string& name,
                                            ash::DeskTemplateType type,
                                            const base::Uuid& uuid) const;

  // Update UI for saved desk library. More specifically, it updates the
  // visibility of the library button, save desk button, and the saved desk
  // grid. The grid contents are not updated.
  void UpdateUIForSavedDeskLibrary();

  // Calls the DeskModel to get all the saved desk entries, with a callback to
  // `OnGetAllEntries`. `saved_desk_name` is used for the name overwrite nudge
  // for duplicate desk names.
  void GetAllEntries(const base::Uuid& item_to_focus,
                     const std::u16string& saved_desk_name,
                     aura::Window* const root_window);

  // Calls the DeskModel to delete the saved desk with the provided `uuid`. Will
  // record histogram if `record_for_type` is specified.
  void DeleteEntry(const base::Uuid& uuid,
                   std::optional<DeskTemplateType> record_for_type);

  // Launches `saved_desk` into a new desk.
  void LaunchSavedDesk(std::unique_ptr<DeskTemplate> saved_desk,
                       aura::Window* root_window);

  // Calls the DeskModel to capture the active desk as a `template_type`, with a
  // callback to `OnAddOrUpdateEntry`. If there are unsupported apps on the
  // active desk, a dialog will open up and we may or may not save the desk
  // asynchronously based on the user's decision.
  void MaybeSaveActiveDeskAsSavedDesk(DeskTemplateType template_type,
                                      aura::Window* root_window_to_show);

  // Saves or updates the `saved_desk` to the model.
  void SaveOrUpdateSavedDesk(bool is_update,
                             aura::Window* const root_window,
                             std::unique_ptr<DeskTemplate> saved_desk);

  // desks_storage::DeskModelObserver:
  void DeskModelLoaded() override {}
  void OnDeskModelDestroying() override;
  void EntriesAddedOrUpdatedRemotely(
      const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>&
          new_entries) override;
  void EntriesRemovedRemotely(const std::vector<base::Uuid>& uuids) override;

 private:
  friend class SavedDeskPresenterTestApi;

  // Launch `saved_desk` into `new_desk`.
  void LaunchSavedDeskIntoNewDesk(std::unique_ptr<DeskTemplate> saved_desk,
                                  aura::Window* root_window,
                                  const Desk* new_desk);

  // Callback after deleting an entry. Will then call `RemoveUIEntries` to
  // update the UI by removing the deleted saved desk.
  void OnDeleteEntry(const base::Uuid& uuid,
                     std::optional<DeskTemplateType> record_for_type,
                     desks_storage::DeskModel::DeleteEntryStatus status);

  // Callback after adding or updating an entry. Will then call
  // `AddOrUpdateUIEntries` to update the UI by adding or updating the saved
  // desk.
  void OnAddOrUpdateEntry(
      bool was_update,
      aura::Window* const root_window,
      const std::u16string& saved_desk_name,
      desks_storage::DeskModel::AddOrUpdateEntryStatus status,
      std::unique_ptr<DeskTemplate> saved_desk);

  // Helper functions for updating the UI.
  void AddOrUpdateUIEntries(
      const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>&
          new_entries);
  void RemoveUIEntries(const std::vector<base::Uuid>& uuids);

  // Returns a copy of a duplicated name to be stored.  This function works by
  // taking the name to be duplicated and adding a "(1)" to it. If the name
  // already has "(1)" then the number inside of the parenthesis will be
  // incremented.
  std::u16string AppendDuplicateNumberToDuplicateName(
      const std::u16string& duplicate_name_u16);

  // Sets `closure` to be invoked when Save & Recall triggers a modal dialog.
  static void SetModalDialogCallbackForTesting(base::OnceClosure closure);
  // Immediately fires the window watcher auto transition timer.
  static void FireWindowWatcherTimerForTesting();

  // Pointer to the session which owns `this`.
  const raw_ptr<OverviewSession> overview_session_;

  base::ScopedObservation<desks_storage::DeskModel,
                          desks_storage::DeskModelObserver>
      desk_model_observation_{this};

  // Test closure that runs after the UI has been updated async after a call to
  // the model.
  base::OnceClosure on_update_ui_closure_for_testing_;

  base::WeakPtrFactory<SavedDeskPresenter> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_SAVED_DESK_PRESENTER_H_
