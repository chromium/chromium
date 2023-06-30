// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_TEST_UTIL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_TEST_UTIL_H_

#include <cstddef>
#include <memory>

#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::system {
class ScopedFakeStatisticsProvider;

}  // namespace ash::system

namespace reporting {

constexpr char kFakeDmToken[] = "FAKE_DM_TOKEN";

class EncryptedReportingClient;

class ReportingServerConnector::TestEnvironment {
 public:
  TestEnvironment();
  TestEnvironment(const TestEnvironment& other) = delete;
  TestEnvironment& operator=(const TestEnvironment& other) = delete;
  ~TestEnvironment();

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  base::Value::Dict request_body(size_t index);

  void SimulateResponseForRequest(size_t index);

  void SimulateCustomResponseForRequest(
      size_t index,
      absl::optional<base::Value::Dict> response);

  void SetDMToken(const std::string& dm_token) const;

  void SetEncryptedReportingClient(
      std::unique_ptr<EncryptedReportingClient> encrypted_reporting_client);

 private:
  std::unique_ptr<ash::system::ScopedFakeStatisticsProvider>
      fake_statistics_provider_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<policy::DeviceManagementService> device_management_service_;
  std::unique_ptr<::policy::CloudPolicyStore> store_;
  std::unique_ptr<::policy::CloudPolicyCore> core_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORTING_SERVER_CONNECTOR_TEST_UTIL_H_
