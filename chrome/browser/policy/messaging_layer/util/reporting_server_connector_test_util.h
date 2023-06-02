// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_TEST_UTIL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_TEST_UTIL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"

namespace reporting {

constexpr char kFakeDmToken[] = "FAKE_DM_TOKEN";

class EncryptedReportingClient;

class ReportingServerConnector::TestEnvironment {
 public:
  TestEnvironment();
  TestEnvironment(const TestEnvironment& other) = delete;
  TestEnvironment& operator=(const TestEnvironment& other) = delete;
  ~TestEnvironment();

  ::policy::MockCloudPolicyClient* client() const;

  void SetEncryptedReportingClient(
      std::unique_ptr<EncryptedReportingClient> encrypted_reporting_client);

 private:
  std::unique_ptr<::policy::CloudPolicyStore> store_;
  std::unique_ptr<::policy::CloudPolicyCore> core_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_TEST_UTIL_H_
