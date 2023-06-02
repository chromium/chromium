// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SAVED_TAB_GROUPS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SAVED_TAB_GROUPS_HELPER_H_

#include <vector>

#include "base/uuid.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"

class SavedTabGroupKeyedService;

namespace saved_tab_groups_helper {

// Checks that a tab or group with a particular uuid exists in the model.
class SavedTabOrGroupExistsChecker : public StatusChangeChecker,
                                     public SavedTabGroupModelObserver {
 public:
  // The caller must ensure that `service` is not null and will outlive this
  // object.
  SavedTabOrGroupExistsChecker(SavedTabGroupKeyedService* service,
                               const base::Uuid& uuid);
  SavedTabOrGroupExistsChecker(const SavedTabOrGroupExistsChecker&) = delete;
  SavedTabOrGroupExistsChecker& operator=(const SavedTabOrGroupExistsChecker&) =
      delete;
  ~SavedTabOrGroupExistsChecker() override;

  // StatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SavedTabGroupModelObserver:
  void SavedTabGroupAddedFromSync(const base::Uuid& uuid) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_uuid,
      const absl::optional<base::Uuid>& tab_uuid = absl::nullopt) override;

 private:
  const base::Uuid uuid_;
  raw_ptr<SavedTabGroupKeyedService> const service_;
};

// Checks that a tab or group with a particular uuid does not exist in the
// model.
class SavedTabOrGroupDoesNotExistChecker : public StatusChangeChecker,
                                           public SavedTabGroupModelObserver {
 public:
  // The caller must ensure that `service` is not null and will outlive this
  // object.
  SavedTabOrGroupDoesNotExistChecker(SavedTabGroupKeyedService* service,
                                     const base::Uuid& uuid);
  SavedTabOrGroupDoesNotExistChecker(
      const SavedTabOrGroupDoesNotExistChecker&) = delete;
  SavedTabOrGroupDoesNotExistChecker& operator=(
      const SavedTabOrGroupDoesNotExistChecker&) = delete;
  ~SavedTabOrGroupDoesNotExistChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SavedTabGroupModelObserver
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup* removed_group) override;

  // Note: Also handles the removal of tabs.
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_uuid,
      const absl::optional<base::Uuid>& tab_uuid = absl::nullopt) override;

 private:
  const base::Uuid uuid_;
  raw_ptr<SavedTabGroupKeyedService> const service_;
};

// Checks that a matching group exists in the model.
class SavedTabGroupMatchesChecker : public StatusChangeChecker,
                                    public SavedTabGroupModelObserver {
 public:
  // The caller must ensure that `service` is not null and will outlive this
  // object.
  SavedTabGroupMatchesChecker(SavedTabGroupKeyedService* service,
                              SavedTabGroup group);
  SavedTabGroupMatchesChecker(const SavedTabGroupMatchesChecker&) = delete;
  SavedTabGroupMatchesChecker& operator=(const SavedTabGroupMatchesChecker&) =
      delete;
  ~SavedTabGroupMatchesChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SavedTabGroupModelObserver
  void SavedTabGroupAddedFromSync(const base::Uuid& uuid) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_uuid,
      const absl::optional<base::Uuid>& tab_uuid = absl::nullopt) override;

 private:
  const SavedTabGroup group_;
  raw_ptr<SavedTabGroupKeyedService> const service_;
};

// Checks that a matching tab exists in the model.
class SavedTabMatchesChecker : public StatusChangeChecker,
                               public SavedTabGroupModelObserver {
 public:
  // The caller must ensure that `service` is not null and will outlive this
  // object.
  SavedTabMatchesChecker(SavedTabGroupKeyedService* service,
                         SavedTabGroupTab tab);
  SavedTabMatchesChecker(const SavedTabMatchesChecker&) = delete;
  SavedTabMatchesChecker& operator=(const SavedTabMatchesChecker&) = delete;
  ~SavedTabMatchesChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SavedTabGroupModelObserver
  void SavedTabGroupAddedFromSync(const base::Uuid& uuid) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_uuid,
      const absl::optional<base::Uuid>& tab_uuid = absl::nullopt) override;

 private:
  const SavedTabGroupTab tab_;
  raw_ptr<SavedTabGroupKeyedService> const service_;
};

// Checks that the model contains saved groups in a certain order.
class GroupOrderChecker : public StatusChangeChecker,
                          public SavedTabGroupModelObserver {
 public:
  // The caller must ensure that `service` is not null and will outlive this
  // object.
  GroupOrderChecker(SavedTabGroupKeyedService* service,
                    std::vector<base::Uuid> group_ids);
  GroupOrderChecker(const GroupOrderChecker&) = delete;
  GroupOrderChecker& operator=(const GroupOrderChecker&) = delete;
  ~GroupOrderChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SavedTabGroupModelObserver
  void SavedTabGroupAddedFromSync(const base::Uuid& uuid) override;
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup* removed_group) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_uuid,
      const absl::optional<base::Uuid>& tab_uuid = absl::nullopt) override;

 private:
  const std::vector<base::Uuid> group_ids_;
  raw_ptr<SavedTabGroupKeyedService> const service_;
};

// Checks that a saved group in the model contains tabs in a certain order.
class TabOrderChecker : public StatusChangeChecker,
                        public SavedTabGroupModelObserver {
 public:
  // The caller must ensure that `service` is not null and will outlive this
  // object.
  TabOrderChecker(SavedTabGroupKeyedService* service,
                  base::Uuid group_id,
                  std::vector<base::Uuid> tab_ids);
  TabOrderChecker(const TabOrderChecker&) = delete;
  TabOrderChecker& operator=(const TabOrderChecker&) = delete;
  ~TabOrderChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SavedTabGroupModelObserver
  void SavedTabGroupAddedFromSync(const base::Uuid& uuid) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_uuid,
      const absl::optional<base::Uuid>& tab_uuid = absl::nullopt) override;

 private:
  const base::Uuid group_id_;
  const std::vector<base::Uuid> tab_ids_;

  raw_ptr<SavedTabGroupKeyedService> const service_;
};
}  // namespace saved_tab_groups_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SAVED_TAB_GROUPS_HELPER_H_
