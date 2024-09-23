// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/cloud/user_policy_signin_service_base.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#else
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#endif

using content::BrowserThread;
using testing::_;
using testing::AnyNumber;
using testing::InvokeWithoutArgs;
using testing::Mock;

namespace em = enterprise_management;

namespace policy {

namespace {

// Parses the upload data in |request| into |request_msg|, and validates the
// request. The query string in the URL must contain the |expected_type| for
// the "request" parameter. Returns true if all checks succeeded, and the
// request data has been parsed into |request_msg|.
bool ValidRequest(const std::string& method,
                  const GURL& url,
                  const std::string& data,
                  const std::string& expected_type,
                  em::DeviceManagementRequest* request_msg) {
  if (method != "POST")
    return false;
  std::string spec = url.spec();
  if (spec.find("request=" + expected_type) == std::string::npos)
    return false;

  if (!request_msg->ParseFromString(data))
    return false;

  return true;
}

void RespondWithBadResponse(const network::ResourceRequest& request,
                            network::TestURLLoaderFactory* factory) {
  network::URLLoaderCompletionStatus status;
  factory->AddResponse(
      request.url, network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));
}

void RespondToRegisterWithSuccess(em::DeviceRegisterRequest::Type expected_type,
                                  bool expect_reregister,
                                  const network::ResourceRequest& request,
                                  network::TestURLLoaderFactory* factory) {
  em::DeviceManagementRequest request_msg;
  if (!ValidRequest(request.method, request.url,
                    network::GetUploadData(request), "register",
                    &request_msg)) {
    RespondWithBadResponse(request, factory);
    return;
  }

  if (!request_msg.has_register_request() ||
      request_msg.has_unregister_request() ||
      request_msg.has_policy_request() ||
      request_msg.has_device_status_report_request() ||
      request_msg.has_session_status_report_request() ||
      request_msg.has_auto_enrollment_request()) {
    RespondWithBadResponse(request, factory);
    return;
  }

  const em::DeviceRegisterRequest& register_request =
      request_msg.register_request();
  if (expect_reregister &&
      (!register_request.has_reregister() || !register_request.reregister())) {
    RespondWithBadResponse(request, factory);
    return;
  } else if (!expect_reregister && register_request.has_reregister() &&
             register_request.reregister()) {
    RespondWithBadResponse(request, factory);
    return;
  }

  if (!register_request.has_type() ||
      register_request.type() != expected_type) {
    RespondWithBadResponse(request, factory);
    return;
  }

  std::string content;
  network::URLLoaderCompletionStatus status;

  em::DeviceManagementResponse response;
  em::DeviceRegisterResponse* register_response =
      response.mutable_register_response();
  register_response->set_device_management_token("s3cr3t70k3n");
  response.SerializeToString(&content);

  status.decoded_body_length = content.size();

  auto head = network::CreateURLResponseHead(net::HTTP_OK);
  head->mime_type = "application/protobuf";

  factory->AddResponse(request.url, std::move(head), content, status);
}

}  // namespace

// Tests the cloud policy stack using a URLRequestJobFactory::ProtocolHandler
// to intercept requests and produce canned responses.
class CloudPolicyManagerTest : public PlatformBrowserTest {
 protected:
  CloudPolicyManagerTest() {}
  ~CloudPolicyManagerTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl,
                                    "http://localhost");
    ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();

    // Set retry delay to prevent timeouts.
    policy::DeviceManagementService::SetRetryDelayForTesting(0);
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(PolicyServiceIsEmpty(g_browser_process->policy_service()))
        << "Pre-existing policies in this machine will make this test fail.";

    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    base::FilePath dest_path =
        g_browser_process->profile_manager()->user_data_dir();
    profile_ = Profile::CreateProfile(
        dest_path.Append(FILE_PATH_LITERAL("New Profile 1")), nullptr,
        Profile::CreateMode::kSynchronous);
#endif

    BrowserPolicyConnector* connector =
        g_browser_process->browser_policy_connector();
    connector->ScheduleServiceInitialization(0);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    policy_manager()->core()->client()->SetURLLoaderFactoryForTesting(
        test_url_loader_factory_->GetSafeWeakWrapper());
#else
    // Mock a signed-in user. This is used by the UserCloudPolicyStore to pass
    // the username to the UserCloudPolicyValidator.
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    identity_test_env_->MakePrimaryAccountAvailable(
        "user@example.com", signin::ConsentLevel::kSync);

    ASSERT_TRUE(policy_manager());
    policy_manager()->Connect(
        g_browser_process->local_state(),
        std::make_unique<CloudPolicyClient>(
            connector->device_management_service(),
            test_url_loader_factory_->GetSafeWeakWrapper()));
#endif
  }

  void TearDownOnMainThread() override {
    // Verify that all the expected requests were handled.
    EXPECT_EQ(0, test_url_loader_factory_->NumPending());
    identity_test_env_.reset();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_.reset();
#endif
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  UserCloudPolicyManagerAsh* policy_manager() {
    return chrome_test_utils::GetProfile(this)->GetUserCloudPolicyManagerAsh();
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  UserCloudPolicyManager* policy_manager() {
    return profile_->GetUserCloudPolicyManager();
  }
#else
  UserCloudPolicyManager* policy_manager() {
    return chrome_test_utils::GetProfile(this)->GetUserCloudPolicyManager();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Register the client of the policy_manager() using a bogus auth token, and
  // returns once the registration gets a result back.
  void Register() {
    ASSERT_TRUE(policy_manager());
    ASSERT_TRUE(policy_manager()->core()->client());

    base::RunLoop run_loop;
    MockCloudPolicyClientObserver observer;
    EXPECT_CALL(observer, OnRegistrationStateChanged(_))
        .Times(AnyNumber())
        .WillRepeatedly(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    EXPECT_CALL(observer, OnClientError(_))
        .Times(AnyNumber())
        .WillRepeatedly(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    policy_manager()->core()->client()->AddObserver(&observer);

    // Give a bogus OAuth token to the |policy_manager|. This should make its
    // CloudPolicyClient fetch the DMToken.
    CloudPolicyClient::RegistrationParameters parameters(
#if BUILDFLAG(IS_CHROMEOS_ASH)
        em::DeviceRegisterRequest::USER,
#else
        em::DeviceRegisterRequest::BROWSER,
#endif
        em::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
    policy_manager()->core()->client()->Register(
        parameters, std::string() /* client_id */,
        "oauth_token_unused" /* oauth_token */);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(&observer);
    policy_manager()->core()->client()->RemoveObserver(&observer);
  }

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // For Lacros use non-main profile in these tests.
  std::unique_ptr<Profile> profile_;
#endif
};

IN_PROC_BROWSER_TEST_F(CloudPolicyManagerTest, Register) {
  test_url_loader_factory_->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        // Accept one register request. The initial request should not include
        // the reregister flag.
        em::DeviceRegisterRequest::Type expected_type =
#if BUILDFLAG(IS_CHROMEOS_ASH)
            em::DeviceRegisterRequest::USER;
#else
            em::DeviceRegisterRequest::BROWSER;
#endif
        RespondToRegisterWithSuccess(expected_type, /*expect_reregister=*/false,
                                     request, test_url_loader_factory_.get());
      }));

  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
  ASSERT_NO_FATAL_FAILURE(Register());
  EXPECT_TRUE(policy_manager()->core()->client()->is_registered());
}

IN_PROC_BROWSER_TEST_F(CloudPolicyManagerTest, RegisterFails) {
  test_url_loader_factory_->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        test_url_loader_factory_->AddResponse(request.url.spec(), std::string(),
                                              net::HTTP_BAD_REQUEST);
      }));

  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
  ASSERT_NO_FATAL_FAILURE(Register());
  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
}

IN_PROC_BROWSER_TEST_F(CloudPolicyManagerTest, RegisterFailsWithRetries) {
  // Fail 4 times with ERR_NETWORK_CHANGED; the first 3 will trigger a retry,
  // the last one will forward the error to the client and unblock the
  // register process.
  int count = 0;
  test_url_loader_factory_->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        network::URLLoaderCompletionStatus status(net::ERR_NETWORK_CHANGED);
        test_url_loader_factory_->AddResponse(
            request.url, network::mojom::URLResponseHead::New(), std::string(),
            status);
        ++count;
      }));

  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
  ASSERT_NO_FATAL_FAILURE(Register());
  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
  EXPECT_EQ(4, count);
}

IN_PROC_BROWSER_TEST_F(CloudPolicyManagerTest, RegisterWithRetry) {
  test_url_loader_factory_->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        em::DeviceRegisterRequest::Type expected_type =
#if BUILDFLAG(IS_CHROMEOS_ASH)
            em::DeviceRegisterRequest::USER;
#else
            em::DeviceRegisterRequest::BROWSER;
#endif

        // Accept one register request after failing once. The retry request
        // should set the reregister flag.
        static bool gave_error = false;
        if (!gave_error) {
          gave_error = true;
          network::URLLoaderCompletionStatus status(net::ERR_NETWORK_CHANGED);
          test_url_loader_factory_->AddResponse(
              request.url, network::mojom::URLResponseHead::New(),
              std::string(), status);
          return;
        }

        RespondToRegisterWithSuccess(expected_type, /*expect_reregister=*/true,
                                     request, test_url_loader_factory_.get());
      }));

  EXPECT_FALSE(policy_manager()->core()->client()->is_registered());
  ASSERT_NO_FATAL_FAILURE(Register());
  EXPECT_TRUE(policy_manager()->core()->client()->is_registered());
}

}  // namespace policy
