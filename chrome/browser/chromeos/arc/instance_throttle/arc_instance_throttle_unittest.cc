// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_instance_throttle.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/throttle_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcInstanceThrottleTest : public testing::Test {
 public:
  ArcInstanceThrottleTest()
      : arc_service_manager_(std::make_unique<ArcServiceManager>()),
        arc_session_manager_(std::make_unique<ArcSessionManager>(
            std::make_unique<ArcSessionRunner>(
                base::BindRepeating(FakeArcSession::Create)))),
        testing_profile_(std::make_unique<TestingProfile>()),
        disable_cpu_restriction_counter_(0),
        enable_cpu_restriction_counter_(0) {
    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());

    ArcBootPhaseMonitorBridge::GetForBrowserContextForTesting(
        testing_profile_.get());
    arc_instance_throttle_ =
        ArcInstanceThrottle::GetForBrowserContextForTesting(
            testing_profile_.get());
    arc_instance_throttle_->set_delegate_for_testing(
        std::make_unique<TestDelegateImpl>(this));
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable* GetPrefs() {
    return testing_profile_->GetTestingPrefService();
  }

  ArcInstanceThrottle* arc_instance_throttle() {
    return arc_instance_throttle_;
  }

  size_t disable_cpu_restriction_counter() const {
    return disable_cpu_restriction_counter_;
  }

  size_t enable_cpu_restriction_counter() const {
    return enable_cpu_restriction_counter_;
  }

 private:
  class TestDelegateImpl : public ArcInstanceThrottle::Delegate {
   public:
    explicit TestDelegateImpl(ArcInstanceThrottleTest* test) : test_(test) {}
    ~TestDelegateImpl() override = default;

    void SetCpuRestriction(bool restrict) override {
      if (!restrict)
        ++(test_->disable_cpu_restriction_counter_);
      else
        ++(test_->enable_cpu_restriction_counter_);
    }

    void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                         base::TimeDelta delta) override {}

    ArcInstanceThrottleTest* test_;
    DISALLOW_COPY_AND_ASSIGN(TestDelegateImpl);
  };
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
  ArcInstanceThrottle* arc_instance_throttle_;
  size_t disable_cpu_restriction_counter_;
  size_t enable_cpu_restriction_counter_;

  DISALLOW_COPY_AND_ASSIGN(ArcInstanceThrottleTest);
};

// Tests that ArcInstanceThrottle can be constructed and destructed.

TEST_F(ArcInstanceThrottleTest, TestConstructDestruct) {}

// Tests that ArcInstanceThrottle adjusts ARC CPU restriction
// when ThrottleInstance is called.
TEST_F(ArcInstanceThrottleTest, TestThrottleInstance) {
  arc_instance_throttle()->set_level_for_testing(
      chromeos::ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, disable_cpu_restriction_counter());

  // ArcInstanceThrottle level is already LOW, expect no change
  arc_instance_throttle()->set_level_for_testing(
      chromeos::ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, disable_cpu_restriction_counter());

  arc_instance_throttle()->set_level_for_testing(
      chromeos::ThrottleObserver::PriorityLevel::CRITICAL);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());

  arc_instance_throttle()->set_level_for_testing(
      chromeos::ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(2U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());
}

}  // namespace arc
