// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"

#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

constexpr char kFakeDeviceId[] = "device_id";
constexpr char kNonDemoDomain[] = "non-demo-mode.com";

void SetDemoConfigPref(DemoSession::DemoModeConfig demo_config) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetInteger(prefs::kDemoModeConfig, static_cast<int>(demo_config));
}

}  // namespace

// Tests locking device to policy::DEVICE_MODE_DEMO mode. It is an equivalent to
// going through online demo mode setup or using offline setup.
class DemoSessionDemoDeviceModeTest : public LoginManagerTest {
 protected:
  DemoSessionDemoDeviceModeTest()
      : LoginManagerTest(true /*should_launch_browser*/,
                         true /* should_initialize_webui */),
        install_attributes_(
            StubInstallAttributes::CreateDemoMode(kFakeDeviceId)) {}
  ~DemoSessionDemoDeviceModeTest() override = default;

  // LoginManagerTest:
  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    SetDemoConfigPref(DemoSession::DemoModeConfig::kOffline);
  }

 private:
  const ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionDemoDeviceModeTest);
};

IN_PROC_BROWSER_TEST_F(DemoSessionDemoDeviceModeTest, IsDemoMode) {
  EXPECT_TRUE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kOffline,
            DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOnline);
  EXPECT_TRUE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kOnline, DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOffline);
  EXPECT_TRUE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kOffline,
            DemoSession::GetDemoConfig());
}

// Tests locking device to demo mode domain without policy::DEVICE_MODE_DEMO
// mode. It is an equivalent to enrolling device directly by using enterprise
// enrollment flow.
class DemoSessionDemoEnrolledDeviceTest : public LoginManagerTest {
 protected:
  DemoSessionDemoEnrolledDeviceTest()
      : LoginManagerTest(true /*should_launch_browser*/,
                         true /* should_initialize_webui */),
        install_attributes_(StubInstallAttributes::CreateCloudManaged(
            DemoSetupController::kDemoModeDomain,
            kFakeDeviceId)) {}
  ~DemoSessionDemoEnrolledDeviceTest() override = default;

  // LoginManagerTest:
  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    SetDemoConfigPref(DemoSession::DemoModeConfig::kOffline);
  }

 private:
  const ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionDemoEnrolledDeviceTest);
};

IN_PROC_BROWSER_TEST_F(DemoSessionDemoEnrolledDeviceTest, IsDemoMode) {
  EXPECT_TRUE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kOffline,
            DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOnline);
  EXPECT_TRUE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kOnline, DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOffline);
  EXPECT_TRUE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kOffline,
            DemoSession::GetDemoConfig());
}

class DemoSessionNonDemoEnrolledDeviceTest : public LoginManagerTest {
 public:
  DemoSessionNonDemoEnrolledDeviceTest()
      : LoginManagerTest(true /*should_launch_browser*/,
                         true /* should_initialize_webui */),
        install_attributes_(
            StubInstallAttributes::CreateCloudManaged(kNonDemoDomain,
                                                      kFakeDeviceId)) {}
  ~DemoSessionNonDemoEnrolledDeviceTest() override = default;

 private:
  ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionNonDemoEnrolledDeviceTest);
};

IN_PROC_BROWSER_TEST_F(DemoSessionNonDemoEnrolledDeviceTest, NotDemoMode) {
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOnline);
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOffline);
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());
}

class DemoSessionConsumerDeviceTest : public LoginManagerTest {
 public:
  DemoSessionConsumerDeviceTest()
      : LoginManagerTest(true /*should_launch_browser*/,
                         true /* should_initialize_webui */),
        install_attributes_(StubInstallAttributes::CreateConsumerOwned()) {}
  ~DemoSessionConsumerDeviceTest() override = default;

 private:
  ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionConsumerDeviceTest);
};

IN_PROC_BROWSER_TEST_F(DemoSessionConsumerDeviceTest, NotDemoMode) {
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOnline);
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOffline);
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());
}

class DemoSessionUnownedDeviceTest : public LoginManagerTest {
 public:
  DemoSessionUnownedDeviceTest()
      : LoginManagerTest(true /*should_launch_browser*/,
                         true /* should_initialize_webui */),
        install_attributes_(StubInstallAttributes::CreateUnset()) {}
  ~DemoSessionUnownedDeviceTest() override = default;

 private:
  ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionUnownedDeviceTest);
};

IN_PROC_BROWSER_TEST_F(DemoSessionUnownedDeviceTest, NotDemoMode) {
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOnline);
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOffline);
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());
}

class DemoSessionActiveDirectoryDeviceTest : public LoginManagerTest {
 public:
  DemoSessionActiveDirectoryDeviceTest()
      : LoginManagerTest(true /*should_launch_browser*/,
                         true /* should_initialize_webui */),
        install_attributes_(StubInstallAttributes::CreateActiveDirectoryManaged(
            DemoSetupController::kDemoModeDomain,
            kFakeDeviceId)) {}
  ~DemoSessionActiveDirectoryDeviceTest() override = default;

 private:
  ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionActiveDirectoryDeviceTest);
};

IN_PROC_BROWSER_TEST_F(DemoSessionActiveDirectoryDeviceTest, NotDemoMode) {
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOnline);
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());

  SetDemoConfigPref(DemoSession::DemoModeConfig::kOffline);
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::DemoModeConfig::kNone, DemoSession::GetDemoConfig());
}

}  // namespace chromeos
