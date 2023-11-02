// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_TEST_UTIL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_TEST_UTIL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"

namespace reporting {

class ReportingServerConnector::TestEnvironment {
 public:
  explicit TestEnvironment(::policy::MockCloudPolicyClient* client);
  TestEnvironment(const TestEnvironment& other) = delete;
  TestEnvironment& operator=(const TestEnvironment& other) = delete;
  ~TestEnvironment();

 private:
  const raw_ptr<::policy::MockCloudPolicyClient> test_client_;
  const raw_ptr<::policy::CloudPolicyClient> saved_client_;
  const raw_ptr<::policy::CloudPolicyCore> saved_core_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_TEST_UTIL_H_
