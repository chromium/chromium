// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

namespace reporting {
namespace {

constexpr char kServerUrl[] = "https://example.com/reporting";

}  // namespace

class FakeDelegate : public EncryptedReportingClient::Delegate {
 public:
  explicit FakeDelegate(
      policy::DeviceManagementService* device_management_service)
      : device_management_service_(device_management_service) {}

  FakeDelegate(const FakeDelegate&) = delete;
  FakeDelegate& operator=(const FakeDelegate&) = delete;

  ~FakeDelegate() override = default;

  policy::DeviceManagementService* device_management_service() const override {
    return device_management_service_;
  }

 private:
  const raw_ptr<policy::DeviceManagementService> device_management_service_;
};

ReportingServerConnector::TestEnvironment::TestEnvironment()
    : store_(std::make_unique<::policy::MockCloudPolicyStore>()),
      core_(std::make_unique<::policy::CloudPolicyCore>(
          ::policy::dm_protocol::kChromeDevicePolicyType,
          std::string(),
          store_.get(),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          network::TestNetworkConnectionTracker::CreateGetter())) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  fake_statistics_provider_ =
      std::make_unique<ash::system::ScopedFakeStatisticsProvider>();
  fake_statistics_provider_->SetMachineStatistic(ash::system::kSerialNumberKey,
                                                 "fake-serial-number");
#endif
  device_management_service_ =
      std::make_unique<policy::DeviceManagementService>(
          std::make_unique<policy::DeviceManagementServiceConfiguration>(
              /*dm_server_url=*/"", /*realtime_reporting_server_url=*/"",
              /*encrypted_reporting_server_url=*/kServerUrl));
  device_management_service_->ScheduleInitialization(0);
  TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
      url_loader_factory_.GetSafeWeakWrapper());

  auto cloud_policy_client = std::make_unique<::policy::CloudPolicyClient>(
      device_management_service_.get(),
      url_loader_factory_.GetSafeWeakWrapper(),
      ::policy::CloudPolicyClient::DeviceDMTokenCallback());
  auto service = std::make_unique<::policy::MockCloudPolicyService>(
      cloud_policy_client.get(), store_.get());
  GetInstance()->core_ = core_.get();
  GetInstance()->core_->ConnectForTesting(std::move(service),
                                          std::move(cloud_policy_client));
  SetDMToken(kFakeDmToken);

  auto delegate =
      std::make_unique<FakeDelegate>(device_management_service_.get());
  SetEncryptedReportingClient(
      EncryptedReportingClient::Create(std::move(delegate)));
}

ReportingServerConnector::TestEnvironment::~TestEnvironment() {
  EncryptedReportingClient::ResetUploadsStateForTest();
  base::Singleton<ReportingServerConnector>::OnExit(nullptr);
}

base::Value::Dict ReportingServerConnector::TestEnvironment::request_body(
    size_t index) {
  CHECK_GT(url_loader_factory()->pending_requests()->size(), index);
  const network::ResourceRequest& request =
      (*url_loader_factory()->pending_requests())[index].request;
  CHECK(request.request_body);
  CHECK(request.request_body->elements());

  std::optional<base::Value> body =
      base::JSONReader::Read(request.request_body->elements()
                                 ->at(0)
                                 .As<network::DataElementBytes>()
                                 .AsStringPiece());
  CHECK(body);
  CHECK(body->is_dict());
  return body->GetDict().Clone();
}

void ReportingServerConnector::TestEnvironment::SimulateResponseForRequest(
    size_t index) {
  auto response = ResponseBuilder(request_body(index)).Build();
  CHECK_OK(response) << response.error();
  SimulateCustomResponseForRequest(index, std::move(*response));
}

void ReportingServerConnector::TestEnvironment::
    SimulateCustomResponseForRequest(size_t index,
                                     StatusOr<base::Value::Dict> response) {
  const std::string& pending_request_url =
      (*url_loader_factory()->pending_requests())[0].request.url.spec();
  std::string response_string = "";
  if (response.has_value()) {
    base::JSONWriter::Write(response.value(), &response_string);
  }
  url_loader_factory()->SimulateResponseForPendingRequest(pending_request_url,
                                                          response_string);
}

void ReportingServerConnector::TestEnvironment::SetDMToken(
    const std::string& dm_token) const {
  GetInstance()->core_->client()->SetupRegistration(dm_token, "client-id", {});
}

void ReportingServerConnector::TestEnvironment::SetEncryptedReportingClient(
    std::unique_ptr<EncryptedReportingClient> encrypted_reporting_client) {
  GetInstance()->encrypted_reporting_client_ =
      std::move(encrypted_reporting_client);
}
}  // namespace reporting
