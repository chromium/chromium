// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"

#include <cstddef>
#include <memory>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/device_management_service_configuration.h"
#include "chrome/browser/policy/messaging_layer/upload/encrypted_reporting_client.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::Invoke;
using testing::SizeIs;
using testing::StartsWith;
using testing::WithArg;

namespace reporting {

constexpr char kServerUrl[] = "https://example.com/reporting";

class FakeEncryptedReportingClientDelegate
    : public EncryptedReportingClient::Delegate {
 public:
  explicit FakeEncryptedReportingClientDelegate(
      std::unique_ptr<policy::DeviceManagementServiceConfiguration> config)
      : device_management_service_(
            std::make_unique<policy::DeviceManagementService>(
                std::move(config))) {
    device_management_service_->ScheduleInitialization(0);
  }

  FakeEncryptedReportingClientDelegate(
      const FakeEncryptedReportingClientDelegate&) = delete;
  FakeEncryptedReportingClientDelegate& operator=(
      const FakeEncryptedReportingClientDelegate&) = delete;

  ~FakeEncryptedReportingClientDelegate() override = default;

  policy::DeviceManagementService* device_management_service() const override {
    EXPECT_TRUE(
        ::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI));
    return device_management_service_.get();
  }

 private:
  const std::unique_ptr<policy::DeviceManagementService>
      device_management_service_;
};

void CloudPolicyClientUpload(
    ::policy::CloudPolicyClient::ResponseCallback callback) {
  // Callback is expected to be called exactly once on UI task runner,
  // regardless of the launching thread.
  ASSERT_TRUE(
      ::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI));
  std::move(callback).Run(base::Value::Dict());
}

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
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kSerialNumberKeyForTest, "fake-serial-number");
#endif
    // Prepare to respond to `ReportingServerConnector::UploadEncryptedReport`.
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        url_loader_factory_.GetSafeWeakWrapper());
    auto config =
        std::make_unique<policy::DeviceManagementServiceConfiguration>(
            /*dm_server_url=*/"", /*realtime_reporting_server_url=*/"",
            /*encrypted_reporting_server_url=*/kServerUrl);
    auto fake_delegate = std::make_unique<FakeEncryptedReportingClientDelegate>(
        std::move(config));
    test_env_.SetEncryptedReportingClient(
        std::make_unique<EncryptedReportingClient>(std::move(fake_delegate)));
  }
  content::BrowserTaskEnvironment task_environment_;

  ReportingServerConnector::TestEnvironment test_env_;

  network::TestURLLoaderFactory url_loader_factory_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif
};

TEST_F(ReportingServerConnectorTest,
       ExecuteUploadEncryptedReportingOnUIThread) {
  // EnableEncryptedReportingClientForUpload is disabled by default,
  // `policy::CloudPolicyClient` will be used for upload.
  EXPECT_CALL(*test_env_.client(), UploadEncryptedReport(_, _, _))
      .WillOnce(WithArg<2>(Invoke(CloudPolicyClientUpload)));

  // Call `ReportingServerConnector::UploadEncryptedReport` from the UI.
  test::TestEvent<StatusOr<base::Value::Dict>> response_event;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReportingServerConnector::UploadEncryptedReport,
                     /*merging_payload=*/base::Value::Dict(),
                     response_event.cb()));

  task_environment_.RunUntilIdle();
  EXPECT_TRUE(url_loader_factory_.pending_requests()->empty());

  EXPECT_OK(response_event.result());
}

TEST_F(ReportingServerConnectorTest,
       ExecuteUploadEncryptedReportingOnArbitraryThread) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnableEncryptedReportingClientForUpload);

  // EnableEncryptedReportingClientForUpload is explicitly disabled,
  // `policy::CloudPolicyClient` will be used for upload.
  EXPECT_CALL(*test_env_.client(), UploadEncryptedReport(_, _, _))
      .WillOnce(WithArg<2>(Invoke(CloudPolicyClientUpload)));

  // Call `ReportingServerConnector::UploadEncryptedReport` from the thread
  // pool.
  test::TestEvent<StatusOr<base::Value::Dict>> response_event;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&ReportingServerConnector::UploadEncryptedReport,
                     /*merging_payload=*/base::Value::Dict(),
                     response_event.cb()));

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(url_loader_factory_.pending_requests()->empty());

  EXPECT_OK(response_event.result());
}

TEST_F(ReportingServerConnectorTest, EncryptedReportingClientForUploadEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kEnableEncryptedReportingClientForUpload);

  // EnableEncryptedReportingClientForUpload is enabled,
  // `EncryptedReportingClient` will be used for upload.
  EXPECT_CALL(*test_env_.client(), UploadEncryptedReport(_, _, _)).Times(0);

  // Call `ReportingServerConnector::UploadEncryptedReport` from the thread
  // pool.
  test::TestEvent<StatusOr<base::Value::Dict>> response_event;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&ReportingServerConnector::UploadEncryptedReport,
                     /*merging_payload=*/base::Value::Dict(),
                     response_event.cb()));

  task_environment_.RunUntilIdle();
  ASSERT_THAT(*url_loader_factory_.pending_requests(), SizeIs(1));

  const std::string& pending_request_url =
      (*url_loader_factory_.pending_requests())[0].request.url.spec();

  EXPECT_THAT(pending_request_url, StartsWith(kServerUrl));

  url_loader_factory_.SimulateResponseForPendingRequest(pending_request_url,
                                                        R"({"key": "value"})");

  EXPECT_OK(response_event.result());
}
}  // namespace reporting
