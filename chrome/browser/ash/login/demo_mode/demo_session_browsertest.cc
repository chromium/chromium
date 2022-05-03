// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_session.h"

#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

void SetDemoConfigPref(DemoSession::DemoModeConfig demo_config) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(prefs::kDemoModeConfig, static_cast<int>(demo_config));
}

void CheckDemoMode() {
  EXPECT_TRUE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kOnline, DemoSession::GetDemoConfig());
}

void CheckNoDemoMode() {
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOnline);
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());
}

// Tests locking device to policy::DEVICE_MODE_DEMO mode. It is an equivalent to
// going through online demo mode setup or using offline setup.
class DemoSessionDemoDeviceModeTest : public OobeBaseTest {
 public:
  DemoSessionDemoDeviceModeTest(const DemoSessionDemoDeviceModeTest&) = delete;
  DemoSessionDemoDeviceModeTest& operator=(
      const DemoSessionDemoDeviceModeTest&) = delete;

 protected:
  DemoSessionDemoDeviceModeTest() = default;
  ~DemoSessionDemoDeviceModeTest() override = default;

  // OobeBaseTest:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    SetDemoConfigPref(DemoSession::DemoModeConfig::kOnline);
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE};
};

IN_PROC_BROWSER_TEST_F(DemoSessionDemoDeviceModeTest, IsDemoMode) {
  CheckDemoMode();
}

// Tests locking device to demo mode domain without policy::DEVICE_MODE_DEMO
// mode. It is an equivalent to enrolling device directly by using enterprise
// enrollment flow.
class DemoSessionDemoEnrolledDeviceTest : public OobeBaseTest {
 public:
  DemoSessionDemoEnrolledDeviceTest(const DemoSessionDemoEnrolledDeviceTest&) =
      delete;
  DemoSessionDemoEnrolledDeviceTest& operator=(
      const DemoSessionDemoEnrolledDeviceTest&) = delete;

 protected:
  DemoSessionDemoEnrolledDeviceTest() : OobeBaseTest() {
    device_state_.set_domain(policy::kDemoModeDomain);
  }

  ~DemoSessionDemoEnrolledDeviceTest() override = default;

  // OobeBaseTest:
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    SetDemoConfigPref(DemoSession::DemoModeConfig::kOnline);
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(DemoSessionDemoEnrolledDeviceTest, IsDemoMode) {
  CheckDemoMode();
}

class DemoSessionNonDemoEnrolledDeviceTest : public OobeBaseTest {
 public:
  DemoSessionNonDemoEnrolledDeviceTest() = default;

  DemoSessionNonDemoEnrolledDeviceTest(
      const DemoSessionNonDemoEnrolledDeviceTest&) = delete;
  DemoSessionNonDemoEnrolledDeviceTest& operator=(
      const DemoSessionNonDemoEnrolledDeviceTest&) = delete;

  ~DemoSessionNonDemoEnrolledDeviceTest() override = default;

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(DemoSessionNonDemoEnrolledDeviceTest, NotDemoMode) {
  CheckNoDemoMode();
}

class DemoSessionConsumerDeviceTest : public OobeBaseTest {
 public:
  DemoSessionConsumerDeviceTest() = default;

  DemoSessionConsumerDeviceTest(const DemoSessionConsumerDeviceTest&) = delete;
  DemoSessionConsumerDeviceTest& operator=(
      const DemoSessionConsumerDeviceTest&) = delete;

  ~DemoSessionConsumerDeviceTest() override = default;

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
};

IN_PROC_BROWSER_TEST_F(DemoSessionConsumerDeviceTest, NotDemoMode) {
  CheckNoDemoMode();
}

class DemoSessionUnownedDeviceTest : public OobeBaseTest {
 public:
  DemoSessionUnownedDeviceTest() = default;

  DemoSessionUnownedDeviceTest(const DemoSessionUnownedDeviceTest&) = delete;
  DemoSessionUnownedDeviceTest& operator=(const DemoSessionUnownedDeviceTest&) =
      delete;

  ~DemoSessionUnownedDeviceTest() override = default;

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_UNOWNED};
};

IN_PROC_BROWSER_TEST_F(DemoSessionUnownedDeviceTest, NotDemoMode) {
  CheckNoDemoMode();
}

class DemoSessionActiveDirectoryDeviceTest : public OobeBaseTest {
 public:
  DemoSessionActiveDirectoryDeviceTest() = default;

  DemoSessionActiveDirectoryDeviceTest(
      const DemoSessionActiveDirectoryDeviceTest&) = delete;
  DemoSessionActiveDirectoryDeviceTest& operator=(
      const DemoSessionActiveDirectoryDeviceTest&) = delete;

  ~DemoSessionActiveDirectoryDeviceTest() override = default;

 private:
  DeviceStateMixin device_state_{
      &mixin_host_,
      DeviceStateMixin::State::OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(DemoSessionActiveDirectoryDeviceTest, NotDemoMode) {
  CheckNoDemoMode();
}

}  // namespace
}  // namespace ash
