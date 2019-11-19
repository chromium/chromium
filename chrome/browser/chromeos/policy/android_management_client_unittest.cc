// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/android_management_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::SaveArg;
using testing::StrictMock;

namespace em = enterprise_management;

namespace policy {

namespace {

const char kAccountId[] = "fake-account-id";
const char kOAuthToken[] = "fake-oauth-token";

}  // namespace

class AndroidManagementClientTest : public testing::Test {
 protected:
  AndroidManagementClientTest()
      : identity_test_environment_(&url_loader_factory_) {
    android_management_response_.mutable_check_android_management_response();
  }

  // testing::Test:
  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    client_.reset(new AndroidManagementClient(
        &service_, shared_url_loader_factory_, kAccountId,
        identity_test_environment_.identity_manager()));

    service_.ScheduleInitialization(0);
    base::RunLoop().RunUntilIdle();
  }

  // Protobuf is used in successfil responsees.
  em::DeviceManagementResponse android_management_response_;

  base::test::SingleThreadTaskEnvironment task_environment_;
  MockDeviceManagementService service_;
  StrictMock<base::MockCallback<AndroidManagementClient::StatusCallback>>
      callback_observer_;
  std::unique_ptr<AndroidManagementClient> client_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_environment_;
};

TEST_F(AndroidManagementClientTest, CheckAndroidManagementCall) {
  DeviceManagementService::JobConfiguration::JobType job_type;
  DeviceManagementService::JobConfiguration::ParameterMap params;
  EXPECT_CALL(service_, StartJob(_))
      .WillOnce(DoAll(service_.CaptureJobType(&job_type),
                      service_.CaptureQueryParams(&params),
                      service_.StartJobOKAsync(android_management_response_)));
  EXPECT_CALL(callback_observer_,
              Run(AndroidManagementClient::Result::UNMANAGED))
      .Times(1);

  // On ChromeOS platform, account_id and email are same.
  AccountInfo account_info =
      identity_test_environment_.MakeAccountAvailable(kAccountId);

  client_->StartCheckAndroidManagement(callback_observer_.Get());

  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          account_info.account_id, kOAuthToken, base::Time::Max());

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(
      DeviceManagementService::JobConfiguration::TYPE_ANDROID_MANAGEMENT_CHECK,
      job_type);
  ASSERT_LT(params[dm_protocol::kParamDeviceID].size(), 64U);
}

}  // namespace policy
