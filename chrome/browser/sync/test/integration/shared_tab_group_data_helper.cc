// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/shared_tab_group_data_helper.h"

#include "components/sync/base/data_type.h"
#include "components/sync/protocol/shared_tab_group_data_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"

namespace tab_groups {

namespace {

std::vector<sync_pb::SharedTabGroupDataSpecifics>
SyncEntitiesToSharedTabGroupSpecifics(
    std::vector<sync_pb::SyncEntity> entities) {
  std::vector<sync_pb::SharedTabGroupDataSpecifics> shared_tab_groups;
  for (sync_pb::SyncEntity& entity : entities) {
    CHECK(entity.specifics().has_shared_tab_group_data());
    sync_pb::SharedTabGroupDataSpecifics specifics;
    specifics.Swap(entity.mutable_specifics()->mutable_shared_tab_group_data());
    shared_tab_groups.push_back(std::move(specifics));
  }
  return shared_tab_groups;
}

}  // namespace

ServerSharedTabGroupMatchChecker::ServerSharedTabGroupMatchChecker(
    const Matcher& matcher)
    : matcher_(matcher) {}

ServerSharedTabGroupMatchChecker::~ServerSharedTabGroupMatchChecker() = default;

bool ServerSharedTabGroupMatchChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for the tab groups committed to the server. ";

  std::vector<sync_pb::SharedTabGroupDataSpecifics> entities =
      SyncEntitiesToSharedTabGroupSpecifics(
          fake_server()->GetSyncEntitiesByDataType(
              syncer::SHARED_TAB_GROUP_DATA));

  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}

}  // namespace tab_groups
