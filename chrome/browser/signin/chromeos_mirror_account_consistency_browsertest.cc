// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/post_task.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/test/browser_test.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kGaiaUrl[] = "https://accounts.google.com";
constexpr char kChromeConnectedHeader[] = "X-Chrome-Connected";
constexpr char kUserEmail[] = "user@gmail.com";
constexpr char kUserGaiaId[] = "1234567890";

void CheckRequestHeader(net::URLRequest* url_request,
                        const char* header_name,
                        const std::string& expected_header_value) {
  bool expected_has_header = !expected_header_value.empty();
  std::string actual_header_value;
  EXPECT_EQ(expected_has_header, url_request->extra_request_headers().GetHeader(
                                     header_name, &actual_header_value))
      << header_name << ": " << actual_header_value;
  if (expected_has_header) {
    EXPECT_EQ(expected_header_value, actual_header_value);
  }
}

void TestMirrorRequestForProfileOnIOThread(
    ProfileIOData* profile_io,
    const std::string& expected_header_value) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  ASSERT_TRUE(profile_io->GetMainRequestContext());
  std::unique_ptr<net::URLRequest> request =
      profile_io->GetMainRequestContext()->CreateRequest(
          GURL(kGaiaUrl), net::DEFAULT_PRIORITY, nullptr,
          TRAFFIC_ANNOTATION_FOR_TESTS);
  signin::ChromeRequestAdapter signin_request_adapter(request.get());
  signin::FixAccountConsistencyRequestHeader(&signin_request_adapter, GURL(),
                                             profile_io);

  CheckRequestHeader(request.get(), kChromeConnectedHeader,
                     expected_header_value);
}

// Checks whether the "X-Chrome-Connected" header of a new request to Google
// contains |expected_header_value|.
void TestMirrorRequestForProfile(Profile* profile,
                                 const std::string& expected_header_value) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ProfileIOData* profile_io =
      ProfileIOData::FromResourceContext(profile->GetResourceContext());

  base::RunLoop run_loop;
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(TestMirrorRequestForProfileOnIOThread, profile_io,
                     expected_header_value),
      run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

// This is a Chrome OS-only test ensuring that mirror account consistency is
// enabled for child accounts, but not enabled for other account types.
class ChromeOsMirrorAccountConsistencyTest : public chromeos::LoginManagerTest {
 protected:
  ~ChromeOsMirrorAccountConsistencyTest() override {}

  ChromeOsMirrorAccountConsistencyTest()
      : LoginManagerTest(false, true /* should_initialize_webui */),
        account_id_(AccountId::FromUserEmailGaiaId(kUserEmail, kUserGaiaId)) {}

  const AccountId account_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeOsMirrorAccountConsistencyTest);
};

IN_PROC_BROWSER_TEST_F(ChromeOsMirrorAccountConsistencyTest,
                       PRE_TestMirrorRequestChromeOsChildAccount) {
  RegisterUser(account_id_);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Mirror is enabled for child accounts.
IN_PROC_BROWSER_TEST_F(ChromeOsMirrorAccountConsistencyTest,
                       TestMirrorRequestChromeOsChildAccount) {
  // Child user.
  LoginUser(account_id_);

  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  ASSERT_EQ(user, user_manager::UserManager::Get()->GetPrimaryUser());
  ASSERT_EQ(user, user_manager::UserManager::Get()->FindUser(account_id_));
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);

  // On Chrome OS this is false.
  ASSERT_FALSE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile));

  // Require account consistency.
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(profile);
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kAccountConsistencyMirrorRequired,
      std::make_unique<base::Value>(true));
  supervised_user_settings_service->SetActive(true);

  // Incognito is always disabled for child accounts.
  PrefService* prefs = profile->GetPrefs();
  prefs->SetInteger(prefs::kIncognitoModeAvailability,
                    IncognitoModePrefs::DISABLED);
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccountConsistencyMirrorRequired));

  ASSERT_EQ(3, signin::PROFILE_MODE_INCOGNITO_DISABLED |
                   signin::PROFILE_MODE_ADD_ACCOUNT_DISABLED);
  TestMirrorRequestForProfile(profile,
                              "mode=3,enable_account_consistency=true");
}

IN_PROC_BROWSER_TEST_F(ChromeOsMirrorAccountConsistencyTest,
                       PRE_TestMirrorRequestChromeOsNotChildAccount) {
  RegisterUser(account_id_);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Mirror is not enabled for non-child accounts.
IN_PROC_BROWSER_TEST_F(ChromeOsMirrorAccountConsistencyTest,
                       TestMirrorRequestChromeOsNotChildAccount) {
  // Not a child user.
  LoginUser(account_id_);

  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  ASSERT_EQ(user, user_manager::UserManager::Get()->GetPrimaryUser());
  ASSERT_EQ(user, user_manager::UserManager::Get()->FindUser(account_id_));
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);

  // On Chrome OS this is false.
  ASSERT_FALSE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile));

  PrefService* prefs = profile->GetPrefs();
  ASSERT_FALSE(prefs->GetBoolean(prefs::kAccountConsistencyMirrorRequired));

  TestMirrorRequestForProfile(profile, "");
}
