// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_launcher.h"
#include "chrome/browser/ash/login/ui/fake_login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/biod/biod_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/dbus/oobe_config/fake_oobe_configuration_client.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/fake_input_method_delegate.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/test/test_context_factories.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr char kActionContinue[] = "continue";

constexpr char kEthServicePath[] = "/service/eth/0";
constexpr char kEthServiceName[] = "eth_service_name";
constexpr char kEthGuid[] = "eth_guid";
constexpr char kEthDevicePath[] = "/device/eth1";
constexpr char kEthName[] = "eth-name";

const StaticOobeScreenId kWelcomeScreen = WelcomeView::kScreenId;
const StaticOobeScreenId kNetworkScreen = NetworkScreenView::kScreenId;
const StaticOobeScreenId kUpdateScreen = UpdateView::kScreenId;
#if BUILDFLAG(PLATFORM_CFM)
const StaticOobeScreenId kEnrollmentScreen = EnrollmentScreenView::kScreenId;
#else   // BUILDFLAG(PLATFORM_CFM)
const StaticOobeScreenId kUserCreationScreen = UserCreationView::kScreenId;
#endif  // BUILDFLAG(PLATFORM_CFM)

OobeScreenId CurrentScreenId(WizardController* wizard_controller) {
  return wizard_controller->current_screen()->screen_id();
}

class ScreenWaiter : public WizardController::ScreenObserver {
 public:
  explicit ScreenWaiter(WizardController& wizard_controller)
      : wizard_controller_(wizard_controller) {}
  ~ScreenWaiter() override = default;

  bool WaitFor(const StaticOobeScreenId& screen_id) {
    base::test::TestFuture<void> screen_reached;

    screen_id_ = screen_id;
    screen_reached_ = screen_reached.GetCallback();

    base::ScopedObservation<WizardController, ScreenWaiter> observation(this);
    observation.Observe(&wizard_controller_.get());

    if (wizard_controller_->current_screen()->screen_id().name ==
        screen_id_.name) {
      return true;
    }
    return screen_reached.Wait();
  }

 private:
  // WizardController::ScreenObserver:
  void OnCurrentScreenChanged(BaseScreen* new_screen) override {
    if (new_screen->screen_id().name == screen_id_.name && screen_reached_) {
      std::move(screen_reached_).Run();
    }
  }

  void OnShutdown() override {}

  raw_ref<WizardController> wizard_controller_;
  StaticOobeScreenId screen_id_;
  base::OnceClosure screen_reached_;
};

// Sets up and tears down all global objects and configuration that needs to
// be done to run unit tests, but is not directly related to the tests.
class WizardControllerTestBase : public ::testing::Test {
 public:
  WizardControllerTestBase() = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    input_method::Initialize();
    AshTestHelper::InitParams params;
    params.start_session = false;
    params.local_state = profile_manager_->local_state()->Get();
    test_context_factories_ = std::make_unique<ui::TestContextFactories>(
        /*enable_pixel_output=*/false);
    ash_test_helper_ = std::make_unique<AshTestHelper>(
        test_context_factories_->GetContextFactory());
    ash_test_helper_->SetUp(std::move(params));
    ash::UserDataAuthClient::InitializeFake();
    chrome_keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeForAsh();
    CHECK(profile_manager_->SetUp());
    profile_manager_->CreateTestingProfile(chrome::kInitialProfile);
    auto* input_method_manager = input_method::InputMethodManager::Get();
    input_method_manager->SetState(input_method_manager->CreateNewState(
        ProfileManager::GetActiveUserProfile()));
    ash::BiodClient::InitializeFake();
    ash::InstallAttributesClient::InitializeFake();
    ash::SessionManagerClient::InitializeFake();
    DBusThreadManager::Initialize();
    OobeConfigurationClient::InitializeFake();
    enrollment_launcher_factory_ =
        std::make_unique<ScopedEnrollmentLauncherFactoryOverrideForTesting>(
            base::BindRepeating(FakeEnrollmentLauncher::Create,
                                &mock_enrollment_launcher_));
    DlcserviceClient::InitializeFake();
    network_portal_detector::InitializeForTesting(&network_portal_detector_);
    chromeos::TpmManagerClient::InitializeFake();
  }

  void TearDown() override {
    chromeos::TpmManagerClient::Shutdown();
    network_portal_detector::InitializeForTesting(nullptr);
    DlcserviceClient::Shutdown();
    enrollment_launcher_factory_.reset();
    OobeConfigurationClient::Shutdown();
    DBusThreadManager::Shutdown();
    ash::SessionManagerClient::Shutdown();
    ash::InstallAttributesClient::Shutdown();
    ash::BiodClient::Shutdown();
    chrome_keyboard_controller_client_test_helper_.reset();
    ash::UserDataAuthClient::Shutdown();
    ash_test_helper_->TearDown();
    test_context_factories_.reset();
    input_method::Shutdown();
    network_handler_test_helper_.reset();
    profile_manager_.reset();
  }

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_ =
      std::make_unique<content::BrowserTaskEnvironment>(
          base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
          base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<ui::TestContextFactories> test_context_factories_;
  std::unique_ptr<AshTestHelper> ash_test_helper_;

  testing::NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher_;
  input_method::FakeInputMethodDelegate delegate_;
  input_method::InputMethodUtil util_{&delegate_};
  OobeConfiguration oobe_configuration_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      chrome_keyboard_controller_client_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::FakeUserManager>()};
  ScopedTestingCrosSettings settings_;
  KioskAppManager kiosk_app_manager_;
  ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ScopedTestDeviceSettingsService scoped_device_settings_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
  std::unique_ptr<ScopedEnrollmentLauncherFactoryOverrideForTesting>
      enrollment_launcher_factory_;
  NetworkPortalDetectorTestImpl network_portal_detector_;
};

}  // namespace

class WizardControllerTest : public WizardControllerTestBase {
 public:
  void SetUp() override {
    WizardControllerTestBase::SetUp();
    auto* web_ui_profile = ProfileManager::GetActiveUserProfile();
    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(
        web_contents_factory_->CreateWebContents(web_ui_profile));

    fake_login_display_host_ = std::make_unique<FakeLoginDisplayHost>();
    auto oobe_ui = std::make_unique<OobeUI>(test_web_ui_.get(),
                                            GURL("chrome://oobe/oobe"));
    fake_login_display_host_->SetOobeUI(oobe_ui.get());
    test_web_ui_->SetController(std::move(oobe_ui));

    fake_update_engine_client_ = UpdateEngineClient::InitializeFakeForTest();

    auto wizard_controller = std::make_unique<WizardController>(
        fake_login_display_host_->GetWizardContext());
    wizard_controller_ = wizard_controller.get();
    fake_login_display_host_->SetWizardController(std::move(wizard_controller));
    wizard_controller_->SetSharedURLLoaderFactoryForTesting(
        test_url_loader_factory_.GetSafeWeakWrapper());

    // Make sure to test OOBE on an "official" build.
    OverrideBranding(/*is_branded=*/true);
  }

  void TearDown() override {
    cros_network_config_test_helper_.network_state_helper()
        .ResetDevicesAndServices();

    fake_update_engine_client_ = nullptr;
    wizard_controller_ = nullptr;
    fake_login_display_host_.reset();
    UpdateEngineClient::Shutdown();
    test_web_ui_.reset();
    web_contents_factory_.reset();
    WizardControllerTestBase::TearDown();
  }

 protected:
  bool AwaitScreen(const StaticOobeScreenId& screen_id) {
    LOG(INFO) << "Waiting for screen " << screen_id.name;
    ScreenWaiter screen_waiter(*wizard_controller_);
    return screen_waiter.WaitFor(screen_id);
  }

  void PerformUserAction(const std::string& action) {
    std::string user_acted_method_path = base::StrCat(
        {"login.", CurrentScreenId(wizard_controller_).external_api_prefix,
         ".userActed"});
    test_web_ui_->ProcessWebUIMessage(GURL(), user_acted_method_path,
                                      base::Value::List().Append(action));
  }

  // Starts network connection asynchronously.
  void StartNetworkConnection() {
    cros_network_config_test_helper_.network_state_helper().AddDevice(
        kEthDevicePath, shill::kTypeEthernet, kEthName);

    cros_network_config_test_helper_.network_state_helper()
        .service_test()
        ->AddService(kEthServicePath, kEthGuid, kEthServiceName,
                     shill::kTypeEthernet, shill::kStateOnline, true);
  }

  void MakeNonCriticalUpdateAvailable() {
    update_engine::StatusResult status;
    status.set_current_operation(update_engine::Operation::UPDATE_AVAILABLE);
    status.set_update_urgency(update_engine::UpdateUrgency::REGULAR);
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  void OverrideBranding(bool is_branded) {
    fake_login_display_host_->GetWizardContext()->is_branded_build = is_branded;
  }

  raw_ptr<WizardController> wizard_controller_ = nullptr;

 private:
  raw_ptr<FakeUpdateEngineClient> fake_update_engine_client_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_;
  std::unique_ptr<FakeLoginDisplayHost> fake_login_display_host_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

// Chromebox For Meetings has forced enrollment.
#if !BUILDFLAG(PLATFORM_CFM)
TEST_F(WizardControllerTest,
       ConsumerOobeFlowShouldContinueToUserCreationOnNonCriticalUpdate) {
  wizard_controller_->Init(/*first_screen=*/ash::OOBE_SCREEN_UNKNOWN);
  ASSERT_TRUE(AwaitScreen(kWelcomeScreen));

  PerformUserAction(kActionContinue);
  ASSERT_TRUE(AwaitScreen(kNetworkScreen));

  StartNetworkConnection();
  ASSERT_TRUE(AwaitScreen(kUpdateScreen));

  MakeNonCriticalUpdateAvailable();
  ASSERT_TRUE(AwaitScreen(kUserCreationScreen));
}
#endif  // !BUILDFLAG(PLATFORM_CFM)

#if BUILDFLAG(PLATFORM_CFM)
TEST_F(WizardControllerTest,
       CfMOobeFlowShouldContinueToEnrollmentOnNonCriticalUpdate) {
  wizard_controller_->Init(/*first_screen=*/ash::OOBE_SCREEN_UNKNOWN);
  ASSERT_TRUE(AwaitScreen(kWelcomeScreen));

  PerformUserAction(kActionContinue);
  ASSERT_TRUE(AwaitScreen(kNetworkScreen));

  StartNetworkConnection();
  ASSERT_TRUE(AwaitScreen(kUpdateScreen));

  MakeNonCriticalUpdateAvailable();
  ASSERT_TRUE(AwaitScreen(kEnrollmentScreen));
}
#endif  // BUILDFLAG(PLATFORM_CFM)

}  // namespace ash
