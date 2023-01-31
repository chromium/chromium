// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/login_detection/password_store_sites.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::PasswordStoreInterface;
namespace login_detection {

class LoginDetectionPasswordStoreSitesBrowserTest
    : public InProcessBrowserTest {
 public:
  LoginDetectionPasswordStoreSitesBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &LoginDetectionPasswordStoreSitesBrowserTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    profile_password_store_ = CreateAndUseTestPasswordStore(context).get();
    account_password_store_ =
        CreateAndUseTestAccountPasswordStore(context).get();
  }

  void AddLoginForSite(password_manager::PasswordStoreInterface* password_store,
                       const GURL& url) {
    password_manager::PasswordForm form;
    form.scheme = password_manager::PasswordForm::Scheme::kHtml;
    form.url = url;
    form.signon_realm = "https://www.chrome.com";
    form.username_value = u"my_username";
    form.password_value = u"my_password";
    form.blocked_by_user = false;
    password_store->AddLogin(form);
    // GetLogins() blocks until reading on the background thread is finished
    // which guarantees that all other tasks on the background thread are
    // finished too.
    passwords_helper::GetLogins(password_store);
    // Once AddLogin() is done on the background thread, the store has posted
    // OnLoginsChanged() on the main thread. Make sure to process it before
    // proceeding.
    base::RunLoop().RunUntilIdle();
  }

  // Wait for the password store taskrunner to complete its event processing.
  void WaitForPasswordStoreUpdate(
      password_manager::PasswordStoreInterface* password_store) {
    // GetLogins() blocks until reading on the background thread is finished
    // which guarantees that all other tasks on the background thread are
    // finished too.
    passwords_helper::GetLogins(password_store);
    // At this point, the password store has completed its processing and posted
    // events (e.g. OnLoginsChanged() or OnGetPasswordStoreResults()) to the
    // main thread. Make sure to process such events before proceeding.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::HistogramTester histogram_tester;
  raw_ptr<PasswordStoreInterface, DanglingUntriaged> profile_password_store_ =
      nullptr;
  raw_ptr<PasswordStoreInterface, DanglingUntriaged> account_password_store_ =
      nullptr;
  base::CallbackListSubscription create_services_subscription_;
};

// The code under test depends on feature EnablePasswordsAccountStorage which
// is not enabled for Chrome OS (ash or lacros).
#if BUILDFLAG(IS_CHROMEOS)
#define DISABLE_ON_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_CHROMEOS(x) x
#endif

IN_PROC_BROWSER_TEST_F(LoginDetectionPasswordStoreSitesBrowserTest,
                       DISABLE_ON_CHROMEOS(ProfilePasswordStore)) {
  PasswordStoreSites password_store_sites(profile_password_store_);
  WaitForPasswordStoreUpdate(profile_password_store_);
  base::RunLoop().RunUntilIdle();

  AddLoginForSite(profile_password_store_,
                  GURL("https://www.foo.com/login.html"));
  WaitForPasswordStoreUpdate(profile_password_store_);
  base::RunLoop().RunUntilIdle();

  // The site and its subdomains should exist in the password store.
  EXPECT_TRUE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://www.foo.com")));
  EXPECT_TRUE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://foo.com")));
  EXPECT_TRUE(password_store_sites.IsSiteInPasswordStore(
      GURL("https://mobile.foo.com")));

  EXPECT_FALSE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://www.bar.com")));

  histogram_tester.ExpectUniqueSample(
      "Login.PasswordStoreSites.InitializedBeforeQuery", true, 4);
}

IN_PROC_BROWSER_TEST_F(LoginDetectionPasswordStoreSitesBrowserTest,
                       DISABLE_ON_CHROMEOS(AccountPasswordStore)) {
  PasswordStoreSites password_store_sites(account_password_store_);
  WaitForPasswordStoreUpdate(account_password_store_);
  base::RunLoop().RunUntilIdle();

  AddLoginForSite(account_password_store_,
                  GURL("https://www.foo.com/login.html"));
  WaitForPasswordStoreUpdate(account_password_store_);
  base::RunLoop().RunUntilIdle();

  // The site and its subdomains should exist in the password store.
  EXPECT_TRUE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://www.foo.com")));
  EXPECT_TRUE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://foo.com")));
  EXPECT_TRUE(password_store_sites.IsSiteInPasswordStore(
      GURL("https://mobile.foo.com")));

  EXPECT_FALSE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://www.bar.com")));

  histogram_tester.ExpectUniqueSample(
      "Login.PasswordStoreSites.InitializedBeforeQuery", true, 4);
}

IN_PROC_BROWSER_TEST_F(LoginDetectionPasswordStoreSitesBrowserTest,
                       DISABLE_ON_CHROMEOS(AccountPasswordStoreExistingLogin)) {
  AddLoginForSite(account_password_store_,
                  GURL("https://www.foo.com/login.html"));
  base::RunLoop().RunUntilIdle();

  PasswordStoreSites password_store_sites(account_password_store_);
  WaitForPasswordStoreUpdate(account_password_store_);

  // The site and its subdomains should exist in the password store.
  EXPECT_TRUE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://www.foo.com")));
  EXPECT_TRUE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://foo.com")));
  EXPECT_TRUE(password_store_sites.IsSiteInPasswordStore(
      GURL("https://mobile.foo.com")));

  histogram_tester.ExpectUniqueSample(
      "Login.PasswordStoreSites.InitializedBeforeQuery", true, 3);
}

IN_PROC_BROWSER_TEST_F(LoginDetectionPasswordStoreSitesBrowserTest,
                       DISABLE_ON_CHROMEOS(ProfilePasswordStoreExistingLogin)) {
  AddLoginForSite(profile_password_store_,
                  GURL("https://www.foo.com/login.html"));
  base::RunLoop().RunUntilIdle();

  PasswordStoreSites password_store_sites(profile_password_store_);
  WaitForPasswordStoreUpdate(profile_password_store_);

  // The site and its subdomains should exist in the password store.
  EXPECT_TRUE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://www.foo.com")));
  EXPECT_TRUE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://foo.com")));
  EXPECT_TRUE(password_store_sites.IsSiteInPasswordStore(
      GURL("https://mobile.foo.com")));

  histogram_tester.ExpectUniqueSample(
      "Login.PasswordStoreSites.InitializedBeforeQuery", true, 3);
}

// Tests querying the password store sites before the password store is
// initialized.
IN_PROC_BROWSER_TEST_F(
    LoginDetectionPasswordStoreSitesBrowserTest,
    DISABLE_ON_CHROMEOS(QueryBeforePasswordStoreInitialize)) {
  PasswordStoreSites password_store_sites(profile_password_store_);

  EXPECT_FALSE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://www.foo.com")));
  histogram_tester.ExpectUniqueSample(
      "Login.PasswordStoreSites.InitializedBeforeQuery", false, 1);

  WaitForPasswordStoreUpdate(profile_password_store_);
  EXPECT_FALSE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://www.foo.com")));

  histogram_tester.ExpectBucketCount(
      "Login.PasswordStoreSites.InitializedBeforeQuery", true, 1);
}

}  // namespace login_detection
