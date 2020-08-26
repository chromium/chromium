// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Dummy interception delegate that automatically accepts multi user
// interception.
class FakeDiceWebSigninInterceptorDelegate
    : public DiceWebSigninInterceptor::Delegate {
 public:
  void ShowSigninInterceptionBubble(
      content::WebContents* web_contents,
      const BubbleParameters& bubble_parameters,
      base::OnceCallback<void(bool)> callback) override {
    bool should_intercept =
        bubble_parameters.interception_type ==
        DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser;
    std::move(callback).Run(should_intercept);
  }
};

// Waits until a new profile is created.
class ProfileWaiter : public ProfileManagerObserver {
 public:
  ProfileWaiter() {
    profile_manager_observer_.Add(g_browser_process->profile_manager());
  }

  ~ProfileWaiter() override = default;

  Profile* WaitForProfileAdded() {
    run_loop_.Run();
    return profile_;
  }

 private:
  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override {
    profile_manager_observer_.RemoveAll();
    profile_ = profile;
    run_loop_.Quit();
  }

  Profile* profile_ = nullptr;
  ScopedObserver<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};
  base::RunLoop run_loop_;
};

// Builds a DiceWebSigninInterceptor with a fake delegate. To be used as a
// testing factory.
std::unique_ptr<KeyedService> BuildDiceWebSigninInterceptorWithFakeDelegate(
    content::BrowserContext* context) {
  return std::make_unique<DiceWebSigninInterceptor>(
      Profile::FromBrowserContext(context),
      std::make_unique<FakeDiceWebSigninInterceptorDelegate>());
}

}  // namespace

class DiceWebSigninInterceptorBrowserTest : public InProcessBrowserTest {
 public:
  DiceWebSigninInterceptorBrowserTest() {
    feature_list_.InitAndEnableFeature(kDiceWebSigninInterceptionFeature);
  }

  Profile* profile() { return browser()->profile(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  }

  void TearDownOnMainThread() override {
    // Must be destroyed before the Profile.
    identity_test_env_profile_adaptor_.reset();
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::Bind(&DiceWebSigninInterceptorBrowserTest::
                               OnWillCreateBrowserContextServices,
                           base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     &test_url_loader_factory_));
    DiceWebSigninInterceptorFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(&BuildDiceWebSigninInterceptorWithFakeDelegate));
  }

  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<
      BrowserContextDependencyManager::CreateServicesCallbackList::Subscription>
      create_services_subscription_;
};

// Tests the complete interception flow including profile and browser creation.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest, InterceptionTest) {
  base::HistogramTester histogram_tester;
  // Setup profile for interception.
  identity_test_env()->MakeAccountAvailable("alice@example.com");
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = kNoHostedDomainFound;
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  account_info.is_child_account = false;
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), intercepted_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int original_tab_count = browser()->tab_strip_model()->count();
  ProfileWaiter profile_waiter;

  // Start the interception.
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(profile());
  interceptor->MaybeInterceptWebSignin(contents, account_info.account_id,
                                       /*is_new_account=*/true,
                                       /*is_sync_signin=*/false);

  // Wait for the interception to be complete.
  Profile* new_profile = profile_waiter.WaitForProfileAdded();
  ASSERT_TRUE(new_profile);
  signin::IdentityManager* new_identity_manager =
      IdentityManagerFactory::GetForProfile(new_profile);
  EXPECT_TRUE(new_identity_manager->HasAccountWithRefreshToken(
      account_info.account_id));

  // Check the profile name.
  ProfileAttributesEntry* entry = nullptr;
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ASSERT_TRUE(
      storage.GetProfileAttributesWithPath(new_profile->GetPath(), &entry));
  ASSERT_TRUE(entry);
  EXPECT_EQ("givenname", base::UTF16ToUTF8(entry->GetLocalProfileName()));
  // Check the profile color.
  EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                  ->UsingAutogeneratedTheme());

  // Add the account to the cookies (simulates the account reconcilor).
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  signin::SetCookieAccounts(new_identity_manager, test_url_loader_factory(),
                            {{account_info.email, account_info.gaia}});

  // A browser has been created for the new profile and the tab was moved there.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
  Browser* added_browser = BrowserList::GetInstance()->get(1);
  ASSERT_TRUE(added_browser);
  EXPECT_EQ(added_browser->profile(), new_profile);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count - 1);
  EXPECT_EQ(added_browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
            intercepted_url);

  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kInterceptMultiUser, 1);
  histogram_tester.ExpectTotalCount("Signin.Intercept.AccountInfoFetchDuration",
                                    1);
  histogram_tester.ExpectTotalCount("Signin.Intercept.ProfileCreationDuration",
                                    1);
}
