// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_session.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

const char kAccountIdEmail[] = "public-session@test.com";

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

/* ============================ Demo Login Tests =============================*/

// Extra parts for setting up the FakeCrOSComponentManager before the real one
// has been initialized on the browser
class DemoLoginTestMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  DemoLoginTestMainExtraParts() = default;
  DemoLoginTestMainExtraParts(const DemoLoginTestMainExtraParts&) = delete;
  DemoLoginTestMainExtraParts& operator=(const DemoLoginTestMainExtraParts&) =
      delete;

  void PostEarlyInitialization() override {
    auto cros_component_manager =
        base::MakeRefCounted<component_updater::FakeCrOSComponentManager>();
    cros_component_manager->set_supported_components({"demo-mode-app"});
    cros_component_manager->ResetComponentState(
        "demo-mode-app",
        component_updater::FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/dev/null"),
            base::FilePath("/run/imageloader/demo-mode-app")));

    platform_part_test_api_ =
        std::make_unique<BrowserProcessPlatformPartTestApi>(
            g_browser_process->platform_part());
    platform_part_test_api_->InitializeCrosComponentManager(
        std::move(cros_component_manager));
  }

  void PostMainMessageLoopRun() override {
    platform_part_test_api_->ShutdownCrosComponentManager();
    platform_part_test_api_.reset();
  }

 private:
  std::unique_ptr<BrowserProcessPlatformPartTestApi> platform_part_test_api_;
};

// Tests that involve asserting state about actual logged-in Demo sessions
//
// Currently this fixture enables the Demo SWA by default - consider extracting
// this feature enablement into a subclass if non-SWA tests are needed
class DemoSessionLoginTest : public LoginManagerTest,
                             public LocalStateMixin::Delegate,
                             public BrowserListObserver,
                             public user_manager::UserManager::Observer,
                             public chromeos::FakePowerManagerClient::Observer {
 public:
  DemoSessionLoginTest() {
    login_manager_mixin_.set_should_launch_browser(true);
    BrowserList::AddObserver(this);
  }

  ~DemoSessionLoginTest() override { BrowserList::RemoveObserver(this); }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    static_cast<ChromeBrowserMainParts*>(browser_main_parts)
        ->AddParts(std::make_unique<DemoLoginTestMainExtraParts>());
    LoginManagerTest::CreatedBrowserMainParts(browser_main_parts);
  }

  void SetUpOnMainThread() override {
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_mixin_.RequestDevicePolicyUpdate();

    enterprise_management::DeviceLocalAccountsProto* const
        device_local_accounts = device_policy_update->policy_payload()
                                    ->mutable_device_local_accounts();
    enterprise_management::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kAccountIdEmail);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    device_local_accounts->set_auto_login_id(kAccountIdEmail);
    device_policy_update.reset();

    // Populate device_local_account policy cache with empty proto so policy
    // isn't marked as missing for the user, which causes
    // ExistingUserController::LoginAsPublicSession to wait endlessly on the
    // policy to be available. In browsertests, the device_local_account_policy
    // is never loaded again after initial device policy storage, likely because
    // policy fetches fail.
    std::unique_ptr<ScopedUserPolicyUpdate> device_local_account_policy_update =
        device_state_mixin_.RequestDeviceLocalAccountPolicyUpdate(
            kAccountIdEmail);
    device_local_account_policy_update.reset();

    // chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->set_keyboard_brightness_percent(
        kInitialBrightness);

    LoginManagerTest::SetUpOnMainThread();
  }

  void WaitForBrowserAdded() {
    base::RunLoop run_loop;
    on_browser_added_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 protected:
  // LocalStateMixin::Delegate
  void SetUpLocalState() override {
    SetDemoConfigPref(DemoSession::DemoModeConfig::kOnline);
  }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    if (on_browser_added_callback_) {
      std::move(on_browser_added_callback_).Run();
    }
  }

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
  base::OnceClosure on_browser_added_callback_;
  static constexpr double kInitialBrightness = 20.0;
  base::WeakPtrFactory<DemoSessionLoginTest> weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(DemoSessionLoginTest, SessionStartup) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  login_manager_mixin_.WaitForActiveSession();
}

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DemoSWALaunchesOnSessionStartup \
  DISABLED_DemoSWALaunchesOnSessionStartup
#else
#define MAYBE_DemoSWALaunchesOnSessionStartup DemoSWALaunchesOnSessionStartup
#endif
IN_PROC_BROWSER_TEST_F(DemoSessionLoginTest,
                       MAYBE_DemoSWALaunchesOnSessionStartup) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;

  login_manager_mixin_.WaitForActiveSession();
  auto* profile = ProfileManager::GetActiveUserProfile();
  SystemWebAppManager::GetForTest(profile)->InstallSystemAppsForTesting();
  WaitForBrowserAdded();

  // Verify that the Demo SWA has been opened
  Browser* demo_app_browser =
      FindSystemWebAppBrowser(profile, SystemWebAppType::DEMO_MODE);
  ASSERT_TRUE(demo_app_browser);
}

IN_PROC_BROWSER_TEST_F(
    DemoSessionLoginTest,
    DemoSessionKeyboardBrightnessIncreaseThreeTimesToOneHundredPercents) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  login_manager_mixin_.WaitForActiveSession();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(chromeos::FakePowerManagerClient::Get()
                ->num_increase_keyboard_brightness_calls(),
            3);
}

}  // namespace
}  // namespace ash
