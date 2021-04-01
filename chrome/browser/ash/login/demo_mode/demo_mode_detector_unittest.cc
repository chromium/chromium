// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_detector.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/ash/login/ui/mock_login_display_host.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/base_event_utils.h"

namespace chromeos {

class DemoModeDetectorTest : public testing::Test {
 protected:
  DemoModeDetectorTest();
  ~DemoModeDetectorTest() override;

  void StartDemoModeDetection();
  void SetTimeOnOobePref(base::TimeDelta time_on_oobe);
  base::TimeDelta GetTimeOnOobePref();
  void DestroyDemoModeDetector();
  void SimulateUserActivity();
  scoped_refptr<base::TestMockTimeTaskRunner> runner_;

 private:
  TestingPrefServiceSimple local_state_;
  DemoModeDetector::Observer observer_;
  ui::UserActivityDetector user_activity_detector_;
  std::unique_ptr<DemoModeDetector> demo_mode_detector_;
  std::unique_ptr<base::ThreadTaskRunnerHandle> runner_handle_;
};

DemoModeDetectorTest::DemoModeDetectorTest() {
  TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
  DemoModeDetector::RegisterPrefs(local_state_.registry());
  runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  // If we don't register the TestMockTimeTaskRunner with a
  // ThreadTaskRunnerHandle, the timers in the test fail
  // to initialize, saying they need a SequencedContext.
  runner_handle_ = std::make_unique<base::ThreadTaskRunnerHandle>(runner_);
}

DemoModeDetectorTest::~DemoModeDetectorTest() {
  demo_mode_detector_.reset();
  runner_handle_.reset();
  TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
}

void DemoModeDetectorTest::StartDemoModeDetection() {
  demo_mode_detector_ = std::make_unique<DemoModeDetector>(
      runner_->GetMockTickClock(), &observer_);
}

void DemoModeDetectorTest::SetTimeOnOobePref(base::TimeDelta time_on_oobe) {
  local_state_.SetUserPref(prefs::kTimeOnOobe,
                           std::make_unique<base::Value>(
                               base::NumberToString(time_on_oobe.InSeconds())));
}

base::TimeDelta DemoModeDetectorTest::GetTimeOnOobePref() {
  return base::TimeDelta::FromSeconds(
      local_state_.GetInt64(prefs::kTimeOnOobe));
}

void DemoModeDetectorTest::SimulateUserActivity() {
  user_activity_detector_.HandleExternalUserActivity();
}

// Tests follow.

// Test to ensure that Demo mode isn't launched before the detector
// has entered the derelict state.
TEST_F(DemoModeDetectorTest, DemoModeWillNotLaunchBeforeDerelict) {
  StartDemoModeDetection();
  // Run for half the timeout.
  runner_->FastForwardBy(DemoModeDetector::kDerelictDetectionTimeout -
                         DemoModeDetector::kDerelictDetectionTimeout / 2);
}

// Test to ensure that Demo mode isn't launched after the detector
// has entered the derelict state but before the idle timeout.
TEST_F(DemoModeDetectorTest,
       DemoModeWillNotLaunchAfterDerelictAndBeforeIdleTimeout) {
  StartDemoModeDetection();
  // Run through the derelict threshold.
  runner_->FastForwardBy(DemoModeDetector::kDerelictDetectionTimeout);
  // Run for 1 minute less than the idle threshold.
  runner_->FastForwardBy(DemoModeDetector::kDerelictIdleTimeout -
                         base::TimeDelta::FromMinutes(1));
}

// Test to ensure that Demo mode isn't launched after the detector
// has entered the derelict state but user activity is preventing the idle
// timeout.
TEST_F(DemoModeDetectorTest,
       DemoModeWillNotLaunchAfterDerelictWithUserActivity) {
  StartDemoModeDetection();

  // Run for through the derelict threshold.
  runner_->FastForwardBy(DemoModeDetector::kDerelictIdleTimeout);

  // Run for 2 more minutes (less than the idle threshold).
  runner_->FastForwardBy(base::TimeDelta::FromMinutes(2));

  // Simulate a user activity event.
  SimulateUserActivity();

  // Run for 3 more minutes (more than the idle threshold).
  runner_->FastForwardBy(base::TimeDelta::FromMinutes(3));

  // Simulate a user activity event.
  SimulateUserActivity();
}

// Test to ensure that Demo mode is launched after the detector
// has entered the derelict state and after the idle timeout.
TEST_F(DemoModeDetectorTest, DemoModeWillLaunchAfterDerelictAndIdleTimeout) {
  StartDemoModeDetection();
  // Run for long enough for all thresholds to be exceeded.
  runner_->FastForwardBy(DemoModeDetector::kDerelictDetectionTimeout +
                         DemoModeDetector::kDerelictIdleTimeout);
}

// Test to ensure that a device in dev mode disables the demo mode.
TEST_F(DemoModeDetectorTest, DemoModeWillNotLaunchInDevMode) {
  // Set the command line dev mode switch.
  auto command_line_ = std::make_unique<base::test::ScopedCommandLine>();
  command_line_->GetProcessCommandLine()->AppendSwitch(
      switches::kSystemDevMode);

  StartDemoModeDetection();

  // Run through the derelict threshold.
  runner_->FastForwardBy(DemoModeDetector::kDerelictDetectionTimeout);
}

// Test to ensure that the --disable-demo-mode switch disables demo mode.
TEST_F(DemoModeDetectorTest, DemoModeWillNotLaunchWhenDisabledBySwitch) {
  // Set the command line dev mode switch.
  auto command_line_ = std::make_unique<base::test::ScopedCommandLine>();
  command_line_->GetProcessCommandLine()->AppendSwitch(
      switches::kDisableDemoMode);

  StartDemoModeDetection();
  // Run through the derelict threshold.
  runner_->FastForwardBy(DemoModeDetector::kDerelictDetectionTimeout);
}

// Test to ensure that demo mode is disabled on "testimage" LsbRelease
// CHROMEOS_RELEASE_TRACK values.
TEST_F(DemoModeDetectorTest, DemoModeWillNotLaunchWhenTestimageInLsbRelease) {
  std::string lsb_release =
      "CHROMEOS_RELEASE_NAME=Chromium OS\n"
      "CHROMEOS_RELEASE_TRACK=testimage\n";

  base::Time release_time;
  EXPECT_TRUE(
      base::Time::FromString("Wed, 24 Oct 2018 12:00:00 PDT", &release_time));

  base::test::ScopedChromeOSVersionInfo version_info(lsb_release, release_time);

  StartDemoModeDetection();
  // Run through the derelict threshold.
  runner_->FastForwardBy(DemoModeDetector::kDerelictDetectionTimeout);
}

// Test to ensure that Demo mode is launched after the detector
// has resumed (i.e. after shutdown/reboot).
TEST_F(DemoModeDetectorTest,
       DemoModeWillLaunchAfterResumedAndDerelictAndIdleTimeout) {
  // Simulate 1 hour less than the threshold elapsed by setting pref.
  const auto elapsed_time = DemoModeDetector::kDerelictDetectionTimeout -
                            base::TimeDelta::FromHours(1);
  SetTimeOnOobePref(elapsed_time);
  EXPECT_EQ(GetTimeOnOobePref(), elapsed_time);

  StartDemoModeDetection();

  // Run another hour to hit the threshold.
  runner_->FastForwardBy(base::TimeDelta::FromHours(1));

  // Run through the idle timeout.
  runner_->FastForwardBy(DemoModeDetector::kDerelictIdleTimeout);
}

}  // namespace chromeos
