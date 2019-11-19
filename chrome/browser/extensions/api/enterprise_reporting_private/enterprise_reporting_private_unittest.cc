// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/policy/fake_browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace extensions {
namespace {

const char kFakeDMToken[] = "fake-dm-token";
const char kFakeClientId[] = "fake-client-id";
const char kFakeMachineNameReport[] = "{\"computername\":\"name\"}";

}  // namespace

// Test for API enterprise.reportingPrivate.uploadChromeDesktopReport
class EnterpriseReportingPrivateUploadChromeDesktopReportTest
    : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateUploadChromeDesktopReportTest() {}

  ExtensionFunction* CreateChromeDesktopReportingFunction(
      const std::string& dm_token) {
    EnterpriseReportingPrivateUploadChromeDesktopReportFunction* function =
        EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
            CreateForTesting(test_url_loader_factory_.GetSafeWeakWrapper());
    auto client = std::make_unique<policy::MockCloudPolicyClient>(
        test_url_loader_factory_.GetSafeWeakWrapper());
    client_ = client.get();
    function->SetCloudPolicyClientForTesting(std::move(client));
    if (dm_token.empty()) {
      function->SetRegistrationInfoForTesting(
          policy::DMToken::CreateEmptyTokenForTesting(), kFakeClientId);
    } else {
      function->SetRegistrationInfoForTesting(
          policy::DMToken::CreateValidTokenForTesting(dm_token), kFakeClientId);
    }
    return function;
  }

  std::string GenerateArgs(const char* name) {
    return base::StringPrintf("[{\"machineName\":%s}]", name);
  }

  std::string GenerateInvalidReport() {
    // This report is invalid as the chromeSignInUser dictionary should not be
    // wrapped in a list.
    return std::string(
        "[{\"browserReport\": "
        "{\"chromeUserProfileReport\":[{\"chromeSignInUser\":\"Name\"}]}}]");
  }

  policy::MockCloudPolicyClient* client_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(
      EnterpriseReportingPrivateUploadChromeDesktopReportTest);
};

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest,
       DeviceIsNotEnrolled) {
  ASSERT_EQ(enterprise_reporting::kDeviceNotEnrolled,
            RunFunctionAndReturnError(
                CreateChromeDesktopReportingFunction(std::string()),
                GenerateArgs(kFakeMachineNameReport)));
}

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest,
       ReportIsNotValid) {
  ASSERT_EQ(enterprise_reporting::kInvalidInputErrorMessage,
            RunFunctionAndReturnError(
                CreateChromeDesktopReportingFunction(kFakeDMToken),
                GenerateInvalidReport()));
}

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest, UploadFailed) {
  ExtensionFunction* function =
      CreateChromeDesktopReportingFunction(kFakeDMToken);
  EXPECT_CALL(*client_, SetupRegistration(kFakeDMToken, kFakeClientId, _))
      .Times(1);
  EXPECT_CALL(*client_, UploadChromeDesktopReportProxy(_, _))
      .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(false)));
  ASSERT_EQ(enterprise_reporting::kUploadFailed,
            RunFunctionAndReturnError(function,
                                      GenerateArgs(kFakeMachineNameReport)));
  ::testing::Mock::VerifyAndClearExpectations(client_);
}

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest,
       UploadSucceeded) {
  ExtensionFunction* function =
      CreateChromeDesktopReportingFunction(kFakeDMToken);
  EXPECT_CALL(*client_, SetupRegistration(kFakeDMToken, kFakeClientId, _))
      .Times(1);
  EXPECT_CALL(*client_, UploadChromeDesktopReportProxy(_, _))
      .WillOnce(WithArgs<1>(policy::ScheduleStatusCallback(true)));
  ASSERT_EQ(nullptr, RunFunctionAndReturnValue(
                         function, GenerateArgs(kFakeMachineNameReport)));
  ::testing::Mock::VerifyAndClearExpectations(client_);
}

// Test for API enterprise.reportingPrivate.getDeviceId
class EnterpriseReportingPrivateGetDeviceIdTest : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateGetDeviceIdTest() = default;

  void SetClientId(const std::string& client_id) {
    storage_.SetClientId(client_id);
  }

 private:
  policy::FakeBrowserDMTokenStorage storage_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseReportingPrivateGetDeviceIdTest);
};

TEST_F(EnterpriseReportingPrivateGetDeviceIdTest, GetDeviceId) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceIdFunction>();
  SetClientId(kFakeClientId);
  std::unique_ptr<base::Value> id =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(id);
  ASSERT_TRUE(id->is_string());
  EXPECT_EQ(kFakeClientId, id->GetString());
}

TEST_F(EnterpriseReportingPrivateGetDeviceIdTest, DeviceIdNotExist) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceIdFunction>();
  SetClientId("");
  ASSERT_EQ(enterprise_reporting::kDeviceIdNotFound,
            RunFunctionAndReturnError(function.get(), "[]"));
}

}  // namespace extensions
