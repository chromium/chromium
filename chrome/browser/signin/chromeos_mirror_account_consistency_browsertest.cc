// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/google/core/common/google_util.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kGaiaDomain[] = "accounts.google.com";
constexpr char kUserEmail[] = "user@gmail.com";
constexpr char kUserGaiaId[] = "1234567890";

// Checks whether the "X-Chrome-Connected" header of a new request to Google
// contains |expected_header_value|.
void TestMirrorRequestForProfile(net::EmbeddedTestServer* test_server,
                                 Profile* profile,
                                 const std::string& expected_header_value) {
  GURL gaia_url(test_server->GetURL("/echoheader?X-Chrome-Connected"));
  GURL::Replacements replace_host;
  replace_host.SetHostStr(kGaiaDomain);
  gaia_url = gaia_url.ReplaceComponents(replace_host);

  Browser* browser = new Browser(Browser::CreateParams(profile, true));
  ui_test_utils::NavigateToURLWithDisposition(
      browser, gaia_url, WindowOpenDisposition::SINGLETON_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  std::string inner_text;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser->tab_strip_model()->GetActiveWebContents(),
      "domAutomationController.send(document.body.innerText);", &inner_text));
  // /echoheader returns "None" if the header isn't set.
  inner_text = (inner_text == "None") ? "" : inner_text;
  EXPECT_EQ(expected_header_value, inner_text);
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    chromeos::LoginManagerTest::SetUpCommandLine(command_line);

    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "www.google.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    // The production code only allows known ports (80 for http and 443 for
    // https), but the test server runs on a random port.
    google_util::IgnorePortNumbersForGoogleURLChecksForTesting();

    // We can't use BrowserTestBase's EmbeddedTestServer because google.com
    // URL's have to be https.
    test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    net::test_server::RegisterDefaultHandlers(test_server_.get());
    ASSERT_TRUE(test_server_->Start());

    chromeos::LoginManagerTest::SetUpOnMainThread();
  }

  const AccountId account_id_;

 protected:
  std::unique_ptr<net::EmbeddedTestServer> test_server_;

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

  // Require account consistency.
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForKey(profile->GetProfileKey());
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
  TestMirrorRequestForProfile(test_server_.get(), profile,
                              "mode=3,enable_account_consistency=true,"
                              "consistency_enabled_by_default=false");
}

IN_PROC_BROWSER_TEST_F(ChromeOsMirrorAccountConsistencyTest,
                       PRE_TestMirrorRequestChromeOsNotChildAccount) {
  RegisterUser(account_id_);
  chromeos::StartupUtils::MarkOobeCompleted();
}

// Mirror is enabled for non-child accounts.
IN_PROC_BROWSER_TEST_F(ChromeOsMirrorAccountConsistencyTest,
                       TestMirrorRequestChromeOsNotChildAccount) {
  // Not a child user.
  LoginUser(account_id_);

  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  ASSERT_EQ(user, user_manager::UserManager::Get()->GetPrimaryUser());
  ASSERT_EQ(user, user_manager::UserManager::Get()->FindUser(account_id_));
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);

  // With Chrome OS Account Manager enabled, this should be true.
  EXPECT_TRUE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile));
  TestMirrorRequestForProfile(test_server_.get(), profile,
                              "mode=0,enable_account_consistency=true,"
                              "consistency_enabled_by_default=false");
}
