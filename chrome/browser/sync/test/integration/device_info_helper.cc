// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/device_info_helper.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/test/fake_server.h"

ServerDeviceInfoMatchChecker::ServerDeviceInfoMatchChecker(
    const Matcher& matcher)
    : matcher_(matcher) {}

ServerDeviceInfoMatchChecker::~ServerDeviceInfoMatchChecker() = default;

void ServerDeviceInfoMatchChecker::OnCommit(
    syncer::DataTypeSet committed_data_types) {
  if (committed_data_types.Has(syncer::DEVICE_INFO)) {
    CheckExitCondition();
  }
}

bool ServerDeviceInfoMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for server DeviceInfo to match: ";
  std::vector<sync_pb::SyncEntity> entities =
      fake_server()->GetSyncEntitiesByDataType(syncer::DEVICE_INFO);

  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}

namespace device_info_helper {

bool WaitForFullDeviceInfoCommitted(const std::string& cache_guid) {
  return ServerDeviceInfoMatchChecker(
             testing::Contains(
                 testing::AllOf(HasCacheGuid(cache_guid), HasSharingFields())))
      .Wait();
}

}  // namespace device_info_helper
