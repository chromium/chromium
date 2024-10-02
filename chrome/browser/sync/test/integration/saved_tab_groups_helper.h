// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SAVED_TAB_GROUPS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SAVED_TAB_GROUPS_HELPER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/sync/base/data_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_pb {
class SavedTabGroupSpecifics;
}  // namespace sync_pb

namespace tab_groups {

MATCHER_P2(HasSpecificsSavedTabGroup, title, color, "") {
  return arg.group().title() == title && arg.group().color() == color;
}

MATCHER_P2(HasSpecificsSavedTab, title, url, "") {
  return arg.tab().title() == title && arg.tab().url() == url;
}

// Checks that a tab or group with a particular uuid exists in the service.
class SavedTabOrGroupExistsChecker : public StatusChangeChecker,
                                     public TabGroupSyncService::Observer {
 public:
  // `service` must not be null and must outlive this object.
  SavedTabOrGroupExistsChecker(TabGroupSyncService* service,
                               const base::Uuid& uuid);
  SavedTabOrGroupExistsChecker(const SavedTabOrGroupExistsChecker&) = delete;
  SavedTabOrGroupExistsChecker& operator=(const SavedTabOrGroupExistsChecker&) =
      delete;
  ~SavedTabOrGroupExistsChecker() override;

  // StatusChangeChecker:
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // TabGroupSyncService::Observer
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override;
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override;

 private:
  const base::Uuid uuid_;
  raw_ptr<TabGroupSyncService> const service_;
};

// Checks that a tab or group with a particular uuid does not exist in the
// service.
class SavedTabOrGroupDoesNotExistChecker
    : public StatusChangeChecker,
      public TabGroupSyncService::Observer {
 public:
  // `service` must not be null and must outlive this object.
  SavedTabOrGroupDoesNotExistChecker(TabGroupSyncService* service,
                                     const base::Uuid& uuid);
  SavedTabOrGroupDoesNotExistChecker(
      const SavedTabOrGroupDoesNotExistChecker&) = delete;
  SavedTabOrGroupDoesNotExistChecker& operator=(
      const SavedTabOrGroupDoesNotExistChecker&) = delete;
  ~SavedTabOrGroupDoesNotExistChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // TabGroupSyncService::Observer
  // Note: Also handles the removal of tabs.
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         TriggerSource source) override;

 private:
  const base::Uuid uuid_;
  raw_ptr<TabGroupSyncService> const service_;
};

// Checks that a matching group exists in the service.
class SavedTabGroupMatchesChecker : public StatusChangeChecker,
                                    public TabGroupSyncService::Observer {
 public:
  // The caller must ensure that `service` is not null and will outlive this
  // object.
  SavedTabGroupMatchesChecker(TabGroupSyncService* service,
                              SavedTabGroup group);
  SavedTabGroupMatchesChecker(const SavedTabGroupMatchesChecker&) = delete;
  SavedTabGroupMatchesChecker& operator=(const SavedTabGroupMatchesChecker&) =
      delete;
  ~SavedTabGroupMatchesChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // TabGroupSyncService::Observer
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override;
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override;

 private:
  const SavedTabGroup group_;
  raw_ptr<TabGroupSyncService> const service_;
};

// Checks that a matching tab exists in the service.
class SavedTabMatchesChecker : public StatusChangeChecker,
                               public TabGroupSyncService::Observer {
 public:
  // The caller must ensure that `service` is not null and will outlive this
  // object.
  SavedTabMatchesChecker(TabGroupSyncService* service, SavedTabGroupTab tab);
  SavedTabMatchesChecker(const SavedTabMatchesChecker&) = delete;
  SavedTabMatchesChecker& operator=(const SavedTabMatchesChecker&) = delete;
  ~SavedTabMatchesChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // TabGroupSyncService::Observer
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override;
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override;

 private:
  const SavedTabGroupTab tab_;
  raw_ptr<TabGroupSyncService> const service_;
};

// Checks that the service contains saved groups in a certain order.
class GroupOrderChecker : public StatusChangeChecker,
                          public TabGroupSyncService::Observer {
 public:
  // The caller must ensure that `service` is not null and will outlive this
  // object.
  GroupOrderChecker(TabGroupSyncService* service,
                    std::vector<base::Uuid> group_ids);
  GroupOrderChecker(const GroupOrderChecker&) = delete;
  GroupOrderChecker& operator=(const GroupOrderChecker&) = delete;
  ~GroupOrderChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // TabGroupSyncService::Observer
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override;
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override;
  void OnTabGroupRemoved(const base::Uuid& sync_id,
                         TriggerSource source) override;

 private:
  const std::vector<base::Uuid> group_ids_;
  raw_ptr<TabGroupSyncService> const service_;
};

// Checks that a saved group in the service contains tabs in a certain order.
class TabOrderChecker : public StatusChangeChecker,
                        public TabGroupSyncService::Observer {
 public:
  // The caller must ensure that `service` is not null and will outlive this
  // object.
  TabOrderChecker(TabGroupSyncService* service,
                  base::Uuid group_id,
                  std::vector<base::Uuid> tab_ids);
  TabOrderChecker(const TabOrderChecker&) = delete;
  TabOrderChecker& operator=(const TabOrderChecker&) = delete;
  ~TabOrderChecker() override;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // TabGroupSyncService::Observer
  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override;
  void OnTabGroupUpdated(const SavedTabGroup& group,
                         TriggerSource source) override;

 private:
  const base::Uuid group_id_;
  const std::vector<base::Uuid> tab_ids_;

  raw_ptr<TabGroupSyncService> const service_;
};

// A helper class that waits for the SAVED_TAB_GROUP entities on the FakeServer
// to match a given GMock matcher.
class ServerSavedTabGroupMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher =
      testing::Matcher<std::vector<sync_pb::SavedTabGroupSpecifics>>;

  explicit ServerSavedTabGroupMatchChecker(const Matcher& matcher);
  ~ServerSavedTabGroupMatchChecker() override;

  // fake_server::FakeServerMatchStatusChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SAVED_TAB_GROUPS_HELPER_H_
