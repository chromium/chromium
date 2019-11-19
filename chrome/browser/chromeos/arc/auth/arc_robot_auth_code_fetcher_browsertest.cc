// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/auth/arc_robot_auth_code_fetcher.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/arc/arc_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr char kFakeUserName[] = "test@example.com";
constexpr char kFakeAuthCode[] = "fake-auth-code";

void ResponseJob(const network::ResourceRequest& request,
                 network::TestURLLoaderFactory* factory) {
  std::string request_body = network::GetUploadData(request);
  enterprise_management::DeviceManagementRequest parsed_request;
  EXPECT_TRUE(
      parsed_request.ParseFromArray(request_body.c_str(), request_body.size()));
  // Check if auth code is requested.
  EXPECT_TRUE(parsed_request.has_service_api_access_request());

  enterprise_management::DeviceManagementResponse response;
  response.mutable_service_api_access_response()->set_auth_code(kFakeAuthCode);

  std::string response_data;
  EXPECT_TRUE(response.SerializeToString(&response_data));

  factory->AddResponse(request.url.spec(), response_data);
}

}  // namespace

class ArcRobotAuthCodeFetcherBrowserTest : public InProcessBrowserTest {
 protected:
  // Test configuration for whether to set up the CloudPolicyClient connection.
  // By default, the test sets up the connection.
  enum class CloudPolicyClientSetup {
    kConnect = 0,
    kSkip = 1,
  };

  explicit ArcRobotAuthCodeFetcherBrowserTest(
      CloudPolicyClientSetup cloud_policy_client_setup =
          CloudPolicyClientSetup::kConnect)
      : cloud_policy_client_setup_(cloud_policy_client_setup) {}

  ~ArcRobotAuthCodeFetcherBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    "http://localhost");
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());

    const AccountId account_id(AccountId::FromUserEmail(kFakeUserName));
    GetFakeUserManager()->AddArcKioskAppUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    if (cloud_policy_client_setup_ == CloudPolicyClientSetup::kSkip)
      return;

    policy::BrowserPolicyConnectorChromeOS* const connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    policy::DeviceCloudPolicyManagerChromeOS* const cloud_policy_manager =
        connector->GetDeviceCloudPolicyManager();

    cloud_policy_manager->StartConnection(
        std::make_unique<policy::MockCloudPolicyClient>(),
        connector->GetInstallAttributes());

    policy::MockCloudPolicyClient* const cloud_policy_client =
        static_cast<policy::MockCloudPolicyClient*>(
            cloud_policy_manager->core()->client());
    cloud_policy_client->SetDMToken("fake-dm-token");
    cloud_policy_client->client_id_ = "client-id";
  }

  void TearDownOnMainThread() override { user_manager_enabler_.reset(); }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void FetchAuthCode(ArcRobotAuthCodeFetcher* fetcher,
                     bool* output_fetch_success,
                     std::string* output_auth_code) {
    base::RunLoop run_loop;
    fetcher->SetURLLoaderFactoryForTesting(
        test_url_loader_factory_.GetSafeWeakWrapper());
    fetcher->Fetch(base::Bind(
        [](bool* output_fetch_success, std::string* output_auth_code,
           base::RunLoop* run_loop, bool fetch_success,
           const std::string& auth_code) {
          *output_fetch_success = fetch_success;
          *output_auth_code = auth_code;
          run_loop->Quit();
        },
        output_fetch_success, output_auth_code, &run_loop));
    // Because the Fetch() operation needs to interact with other threads,
    // RunUntilIdle() won't work here. Instead, use Run() and Quit() explicitly
    // in the callback.
    run_loop.Run();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  // Whether to connect the CloudPolicyClient.
  CloudPolicyClientSetup cloud_policy_client_setup_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(ArcRobotAuthCodeFetcherBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ArcRobotAuthCodeFetcherBrowserTest,
                       RequestAccountInfoSuccess) {
  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        ResponseJob(request, test_url_loader_factory());
      }));

  std::string auth_code;
  bool fetch_success = false;

  auto robot_fetcher = std::make_unique<ArcRobotAuthCodeFetcher>();
  FetchAuthCode(robot_fetcher.get(), &fetch_success, &auth_code);

  EXPECT_TRUE(fetch_success);
  EXPECT_EQ(kFakeAuthCode, auth_code);
}

IN_PROC_BROWSER_TEST_F(ArcRobotAuthCodeFetcherBrowserTest,
                       RequestAccountInfoError) {
  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        test_url_loader_factory()->AddResponse(
            request.url.spec(), std::string(), net::HTTP_BAD_REQUEST);
      }));

  // We expect auth_code is empty in this case. So initialize with non-empty
  // value.
  std::string auth_code = "NOT-YET-FETCHED";
  bool fetch_success = true;

  auto robot_fetcher = std::make_unique<ArcRobotAuthCodeFetcher>();
  FetchAuthCode(robot_fetcher.get(), &fetch_success, &auth_code);

  EXPECT_FALSE(fetch_success);
  // Use EXPECT_EQ for better logging in case of failure.
  EXPECT_EQ(std::string(), auth_code);
}

class ArcRobotAuthCodeFetcherOfflineBrowserTest
    : public ArcRobotAuthCodeFetcherBrowserTest {
 protected:
  ArcRobotAuthCodeFetcherOfflineBrowserTest()
      : ArcRobotAuthCodeFetcherBrowserTest(CloudPolicyClientSetup::kSkip) {}

  ~ArcRobotAuthCodeFetcherOfflineBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcRobotAuthCodeFetcherOfflineBrowserTest);
};

// Tests that the fetch fails when CloudPolicyClient has not been set up yet.
IN_PROC_BROWSER_TEST_F(ArcRobotAuthCodeFetcherOfflineBrowserTest,
                       RequestAccountInfo) {
  // We expect auth_code is empty in this case. So initialize with non-empty
  // value.
  std::string auth_code = "NOT-YET-FETCHED";
  bool fetch_success = true;

  auto robot_fetcher = std::make_unique<ArcRobotAuthCodeFetcher>();
  FetchAuthCode(robot_fetcher.get(), &fetch_success, &auth_code);

  EXPECT_FALSE(fetch_success);
  // Use EXPECT_EQ for better logging in case of failure.
  EXPECT_EQ(std::string(), auth_code);
}

}  // namespace arc
