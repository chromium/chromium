// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/session_manager_state_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/remove_user_delegate.h"
#include "components/user_manager/user_manager.h"

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

namespace chromeos {

class DeviceIDTest : public OobeBaseTest,
                     public user_manager::RemoveUserDelegate {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
    command_line->AppendSwitch(ash::switches::kShowWebUiLogin);
  }

  void SetUpOnMainThread() override {
    user_removal_loop_.reset(new base::RunLoop);
    OobeBaseTest::SetUpOnMainThread();
    LoadRefreshTokenToDeviceIdMap();
  }

  void TearDownOnMainThread() override {
    SaveRefreshTokenToDeviceIdMap();
    OobeBaseTest::TearDownOnMainThread();
  }

  std::string GetDeviceId(const AccountId& account_id) {
    return user_manager::known_user::GetDeviceId(account_id);
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
  // If |refresh_token| is not empty, checks that device ID associated with the
  // |refresh_token| in GAIA is the same as ID saved on device.
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

  void SignInOnline(const std::string& user_id,
                    const std::string& password,
                    const std::string& refresh_token,
                    const std::string& gaia_id) {
    WaitForGaiaPageLoad();

    FakeGaia::MergeSessionParams params;
    params.email = user_id;
    params.refresh_token = refresh_token;
    fake_gaia_.fake_gaia()->UpdateMergeSessionParams(params);
    fake_gaia_.fake_gaia()->MapEmailToGaiaId(user_id, gaia_id);

    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(user_id, password, "[]");

    test::WaitForPrimaryUserSessionStart();
  }

  void SignInOffline(const std::string& user_id, const std::string& password) {
    WaitForSigninScreen();

    test::OobeJS().ExecuteAsync(base::StringPrintf(
        "chrome.send('authenticateUser', ['%s', '%s', false])", user_id.c_str(),
        password.c_str()));
    test::WaitForPrimaryUserSessionStart();
  }

  void RemoveUser(const AccountId& account_id) {
    user_manager::UserManager::Get()->RemoveUser(account_id, this);
    user_removal_loop_->Run();
  }

 private:
  // user_manager::RemoveUserDelegate:
  void OnBeforeUserRemoved(const AccountId& account_id) override {}

  void OnUserRemoved(const AccountId& account_id) override {
    user_removal_loop_->Quit();
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
    std::unique_ptr<base::Value> value(
        base::JSONReader::ReadDeprecated(file_contents));
    base::DictionaryValue* dictionary;
    EXPECT_TRUE(value->GetAsDictionary(&dictionary));
    FakeGaia::RefreshTokenToDeviceIdMap map;
    for (base::DictionaryValue::Iterator it(*dictionary); !it.IsAtEnd();
         it.Advance()) {
      std::string device_id;
      EXPECT_TRUE(it.value().GetAsString(&device_id));
      map[it.key()] = device_id;
    }
    fake_gaia_.fake_gaia()->SetRefreshTokenToDeviceIdMap(map);
  }

  void SaveRefreshTokenToDeviceIdMap() {
    base::DictionaryValue dictionary;
    for (const auto& kv :
         fake_gaia_.fake_gaia()->refresh_token_to_device_id_map())
      dictionary.SetKey(kv.first, base::Value(kv.second));
    std::string json;
    EXPECT_TRUE(base::JSONWriter::Write(dictionary, &json));
    EXPECT_EQ(static_cast<int>(json.length()),
              base::WriteFile(GetRefreshTokenToDeviceIdMapFilePath(),
                              json.c_str(), json.length()));
  }

  std::unique_ptr<base::RunLoop> user_removal_loop_;
  FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
};

// Add the first user and check that device ID is consistent.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, PRE_PRE_PRE_PRE_PRE_NewUsers) {
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
  WaitForSigninScreen();
  test::OobeJS().ExecuteAsync("chrome.send('showAddUser')");
  SignInOnline(kSecondUserEmail, kSecondUserPassword, kSecondUserRefreshToken1,
               kSecondUserGaiaId);
  CheckDeviceIDIsConsistent(AccountId::FromUserEmail(kSecondUserEmail),
                            kSecondUserRefreshToken1);
}

// Remove the second user.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, PRE_NewUsers) {
  WaitForSigninScreen();
  RemoveUser(AccountId::FromUserEmail(kSecondUserEmail));
}

// Add the second user back. Verify that device ID has been changed.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, NewUsers) {
  EXPECT_TRUE(GetDeviceId(AccountId::FromUserEmail(kSecondUserEmail)).empty());
  SignInOnline(kSecondUserEmail, kSecondUserPassword, kSecondUserRefreshToken2,
               kSecondUserGaiaId);
  CheckDeviceIDIsConsistent(AccountId::FromUserEmail(kSecondUserEmail),
                            kSecondUserRefreshToken2);
  EXPECT_NE(GetDeviceIdFromGAIA(kSecondUserRefreshToken1),
            GetDeviceId(AccountId::FromUserEmail(kSecondUserEmail)));
}

// Set up a user that has a device ID stored in preference only.
IN_PROC_BROWSER_TEST_F(DeviceIDTest, PRE_Migration) {
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
  user_manager::known_user::SetStringPref(
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
  user_manager::known_user::SetStringPref(
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
  // Last param |auth_code| is empty, because we don't pass a device ID to GAIA
  // in this case.
  CheckDeviceIDIsConsistent(
      AccountId::FromUserEmail(FakeGaiaMixin::kFakeUserEmail), std::string());
}

}  // namespace chromeos
