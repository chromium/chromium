// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/device_info_helper.h"

ServerDeviceInfoMatchChecker::ServerDeviceInfoMatchChecker(
    fake_server::FakeServer* fake_server,
    const Matcher& matcher)
    : fake_server_(fake_server), matcher_(matcher) {
  fake_server->AddObserver(this);
}

ServerDeviceInfoMatchChecker::~ServerDeviceInfoMatchChecker() {
  fake_server_->RemoveObserver(this);
}

void ServerDeviceInfoMatchChecker::OnCommit(
    const std::string& committer_invalidator_client_id,
    syncer::ModelTypeSet committed_model_types) {
  if (committed_model_types.Has(syncer::DEVICE_INFO)) {
    CheckExitCondition();
  }
}

bool ServerDeviceInfoMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByModelType(syncer::DEVICE_INFO);

  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}
