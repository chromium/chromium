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
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
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

// Parses the DeviceManagementRequest in |request_data| and writes a serialized
// DeviceManagementResponse to |response_data|.
void ConstructResponse(const std::string& request_data,
                       std::string* response_data) {
  em::DeviceManagementRequest request;
  ASSERT_TRUE(request.ParseFromString(request_data));
  em::DeviceManagementResponse response;
  if (request.has_register_request()) {
    response.mutable_register_response()->set_device_management_token(
        "fake_token");
  } else if (request.has_service_api_access_request()) {
    response.mutable_service_api_access_response()->set_auth_code(
        "fake_auth_code");
  } else if (request.has_unregister_request()) {
    response.mutable_unregister_response();
  } else if (request.has_policy_request()) {
    response.mutable_policy_response()->add_response();
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
  MOCK_METHOD3(OnJobDone, void(DeviceManagementStatus, int,
                               const em::DeviceManagementResponse&));

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

  void RecordAuthCode(DeviceManagementStatus status,
                      int net_error,
                      const em::DeviceManagementResponse& response) {
    robot_auth_code_ = response.service_api_access_response().auth_code();
  }

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> GetFactory() {
    return test_shared_loader_factory_
               ? test_shared_loader_factory_
               : g_browser_process->system_network_context_manager()
                     ->GetSharedURLLoaderFactory();
  }

  void PerformRegistration() {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _, _))
        .WillOnce(DoAll(
            Invoke(this, &DeviceManagementServiceIntegrationTest::RecordToken),
            InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle)));
    std::unique_ptr<DeviceManagementRequestJob> job(service_->CreateJob(
        DeviceManagementRequestJob::TYPE_REGISTRATION, GetFactory()));
    job->SetAuthData(DMAuth::FromOAuthToken("oauth_token"));
    job->SetClientID("testid");
    job->GetRequest()->mutable_register_request();
    job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                          base::Unretained(this)));
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
    test_server_.reset(
        new LocalPolicyTestServer("device_management_service_browsertest"));
    ASSERT_TRUE(test_server_->Start());
  }

  void RecordToken(DeviceManagementStatus status,
                   int net_error,
                   const em::DeviceManagementResponse& response) {
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
  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _, _))
      .WillOnce(DoAll(
          Invoke(this, &DeviceManagementServiceIntegrationTest::RecordAuthCode),
          InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle)));
  std::unique_ptr<DeviceManagementRequestJob> job(service_->CreateJob(
      DeviceManagementRequestJob::TYPE_API_AUTH_CODE_FETCH, GetFactory()));
  job->SetAuthData(DMAuth::FromDMToken(token_));
  job->SetClientID("testid");
  em::DeviceServiceApiAccessRequest* request =
      job->GetRequest()->mutable_service_api_access_request();
  request->add_auth_scope("authScope4Test");
  request->set_oauth2_client_id("oauth2ClientId4Test");
  job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                        base::Unretained(this)));
  run_loop.Run();
  ASSERT_EQ("fake_auth_code", robot_auth_code_);
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, PolicyFetch) {
  PerformRegistration();

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle));
  std::unique_ptr<DeviceManagementRequestJob> job(service_->CreateJob(
      DeviceManagementRequestJob::TYPE_POLICY_FETCH, GetFactory()));
  job->SetAuthData(DMAuth::FromDMToken(token_));
  job->SetClientID("testid");
  em::DevicePolicyRequest* request =
      job->GetRequest()->mutable_policy_request();
  request->add_request()->set_policy_type(dm_protocol::kChromeUserPolicyType);
  job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                        base::Unretained(this)));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, Unregistration) {
  PerformRegistration();

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle));
  std::unique_ptr<DeviceManagementRequestJob> job(service_->CreateJob(
      DeviceManagementRequestJob::TYPE_UNREGISTRATION, GetFactory()));
  job->SetAuthData(DMAuth::FromDMToken(token_));
  job->SetClientID("testid");
  job->GetRequest()->mutable_unregister_request();
  job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                        base::Unretained(this)));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, AutoEnrollment) {
  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle));
  std::unique_ptr<DeviceManagementRequestJob> job(service_->CreateJob(
      DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT, GetFactory()));
  job->SetAuthData(DMAuth::NoAuth());
  job->SetClientID("testid");
  job->GetRequest()->mutable_auto_enrollment_request()->set_remainder(0);
  job->GetRequest()->mutable_auto_enrollment_request()->set_modulus(1);
  job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                        base::Unretained(this)));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest,
                       AppInstallReport) {
  PerformRegistration();

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _, _))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle));
  std::unique_ptr<DeviceManagementRequestJob> job(service_->CreateJob(
      DeviceManagementRequestJob::TYPE_UPLOAD_APP_INSTALL_REPORT,
      GetFactory()));
  job->SetAuthData(DMAuth::FromDMToken(token_));
  job->SetClientID("testid");
  job->GetRequest()->mutable_app_install_report_request();
  job->Start(base::AdaptCallbackForRepeating(
      base::BindOnce(&DeviceManagementServiceIntegrationTest::OnJobDone,
                     base::Unretained(this))));
  run_loop.Run();
}

INSTANTIATE_TEST_CASE_P(
    DeviceManagementServiceIntegrationTestInstance,
    DeviceManagementServiceIntegrationTest,
    testing::Values(&DeviceManagementServiceIntegrationTest::InitCannedResponse,
                    &DeviceManagementServiceIntegrationTest::InitTestServer));

}  // namespace policy
