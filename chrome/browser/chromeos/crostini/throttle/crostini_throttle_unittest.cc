// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/throttle/crostini_throttle.h"
#include <memory>
#include "base/macros.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

class CrostiniThrottleTest : public testing::Test {
 public:
  CrostiniThrottleTest()
      : crostini_helper_(&profile_), crostini_throttle_(&profile_) {
    crostini_throttle_.set_delegate_for_testing(
        std::make_unique<TestDelegateImpl>(this));
  }

 protected:
  CrostiniThrottle* crostini_throttle() { return &crostini_throttle_; }

  size_t disable_cpu_restriction_counter() const {
    return disable_cpu_restriction_counter_;
  }

  size_t enable_cpu_restriction_counter() const {
    return enable_cpu_restriction_counter_;
  }

 private:
  class TestDelegateImpl : public CrostiniThrottle::Delegate {
   public:
    explicit TestDelegateImpl(CrostiniThrottleTest* test) : test_(test) {}
    ~TestDelegateImpl() override = default;

    void SetCpuRestriction(bool restrict) override {
      if (restrict)
        ++(test_->enable_cpu_restriction_counter_);
      else
        ++(test_->disable_cpu_restriction_counter_);
    }

    CrostiniThrottleTest* test_;
    DISALLOW_COPY_AND_ASSIGN(TestDelegateImpl);
  };

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  CrostiniTestHelper crostini_helper_;
  CrostiniThrottle crostini_throttle_;
  size_t disable_cpu_restriction_counter_{0};
  size_t enable_cpu_restriction_counter_{0};

  DISALLOW_COPY_AND_ASSIGN(CrostiniThrottleTest);
};

// Tests that CrostiniThrottle can be constructed and destructed.
TEST_F(CrostiniThrottleTest, TestConstructDestruct) {}

// Tests that CrostiniThrottle adjusts CPU restriction
// when ThrottleInstance is called.
TEST_F(CrostiniThrottleTest, TestThrottleInstance) {
  crostini_throttle()->set_level_for_testing(
      chromeos::ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, disable_cpu_restriction_counter());

  // CrostiniThrottle level is already LOW, expect no change
  crostini_throttle()->set_level_for_testing(
      chromeos::ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(0U, disable_cpu_restriction_counter());

  crostini_throttle()->set_level_for_testing(
      chromeos::ThrottleObserver::PriorityLevel::CRITICAL);
  EXPECT_EQ(1U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());

  crostini_throttle()->set_level_for_testing(
      chromeos::ThrottleObserver::PriorityLevel::LOW);
  EXPECT_EQ(2U, enable_cpu_restriction_counter());
  EXPECT_EQ(1U, disable_cpu_restriction_counter());
}

}  // namespace crostini
