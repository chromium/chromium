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

#if BUILDFLAG(IS_CHROMEOS)
#include "base/test/scoped_feature_list.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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

// Test ReportingServerConnector(). Because the function essentially obtains
// cloud_policy_client through a series of linear function calls, it's not
// meaningful to check whether the CloudPolicyClient matches the expectation,
// which would essentially repeat the function itself. Rather, the test focus
// on whether the callback is triggered for the right number of times and on
// the right thread, which are the only addition of the function.
class ReportingServerConnectorTest : public ::testing::Test {
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

TEST_F(ReportingServerConnectorTest,
       ExecuteUploadEncryptedReportingOnUIThread) {
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

  EXPECT_TRUE(response_event.result().has_value());
}

TEST_F(ReportingServerConnectorTest,
       ExecuteUploadEncryptedReportingOnArbitraryThread) {
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

  EXPECT_TRUE(response_event.result().has_value());
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

  // Enable EnableReportingFromUnmanagedDevices feature. Required to
  // upload records from an unmanaged device.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{kEnableReportingFromUnmanagedDevices},
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

  EXPECT_TRUE(response_event.result().has_value());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace reporting
