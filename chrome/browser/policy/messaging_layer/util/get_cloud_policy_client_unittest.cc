// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

// The test is disabled for Android. The function is currently not being used on
// Android. We should enable this test when Android becomes supported.
#if !BUILDFLAG(IS_ANDROID)

#include "chrome/browser/policy/messaging_layer/util/get_cloud_policy_client.h"

#include <atomic>
#include <cstddef>

#include "base/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/reporting/util/statusor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/tpm/stub_install_attributes.h"
#endif

using ::testing::_;

namespace reporting {

// Test GetCloudPolicyClient(). Because the function essentially obtains
// cloud_policy_client through a series of linear function calls, it's not
// meaningful to check whether the CloudPolicyClient matches the expectation,
// which would essentially repeat the function itself. Rather, the test focus
// on whether the callback is triggered for the right number of times and on
// the right thread, which are the only addition of the function.
class GetCloudPolicyClientTest : public ::testing::TestWithParam<bool> {
 public:
  // The Callback function passed to GetCloudPolicyClient in tests. We do not
  // use MOCK_METHOD/EXPECT_CALL here because they are not thread safe. Instead,
  // count called_count_ in the test body.
  void Callback(StatusOr<policy::CloudPolicyClient*> client) {
    // Because we haven't set up the test environment, client can be
    // either OK or error. Ensure this is called on the UI thread.
    ASSERT_TRUE(
        content::GetUIThreadTaskRunner({})->RunsTasksInCurrentSequence())
        << "Callback is not called on the UI thread.";
    ++called_count_;
  }

  // Should GetCloudPolicyClient called on the UI thread in this test case?
  bool called_on_ui_thread() const { return GetParam(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  // Number of times callback being called in a test.
  std::atomic<uint64_t> called_count_{0};
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Without setting up install attributes, browser_policy_connector_ash()
  // call inside GetCloudPolicyClient will fail a check.
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_{
      ash::StubInstallAttributes::CreateCloudManaged("test-domain",
                                                     "FAKE-DEVICE-ID")};
#endif
};

TEST_P(GetCloudPolicyClientTest, CallbackThreadAndCount) {
  scoped_refptr<base::TaskRunner> task_runner;
  if (called_on_ui_thread()) {
    task_runner = content::GetUIThreadTaskRunner({});
  } else {
    task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  }

  // Call GetCloudPolicyClient from the designated sequence
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](GetCloudPolicyClientTest* test) {
            // Sanity check: the posting thread is or is not the UI
            // thread
            ASSERT_EQ(content::GetUIThreadTaskRunner({})
                          ->RunsTasksInCurrentSequence(),
                      test->called_on_ui_thread())
                << "In this test case, GetCloudPolicyClient is intended to be "
                   "called "
                << (test->called_on_ui_thread() ? "" : "not ")
                << "on the UI thread but the opposite is true.";
            GetCloudPolicyClientCb().Run(base::BindOnce(
                &GetCloudPolicyClientTest::Callback, base::Unretained(test)));
          },
          base::Unretained(this)));
  task_environment_.RunUntilIdle();

  // Callback is called exactly once
  ASSERT_EQ(called_count_, 1U)
      << "Callback is called " << called_count_
      << " times, but it should be called exactly once.";
}

INSTANTIATE_TEST_SUITE_P(CalledOnOrNotOnUIThread,
                         GetCloudPolicyClientTest,
                         testing::Bool());

}  // namespace reporting

#endif  // !BUILDFLAG(IS_ANDROID)
