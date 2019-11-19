// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/assistant/assistant_util.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/events/devices/device_data_manager.h"

namespace assistant {
namespace {

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr char kTestGaiaId[] = "1234567890";

class FakeUserManagerWithLocalState : public chromeos::FakeChromeUserManager {
 public:
  explicit FakeUserManagerWithLocalState(
      TestingProfileManager* testing_profile_manager)
      : testing_profile_manager_(testing_profile_manager),
        test_local_state_(std::make_unique<TestingPrefServiceSimple>()) {
    RegisterPrefs(test_local_state_->registry());
  }

  PrefService* GetLocalState() const override {
    return test_local_state_.get();
  }

  TestingProfileManager* testing_profile_manager() {
    return testing_profile_manager_;
  }

 private:
  // Unowned pointer.
  TestingProfileManager* const testing_profile_manager_;

  std::unique_ptr<TestingPrefServiceSimple> test_local_state_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserManagerWithLocalState);
};

class ScopedLogIn {
 public:
  ScopedLogIn(
      FakeUserManagerWithLocalState* fake_user_manager,
      signin::IdentityTestEnvironment* identity_test_env,
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::USER_TYPE_REGULAR)
      : fake_user_manager_(fake_user_manager),
        identity_test_env_(identity_test_env),

        account_id_(account_id) {
    // Prevent access to DBus. This switch is reset in case set from test SetUp
    // due massive usage of InitFromArgv.
    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kTestType))
      command_line.AppendSwitch(switches::kTestType);

    switch (user_type) {
      case user_manager::USER_TYPE_REGULAR:  // fallthrough
      case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
        LogIn();
        break;
      case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
        LogInAsPublicAccount();
        break;
      case user_manager::USER_TYPE_KIOSK_APP:
        LogInKioskApp();
        break;
      case user_manager::USER_TYPE_ARC_KIOSK_APP:
        LogInArcKioskApp();
        break;
      default:
        NOTREACHED();
    }
  }

  ~ScopedLogIn() { fake_user_manager_->RemoveUserFromList(account_id_); }

 private:
  void LogIn() {
    fake_user_manager_->AddUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
    if (!identity_test_env_->identity_manager()->HasPrimaryAccount()) {
      identity_test_env_->MakePrimaryAccountAvailable(
          account_id_.GetUserEmail());
    }
  }

  void LogInAsPublicAccount() {
    fake_user_manager_->AddPublicAccountUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  void LogInKioskApp() {
    fake_user_manager_->AddKioskAppUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  void LogInArcKioskApp() {
    fake_user_manager_->AddArcKioskAppUser(account_id_);
    fake_user_manager_->LoginUser(account_id_);
  }

  FakeUserManagerWithLocalState* fake_user_manager_;
  signin::IdentityTestEnvironment* identity_test_env_;
  const AccountId account_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLogIn);
};

}  // namespace

class ChromeAssistantUtilTest : public testing::Test {
 public:
  ChromeAssistantUtilTest() = default;
  ~ChromeAssistantUtilTest() override = default;

  void SetUp() override {
    command_line_ = std::make_unique<base::test::ScopedCommandLine>();

    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile(
        kTestProfileName, /*prefs=*/{}, base::UTF8ToUTF16(kTestProfileName),
        /*avatar_id=*/0, /*supervised_user_id=*/{},
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<FakeUserManagerWithLocalState>(
            profile_manager_.get()));

    ui::DeviceDataManager::CreateInstance();
  }

  void TearDown() override {
    ui::DeviceDataManager::DeleteInstance();
    identity_test_env_adaptor_.reset();
    user_manager_enabler_.reset();
    profile_manager_->DeleteTestingProfile(kTestProfileName);
    profile_ = nullptr;
    profile_manager_.reset();
    command_line_.reset();
  }

  TestingProfile* profile() { return profile_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  FakeUserManagerWithLocalState* GetFakeUserManager() const {
    return static_cast<FakeUserManagerWithLocalState*>(
        user_manager::UserManager::Get());
  }

 private:
  std::unique_ptr<base::test::ScopedCommandLine> command_line_;
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  // Owned by |profile_manager_|
  TestingProfile* profile_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeAssistantUtilTest);
};

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_SecondaryUser) {
  ScopedLogIn login2(
      GetFakeUserManager(), identity_test_env(),
      AccountId::FromUserEmailGaiaId("user2@gmail.com", "0123456789"));
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));

  EXPECT_EQ(ash::mojom::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_SupervisedUser) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));
  profile()->SetSupervisedUserId("foo");
  EXPECT_EQ(ash::mojom::AssistantAllowedState::DISALLOWED_BY_SUPERVISED_USER,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_Locale) {
  profile()->GetTestingPrefService()->SetString(
      language::prefs::kApplicationLocale, "he");
  UErrorCode error_code = U_ZERO_ERROR;
  const icu::Locale& old_locale = icu::Locale::getDefault();
  icu::Locale::setDefault(icu::Locale("he"), error_code);
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    AccountId::FromUserEmailGaiaId(
                        profile()->GetProfileUserName(), kTestGaiaId));

  EXPECT_EQ(ash::mojom::AssistantAllowedState::DISALLOWED_BY_LOCALE,
            IsAssistantAllowedForProfile(profile()));
  icu::Locale::setDefault(old_locale, error_code);
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_DemoMode) {
  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kOnline);
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_EQ(ash::mojom::AssistantAllowedState::DISALLOWED_BY_DEMO_MODE,
            IsAssistantAllowedForProfile(profile()));

  chromeos::DemoSession::SetDemoConfigForTesting(
      chromeos::DemoSession::DemoModeConfig::kNone);
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_PublicSession) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  EXPECT_EQ(ash::mojom::AssistantAllowedState::DISALLOWED_BY_PUBLIC_SESSION,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_NonGmail) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    AccountId::FromUserEmailGaiaId("user2@someotherdomain.com",
                                                   "0123456789"));

  EXPECT_EQ(ash::mojom::AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_GoogleMail) {
  ScopedLogIn login(
      GetFakeUserManager(), identity_test_env(),
      AccountId::FromUserEmailGaiaId("user2@googlemail.com", "0123456789"));

  EXPECT_EQ(ash::mojom::AssistantAllowedState::ALLOWED,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForKiosk_KioskApp) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_KIOSK_APP);

  EXPECT_EQ(ash::mojom::AssistantAllowedState::DISALLOWED_BY_KIOSK_MODE,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForKiosk_ArcKioskApp) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    AccountId::FromUserEmail(profile()->GetProfileUserName()),
                    user_manager::USER_TYPE_ARC_KIOSK_APP);

  EXPECT_EQ(ash::mojom::AssistantAllowedState::DISALLOWED_BY_KIOSK_MODE,
            IsAssistantAllowedForProfile(profile()));
}

}  // namespace assistant
