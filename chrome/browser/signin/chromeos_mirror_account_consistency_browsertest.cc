// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/google/core/common/google_switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kGaiaDomain[] = "accounts.google.com";

// Checks whether the "X-Chrome-Connected" header of a new request to Google
// contains |expected_header_value|.
void TestMirrorRequestForProfile(net::EmbeddedTestServer* test_server,
                                 Profile* profile,
                                 const std::string& expected_header_value) {
  GURL gaia_url(test_server->GetURL("/echoheader?X-Chrome-Connected"));
  GURL::Replacements replace_host;
  replace_host.SetHostStr(kGaiaDomain);
  gaia_url = gaia_url.ReplaceComponents(replace_host);

  Browser* browser = Browser::Create(Browser::CreateParams(profile, true));
  ui_test_utils::NavigateToURLWithDisposition(
      browser, gaia_url, WindowOpenDisposition::SINGLETON_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  std::string inner_text =
      content::EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                      "document.body.innerText;")
          .ExtractString();
  // /echoheader returns "None" if the header isn't set.
  inner_text = (inner_text == "None") ? "" : inner_text;
  EXPECT_EQ(expected_header_value, inner_text);
}

}  // namespace

// This is a Chrome OS-only test ensuring that mirror account consistency is
// enabled for child accounts, but not enabled for other account types.
class ChromeOsMirrorAccountConsistencyTest : public ash::LoginManagerTest {
 public:
  ChromeOsMirrorAccountConsistencyTest(
      const ChromeOsMirrorAccountConsistencyTest&) = delete;
  ChromeOsMirrorAccountConsistencyTest& operator=(
      const ChromeOsMirrorAccountConsistencyTest&) = delete;

 protected:
  ~ChromeOsMirrorAccountConsistencyTest() override {}

  ChromeOsMirrorAccountConsistencyTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(1);
    account_id_ = login_mixin_.users()[0].account_id;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ash::LoginManagerTest::SetUpCommandLine(command_line);

    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "www.google.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

    // The production code only allows known ports (80 for http and 443 for
    // https), but the test server runs on a random port.
    command_line->AppendSwitch(switches::kIgnoreGooglePortNumbers);
  }

  void SetUpOnMainThread() override {
    // We can't use BrowserTestBase's EmbeddedTestServer because google.com
    // URL's have to be https.
    test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    net::test_server::RegisterDefaultHandlers(test_server_.get());
    ASSERT_TRUE(test_server_->Start());

    ash::LoginManagerTest::SetUpOnMainThread();
  }

  AccountId account_id_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};

 protected:
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
};

// Mirror is enabled for child accounts.
IN_PROC_BROWSER_TEST_F(ChromeOsMirrorAccountConsistencyTest,
                       TestMirrorRequestChromeOsChildAccount) {
  // Child user.
  LoginUser(account_id_);

  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  ASSERT_EQ(user, user_manager::UserManager::Get()->GetPrimaryUser());
  ASSERT_EQ(user, user_manager::UserManager::Get()->FindUser(account_id_));
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);

  // Supervised flag uses `FindExtendedAccountInfoForAccountWithRefreshToken`,
  // so wait for tokens to be loaded.
  signin::WaitForRefreshTokensLoaded(
      IdentityManagerFactory::GetForProfile(profile));

  supervised_user::SupervisedUserSettingsService*
      supervised_user_settings_service =
          SupervisedUserSettingsServiceFactory::GetForKey(
              profile->GetProfileKey());
  supervised_user_settings_service->SetActive(true);

  // Incognito is always disabled for child accounts.
  PrefService* prefs = profile->GetPrefs();
  prefs->SetInteger(
      policy::policy_prefs::kIncognitoModeAvailability,
      static_cast<int>(policy::IncognitoModeAvailability::kDisabled));
  ASSERT_EQ(1, signin::PROFILE_MODE_INCOGNITO_DISABLED);

  // TODO(http://crbug.com/1134144): This test seems to test supervised profiles
  // instead of child accounts. With the current implementation,
  // X-Chrome-Connected header gets a supervised=true argument only for child
  // profiles. Verify if these tests needs to be updated to use child accounts
  // or whether supervised profiles need to be supported as well.
  TestMirrorRequestForProfile(
      test_server_.get(), profile,
      "source=Chrome,mode=1,enable_account_consistency=true,supervised=false,"
      "consistency_enabled_by_default=false");
}

// Mirror is enabled for non-child accounts.
IN_PROC_BROWSER_TEST_F(ChromeOsMirrorAccountConsistencyTest,
                       TestMirrorRequestChromeOsNotChildAccount) {
  // Not a child user.
  LoginUser(account_id_);

  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  ASSERT_EQ(user, user_manager::UserManager::Get()->GetPrimaryUser());
  ASSERT_EQ(user, user_manager::UserManager::Get()->FindUser(account_id_));
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);

  // Supervised flag uses `FindExtendedAccountInfoForAccountWithRefreshToken`,
  // so wait for tokens to be loaded.
  signin::WaitForRefreshTokensLoaded(
      IdentityManagerFactory::GetForProfile(profile));

  // With Chrome OS Account Manager enabled, this should be true.
  EXPECT_TRUE(
      AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile));
  TestMirrorRequestForProfile(
      test_server_.get(), profile,
      "source=Chrome,mode=0,enable_account_consistency=true,supervised=false,"
      "consistency_enabled_by_default=false");
}
