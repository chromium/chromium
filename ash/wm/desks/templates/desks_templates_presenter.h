// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_PRESENTER_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_PRESENTER_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/desks_storage/core/desk_model_observer.h"

namespace ash {

class DeskTemplate;
class OverviewSession;

// DesksTemplatesPresenter is the presenter for the desks templates UI. It
// handles all calls to the model, and lets the UI know what to show or update.
// OverviewSession will create and own an instance of this object. It will be
// created after the desks bar is visible and destroyed once we receive an
// overview shutdown signal, to prevent calls to the model during shutdown.
class ASH_EXPORT DesksTemplatesPresenter : desks_storage::DeskModelObserver {
 public:
  explicit DesksTemplatesPresenter(OverviewSession* overview_session);
  DesksTemplatesPresenter(const DesksTemplatesPresenter&) = delete;
  DesksTemplatesPresenter& operator=(const DesksTemplatesPresenter&) = delete;
  ~DesksTemplatesPresenter() override;

  // desks_storage::DeskModelObserver:
  // TODO(sammiequon): Implement these once the model starts sending these
  // messages.
  void DeskModelLoaded() override {}
  void EntriesAddedOrUpdatedRemotely(
      const std::vector<const ash::DeskTemplate*>& new_entries) override {}
  void EntriesRemovedRemotely(const std::vector<std::string>& uuids) override {}
  void EntriesAddedOrUpdatedLocally(
      const std::vector<const ash::DeskTemplate*>& new_entries) override {}
  void EntriesRemovedLocally(const std::vector<std::string>& uuids) override {}

 private:
  friend class DesksTemplatesPresenterTestApi;

  // Callback ran after querying the model for a list of entries.
  void OnGetAllEntries(desks_storage::DeskModel::GetAllEntriesStatus status,
                       std::vector<DeskTemplate*> entries);

  // Pointer to the session which owns `this`.
  OverviewSession* const overview_session_;

  // Test closure that runs after the UI has been updated async after a call to
  // the model.
  base::OnceClosure on_update_ui_closure_for_testing_;

  base::ScopedObservation<desks_storage::DeskModel,
                          desks_storage::DeskModelObserver>
      desk_model_observation_{this};

  base::WeakPtrFactory<DesksTemplatesPresenter> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_PRESENTER_H_
