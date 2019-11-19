// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/test_support/local_policy_test_server.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

const char kClientID[] = "testid";
const char kDMToken[] = "fake_token";

// Parses the DeviceManagementRequest in |request_data| and writes a serialized
// DeviceManagementResponse to |response_data|.
void ConstructResponse(const std::string& request_data,
                       std::string* response_data) {
  em::DeviceManagementRequest request;
  ASSERT_TRUE(request.ParseFromString(request_data));
  em::DeviceManagementResponse response;
  if (request.has_register_request()) {
    response.mutable_register_response()->set_device_management_token(kDMToken);
  } else if (request.has_service_api_access_request()) {
    response.mutable_service_api_access_response()->set_auth_code(
        "fake_auth_code");
  } else if (request.has_unregister_request()) {
    response.mutable_unregister_response();
  } else if (request.has_policy_request()) {
    response.mutable_policy_response()->add_responses();
  } else if (request.has_auto_enrollment_request()) {
    response.mutable_auto_enrollment_response();
  } else if (request.has_app_install_report_request()) {
    response.mutable_app_install_report_response();
  } else {
    FAIL() << "Failed to parse request.";
  }
  ASSERT_TRUE(response.SerializeToString(response_data));
}

void OnRequest(network::TestURLLoaderFactory* test_factory,
               const network::ResourceRequest& request) {
  std::string upload_data(network::GetUploadData(request));
  if (upload_data.empty())
    return;

  std::string response_data;
  ConstructResponse(upload_data, &response_data);
  test_factory->AddResponse(request.url.spec(), response_data);
}

}  // namespace

class DeviceManagementServiceIntegrationTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<
          std::string (DeviceManagementServiceIntegrationTest::*)(void)> {
 public:
  MOCK_METHOD4(OnJobDone,
               void(DeviceManagementService::Job*,
                    DeviceManagementStatus,
                    int,
                    const std::string&));

  std::string InitCannedResponse() {
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
    test_url_loader_factory_->SetInterceptor(
        base::BindRepeating(&OnRequest, test_url_loader_factory_.get()));

    return "http://localhost";
  }

  std::string InitTestServer() {
    StartTestServer();
    return test_server_->GetServiceURL().spec();
  }

  void RecordAuthCode(DeviceManagementService::Job* job,
                      DeviceManagementStatus code,
                      int net_error,
                      const std::string& response_body) {
    em::DeviceManagementResponse response;
    ASSERT_TRUE(response.ParseFromString(response_body));
    robot_auth_code_ = response.service_api_access_response().auth_code();
  }

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> GetFactory() {
    return test_shared_loader_factory_
               ? test_shared_loader_factory_
               : g_browser_process->system_network_context_manager()
                     ->GetSharedURLLoaderFactory();
  }

  std::unique_ptr<DeviceManagementService::Job> StartJob(
      DeviceManagementService::JobConfiguration::JobType type,
      bool critical,
      std::unique_ptr<DMAuth> auth_data,
      base::Optional<std::string> oauth_token,
      const em::DeviceManagementRequest request) {
    std::string payload;
    request.SerializeToString(&payload);
    std::unique_ptr<FakeJobConfiguration> config =
        std::make_unique<FakeJobConfiguration>(
            service_.get(), type, kClientID, critical, std::move(auth_data),
            oauth_token, GetFactory(),
            base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                       base::Unretained(this)),
            base::DoNothing());
    config->SetRequestPayload(payload);
    return service_->CreateJob(std::move(config));
  }

  void PerformRegistration() {
    base::RunLoop run_loop;

    EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, _))
        .WillOnce(DoAll(
            Invoke(this, &DeviceManagementServiceIntegrationTest::RecordToken),
            InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit)));

    em::DeviceManagementRequest request;
    request.mutable_register_request();
    std::unique_ptr<DeviceManagementService::Job> job =
        StartJob(DeviceManagementService::JobConfiguration::TYPE_REGISTRATION,
                 false, DMAuth::NoAuth(), "oauth_token", request);

    run_loop.Run();
  }

  void SetUpOnMainThread() override {
    std::string service_url((this->*(GetParam()))());
    service_.reset(new DeviceManagementService(
        std::unique_ptr<DeviceManagementService::Configuration>(
            new MockDeviceManagementServiceConfiguration(service_url))));
    service_->ScheduleInitialization(0);
  }

  void TearDownOnMainThread() override {
    service_.reset();
    test_server_.reset();
  }

  void StartTestServer() {
    test_server_.reset(new LocalPolicyTestServer(
        "chrome/test/data/policy/"
        "policy_device_management_service_browsertest.json"));
    ASSERT_TRUE(test_server_->Start());
  }

  void RecordToken(DeviceManagementService::Job* job,
                   DeviceManagementStatus code,
                   int net_error,
                   const std::string& response_body) {
    em::DeviceManagementResponse response;
    ASSERT_TRUE(response.ParseFromString(response_body));
    token_ = response.register_response().device_management_token();
  }

  std::string token_;
  std::string robot_auth_code_;
  std::unique_ptr<DeviceManagementService> service_;
  std::unique_ptr<LocalPolicyTestServer> test_server_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, Registration) {
  PerformRegistration();
  EXPECT_FALSE(token_.empty());
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest,
                       ApiAuthCodeFetch) {
  PerformRegistration();

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, _))
      .WillOnce(DoAll(
          Invoke(this, &DeviceManagementServiceIntegrationTest::RecordAuthCode),
          InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit)));

  em::DeviceManagementRequest request;
  em::DeviceServiceApiAccessRequest* device_request =
      request.mutable_service_api_access_request();
  device_request->add_auth_scopes("authScope4Test");
  device_request->set_oauth2_client_id("oauth2ClientId4Test");
  std::unique_ptr<DeviceManagementService::Job> job = StartJob(
      DeviceManagementService::JobConfiguration::TYPE_API_AUTH_CODE_FETCH,
      false, DMAuth::FromDMToken(token_), "", request);

  run_loop.Run();
  ASSERT_EQ("fake_auth_code", robot_auth_code_);
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, PolicyFetch) {
  PerformRegistration();

  base::RunLoop run_loop;

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  em::DeviceManagementRequest request;
  request.mutable_policy_request()->add_requests()->set_policy_type(
      dm_protocol::kChromeUserPolicyType);
  std::unique_ptr<DeviceManagementService::Job> job =
      StartJob(DeviceManagementService::JobConfiguration::TYPE_POLICY_FETCH,
               false, DMAuth::FromDMToken(token_), "", request);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, Unregistration) {
  PerformRegistration();

  base::RunLoop run_loop;

  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  em::DeviceManagementRequest request;
  request.mutable_unregister_request();
  std::unique_ptr<DeviceManagementService::Job> job =
      StartJob(DeviceManagementService::JobConfiguration::TYPE_UNREGISTRATION,
               false, DMAuth::FromDMToken(token_), "", request);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, AutoEnrollment) {
  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  em::DeviceManagementRequest request;
  request.mutable_auto_enrollment_request()->set_remainder(0);
  request.mutable_auto_enrollment_request()->set_modulus(1);
  std::unique_ptr<DeviceManagementService::Job> job =
      StartJob(DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
               false, DMAuth::NoAuth(), "", request);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest,
                       AppInstallReport) {
  PerformRegistration();

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnJobDone(_, DM_STATUS_SUCCESS, _, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  em::DeviceManagementRequest request;
  request.mutable_app_install_report_request();
  std::unique_ptr<DeviceManagementService::Job> job = StartJob(
      DeviceManagementService::JobConfiguration::TYPE_UPLOAD_APP_INSTALL_REPORT,
      false, DMAuth::FromDMToken(token_), "", request);

  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    DeviceManagementServiceIntegrationTestInstance,
    DeviceManagementServiceIntegrationTest,
    testing::Values(&DeviceManagementServiceIntegrationTest::InitCannedResponse,
                    &DeviceManagementServiceIntegrationTest::InitTestServer));

}  // namespace policy
