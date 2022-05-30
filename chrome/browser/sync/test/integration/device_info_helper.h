// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_DEVICE_INFO_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_DEVICE_INFO_HELPER_H_

#include <ostream>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_pb {
class SyncEntity;
}

// A helper class that waits for a certain set of DeviceInfos on the FakeServer.
// The desired state is passed in as a GTest matcher.
class ServerDeviceInfoMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher = testing::Matcher<std::vector<sync_pb::SyncEntity>>;

  explicit ServerDeviceInfoMatchChecker(const Matcher& matcher);
  ~ServerDeviceInfoMatchChecker() override;
  ServerDeviceInfoMatchChecker(const ServerDeviceInfoMatchChecker&) = delete;
  ServerDeviceInfoMatchChecker& operator=(const ServerDeviceInfoMatchChecker&) =
      delete;

  // FakeServer::Observer overrides.
  void OnCommit(const std::string& committer_invalidator_client_id,
                syncer::ModelTypeSet committed_model_types) override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_DEVICE_INFO_HELPER_H_
