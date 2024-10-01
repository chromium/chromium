// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

char kRefreshToken1[] = "refresh_token_1";
char kRefreshToken2[] = "refresh_token_2";
const base::FilePath::CharType kRefreshTokenToDeviceIdMapFile[] =
    FILE_PATH_LITERAL("refrest_token_to_device_id.json");

char kSecondUserEmail[] = "second_user@gmail.com";
char kSecondUserPassword[] = "password";
char kSecondUserGaiaId[] = "4321";
char kSecondUserRefreshToken1[] = "refresh_token_second_user_1";
char kSecondUserRefreshToken2[] = "refresh_token_second_user_2";

}  // namespace

class DeviceIDTest : public OobeBaseTest,
                     public user_manager::UserManager::Observer {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
  }

  void SetUpOnMainThread() override {
    user_removal_signal_ = std::make_unique<base::test::TestFuture<void>>();
    OobeBaseTest::SetUpOnMainThread();
    LoadRefreshTokenToDeviceIdMap();
    user_manager::UserManager::Get()->AddObserver(this);
  }

  void TearDownOnMainThread() override {
    user_manager::UserManager::Get()->RemoveObserver(this);
    SaveRefreshTokenToDeviceIdMap();
    OobeBaseTest::TearDownOnMainThread();
  }

  std::string GetDeviceId(const AccountId& account_id) {
    user_manager::KnownUser known_user(g_browser_process->local_state());
    return known_user.GetDeviceId(account_id);
  }

  std::string GetDeviceIdFromProfile(const AccountId& account_id) {
    return GetSigninScopedDeviceIdForProfile(
        ProfileHelper::Get()->GetProfileByUser(
            user_manager::UserManager::Get()->FindUser(account_id)));
  }

  std::string GetDeviceIdFromGAIA(const std::string& refresh_token) {
    return fake_gaia_.fake_gaia()->GetDeviceIdByRefreshToken(refresh_token);
  }

  // Checks that user's device ID retrieved from UserManager and Profile are the
  // same.
  // If `refresh_token` is not empty, checks that device ID associated with the
  // `refresh_token` in GAIA is the same as ID saved on device.
  void CheckDeviceIDIsConsistent(const AccountId& account_id,
                                 const std::string& refresh_token) {
    const std::string device_id_in_profile = GetDeviceIdFromProfile(account_id);
    const std::string device_id_in_local_state = GetDeviceId(account_id);

    EXPECT_FALSE(device_id_in_profile.empty());
    EXPECT_EQ(device_id_in_profile, device_id_in_local_state);

    if (!refresh_token.empty()) {
      const std::string device_id_in_gaia = GetDeviceIdFromGAIA(refresh_token);
      EXPECT_EQ(device_id_in_profile, device_id_in_gaia);
    }
  }

  // This is a helper function to online login the user using fake gaia mixin.
  // Preconditions:
  //  - GaiaScreen should be shown.
  // Postconditions:
  //  - Install attributes for the user exist.
  //  - User session starts.
  void SignInOnline(const std::string& user_id,
                    const std::string& password,
                    const std::string& refresh_token,
                    const std::string& gaia_id) {
    cryptohome_mixin_.ApplyAuthConfigIfUserExists(
        AccountId::FromUserEmail(user_id),
        test::UserAuthConfig::Create(test::kDefaultAuthSetup));

    OobeScreenWaiter(GaiaView::kScreenId).Wait();
    // On a real device the first user would create the install attributes file,
    // emulate that, so the following users don't try to establish ownership.
    EnsureInstallAttributesCreated();

    FakeGaia::Configuration params;
    params.email = user_id;
    params.refresh_token = refresh_token;
    fake_gaia_.fake_gaia()->UpdateConfiguration(params);
    fake_gaia_.fake_gaia()->MapEmailToGaiaId(user_id, gaia_id);

    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(user_id, password, "[]");

    test::WaitForPrimaryUserSessionStart();
  }

  void SignInOffline(const std::string& user_id, const std::string& password) {
    cryptohome_mixin_.ApplyAuthConfigIfUserExists(
        AccountId::FromUserEmail(user_id),
        test::UserAuthConfig::Create(test::kDefaultAuthSetup));

    LoginScreenTestApi::SubmitPassword(AccountId::FromUserEmail(user_id),
                                       FakeGaiaMixin::kFakeUserPassword,
                                       false /* check_if_submittable */);
    test::WaitForPrimaryUserSessionStart();
  }

  void RemoveUser(const AccountId& account_id) {
    ASSERT_TRUE(LoginScreenTestApi::RemoveUser(account_id));
    EXPECT_TRUE(user_removal_signal_->Wait());
  }

 private:
  void LocalStateChanged(user_manager::UserManager* manager) override {
    if (user_removal_signal_ && !user_removal_signal_->IsReady()) {
      user_removal_signal_->SetValue();
    }
  }

  base::FilePath GetRefreshTokenToDeviceIdMapFilePath() const {
    return base::CommandLine::ForCurrentProcess()
        ->GetSwitchValuePath(::switches::kUserDataDir)
        .Append(kRefreshTokenToDeviceIdMapFile);
  }

  void LoadRefreshTokenToDeviceIdMap() {
    std::string file_contents;
    if (!base::ReadFileToString(GetRefreshTokenToDeviceIdMapFilePath(),
                                &file_contents))
      return;
    std::optional<base::Value> value = base::JSONReader::Read(file_contents);
    EXPECT_TRUE(value->is_dict());
    base::Value::Dict& dictionary = value->GetDict();
    FakeGaia::RefreshTokenToDeviceIdMap map;
    for (auto item : dictionary) {
      ASSERT_TRUE(item.second.is_string());
      map[item.first] = item.second.GetString();
    }
    fake_gaia_.fake_gaia()->SetRefreshTokenToDeviceIdMap(map);
  }

  void SaveRefreshTokenToDeviceIdMap() {
    base::Value::Dict dictionary;
    for (const auto& kv :
         fake_gaia_.fake_gaia()->refresh_token_to_device_id_map())
      dictionary.Set(kv.first, kv.second);
    std::string json;
    EXPECT_TRUE(base::JSONWriter::Write(dictionary, &json));
    EXPECT_TRUE(base::WriteFile(GetRefreshTokenToDeviceIdMapFilePath(), json));
  }

  void EnsureInstallAttributesCreated() {
    base::FilePath install_attrs_path = base::PathService::CheckedGet(
        chromeos::dbus_paths::FILE_INSTALL_ATTRIBUTES);
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!base::PathExists(install_attrs_path)) {
      EXPECT_TRUE(
          base::WriteFile(install_attrs_path, "fake_install_attributes_data"));
    }
  }

  std::unique_ptr<base::test::TestFuture<void>> user_removal_signal_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
};

// Add the first user and check that device ID is consistent.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, PRE_PRE_PRE_PRE_PRE_NewUsers) {
  LoginDisplayHost::default_host()
      ->GetWizardController()
      ->SkipToLoginForTesting();
  SignInOnline(FakeGaiaMixin::kFakeUserEmail, FakeGaiaMixin::kFakeUserPassword,
               kRefreshToken1, FakeGaiaMixin::kFakeUserGaiaId);
  CheckDeviceIDIsConsistent(
      AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail), kRefreshToken1);
}

// Authenticate the first user through GAIA and verify that device ID remains
// the same.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, PRE_PRE_PRE_PRE_NewUsers) {
  const std::string device_id =
      GetDeviceId(AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail));
  EXPECT_FALSE(device_id.empty());
  EXPECT_EQ(device_id, GetDeviceIdFromGAIA(kRefreshToken1));

  ASSERT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  test::TapForPersonalUseCrRadioButton();
  test::TapUserCreationNext();
  SignInOnline(FakeGaiaMixin::kFakeUserEmail, FakeGaiaMixin::kFakeUserPassword,
               kRefreshToken2, FakeGaiaMixin::kFakeUserGaiaId);
  CheckDeviceIDIsConsistent(
      AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail), kRefreshToken2);

  CHECK_EQ(
      device_id,
      GetDeviceId(AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail)));
}

// Authenticate the first user offline and verify that device ID remains
// the same.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, PRE_PRE_PRE_NewUsers) {
  const std::string device_id =
      GetDeviceId(AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail));
  EXPECT_FALSE(device_id.empty());

  SignInOffline(FakeGaiaMixin::kFakeUserEmail,
                FakeGaiaMixin::kFakeUserPassword);
  CheckDeviceIDIsConsistent(
      AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail), kRefreshToken2);

  // Verify that device ID remained the same after offline auth.
  CHECK_EQ(
      device_id,
      GetDeviceId(AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail)));
}

// Add the second user.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, PRE_PRE_NewUsers) {
  ASSERT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  test::TapForPersonalUseCrRadioButton();
  test::TapUserCreationNext();
  SignInOnline(kSecondUserEmail, kSecondUserPassword, kSecondUserRefreshToken1,
               kSecondUserGaiaId);
  CheckDeviceIDIsConsistent(AccountId::FromUserEmail(kSecondUserEmail),
                            kSecondUserRefreshToken1);
}

// Remove the second user.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, PRE_NewUsers) {
  RemoveUser(AccountId::FromUserEmail(kSecondUserEmail));
}

IN_PROC_BROWSER_TEST_F(DeviceIDTest, NewUsers) {
  EXPECT_TRUE(GetDeviceId(AccountId::FromUserEmail(kSecondUserEmail)).empty());
  ASSERT_TRUE(LoginScreenTestApi::ClickAddUserButton());
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  test::TapForPersonalUseCrRadioButton();
  test::TapUserCreationNext();
  SignInOnline(kSecondUserEmail, kSecondUserPassword, kSecondUserRefreshToken2,
               kSecondUserGaiaId);
  CheckDeviceIDIsConsistent(AccountId::FromUserEmail(kSecondUserEmail),
                            kSecondUserRefreshToken2);
  EXPECT_NE(GetDeviceIdFromGAIA(kSecondUserRefreshToken1),
            GetDeviceId(AccountId::FromUserEmail(kSecondUserEmail)));
}

// Set up a user that has a device ID stored in preference only.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, PRE_Migration) {
  LoginDisplayHost::default_host()
      ->GetWizardController()
      ->SkipToLoginForTesting();
  SignInOnline(FakeGaiaMixin::kFakeUserEmail, FakeGaiaMixin::kFakeUserPassword,
               kRefreshToken1, FakeGaiaMixin::kFakeUserGaiaId);

  // Simulate user that has device ID saved only in preferences (pre-M44).
  PrefService* prefs =
      ProfileHelper::Get()
          ->GetProfileByUser(user_manager::UserManager::Get()->GetActiveUser())
          ->GetPrefs();
  prefs->SetString(
      prefs::kGoogleServicesSigninScopedDeviceId,
      GetDeviceId(AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail)));

  // Can't use SetKnownUserDeviceId here, because it forbids changing a device
  // ID.
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetStringPref(
      AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail), "device_id",
      std::string());
}

// Tests that after the first sign in the device ID has been moved to the Local
// state.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, Migration) {
  EXPECT_TRUE(
      GetDeviceId(AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail))
          .empty());
  SignInOffline(FakeGaiaMixin::kFakeUserEmail,
                FakeGaiaMixin::kFakeUserPassword);
  CheckDeviceIDIsConsistent(
      AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail), kRefreshToken1);
}

// Set up a user that doesn't have a device ID.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, PRE_LegacyUsers) {
  LoginDisplayHost::default_host()
      ->GetWizardController()
      ->SkipToLoginForTesting();
  SignInOnline(FakeGaiaMixin::kFakeUserEmail, FakeGaiaMixin::kFakeUserPassword,
               kRefreshToken1, FakeGaiaMixin::kFakeUserGaiaId);

  PrefService* prefs =
      ProfileHelper::Get()
          ->GetProfileByUser(user_manager::UserManager::Get()->GetActiveUser())
          ->GetPrefs();
  EXPECT_TRUE(
      prefs->GetString(prefs::kGoogleServicesSigninScopedDeviceId).empty());

  // Can't use SetKnownUserDeviceId here, because it forbids changing a device
  // ID.
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetStringPref(
      AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail), "device_id",
      std::string());
}

// Tests that device ID has been generated after the first sign in.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, LegacyUsers) {
  EXPECT_TRUE(
      GetDeviceId(AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail))
          .empty());
  SignInOffline(FakeGaiaMixin::kFakeUserEmail,
                FakeGaiaMixin::kFakeUserPassword);
  // Last param `auth_code` is empty, because we don't pass a device ID to GAIA
  // in this case.
  CheckDeviceIDIsConsistent(
      AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail), std::string());
}

}  // namespace ash
