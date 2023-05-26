// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"

#include <memory>
#include <utility>

#include "base/memory/singleton.h"
#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "services/network/test/test_network_connection_tracker.h"

namespace reporting {

ReportingServerConnector::TestEnvironment::TestEnvironment()
    : store_(std::make_unique<::policy::MockCloudPolicyStore>()),
      core_(std::make_unique<::policy::CloudPolicyCore>(
          ::policy::dm_protocol::kChromeDevicePolicyType,
          std::string(),
          store_.get(),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          network::TestNetworkConnectionTracker::CreateGetter())) {
  auto mock_client = std::make_unique<::policy::MockCloudPolicyClient>();
  mock_client->SetDMToken(
      ::policy::DMToken::CreateValidToken(kFakeDmToken).value());
  auto service = std::make_unique<::policy::MockCloudPolicyService>(
      mock_client.get(), store_.get());
  GetInstance()->core_ = core_.get();
  GetInstance()->core_->ConnectForTesting(std::move(service),
                                          std::move(mock_client));
}

ReportingServerConnector::TestEnvironment::~TestEnvironment() {
  base::Singleton<ReportingServerConnector>::OnExit(nullptr);
}

::policy::MockCloudPolicyClient*
ReportingServerConnector::TestEnvironment::client() const {
  return reinterpret_cast<::policy::MockCloudPolicyClient*>(
      GetInstance()->core_->client());
}

void ReportingServerConnector::TestEnvironment::SetEncryptedReportingClient(
    std::unique_ptr<EncryptedReportingClient> encrypted_reporting_client) {
  GetInstance()->encrypted_reporting_client_ =
      std::move(encrypted_reporting_client);
}
}  // namespace reporting
