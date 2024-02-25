// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_WORKSPACE_DESK_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_WORKSPACE_DESK_HELPER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/desks_storage/core/desk_model_observer.h"

namespace desks_storage {
class DeskSyncService;
}  // namespace desks_storage

namespace workspace_desk_helper {

// Class that allows waiting until a particular desk |uuid| is exposed by the
// DeskModel in |service|.
class DeskUuidChecker : public StatusChangeChecker,
                        public desks_storage::DeskModelObserver {
 public:
  // The caller must ensure that |service| is not null and will outlive this
  // object.
  DeskUuidChecker(desks_storage::DeskSyncService* service,
                  const base::Uuid& uuid);
  DeskUuidChecker(const DeskUuidChecker&) = delete;
  DeskUuidChecker& operator=(const DeskUuidChecker&) = delete;
  ~DeskUuidChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // DeskModelObserver implementation.
  void DeskModelLoaded() override;
  void EntriesAddedOrUpdatedRemotely(
      const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
          new_entries) override;
  void EntriesRemovedRemotely(const std::vector<base::Uuid>& uuids) override;

 private:
  const base::Uuid uuid_;
  const raw_ptr<desks_storage::DeskSyncService> service_;
};

// Class that allows waiting until a particular desk |uuid| is deleted by the
// DeskModel in |service|.
class DeskUuidDeletedChecker : public StatusChangeChecker,
                               public desks_storage::DeskModelObserver {
 public:
  // The caller must ensure that |service| is not null and will outlive this
  // object.
  DeskUuidDeletedChecker(desks_storage::DeskSyncService* service,
                         const base::Uuid& uuid);
  DeskUuidDeletedChecker(const DeskUuidDeletedChecker&) = delete;
  DeskUuidDeletedChecker& operator=(const DeskUuidDeletedChecker&) = delete;
  ~DeskUuidDeletedChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // DeskModelObserver implementation.
  void DeskModelLoaded() override;
  void EntriesAddedOrUpdatedRemotely(
      const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
          new_entries) override;
  void EntriesRemovedRemotely(const std::vector<base::Uuid>& uuids) override;

 private:
  const base::Uuid uuid_;
  const raw_ptr<desks_storage::DeskSyncService> service_;
};

// Class that allows waiting until the bridge is ready.
class DeskModelReadyChecker : public StatusChangeChecker,
                              public desks_storage::DeskModelObserver {
 public:
  // The caller must ensure that |service| is not null and will outlive this
  // object.
  explicit DeskModelReadyChecker(desks_storage::DeskSyncService* service);
  DeskModelReadyChecker(const DeskModelReadyChecker&) = delete;
  DeskModelReadyChecker& operator=(const DeskModelReadyChecker&) = delete;
  ~DeskModelReadyChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // DeskModelObserver implementation.
  void DeskModelLoaded() override;
  void EntriesAddedOrUpdatedRemotely(
      const std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>>&
          new_entries) override;
  void EntriesRemovedRemotely(const std::vector<base::Uuid>& uuids) override;

 private:
  const raw_ptr<desks_storage::DeskSyncService> service_;
};

}  // namespace workspace_desk_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_WORKSPACE_DESK_HELPER_H_
