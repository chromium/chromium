// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/lock/screen_locker.h"

#include <memory>

#include "ash/components/audio/cras_audio_handler.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/ash/lock_screen_apps/state_controller.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#include "chrome/browser/ui/ash/accessibility/fake_accessibility_controller.h"
#include "chrome/browser/ui/ash/assistant/assistant_client_impl.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/test_login_screen.h"
#include "chrome/browser/ui/ash/test_session_controller.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/audio/cras_audio_client.h"
#include "chromeos/dbus/biod/biod_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/login/session/session_termination_manager.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/audio_service.h"
#include "content/public/test/browser_task_environment.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "media/audio/test_audio_thread.h"
#include "services/audio/public/cpp/sounds/audio_stream_handler.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"
#include "services/audio/public/cpp/sounds/test_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

std::unique_ptr<KeyedService> CreateCertificateProviderService(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::CertificateProviderService>();
}

}  // namespace

class ScreenLockerUnitTest : public testing::Test {
 public:
  ScreenLockerUnitTest() = default;
  ~ScreenLockerUnitTest() override = default;

  void SetUp() override {
    DBusThreadManager::Initialize();
    BiodClient::InitializeFake();
    CrasAudioClient::InitializeFake();
    TpmManagerClient::InitializeFake();
    CryptohomeMiscClient::InitializeFake();
    UserDataAuthClient::InitializeFake();

    // MojoSystemInfoDispatcher dependency:
    bluez::BluezDBusManager::GetSetterForTesting();

    // Initialize SessionControllerClientImpl and dependencies:
    LoginState::Initialize();
    CHECK(testing_profile_manager_.SetUp());

    // Set up certificate provider service for the signin profile.
    CertificateProviderServiceFactory::GetInstance()->SetTestingFactory(
        testing_profile_manager_.CreateTestingProfile(chrome::kInitialProfile),
        base::BindRepeating(&CreateCertificateProviderService));

    session_controller_client_ =
        std::make_unique<SessionControllerClientImpl>();
    session_controller_client_->Init();

    // Initialize AssistantClientImpl:
    assistant_client_ = std::make_unique<AssistantClientImpl>();

    // Initialize AccessibilityManager and dependencies:
    observer_ = std::make_unique<audio::TestObserver>((base::DoNothing()));
    audio::AudioStreamHandler::SetObserverForTesting(observer_.get());

    audio::SoundsManager::Create(content::GetAudioServiceStreamFactoryBinder());
    input_method::InputMethodManager::Initialize(
        // Owned by InputMethodManager
        new input_method::MockInputMethodManagerImpl());
    CrasAudioHandler::InitializeForTesting();
    AccessibilityManager::Initialize();

    // Initialize ScreenLocker dependencies:
    chromeos::ProfileHelper::GetSigninProfile();
    SystemSaltGetter::Initialize();
  }

  void CreateSessionForUser(bool is_public_account) {
    const AccountId account_id =
        AccountId::FromUserEmail("testemail@example.com");
    if (is_public_account) {
      fake_user_manager_->AddPublicAccountUser(account_id);
    } else {
      fake_user_manager_->AddUser(account_id);
    }
    fake_user_manager_->LoginUser(account_id);
    CHECK(user_manager::UserManager::Get()->GetPrimaryUser());
    session_manager::SessionManager::Get()->CreateSession(
        account_id, account_id.GetUserEmail(), false);
  }

  void TearDown() override {
    SystemSaltGetter::Shutdown();
    AccessibilityManager::Shutdown();
    CrasAudioHandler::Shutdown();
    input_method::InputMethodManager::Shutdown();
    audio::SoundsManager::Shutdown();
    audio::AudioStreamHandler::SetObserverForTesting(nullptr);
    observer_.reset();
    assistant_client_.reset();
    session_controller_client_.reset();
    LoginState::Shutdown();
    bluez::BluezDBusManager::Shutdown();
    UserDataAuthClient::Shutdown();
    CryptohomeMiscClient::Shutdown();
    TpmManagerClient::Shutdown();
    CrasAudioClient::Shutdown();
    BiodClient::Shutdown();
    DBusThreadManager::Shutdown();
  }

 protected:
  // Needed for main loop and posting async tasks.
  content::BrowserTaskEnvironment task_environment_;

  // ViewsScreenLocker dependencies:
  lock_screen_apps::StateController state_controller_;
  // * MojoSystemInfoDispatcher dependencies:
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
  system::ScopedFakeStatisticsProvider fake_statictics_provider_;
  // * ChromeUserSelectionScreen dependencies:
  chromeos::ScopedStubInstallAttributes test_install_attributes_;

  // ScreenLocker dependencies:
  // * AccessibilityManager dependencies:
  FakeAccessibilityController fake_accessibility_controller_;
  // * LoginScreenClient dependencies:
  session_manager::SessionManager session_manager_;
  TestLoginScreen test_login_screen_;
  LoginScreenClient login_screen_client_;
  // * SessionControllerClientImpl dependencies:
  FakeChromeUserManager* fake_user_manager_{new FakeChromeUserManager()};
  user_manager::ScopedUserManager scoped_user_manager_{
      base::WrapUnique(fake_user_manager_)};
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  ScopedDeviceSettingsTestHelper device_settings_test_helper_;
  TestSessionController test_session_controller_;
  std::unique_ptr<SessionControllerClientImpl> session_controller_client_;
  std::unique_ptr<AssistantClientImpl> assistant_client_;
  chromeos::SessionTerminationManager session_termination_manager_;

  std::unique_ptr<audio::TestObserver> observer_;
  DISALLOW_COPY_AND_ASSIGN(ScreenLockerUnitTest);
};

// Chrome notifies Ash when screen is locked. Ash is responsible for suspending
// the device.
TEST_F(ScreenLockerUnitTest, VerifyAshIsNotifiedOfScreenLocked) {
  CreateSessionForUser(/*is_public_account=*/false);

  EXPECT_EQ(0, test_session_controller_.lock_animation_complete_call_count());
  ScreenLocker::Show();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, test_session_controller_.lock_animation_complete_call_count());
  ScreenLocker::Hide();
  // Needed to perform internal cleanup scheduled in ScreenLocker::Hide()
  base::RunLoop().RunUntilIdle();
}

// Tests that `GetUsersToShow()` returns an empty list when the user is a
// Managed Guest Session.
TEST_F(ScreenLockerUnitTest, GetUsersToShow) {
  CreateSessionForUser(/*is_public_account=*/true);

  ScreenLocker::Show();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ScreenLocker::default_screen_locker()->GetUsersToShow().empty());
  ScreenLocker::Hide();
  // Needed to perform internal cleanup scheduled in ScreenLocker::Hide()
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
