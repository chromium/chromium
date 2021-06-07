// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_DEVICE_INFO_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_DEVICE_INFO_HELPER_H_

#include <ostream>
#include <string>

#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "testing/gtest/include/gtest/gtest.h"

// A helper class that waits for a certain set of DeviceInfos on the FakeServer.
// The desired state is passed in as a GTest matcher.
class ServerDeviceInfoMatchChecker : public StatusChangeChecker,
                                     fake_server::FakeServer::Observer {
 public:
  using Matcher = testing::Matcher<std::vector<sync_pb::SyncEntity>>;

  ServerDeviceInfoMatchChecker(fake_server::FakeServer* fake_server,
                               const Matcher& matcher);
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
  fake_server::FakeServer* const fake_server_;
  const Matcher matcher_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_DEVICE_INFO_HELPER_H_
