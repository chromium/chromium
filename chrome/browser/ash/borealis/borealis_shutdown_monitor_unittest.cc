// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_shutdown_monitor.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/borealis/borealis_context_manager_mock.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

class BorealisShutdownMonitorTest : public testing::Test {
 protected:
  BorealisShutdownMonitorTest()
      : service_fake_(BorealisServiceFake::UseFakeForTesting(&profile_)),
        features_(&profile_) {
    borealis_window_manager_ =
        std::make_unique<BorealisWindowManager>(&profile_);

    service_fake_->SetFeaturesForTesting(&features_);
    service_fake_->SetContextManagerForTesting(&context_manager_mock_);
    service_fake_->SetWindowManagerForTesting(borealis_window_manager_.get());
  }

  Profile* profile() { return &profile_; }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<BorealisWindowManager> borealis_window_manager_;
  raw_ptr<BorealisServiceFake> service_fake_;
  BorealisFeatures features_;
  testing::StrictMock<BorealisContextManagerMock> context_manager_mock_;
};

TEST_F(BorealisShutdownMonitorTest, CanShutdownImmediately) {
  BorealisShutdownMonitor monitor(profile());

  EXPECT_CALL(context_manager_mock_, ShutDownBorealis(testing::_));
  monitor.ShutdownNow();
}

TEST_F(BorealisShutdownMonitorTest, CanShutdownWithDelay) {
  BorealisShutdownMonitor monitor(profile());

  monitor.SetShutdownDelayForTesting(base::Seconds(0));
  monitor.ShutdownWithDelay();

  EXPECT_CALL(context_manager_mock_, ShutDownBorealis(testing::_));
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisShutdownMonitorTest, CancelDelayedShutdownPreventsIt) {
  BorealisShutdownMonitor monitor(profile());

  EXPECT_CALL(context_manager_mock_, ShutDownBorealis(testing::_)).Times(0);

  monitor.SetShutdownDelayForTesting(base::Seconds(0));
  monitor.ShutdownWithDelay();

  monitor.CancelDelayedShutdown();
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisShutdownMonitorTest, LaterShutdownOverridesEarlier) {
  BorealisShutdownMonitor monitor(profile());

  EXPECT_CALL(context_manager_mock_, ShutDownBorealis(testing::_)).Times(0);

  monitor.SetShutdownDelayForTesting(base::Seconds(0));
  monitor.ShutdownWithDelay();

  // I'm assuming this thread won't be idle for 99 seconds.
  monitor.SetShutdownDelayForTesting(base::Seconds(99));
  monitor.ShutdownWithDelay();

  task_environment_.RunUntilIdle();
}

TEST_F(BorealisShutdownMonitorTest, DeletingMonitorCancelsShutdowns) {
  auto monitor = std::make_unique<BorealisShutdownMonitor>(profile());

  EXPECT_CALL(context_manager_mock_, ShutDownBorealis(testing::_)).Times(0);

  monitor->SetShutdownDelayForTesting(base::Seconds(0));
  monitor->ShutdownWithDelay();
  monitor.reset();

  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace borealis
