// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/wizard_controller.h"

#include <memory>
#include <utility>

#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_launcher.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"
#include "chrome/browser/ash/net/rollback_network_config/fake_rollback_network_config.h"
#include "chrome/browser/ash/net/rollback_network_config/rollback_network_config_service.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/browser/ui/ash/login/fake_login_display_host.h"
#include "chrome/browser/ui/ash/wallpaper/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/demo_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/dbus/biod/biod_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/oobe_config/fake_oobe_configuration_client.h"
#include "chromeos/ash/components/dbus/oobe_config/oobe_configuration_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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
#if !BUILDFLAG(PLATFORM_CFM)
constexpr char kActionContinueSetup[] = "continue-setup";
constexpr char kActionStartSetup[] = "start-setup";
constexpr char kActionBack[] = "back";
constexpr char kActionSetUpDemoMode[] = "setupDemoMode";
constexpr char kActionTosAccept[] = "tos-accept";
#endif  // !BUILDFLAG(PLATFORM_CFM)

constexpr char kEthServicePath[] = "/service/eth/0";
constexpr char kEthServiceName[] = "eth_service_name";
constexpr char kEthGuid[] = "eth_guid";
constexpr char kEthDevicePath[] = "/device/eth1";
constexpr char kEthName[] = "eth-name";

constexpr StaticOobeScreenId kWelcomeScreen = WelcomeView::kScreenId;
constexpr StaticOobeScreenId kNetworkScreen = NetworkScreenView::kScreenId;
constexpr StaticOobeScreenId kUpdateScreen = UpdateView::kScreenId;
constexpr StaticOobeScreenId kAutoEnrollmentCheckScreen =
    AutoEnrollmentCheckScreenView::kScreenId;
constexpr StaticOobeScreenId kEnrollmentScreen =
    EnrollmentScreenView::kScreenId;
#if !BUILDFLAG(PLATFORM_CFM)
constexpr StaticOobeScreenId kDemoModePreferenceScreen =
    DemoPreferencesScreenView::kScreenId;
constexpr StaticOobeScreenId kDemoSetupScreen = DemoSetupScreenView::kScreenId;
constexpr StaticOobeScreenId kConsolidateConsentScreen =
    ConsolidatedConsentScreenView::kScreenId;
constexpr StaticOobeScreenId kUserCreationScreen = UserCreationView::kScreenId;
constexpr StaticOobeScreenId kGaiaSigninScreen = GaiaScreenHandler::kScreenId;
#endif  // !BUILDFLAG(PLATFORM_CFM)

// Converts an arbitrary number of arguments to a list of `base::Value`.
base::Value::List ToList() {
  return base::Value::List();
}
template <typename A, typename... Args>
base::Value::List ToList(A&& value, Args&&... values) {
  auto list = ToList(values...);
  list.Insert(list.begin(), base::Value(std::move(value)));
  return list;
}

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

void CreateExtensionServiceFor(Profile* profile) {
  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(),
      base::FilePath() /* install_directory */, false /* autoupdate_enabled */);
}

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
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    prefs->SetInitializationCompleted();
    RegisterUserProfilePrefs(prefs->registry());
    profile_ = profile_manager_->CreateTestingProfile(
        ash::kSigninBrowserContextBaseName, std::move(prefs),
        base::UTF8ToUTF16(ash::kSigninBrowserContextBaseName), 0,
        TestingProfile::TestingFactories());
    auto* input_method_manager = input_method::InputMethodManager::Get();
    input_method_manager->SetState(
        input_method_manager->CreateNewState(profile_));
    ash::BiodClient::InitializeFake();
    ash::InstallAttributesClient::InitializeFake();
    DBusThreadManager::Initialize();
    OobeConfigurationClient::InitializeFake();
    enrollment_launcher_factory_ =
        std::make_unique<ScopedEnrollmentLauncherFactoryOverrideForTesting>(
            base::BindRepeating(FakeEnrollmentLauncher::Create,
                                &mock_enrollment_launcher_));
    network_portal_detector::InitializeForTesting(&network_portal_detector_);
    chromeos::TpmManagerClient::InitializeFake();
    StatsReportingController::Initialize(
        profile_manager_->local_state()->Get());
    CreateExtensionServiceFor(profile_.get());
    CreateExtensionServiceFor(
        profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true));
    auth_events_recorder_ = AuthEventsRecorder::CreateForTesting();
    wallpaper_controller_client_ = std::make_unique<
        WallpaperControllerClientImpl>(
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
    wallpaper_controller_client_->InitForTesting(WallpaperController::Get());
  }

  void TearDown() override {
    wallpaper_controller_client_.reset();
    auth_events_recorder_.reset();
    extensions::ExtensionSystem::Get(profile_)->Shutdown();
    extensions::ExtensionSystem::Get(
        profile_->GetPrimaryOTRProfile(/*create_if_needed=*/false))
        ->Shutdown();
    StatsReportingController::Shutdown();
    chromeos::TpmManagerClient::Shutdown();
    network_portal_detector::InitializeForTesting(nullptr);
    enrollment_launcher_factory_.reset();
    OobeConfigurationClient::Shutdown();
    DBusThreadManager::Shutdown();
    ash::InstallAttributesClient::Shutdown();
    ash::BiodClient::Shutdown();
    chrome_keyboard_controller_client_test_helper_.reset();
    ash::UserDataAuthClient::Shutdown();
    ash_test_helper_->TearDown();
    test_context_factories_.reset();
    input_method::Shutdown();
    network_handler_test_helper_.reset();
    // Need to call `StartTearDown` otherwise timezone resolver still registered
    // with prefs when we delete profile manager.
    TestingBrowserProcess::GetGlobal()->platform_part()->StartTearDown();
    profile_ = nullptr;
    profile_manager_.reset();
  }

  void FakeInstallAttributesForDemoMode() {
    scoped_stub_install_attributes_.Get()->set_device_locked(true);
    scoped_stub_install_attributes_.Get()->SetDemoMode();
  }

 protected:
  testing::NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher_;

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_ =
      std::make_unique<content::BrowserTaskEnvironment>(
          base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
          base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_{std::make_unique<user_manager::FakeUserManager>()};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<ui::TestContextFactories> test_context_factories_;
  std::unique_ptr<AshTestHelper> ash_test_helper_;

  input_method::FakeInputMethodDelegate delegate_;
  input_method::InputMethodUtil util_{&delegate_};
  OobeConfiguration oobe_configuration_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      chrome_keyboard_controller_client_test_helper_;
  ScopedTestingCrosSettings settings_;
  KioskChromeAppManager kiosk_chrome_app_manager_;
  ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ash::ScopedDeviceSettingsTestHelper device_settings_test_helper_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
  std::unique_ptr<ScopedEnrollmentLauncherFactoryOverrideForTesting>
      enrollment_launcher_factory_;
  NetworkPortalDetectorTestImpl network_portal_detector_;
  std::unique_ptr<AuthEventsRecorder> auth_events_recorder_;
  std::unique_ptr<WallpaperControllerClientImpl> wallpaper_controller_client_;
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
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    wizard_controller_->SetSharedURLLoaderFactoryForTesting(
        test_url_loader_factory_->GetSafeWeakWrapper());

    // Make sure to test OOBE on an "official" build.
    OverrideBranding(/*is_branded=*/true);
  }

  void TearDown() override {
    cros_network_config_test_helper_.network_state_helper()
        .ResetDevicesAndServices();

    test_url_loader_factory_.reset();
    wizard_controller_ = nullptr;
    fake_update_engine_client_ = nullptr;
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

  void SetOobeConfiguration(const std::string& config) {
    static_cast<FakeOobeConfigurationClient*>(OobeConfigurationClient::Get())
        ->SetConfiguration(config);
    OobeConfiguration::Get()->CheckConfiguration();
  }

  template <typename... Args>
  void PerformUserAction(std::string action, Args... args) {
    std::string user_acted_method_path = base::StrCat(
        {"login.", CurrentScreenId(wizard_controller_).external_api_prefix,
         ".userActed"});
    test_web_ui_->ProcessWebUIMessage(GURL(), user_acted_method_path,
                                      ToList(action, args...));
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
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  SigninProfileHandler signing_profile_handler_;
};

// Chromebox For Meetings (CFM) has forced enrollment. Tests that want to do
// other things must not run on CFM builds.
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

TEST_F(WizardControllerTest, DemoModeOobeFlowEndsOnGaiaScreenAndCompletesOobe) {
  wizard_controller_->Init(kWelcomeScreen);
  ASSERT_TRUE(AwaitScreen(kWelcomeScreen));
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  PerformUserAction(kActionSetUpDemoMode);
  ASSERT_TRUE(AwaitScreen(kNetworkScreen));
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  StartNetworkConnection();
  ASSERT_TRUE(AwaitScreen(kDemoModePreferenceScreen));

  PerformUserAction(kActionContinueSetup, "Retailer Name", "Store Number");
  ASSERT_TRUE(AwaitScreen(kUpdateScreen));

  MakeNonCriticalUpdateAvailable();
  ASSERT_TRUE(AwaitScreen(kConsolidateConsentScreen));

  // Consolidate consent screen with DCHECKs turned on crashes if you are too
  // fast. Need to wait for it to fetch owner state asynchronously.
  base::RunLoop().RunUntilIdle();

  PerformUserAction(kActionTosAccept, /*enable_usage=*/true,
                    /*enable_backup=*/true, /*enable_location=*/true,
                    "TOS Content", /*enable_recovery=*/true);
  ASSERT_TRUE(AwaitScreen(kDemoSetupScreen));

  base::test::TestFuture<void> enrollment_signal;
  EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAttestation())
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        FakeInstallAttributesForDemoMode();
        mock_enrollment_launcher_.status_consumer()->OnDeviceEnrolled();
        (enrollment_signal.GetCallback()).Run();
      }));

  PerformUserAction(kActionStartSetup);

  ASSERT_TRUE(enrollment_signal.Wait());

  ASSERT_TRUE(AwaitScreen(kGaiaSigninScreen));
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

TEST_F(WizardControllerTest, BackOnNetworkScreenCancelsDemoMode) {
  wizard_controller_->Init(kWelcomeScreen);
  ASSERT_TRUE(AwaitScreen(kWelcomeScreen));
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  PerformUserAction(kActionSetUpDemoMode);
  ASSERT_TRUE(AwaitScreen(kNetworkScreen));
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  PerformUserAction(kActionBack);
  ASSERT_TRUE(AwaitScreen(kWelcomeScreen));
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
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

class WizardControllerAfterRollbackTest : public WizardControllerTest {
 public:
  void SetUp() override {
    WizardControllerTest::SetUp();
    rollback_network_config_ = static_cast<FakeRollbackNetworkConfig*>(
        rollback_network_config::OverrideInProcessInstanceForTesting(
            std::make_unique<FakeRollbackNetworkConfig>()));
    SetOobeConfiguration(kRollbackOobeConfig);
  }
  void TearDown() override {
    rollback_network_config_ = nullptr;
    rollback_network_config::Shutdown();
    WizardControllerTest::TearDown();
  }

 protected:
  raw_ptr<FakeRollbackNetworkConfig> rollback_network_config_;

 private:
  static constexpr char kRollbackOobeConfig[] = R"({
    "enrollmentRestoreAfterRollback": true,
    "eulaAutoAccept": true,
    "eulaSendStatistics": true,
    "networkUseConnected": true,
    "welcomeNext": true,
    "networkConfig": "{\"NetworkConfigurations\":[{
     \"GUID\":\"wpa-psk-network-guid\",
     \"Type\": \"WiFi\",
     \"Name\": \"WiFi\",
     \"WiFi\": {
       \"Security\": \"WPA-PSK\",
       \"Passphrase\": \"wpa-psk-network-passphrase\"
    }}]}"
  })";
};

TEST_F(WizardControllerAfterRollbackTest, AdvanceToEnrollmentAfterRollback) {
  wizard_controller_->Init(kAutoEnrollmentCheckScreen);
  ASSERT_TRUE(AwaitScreen(kEnrollmentScreen));
}

TEST_F(WizardControllerAfterRollbackTest, ImportNetworkConfigAfterRollback) {
  base::test::TestFuture<void> config_imported;
  rollback_network_config_->RegisterImportClosure(
      config_imported.GetCallback());

  wizard_controller_->Init(ash::OOBE_SCREEN_UNKNOWN);

  ASSERT_TRUE(config_imported.Wait());

  auto* imported_config = rollback_network_config_->imported_config();
  ASSERT_TRUE(imported_config != nullptr);
  ASSERT_TRUE(imported_config->is_dict());

  const base::Value::List* network_list =
      imported_config->GetDict().FindList("NetworkConfigurations");
  ASSERT_TRUE(network_list);

  const base::Value& network = (*network_list)[0];
  ASSERT_TRUE(network.is_dict());

  const std::string* guid = network.GetDict().FindString("GUID");
  ASSERT_TRUE(guid);
  EXPECT_EQ(*guid, "wpa-psk-network-guid");
}

}  // namespace ash
