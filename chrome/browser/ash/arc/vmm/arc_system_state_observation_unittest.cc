// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_system_state_observation.h"

#include "chrome/browser/ash/arc/idle_manager/arc_background_service_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_active_window_throttle_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcSystemStateObservationTest : public testing::Test {
 public:
  ArcSystemStateObservationTest() = default;

  ArcSystemStateObservationTest(const ArcSystemStateObservationTest&) = delete;
  ArcSystemStateObservationTest& operator=(
      const ArcSystemStateObservationTest&) = delete;

  ~ArcSystemStateObservationTest() override = default;

  void SetUp() override {
    // Order matters: TestingProfile must be after ArcServiceManager.
    testing_profile_ = std::make_unique<TestingProfile>();

    observation_ =
        std::make_unique<ArcSystemStateObservation>(testing_profile_.get());

    active_window_observer_ =
        observation_->GetObserverByName(kArcActiveWindowThrottleObserverName);
    background_service_observer_ =
        observation_->GetObserverByName(kArcBackgroundServiceObserverName);
  }

  void TearDown() override { testing_profile_.reset(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> testing_profile_;

  std::unique_ptr<ArcSystemStateObservation> observation_;

  ash::ThrottleObserver* active_window_observer_;
  ash::ThrottleObserver* background_service_observer_;
};

TEST_F(ArcSystemStateObservationTest, TestConstructDestruct) {}

// TODO(sstan): Test the ARC system running state update from mojo.

}  // namespace arc
