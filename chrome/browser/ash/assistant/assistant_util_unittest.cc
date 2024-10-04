// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/assistant/assistant_util.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "components/account_id/account_id.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/events/devices/device_data_manager.h"

namespace assistant {
namespace {

constexpr char kTestProfileName[] = "user@gmail.com";
constexpr char16_t kTestProfileName16[] = u"user@gmail.com";
constexpr char kTestGaiaId[] = "1234567890";

class ScopedSpoofGoogleBrandedDevice {
 public:
  ScopedSpoofGoogleBrandedDevice() { OverrideIsGoogleDeviceForTesting(true); }
  ~ScopedSpoofGoogleBrandedDevice() { OverrideIsGoogleDeviceForTesting(false); }
};

class ScopedLogIn {
 public:
  ScopedLogIn(
      ash::FakeChromeUserManager* fake_user_manager,
      signin::IdentityTestEnvironment* identity_test_env,
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::UserType::kRegular)
      : fake_user_manager_(fake_user_manager),
        identity_test_env_(identity_test_env),
        account_id_(account_id) {
    PreventAccessToDBus();
    RunValidityChecks(user_type);
    AddUser(user_type);

    fake_user_manager_->LoginUser(account_id_);

    MakeAccountAvailableAsPrimaryAccount(user_type);
  }

  ScopedLogIn(const ScopedLogIn&) = delete;
  ScopedLogIn& operator=(const ScopedLogIn&) = delete;

  ~ScopedLogIn() { fake_user_manager_->RemoveUserFromList(account_id_); }

 private:
  // Prevent access to DBus. This switch is reset in case set from test SetUp
  // due massive usage of InitFromArgv.
  void PreventAccessToDBus() {
    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    if (!command_line.HasSwitch(switches::kTestType))
      command_line.AppendSwitch(switches::kTestType);
  }

  void MakeAccountAvailableAsPrimaryAccount(user_manager::UserType user_type) {
    // Guest user can never be a primary account.
    if (user_type == user_manager::UserType::kGuest) {
      return;
    }

    if (!identity_test_env_->identity_manager()->HasPrimaryAccount(
            signin::ConsentLevel::kSignin)) {
      identity_test_env_->MakePrimaryAccountAvailable(
          account_id_.GetUserEmail(), signin::ConsentLevel::kSignin);
    }
  }

  // Run validity checks ensuring the account id is valid for the given user
  // type. If these checks go off your test is testing something that can not
  // happen.
  void RunValidityChecks(user_manager::UserType user_type) const {
    switch (user_type) {
      case user_manager::UserType::kRegular:
      case user_manager::UserType::kChild:
        EXPECT_TRUE(IsGaiaAccount());
        return;
      case user_manager::UserType::kPublicAccount:
      case user_manager::UserType::kKioskApp:
      case user_manager::UserType::kWebKioskApp:
      case user_manager::UserType::kKioskIWA:
        EXPECT_FALSE(IsGaiaAccount());
        return;
      case user_manager::UserType::kGuest:
        // Guest user must use the guest user account id.
        EXPECT_EQ(account_id_, user_manager::GuestAccountId());
        return;
    }
  }

  void AddUser(user_manager::UserType user_type) {
    switch (user_type) {
      case user_manager::UserType::kRegular:
        fake_user_manager_->AddUser(account_id_);
        return;
      case user_manager::UserType::kPublicAccount:
        fake_user_manager_->AddPublicAccountUser(account_id_);
        return;
      case user_manager::UserType::kKioskApp:
        fake_user_manager_->AddKioskAppUser(account_id_);
        return;
      case user_manager::UserType::kWebKioskApp:
        fake_user_manager_->AddWebKioskAppUser(account_id_);
        return;
      case user_manager::UserType::kKioskIWA:
        fake_user_manager_->AddKioskIwaUser(account_id_);
        return;
      case user_manager::UserType::kChild:
        fake_user_manager_->AddChildUser(account_id_);
        return;
      case user_manager::UserType::kGuest:
        fake_user_manager_->AddGuestUser();
        return;
    }
  }

  bool IsGaiaAccount() const {
    return account_id_.GetAccountType() == AccountType::GOOGLE;
  }

  raw_ptr<ash::FakeChromeUserManager> fake_user_manager_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  const AccountId account_id_;
};

}  // namespace

class ChromeAssistantUtilTest : public testing::Test {
 public:
  ChromeAssistantUtilTest() = default;

  ChromeAssistantUtilTest(const ChromeAssistantUtilTest&) = delete;
  ChromeAssistantUtilTest& operator=(const ChromeAssistantUtilTest&) = delete;

  ~ChromeAssistantUtilTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &local_state_);
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile(
        kTestProfileName, /*prefs=*/{}, kTestProfileName16,
        /*avatar_id=*/0,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    ui::DeviceDataManager::CreateInstance();
  }

  void TearDown() override {
    ui::DeviceDataManager::DeleteInstance();
    identity_test_env_adaptor_.reset();
    profile_manager_->DeleteTestingProfile(kTestProfileName);
    profile_ = nullptr;
    profile_manager_.reset();
    fake_user_manager_.Reset();
  }

  TestingProfile* profile() { return profile_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return fake_user_manager_.Get();
  }

  AccountId GetActiveDirectoryUserAccountId(const TestingProfile* profile) {
    return AccountId::AdFromUserEmailObjGuid(profile->GetProfileUserName(),
                                             "<obj_guid>");
  }

  AccountId GetNonGaiaUserAccountId(const TestingProfile* profile) {
    return AccountId::FromUserEmail(profile->GetProfileUserName());
  }

  AccountId GetGaiaUserAccountId(const TestingProfile* profile) {
    return AccountId::FromUserEmailGaiaId(profile->GetProfileUserName(),
                                          kTestGaiaId);
  }

  AccountId GetGaiaUserAccountId(const std::string& user_name,
                                 const std::string& gaia_id) {
    return AccountId::FromUserEmailGaiaId(user_name, gaia_id);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  // Owned by |profile_manager_|
  raw_ptr<TestingProfile, DanglingUntriaged> profile_ = nullptr;
};

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_PrimaryUser) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetGaiaUserAccountId(profile()));

  EXPECT_EQ(ash::assistant::AssistantAllowedState::ALLOWED,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_SecondaryUser) {
  ScopedLogIn secondary_user_login(
      GetFakeUserManager(), identity_test_env(),
      GetGaiaUserAccountId("user2@gmail.com", "0123456789"));
  ScopedLogIn primary_user_login(GetFakeUserManager(), identity_test_env(),
                                 GetGaiaUserAccountId(profile()));

  EXPECT_EQ(
      ash::assistant::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER,
      IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_ChildUser) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetGaiaUserAccountId(profile()),
                    user_manager::UserType::kChild);

  EXPECT_EQ(ash::assistant::AssistantAllowedState::ALLOWED,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_GuestUser) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    user_manager::GuestAccountId(),
                    user_manager::UserType::kGuest);

  EXPECT_EQ(
      ash::assistant::AssistantAllowedState::DISALLOWED_BY_NONPRIMARY_USER,
      IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_Locale) {
  profile()->GetTestingPrefService()->SetString(
      language::prefs::kApplicationLocale, "he");
  UErrorCode error_code = U_ZERO_ERROR;
  const icu::Locale& old_locale = icu::Locale::getDefault();
  icu::Locale::setDefault(icu::Locale("he"), error_code);
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetGaiaUserAccountId(profile()));

  EXPECT_EQ(ash::assistant::AssistantAllowedState::DISALLOWED_BY_LOCALE,
            IsAssistantAllowedForProfile(profile()));
  icu::Locale::setDefault(old_locale, error_code);
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_DemoMode) {
  ash::DemoSession::SetDemoConfigForTesting(
      ash::DemoSession::DemoModeConfig::kOnline);
  profile()->ScopedCrosSettingsTestHelper()->InstallAttributes()->SetDemoMode();
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetNonGaiaUserAccountId(profile()),
                    user_manager::UserType::kPublicAccount);
  EXPECT_EQ(ash::assistant::AssistantAllowedState::DISALLOWED_BY_DEMO_MODE,
            IsAssistantAllowedForProfile(profile()));

  ash::DemoSession::SetDemoConfigForTesting(
      ash::DemoSession::DemoModeConfig::kNone);
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_PublicSession) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetNonGaiaUserAccountId(profile()),
                    user_manager::UserType::kPublicAccount);
  EXPECT_EQ(ash::assistant::AssistantAllowedState::DISALLOWED_BY_PUBLIC_SESSION,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_NonGmail) {
  ScopedLogIn login(
      GetFakeUserManager(), identity_test_env(),
      GetGaiaUserAccountId("user2@someotherdomain.com", "0123456789"));

  EXPECT_EQ(ash::assistant::AssistantAllowedState::DISALLOWED_BY_ACCOUNT_TYPE,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForProfile_GoogleMail) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetGaiaUserAccountId("user2@googlemail.com", "0123456789"));

  EXPECT_EQ(ash::assistant::AssistantAllowedState::ALLOWED,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest,
       IsAssistantAllowed_AllowsNonGmailOnGoogleBrandedDevices) {
  ScopedLogIn login(
      GetFakeUserManager(), identity_test_env(),
      GetGaiaUserAccountId("user2@someotherdomain.com", "0123456789"));

  ScopedSpoofGoogleBrandedDevice make_google_branded_device;
  EXPECT_EQ(ash::assistant::AssistantAllowedState::ALLOWED,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForKiosk_KioskApp) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetNonGaiaUserAccountId(profile()),
                    user_manager::UserType::kKioskApp);

  EXPECT_EQ(ash::assistant::AssistantAllowedState::DISALLOWED_BY_KIOSK_MODE,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowedForKiosk_WebKioskApp) {
  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetNonGaiaUserAccountId(profile()),
                    user_manager::UserType::kWebKioskApp);

  EXPECT_EQ(ash::assistant::AssistantAllowedState::DISALLOWED_BY_KIOSK_MODE,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowed_DLCEnabled) {
  feature_list_.InitAndEnableFeature(
      ash::assistant::features::kEnableLibAssistantDLC);

  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetGaiaUserAccountId("user2@googlemail.com", "0123456789"));

  EXPECT_EQ(ash::assistant::AssistantAllowedState::ALLOWED,
            IsAssistantAllowedForProfile(profile()));
}

TEST_F(ChromeAssistantUtilTest, IsAssistantAllowed_DLCDisabled) {
  feature_list_.InitAndDisableFeature(
      ash::assistant::features::kEnableLibAssistantDLC);

  ScopedLogIn login(GetFakeUserManager(), identity_test_env(),
                    GetGaiaUserAccountId("user2@googlemail.com", "0123456789"));

  EXPECT_EQ(ash::assistant::AssistantAllowedState::DISALLOWED_BY_NO_BINARY,
            IsAssistantAllowedForProfile(profile()));
}

}  // namespace assistant
