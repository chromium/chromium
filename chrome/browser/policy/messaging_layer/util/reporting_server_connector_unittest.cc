// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"

#include <cstddef>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::WithArg;

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
    // Prepare to respond to `ReportingServerConnector::UploadEncryptedReport`.
    // Callback is expected to be called exactly once on UI task runner,
    // regardless of the launching thread.
    EXPECT_CALL(mock_client_, UploadEncryptedReport(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([](::policy::CloudPolicyClient::ResponseCallback callback) {
              ASSERT_TRUE(::content::BrowserThread::CurrentlyOn(
                  ::content::BrowserThread::UI));
              std::move(callback).Run(base::Value::Dict());
            })));
  }
  content::BrowserTaskEnvironment task_environment_;

  policy::MockCloudPolicyClient mock_client_;
  ReportingServerConnector::TestEnvironment test_env_{&mock_client_};
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
  EXPECT_OK(response_event.result());
}

TEST_F(ReportingServerConnectorTest,
       ExecuteUploadEncryptedReportingOnArbitraryThread) {
  // Call `ReportingServerConnector::UploadEncryptedReport` from the thread
  // pool.
  test::TestEvent<StatusOr<base::Value::Dict>> response_event;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&ReportingServerConnector::UploadEncryptedReport,
                     /*merging_payload=*/base::Value::Dict(),
                     response_event.cb()));
  EXPECT_OK(response_event.result());
}
}  // namespace reporting
