// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SHARED_TAB_GROUP_DATA_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SHARED_TAB_GROUP_DATA_HELPER_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/saved_tab_groups/saved_tab_group_model_observer.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace tab_groups {

class SavedTabGroupModel;

MATCHER_P2(HasSpecificsSharedTabGroup, title, color, "") {
  return arg.tab_group().title() == title && arg.tab_group().color() == color;
}

MATCHER_P2(HasSpecificsSharedTab, title, url, "") {
  return arg.tab().title() == title && arg.tab().url() == url;
}

MATCHER_P3(HasSharedGroupMetadata, title, color, collaboration_id, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.color() == color &&
         arg.collaboration_id() == collaboration_id;
}

MATCHER_P2(HasTabMetadata, title, url, "") {
  return base::UTF16ToUTF8(arg.title()) == title && arg.url() == GURL(url);
}

// A helper class that waits for the SAVED_TAB_GROUP entities on the FakeServer
// to match a given GMock matcher.
class ServerSharedTabGroupMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher =
      testing::Matcher<std::vector<sync_pb::SharedTabGroupDataSpecifics>>;

  explicit ServerSharedTabGroupMatchChecker(const Matcher& matcher);
  ~ServerSharedTabGroupMatchChecker() override;

  // fake_server::FakeServerMatchStatusChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

class SharedTabGroupsMatchChecker : public SavedTabGroupModelObserver,
                                    public StatusChangeChecker {
 public:
  SharedTabGroupsMatchChecker(SavedTabGroupModel& model_1,
                              SavedTabGroupModel& model_2);
  ~SharedTabGroupsMatchChecker() override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // SavedTabGroupModelObserver overrides.
  void SavedTabGroupAddedFromSync(const base::Uuid& guid) override;
  void SavedTabGroupRemovedFromSync(
      const SavedTabGroup& removed_group) override;
  void SavedTabGroupUpdatedFromSync(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override;

 private:
  raw_ref<SavedTabGroupModel> model_1_;
  raw_ref<SavedTabGroupModel> model_2_;

  base::ScopedObservation<SavedTabGroupModel, SharedTabGroupsMatchChecker>
      observation_1_{this};
  base::ScopedObservation<SavedTabGroupModel, SharedTabGroupsMatchChecker>
      observation_2_{this};
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SHARED_TAB_GROUP_DATA_HELPER_H_
