// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/encrypted_reporting_job_configuration.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::HasSubstr;
using testing::Invoke;
using testing::SizeIs;
using testing::StartsWith;
using testing::WithArg;

namespace reporting {

struct ReportingServerConnectorTestCase {
  std::string test_name;
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
};

// Test ReportingServerConnector(). Because the function essentially obtains
// cloud_policy_client through a series of linear function calls, it's not
// meaningful to check whether the CloudPolicyClient matches the expectation,
// which would essentially repeat the function itself. Rather, the test focus
// on whether the callback is triggered for the right number of times and on
// the right thread, which are the only addition of the function.
class ReportingServerConnectorTest
    : public ::testing::TestWithParam<ReportingServerConnectorTestCase> {
 protected:
  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    install_attributes_.Get()->SetCloudManaged("fake-domain-name",
                                               "fake-device-id");
#endif
  }

  void VerifyDmTokenHeader() {
    // Verify request header contains dm token
    const net::HttpRequestHeaders& headers =
        test_env_.url_loader_factory()->GetPendingRequest(0)->request.headers;
    ASSERT_TRUE(headers.HasHeader(policy::dm_protocol::kAuthHeader));
    std::string auth_header;
    headers.GetHeader(policy::dm_protocol::kAuthHeader, &auth_header);
    EXPECT_THAT(auth_header, HasSubstr(kFakeDmToken));
  }

  content::BrowserTaskEnvironment task_environment_;

  ReportingServerConnector::TestEnvironment test_env_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ScopedStubInstallAttributes install_attributes_ =
      ash::ScopedStubInstallAttributes();
#endif
};

TEST_P(ReportingServerConnectorTest,
       ExecuteUploadEncryptedReportingOnUIThread) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(GetParam().enabled_features,
                                       GetParam().disabled_features);

  // Call `ReportingServerConnector::UploadEncryptedReport` from the UI.
  test::TestEvent<StatusOr<base::Value::Dict>> response_event;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReportingServerConnector::UploadEncryptedReport,
                     /*merging_payload=*/base::Value::Dict(),
                     response_event.cb()));

  task_environment_.RunUntilIdle();
  ASSERT_THAT(*test_env_.url_loader_factory()->pending_requests(), SizeIs(1));

  VerifyDmTokenHeader();

  test_env_.SimulateResponseForRequest(0);

  EXPECT_OK(response_event.result());
}

TEST_P(ReportingServerConnectorTest,
       ExecuteUploadEncryptedReportingOnArbitraryThread) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(GetParam().enabled_features,
                                       GetParam().disabled_features);

  // Call `ReportingServerConnector::UploadEncryptedReport` from the
  // thread pool.
  test::TestEvent<StatusOr<base::Value::Dict>> response_event;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&ReportingServerConnector::UploadEncryptedReport,
                     /*merging_payload=*/base::Value::Dict(),
                     response_event.cb()));

  task_environment_.RunUntilIdle();
  ASSERT_THAT(*test_env_.url_loader_factory()->pending_requests(), SizeIs(1));

  VerifyDmTokenHeader();

  test_env_.SimulateResponseForRequest(0);

  EXPECT_OK(response_event.result());
}

// This test verifies that we can upload from an unmanaged device when the
// proper features are enabled.
// TODO(b/281905099): remove feature dependencies after roll out.
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(ReportingServerConnectorTest, UploadFromUnmanagedDevice) {
  // Set the device management state to unmanaged.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  install_attributes_.Get()->SetConsumerOwned();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto params = crosapi::mojom::BrowserInitParams::New();
  params->is_device_enterprised_managed = false;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
#endif

  // Enable EnableEncryptedReportingClientForUpload and
  // EnableReportingFromUnmanagedDevices features. Both are required to
  // upload records from an unmanaged device.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{kEnableReportingFromUnmanagedDevices,
                            kEnableEncryptedReportingClientForUpload},
      /*disabled_features=*/{});

  // Call `ReportingServerConnector::UploadEncryptedReport` from the
  // thread pool.
  test::TestEvent<StatusOr<base::Value::Dict>> response_event;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&ReportingServerConnector::UploadEncryptedReport,
                     /*merging_payload=*/base::Value::Dict(),
                     response_event.cb()));

  task_environment_.RunUntilIdle();
  ASSERT_THAT(*test_env_.url_loader_factory()->pending_requests(), SizeIs(1));

  // Verify request header DOES NOT contain a dm token
  const net::HttpRequestHeaders& headers =
      test_env_.url_loader_factory()->GetPendingRequest(0)->request.headers;
  EXPECT_FALSE(headers.HasHeader(policy::dm_protocol::kAuthHeader));

  test_env_.SimulateResponseForRequest(0);

  EXPECT_OK(response_event.result());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

INSTANTIATE_TEST_SUITE_P(
    ReportingServerConnectorTests,
    ReportingServerConnectorTest,
    ::testing::ValuesIn<ReportingServerConnectorTestCase>(
        {{.test_name = "EncryptedReportingClientDisabled",
          .disabled_features = {kEnableEncryptedReportingClientForUpload}},
         {.test_name = "EncryptedReportingClientEnabled",
          .enabled_features = {kEnableEncryptedReportingClientForUpload}}}),
    [](const ::testing::TestParamInfo<ReportingServerConnectorTest::ParamType>&
           info) { return info.param.test_name; });
}  // namespace reporting
