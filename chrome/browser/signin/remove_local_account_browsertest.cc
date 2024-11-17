// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/fake_gaia.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#endif

namespace {

using testing::Contains;
using testing::Not;

MATCHER_P(ListedAccountMatchesGaiaId, gaia_id, "") {
  return arg.gaia_id == std::string(gaia_id);
}

const char kTestGaiaId[] = "123";

class RemoveLocalAccountTest : public MixinBasedInProcessBrowserTest {
 protected:
  RemoveLocalAccountTest()
      : embedded_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    embedded_test_server_.RegisterRequestHandler(base::BindRepeating(
        &FakeGaia::HandleRequest, base::Unretained(&fake_gaia_)));
  }

  ~RemoveLocalAccountTest() override = default;

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  signin::AccountsInCookieJarInfo WaitUntilAccountsInCookieUpdated() {
    signin::TestIdentityManagerObserver observer(identity_manager());
    base::RunLoop run_loop;
    observer.SetOnAccountsInCookieUpdatedCallback(run_loop.QuitClosure());
    run_loop.Run();
    return observer.AccountsInfoFromAccountsInCookieUpdatedCallback();
  }

  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(embedded_test_server_.InitializeAndListen());
    const GURL base_url = embedded_test_server_.base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    fake_gaia_.Initialize();

    FakeGaia::Configuration params;
    params.signed_out_gaia_ids.push_back(kTestGaiaId);
    fake_gaia_.UpdateConfiguration(params);

    embedded_test_server_.StartAcceptingConnections();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // `ChromeSigninClient` uses `ash::DelayNetworkCall()` which requires
    // simulating being online.
    network_portal_detector_.SimulateDefaultNetworkState(
        ash::NetworkPortalDetectorMixin::NetworkStatus::kOnline);
#endif
  }

  FakeGaia fake_gaia_;
  net::EmbeddedTestServer embedded_test_server_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};
#endif
};

IN_PROC_BROWSER_TEST_F(RemoveLocalAccountTest, ShouldNotifyObservers) {
  // To enforce an initial ListAccounts fetch and the corresponding notification
  // to observers, make the current list as stale. This is done for the purpose
  // of documenting assertions on the AccountsInCookieJarInfo passed to
  // observers during notification.
  signin::SetFreshnessOfAccountsInGaiaCookie(identity_manager(),
                                             /*accounts_are_fresh=*/false);

  ASSERT_FALSE(identity_manager()->GetAccountsInCookieJar().AreAccountsFresh());
  const signin::AccountsInCookieJarInfo
      cookie_jar_info_in_initial_notification =
          WaitUntilAccountsInCookieUpdated();
  ASSERT_TRUE(cookie_jar_info_in_initial_notification.AreAccountsFresh());
  ASSERT_THAT(cookie_jar_info_in_initial_notification.GetSignedOutAccounts(),
              Contains(ListedAccountMatchesGaiaId(kTestGaiaId)));

  const signin::AccountsInCookieJarInfo initial_cookie_jar_info =
      identity_manager()->GetAccountsInCookieJar();
  ASSERT_TRUE(initial_cookie_jar_info.AreAccountsFresh());
  ASSERT_THAT(initial_cookie_jar_info.GetSignedOutAccounts(),
              Contains(ListedAccountMatchesGaiaId(kTestGaiaId)));

  // Open a FakeGaia page that issues the desired HTTP response header with
  // Google-Accounts-RemoveLocalAccount.
  chrome::AddTabAt(browser(),
                   fake_gaia_.GetFakeRemoveLocalAccountURL(kTestGaiaId),
                   /*index=*/0,
                   /*foreground=*/true);

  // Wait until observers are notified with OnAccountsInCookieUpdated().
  const signin::AccountsInCookieJarInfo
      cookie_jar_info_in_updated_notification =
          WaitUntilAccountsInCookieUpdated();

  EXPECT_TRUE(cookie_jar_info_in_updated_notification.AreAccountsFresh());
  EXPECT_THAT(cookie_jar_info_in_updated_notification.GetSignedOutAccounts(),
              Not(Contains(ListedAccountMatchesGaiaId(kTestGaiaId))));

  const signin::AccountsInCookieJarInfo updated_cookie_jar_info =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_TRUE(updated_cookie_jar_info.AreAccountsFresh());
  EXPECT_THAT(updated_cookie_jar_info.GetSignedOutAccounts(),
              Not(Contains(ListedAccountMatchesGaiaId(kTestGaiaId))));
}

}  // namespace
