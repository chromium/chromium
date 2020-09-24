// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include <map>

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_window.h"
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
#include "components/account_id/account_id.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
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
    EXPECT_EQ(bubble_parameters.interception_type, expected_interception_type_);
    std::move(callback).Run(true);
  }
  void ShowProfileCustomizationBubble(Browser* browser) override {
    EXPECT_FALSE(customized_browser_)
        << "Customization must be shown only once.";
    customized_browser_ = browser;
  }

  Browser* customized_browser() { return customized_browser_; }
  void set_expected_interception_type(
      DiceWebSigninInterceptor::SigninInterceptionType type) {
    expected_interception_type_ = type;
  }

 private:
  Browser* customized_browser_ = nullptr;
  DiceWebSigninInterceptor::SigninInterceptionType expected_interception_type_ =
      DiceWebSigninInterceptor::SigninInterceptionType::kMultiUser;
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


// Runs the interception and returns the new profile that was created.
Profile* InterceptAndWaitProfileCreation(content::WebContents* contents,
                                         const CoreAccountId& account_id) {
  ProfileWaiter profile_waiter;
  // Start the interception.
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(
          Profile::FromBrowserContext(contents->GetBrowserContext()));
  interceptor->MaybeInterceptWebSignin(contents, account_id,
                                       /*is_new_account=*/true,
                                       /*is_sync_signin=*/false);
  // Wait for the interception to be complete.
  return profile_waiter.WaitForProfileAdded();
}

// Checks that the interception histograms were correctly recorded.
void CheckHistograms(const base::HistogramTester& histogram_tester,
                     SigninInterceptionHeuristicOutcome outcome) {
  int profile_switch_count =
      outcome == SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch
          ? 1
          : 0;
  int profile_creation_count = 1 - profile_switch_count;

  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      outcome, 1);
  histogram_tester.ExpectTotalCount("Signin.Intercept.AccountInfoFetchDuration",
                                    profile_creation_count);
  histogram_tester.ExpectTotalCount("Signin.Intercept.ProfileCreationDuration",
                                    profile_creation_count);
  histogram_tester.ExpectTotalCount("Signin.Intercept.ProfileSwitchDuration",
                                    profile_switch_count);
  histogram_tester.ExpectTotalCount("Signin.Intercept.SessionStartupDuration",
                                    1);
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

  content::WebContents* AddTab(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  FakeDiceWebSigninInterceptorDelegate* GetInterceptorDelegate(
      Profile* profile) {
    // Make sure the interceptor has been created.
    DiceWebSigninInterceptorFactory::GetForProfile(profile);
    FakeDiceWebSigninInterceptorDelegate* interceptor_delegate =
        interceptor_delegates_[profile];
    return interceptor_delegate;
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
        base::BindRepeating(&DiceWebSigninInterceptorBrowserTest::
                                BuildDiceWebSigninInterceptorWithFakeDelegate,
                            base::Unretained(this)));
  }

  // Builds a DiceWebSigninInterceptor with a fake delegate. To be used as a
  // testing factory.
  std::unique_ptr<KeyedService> BuildDiceWebSigninInterceptorWithFakeDelegate(
      content::BrowserContext* context) {
    std::unique_ptr<FakeDiceWebSigninInterceptorDelegate> fake_delegate =
        std::make_unique<FakeDiceWebSigninInterceptorDelegate>();
    interceptor_delegates_[context] = fake_delegate.get();
    return std::make_unique<DiceWebSigninInterceptor>(
        Profile::FromBrowserContext(context), std::move(fake_delegate));
  }

  base::test::ScopedFeatureList feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  std::unique_ptr<
      BrowserContextDependencyManager::CreateServicesCallbackList::Subscription>
      create_services_subscription_;
  std::map<content::BrowserContext*, FakeDiceWebSigninInterceptorDelegate*>
      interceptor_delegates_;
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
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  Profile* new_profile =
      InterceptAndWaitProfileCreation(web_contents, account_info.account_id);
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

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptMultiUser);
  // Profile customization UI was shown exactly once in the new profile.
  EXPECT_EQ(GetInterceptorDelegate(new_profile)->customized_browser(),
            added_browser);
  EXPECT_EQ(GetInterceptorDelegate(profile())->customized_browser(), nullptr);
}

// Tests the complete profile switch flow when the profile is not loaded.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest, SwitchAndLoad) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Add a profile in the cache (simulate the profile on disk).
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage* profile_storage =
      &profile_manager->GetProfileAttributesStorage();
  const base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  profile_storage->AddProfile(
      profile_path, base::UTF8ToUTF16("TestProfileName"), account_info.gaia,
      base::UTF8ToUTF16("TestGaiaName"),
      /*is_consented_primary_account=*/false, /*icon_index=*/0,
      /*supervised_user_id*/ std::string(), EmptyAccountId());
  ProfileAttributesEntry* entry = nullptr;
  ASSERT_TRUE(
      profile_storage->GetProfileAttributesWithPath(profile_path, &entry));
  ASSERT_TRUE(entry);
  ASSERT_EQ(entry->GetGAIAId(), account_info.gaia);

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  GetInterceptorDelegate(profile())->set_expected_interception_type(
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch);
  Profile* new_profile =
      InterceptAndWaitProfileCreation(web_contents, account_info.account_id);
  ASSERT_TRUE(new_profile);
  signin::IdentityManager* new_identity_manager =
      IdentityManagerFactory::GetForProfile(new_profile);
  EXPECT_TRUE(new_identity_manager->HasAccountWithRefreshToken(
      account_info.account_id));

  // Check that the right profile was opened.
  EXPECT_EQ(new_profile->GetPath(), profile_path);

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

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch);
  // Profile customization was not shown.
  EXPECT_EQ(GetInterceptorDelegate(new_profile)->customized_browser(), nullptr);
  EXPECT_EQ(GetInterceptorDelegate(profile())->customized_browser(), nullptr);
}

// Tests the complete profile switch flow when the profile is already loaded.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest, SwitchAlreadyOpen) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Create another profile with a browser window.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop loop;
  Profile* other_profile = nullptr;
  ProfileManager::CreateCallback callback = base::BindLambdaForTesting(
      [&other_profile, &loop](Profile* profile, Profile::CreateStatus status) {
        DCHECK_EQ(status, Profile::CREATE_STATUS_INITIALIZED);
        other_profile = profile;
        loop.Quit();
      });
  profiles::SwitchToProfile(profile_path, /*always_create=*/true,
                            std::move(callback));
  loop.Run();
  ASSERT_TRUE(other_profile);
  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
  Browser* other_browser = BrowserList::GetInstance()->get(1);
  ASSERT_TRUE(other_browser);
  ASSERT_EQ(other_browser->profile(), other_profile);
  // Add the account to the other profile.
  signin::IdentityManager* other_identity_manager =
      IdentityManagerFactory::GetForProfile(other_profile);
  other_identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
      account_info.gaia, account_info.email, "dummy_refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  other_identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_info.account_id);

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();
  int other_original_tab_count = other_browser->tab_strip_model()->count();

  // Start the interception.
  GetInterceptorDelegate(profile())->set_expected_interception_type(
      DiceWebSigninInterceptor::SigninInterceptionType::kProfileSwitch);
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(profile());
  interceptor->MaybeInterceptWebSignin(web_contents, account_info.account_id,
                                       /*is_new_account=*/true,
                                       /*is_sync_signin=*/false);

  // Add the account to the cookies (simulates the account reconcilor).
  signin::SetCookieAccounts(other_identity_manager, test_url_loader_factory(),
                            {{account_info.email, account_info.gaia}});

  // The tab was moved to the new browser.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count - 1);
  EXPECT_EQ(other_browser->tab_strip_model()->count(),
            other_original_tab_count + 1);
  EXPECT_EQ(other_browser->tab_strip_model()->GetActiveWebContents()->GetURL(),
            intercepted_url);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch);
  // Profile customization was not shown.
  EXPECT_EQ(GetInterceptorDelegate(other_profile)->customized_browser(),
            nullptr);
  EXPECT_EQ(GetInterceptorDelegate(profile())->customized_browser(), nullptr);
}
