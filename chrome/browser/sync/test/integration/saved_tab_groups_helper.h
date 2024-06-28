// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SAVED_TAB_GROUPS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SAVED_TAB_GROUPS_HELPER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"

namespace tab_groups {
class SavedTabGroupModel;

// Checks that a tab or group with a particular uuid exists in the model.
class SavedTabOrGroupExistsChecker : public StatusChangeChecker,
                                     public SavedTabGroupModelObserver {
 public:
  // `model` must not be null and must outlive this object.
  SavedTabOrGroupExistsChecker(SavedTabGroupModel* model,
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
      const std::optional<base::Uuid>& tab_uuid) override;

 private:
  const base::Uuid uuid_;
  raw_ptr<SavedTabGroupModel> const model_;
};

// Checks that a tab or group with a particular uuid does not exist in the
// model.
class SavedTabOrGroupDoesNotExistChecker : public StatusChangeChecker,
                                           public SavedTabGroupModelObserver {
 public:
  // `model` must not be null and must outlive this object.
  SavedTabOrGroupDoesNotExistChecker(SavedTabGroupModel* model,
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
      const SavedTabGroup& removed_group) override;

  // Note: Also handles the removal of tabs.
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_uuid,
      const std::optional<base::Uuid>& tab_uuid) override;

 private:
  const base::Uuid uuid_;
  raw_ptr<SavedTabGroupModel> const model_;
};

// Checks that a matching group exists in the model.
class SavedTabGroupMatchesChecker : public StatusChangeChecker,
                                    public SavedTabGroupModelObserver {
 public:
  // The caller must ensure that `model` is not null and will outlive this
  // object.
  SavedTabGroupMatchesChecker(SavedTabGroupModel* model, SavedTabGroup group);
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
      const std::optional<base::Uuid>& tab_uuid) override;

 private:
  const SavedTabGroup group_;
  raw_ptr<SavedTabGroupModel> const model_;
};

// Checks that a matching tab exists in the model.
class SavedTabMatchesChecker : public StatusChangeChecker,
                               public SavedTabGroupModelObserver {
 public:
  // The caller must ensure that `model` is not null and will outlive this
  // object.
  SavedTabMatchesChecker(SavedTabGroupModel* model, SavedTabGroupTab tab);
  SavedTabMatchesChecker(const SavedTabMatchesChecker&) = delete;
  SavedTabMatchesChecker& operator=(const SavedTabMatchesChecker&) = delete;
  ~SavedTabMatchesChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SavedTabGroupModelObserver
  void SavedTabGroupAddedFromSync(const base::Uuid& uuid) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_uuid,
      const std::optional<base::Uuid>& tab_uuid) override;

 private:
  const SavedTabGroupTab tab_;
  raw_ptr<SavedTabGroupModel> const model_;
};

// Checks that the model contains saved groups in a certain order.
class GroupOrderChecker : public StatusChangeChecker,
                          public SavedTabGroupModelObserver {
 public:
  // The caller must ensure that `model` is not null and will outlive this
  // object.
  GroupOrderChecker(SavedTabGroupModel* model,
                    std::vector<base::Uuid> group_ids);
  GroupOrderChecker(const GroupOrderChecker&) = delete;
  GroupOrderChecker& operator=(const GroupOrderChecker&) = delete;
  ~GroupOrderChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SavedTabGroupModelObserver
  void SavedTabGroupAddedFromSync(const base::Uuid& uuid) override;
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup& removed_group) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_uuid,
      const std::optional<base::Uuid>& tab_uuid) override;

 private:
  const std::vector<base::Uuid> group_ids_;
  raw_ptr<SavedTabGroupModel> const model_;
};

// Checks that a saved group in the model contains tabs in a certain order.
class TabOrderChecker : public StatusChangeChecker,
                        public SavedTabGroupModelObserver {
 public:
  // The caller must ensure that `service` is not null and will outlive this
  // object.
  TabOrderChecker(SavedTabGroupModel* model,
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
      const std::optional<base::Uuid>& tab_uuid) override;

 private:
  const base::Uuid group_id_;
  const std::vector<base::Uuid> tab_ids_;

  raw_ptr<SavedTabGroupModel> const model_;
};
}  // namespace tab_groups

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SAVED_TAB_GROUPS_HELPER_H_
