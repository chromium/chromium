// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"

#include "base/memory/singleton.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"

namespace reporting {

ReportingServerConnector::TestEnvironment::TestEnvironment(
    ::policy::MockCloudPolicyClient* client)
    : test_client_(client),
      saved_client_(GetInstance()->client_),
      saved_core_(GetInstance()->core_) {
  client->SetDMToken("DUMMY_DM_TOKEN");  // Register mock client
  GetInstance()->client_ = client;
}

ReportingServerConnector::TestEnvironment::~TestEnvironment() {
  DCHECK_EQ(GetInstance()->client_, test_client_)
      << "Client was illegally altered by the test";
  GetInstance()->core_ = saved_core_;
  GetInstance()->client_ = saved_client_;
  base::Singleton<ReportingServerConnector>::OnExit(nullptr);
}
}  // namespace reporting
