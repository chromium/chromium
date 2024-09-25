// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_system_state_observation.h"

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/idle_manager/arc_background_service_observer.h"
#include "chrome/browser/ash/arc/idle_manager/arc_window_observer.h"
#include "chrome/browser/ash/arc/instance_throttle/arc_active_window_throttle_observer.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {
ArcAppListPrefs::AppInfo MakePlayStoreInfo(bool ready) {
  return ArcAppListPrefs::AppInfo(
      kPlayStoreAppId, kPlayStorePackage, kPlayStoreActivity,
      std::string() /* intent_uri */, std::string() /* icon_resource_id */,
      "" /* version_name */, base::Time() /* last_launch_time */,
      base::Time() /* install_time */, true /* sticky */,
      true /* notifications_enabled */,
      arc::mojom::ArcResizeLockState::UNDEFINED,
      true /* resize_lock_needs_confirmation */,
      ArcAppListPrefs::WindowLayout(), ready /* ready */, false /* suspended */,
      true /* show_in_launcher*/, false /* shortcut */, true /* launchable */,
      false /* need_fixup */, std::nullopt /* app_size */,
      std::nullopt /* data_size */,
      mojom::AppCategory::kUndefined /* app_category */);
}
}  // namespace

class ArcSystemStateObservationTest : public testing::Test {
 public:
  ArcSystemStateObservationTest() {
    arc_test().SetUp(&profile_);

    observation_ = std::make_unique<ArcSystemStateObservation>(&profile_);

    active_window_observer_ =
        observation_->GetObserverByName(kArcActiveWindowThrottleObserverName);
    background_service_observer_ =
        observation_->GetObserverByName(kArcBackgroundServiceObserverName);
    arc_window_observer_ =
        observation_->GetObserverByName(kArcWindowObserverName);
  }
  ArcSystemStateObservationTest(const ArcSystemStateObservationTest&) = delete;
  ArcSystemStateObservationTest& operator=(
      const ArcSystemStateObservationTest&) = delete;

  ~ArcSystemStateObservationTest() override {
    observation_.reset();
    arc_test().TearDown();
  }

  ArcSystemStateObservation* observation() { return observation_.get(); }
  ArcAppTest& arc_test() { return arc_test_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestingProfile profile_;
  ArcAppTest arc_test_;

  std::unique_ptr<ArcSystemStateObservation> observation_;

  raw_ptr<ash::ThrottleObserver, DanglingUntriaged> active_window_observer_;
  raw_ptr<ash::ThrottleObserver, DanglingUntriaged>
      background_service_observer_;
  raw_ptr<ash::ThrottleObserver, DanglingUntriaged> arc_window_observer_;
};

TEST_F(ArcSystemStateObservationTest, TestConstructDestruct) {}

TEST_F(ArcSystemStateObservationTest, TestCallback) {
  int reset_count = 0;
  observation()->OnAppStatesChanged(kPlayStoreAppId, MakePlayStoreInfo(true));
  observation()->SetDurationResetCallback(
      base::BindLambdaForTesting([&]() { reset_count++; }));
  observation()->ThrottleInstance(false);
  EXPECT_EQ(reset_count, 1);
}

TEST_F(ArcSystemStateObservationTest, NotPeaceIfArcNotConnected) {
  // ARC haven't connected.
  observation()->ThrottleInstance(true);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::nullopt, observation()->GetPeaceDuration());

  observation()->OnAppStatesChanged(kPlayStoreAppId, MakePlayStoreInfo(true));
  observation()->ThrottleInstance(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(std::nullopt, observation()->GetPeaceDuration());
}
// TODO(sstan): Test the ARC system running state update from mojo.

}  // namespace arc
