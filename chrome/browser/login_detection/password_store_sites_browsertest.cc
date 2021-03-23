// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/login_detection/password_store_sites.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace login_detection {

class LoginDetectionPasswordStoreSitesBrowserTest
    : public InProcessBrowserTest {
 public:
  LoginDetectionPasswordStoreSitesBrowserTest() = default;

  scoped_refptr<password_manager::PasswordStore> GetProfilePasswordStore() {
    return PasswordStoreFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
  }

  scoped_refptr<password_manager::PasswordStore> GetAccountPasswordStore() {
    return AccountPasswordStoreFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
  }

  void AddLoginForSite(password_manager::PasswordStore* password_store,
                       const GURL& url) {
    password_manager::PasswordForm form;
    form.scheme = password_manager::PasswordForm::Scheme::kHtml;
    form.url = url;
    form.signon_realm = "https://www.chrome.com";
    form.username_value = u"my_username";
    form.password_value = u"my_password";
    form.blocked_by_user = false;
    passwords_helper::AddLogin(password_store, form);
    base::RunLoop().RunUntilIdle();
  }

  // Wait for the password store taskrunner to complete its event processing.
  void WaitForPasswordStoreUpdate(
      password_manager::PasswordStore* password_store) {
    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    password_store->ScheduleTask(base::BindOnce(
        &base::WaitableEvent::Signal, base::Unretained(&waitable_event)));
    waitable_event.Wait();

    // At this point, the password store has completed its processing and posted
    // events to the main thread.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::HistogramTester histogram_tester;
};

// The code under test depends on feature EnablePasswordsAccountStorage which
// is not enabled for Chrome OS (ash or lacros).
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define DISABLE_ON_CHROMEOS(x) DISABLED_##x
#else
#define DISABLE_ON_CHROMEOS(x) x
#endif

IN_PROC_BROWSER_TEST_F(LoginDetectionPasswordStoreSitesBrowserTest,
                       DISABLE_ON_CHROMEOS(ProfilePasswordStore)) {
  auto password_store = GetProfilePasswordStore();
  PasswordStoreSites password_store_sites(password_store);
  WaitForPasswordStoreUpdate(password_store.get());
  base::RunLoop().RunUntilIdle();

  AddLoginForSite(password_store.get(), GURL("https://www.foo.com/login.html"));
  WaitForPasswordStoreUpdate(password_store.get());
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
  auto password_store = GetAccountPasswordStore();
  PasswordStoreSites password_store_sites(password_store);
  WaitForPasswordStoreUpdate(password_store.get());
  base::RunLoop().RunUntilIdle();

  AddLoginForSite(password_store.get(), GURL("https://www.foo.com/login.html"));
  WaitForPasswordStoreUpdate(password_store.get());
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
  auto password_store = GetAccountPasswordStore();
  AddLoginForSite(password_store.get(), GURL("https://www.foo.com/login.html"));
  base::RunLoop().RunUntilIdle();

  PasswordStoreSites password_store_sites(password_store);
  WaitForPasswordStoreUpdate(password_store.get());

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
  auto password_store = GetProfilePasswordStore();
  AddLoginForSite(password_store.get(), GURL("https://www.foo.com/login.html"));
  base::RunLoop().RunUntilIdle();

  PasswordStoreSites password_store_sites(password_store);
  WaitForPasswordStoreUpdate(password_store.get());

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
  auto password_store = GetProfilePasswordStore();
  PasswordStoreSites password_store_sites(password_store);

  EXPECT_FALSE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://www.foo.com")));
  histogram_tester.ExpectUniqueSample(
      "Login.PasswordStoreSites.InitializedBeforeQuery", false, 1);

  WaitForPasswordStoreUpdate(password_store.get());
  EXPECT_FALSE(
      password_store_sites.IsSiteInPasswordStore(GURL("https://www.foo.com")));

  histogram_tester.ExpectBucketCount(
      "Login.PasswordStoreSites.InitializedBeforeQuery", true, 1);
}

}  // namespace login_detection
