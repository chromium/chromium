// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"

#include <limits>
#include <map>
#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {

class NetworkConditionServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    UpdateDownstreamThroughputKbps(kInitDownstreamThroughput);
  }

  // Update downstream throughput and clear UI thread task queue.
  void UpdateDownstreamThroughputKbps(int32_t downstream_throughput) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(
                       [](int32_t downstream_throughput) {
                         g_browser_process->network_quality_tracker()
                             ->ReportRTTsAndThroughputForTesting(
                                 base::Milliseconds(100),
                                 downstream_throughput);
                       },
                       downstream_throughput));
    task_environment_.RunUntilIdle();
  }

  static constexpr int32_t kInitDownstreamThroughput = 100;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(NetworkConditionServiceTest,
       SuccessfulInitializationAndUpdateAndDestroy) {
  {
    NetworkConditionService network_condition_service;
    // The UI thread task to subscribe to
    // g_browser_process->network_quality_tracker() should have been posted by
    // the constructor, but hasn't been executed. The current upload_rate_
    // should be max.
    ASSERT_EQ(network_condition_service.GetUploadRate(),
              std::numeric_limits<uint64_t>::max())
        << "network_condition_service->GetUploadRate() must be "
           "std::numeric_limits<uint64_t>::max() before the UI thread has "
           "executed the subscription task.";

    // Execute the subscription task (queued by the constructor of
    // NetworkConditionService) on the UI thread.
    task_environment_.RunUntilIdle();
    // After subscription, we should be on the initial download throughput
    // because the current download throughput is assigned to upload_rate_ right
    // before subscription.
    ASSERT_EQ(network_condition_service.GetUploadRate(),
              NetworkConditionService::NetworkConditionServiceImpl::
                  ConvertKbpsToBytesPerSec(kInitDownstreamThroughput))
        << "network_condition_service->GetUploadRate() must be set to the "
           "initial downstream throughput after the UI thread has executed the "
           "subscription task.";

    // g_browser_process->network_quality_tracker() notifies
    // network_condition_service about a change in download speed. We should
    // have the new estimated upload rate.
    static constexpr int32_t new_downstream_throughput = 200;
    UpdateDownstreamThroughputKbps(new_downstream_throughput);
    ASSERT_EQ(network_condition_service.GetUploadRate(),
              NetworkConditionService::NetworkConditionServiceImpl::
                  ConvertKbpsToBytesPerSec(new_downstream_throughput))
        << "network_condition_service->GetUploadRate() must be set to the new "
           "downstream throughput after "
           "g_browser_process->network_quality_tracker() notifies.";

    // Clear the queue in case there are leftovers
    task_environment_.RunUntilIdle();
    ASSERT_EQ(task_environment_.GetPendingMainThreadTaskCount(),
              static_cast<size_t>(0));
  }  // network_condition_service destructs here

  // THe task to destroy the observer should be enqueued by the destructor of
  // network_condition_service
  ASSERT_EQ(task_environment_.GetPendingMainThreadTaskCount(),
            static_cast<size_t>(1));
  // Execute the unsubscription task.
  task_environment_.RunUntilIdle();
}

}  // namespace reporting
