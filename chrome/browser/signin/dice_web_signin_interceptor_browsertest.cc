// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/dice_intercepted_session_startup_helper.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/signin/dice_web_signin_interceptor_delegate.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace {

// Fake response for OAuth multilogin.
const char kMultiloginSuccessResponse[] =
    R"()]}'
       {
         "status": "OK",
         "cookies":[
           {
             "name":"SID",
             "value":"SID_value",
             "domain":".google.fr",
             "path":"/",
             "isSecure":true,
             "isHttpOnly":false,
             "priority":"HIGH",
             "maxAge":63070000
           }
         ]
       }
      )";

class FakeDiceWebSigninInterceptorDelegate;

class FakeBubbleHandle : public ScopedWebSigninInterceptionBubbleHandle,
                         public base::SupportsWeakPtr<FakeBubbleHandle> {
 public:
  ~FakeBubbleHandle() override = default;
};

// Dummy interception delegate that automatically accepts multi user
// interception.
class FakeDiceWebSigninInterceptorDelegate
    : public DiceWebSigninInterceptorDelegate {
 public:
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
  ShowSigninInterceptionBubble(
      content::WebContents* web_contents,
      const BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback) override {
    EXPECT_EQ(bubble_parameters.interception_type, expected_interception_type_);
    auto bubble_handle = std::make_unique<FakeBubbleHandle>();
    weak_bubble_handle_ = bubble_handle->AsWeakPtr();
    // The callback must not be called synchronously (see the documentation for
    // ShowSigninInterceptionBubble).
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), expected_interception_result_));
    return bubble_handle;
  }

  void ShowFirstRunExperienceInNewProfile(
      Browser* browser,
      const CoreAccountId& account_id,
      WebSigninInterceptor::SigninInterceptionType interception_type) override {
    EXPECT_FALSE(fre_browser_)
        << "First run experience must be shown only once.";
    EXPECT_EQ(interception_type, expected_interception_type_);
    fre_browser_ = browser;
    fre_account_id_ = account_id;
  }

  Browser* fre_browser() { return fre_browser_; }

  const CoreAccountId& fre_account_id() { return fre_account_id_; }

  void set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType type) {
    expected_interception_type_ = type;
  }

  void set_expected_interception_result(SigninInterceptionResult result) {
    expected_interception_result_ = result;
  }

  bool intercept_bubble_shown() const { return weak_bubble_handle_.get(); }

  bool intercept_bubble_destroyed() const {
    return weak_bubble_handle_.WasInvalidated();
  }

 private:
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> fre_browser_ = nullptr;
  CoreAccountId fre_account_id_;
  WebSigninInterceptor::SigninInterceptionType expected_interception_type_ =
      WebSigninInterceptor::SigninInterceptionType::kMultiUser;
  SigninInterceptionResult expected_interception_result_ =
      SigninInterceptionResult::kAccepted;
  base::WeakPtr<FakeBubbleHandle> weak_bubble_handle_;
};

class BrowserCloseObserver : public BrowserListObserver {
 public:
  explicit BrowserCloseObserver(Browser* browser) : browser_(browser) {
    BrowserList::AddObserver(this);
  }

  BrowserCloseObserver(const BrowserCloseObserver&) = delete;
  BrowserCloseObserver& operator=(const BrowserCloseObserver&) = delete;

  ~BrowserCloseObserver() override { BrowserList::RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

  // BrowserListObserver implementation.
  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_) {
      run_loop_.Quit();
    }
  }

 private:
  raw_ptr<Browser> browser_;
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
  histogram_tester.ExpectUniqueSample("Signin.Intercept.HeuristicOutcome",
                                      outcome, 1);
}

}  // namespace

class DiceWebSigninInterceptorBrowserTest : public SigninBrowserTestBase {
 public:
  DiceWebSigninInterceptorBrowserTest()
      : SigninBrowserTestBase(/*use_main_profile=*/true) {}

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

  void SetupGaiaResponses() {
    // Instantly return from Gaia calls, to avoid timing out when injecting the
    // account in the new profile.
    network::TestURLLoaderFactory* loader_factory = test_url_loader_factory();
    loader_factory->SetInterceptor(base::BindLambdaForTesting(
        [loader_factory](const network::ResourceRequest& request) {
          std::string path = request.url.path();
          if (path == "/ListAccounts" || path == "/GetCheckConnectionInfo") {
            loader_factory->AddResponse(request.url.spec(), std::string());
            return;
          }
          if (path == "/oauth/multilogin") {
            loader_factory->AddResponse(request.url.spec(),
                                        kMultiloginSuccessResponse);
            return;
          }
        }));
  }

 private:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    DiceWebSigninInterceptorFactory::GetForProfile(GetProfile())
        ->SetInterceptedAccountProfileSeparationPoliciesForTesting(
            policy::ProfileSeparationPolicies(""));
  }

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    SigninBrowserTestBase::OnWillCreateBrowserContextServices(context);
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
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  SetupGaiaResponses();

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  Profile* new_profile =
      InterceptAndWaitProfileCreation(web_contents, account_info.account_id);
  ASSERT_TRUE(new_profile);
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_shown());
  signin::IdentityManager* new_identity_manager =
      IdentityManagerFactory::GetForProfile(new_profile);
  EXPECT_TRUE(new_identity_manager->HasAccountWithRefreshToken(
      account_info.account_id));

  IdentityTestEnvironmentProfileAdaptor adaptor(new_profile);
  adaptor.identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

  // Check the profile name.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(new_profile->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_EQ("givenname", base::UTF16ToUTF8(entry->GetLocalProfileName()));
  // Check the profile color.
  if (features::IsChromeWebuiRefresh2023()) {
    EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                    ->GetUserColor()
                    .has_value());
  } else {
    EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                    ->UsingAutogeneratedTheme());
  }

  // A browser has been created for the new profile and the tab was moved there.
  Browser* added_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(added_browser);
  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_EQ(added_browser->profile(), new_profile);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count - 1);
  EXPECT_EQ(
      added_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptMultiUser);
  // Interception bubble is destroyed in the source profile, and was not shown
  // in the new profile.
  FakeDiceWebSigninInterceptorDelegate* new_interceptor_delegate =
      GetInterceptorDelegate(new_profile);
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_destroyed());
  EXPECT_FALSE(new_interceptor_delegate->intercept_bubble_shown());
  EXPECT_FALSE(new_interceptor_delegate->intercept_bubble_destroyed());
  // First run experience UI was shown exactly once in the new profile.
  EXPECT_EQ(new_interceptor_delegate->fre_browser(), added_browser);
  EXPECT_EQ(new_interceptor_delegate->fre_account_id(),
            account_info.account_id);
  EXPECT_EQ(source_interceptor_delegate->fre_browser(), nullptr);
}

// Tests the complete profile switch flow when the profile is not loaded.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest, SwitchAndLoad) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  // Add a profile in the cache (simulate the profile on disk).
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage* profile_storage =
      &profile_manager->GetProfileAttributesStorage();
  const base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"TestProfileName";
  params.gaia_id = account_info.gaia;
  params.user_name = base::UTF8ToUTF16(account_info.email);
  profile_storage->AddProfile(std::move(params));
  ProfileAttributesEntry* entry =
      profile_storage->GetProfileAttributesWithPath(profile_path);
  ASSERT_TRUE(entry);
  ASSERT_EQ(entry->GetGAIAId(), account_info.gaia);

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch);
  Profile* new_profile =
      InterceptAndWaitProfileCreation(web_contents, account_info.account_id);
  ASSERT_TRUE(new_profile);
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_shown());
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
  EXPECT_EQ(
      added_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch);
  // Interception bubble was closed.
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_destroyed());
  // First run experience was not shown.
  EXPECT_EQ(GetInterceptorDelegate(new_profile)->fre_browser(), nullptr);
  EXPECT_EQ(source_interceptor_delegate->fre_browser(), nullptr);
}

// Tests the complete profile switch flow when the profile is already loaded.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest, SwitchAlreadyOpen) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  // Create another profile with a browser window.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop loop;
  Profile* other_profile = nullptr;
  base::OnceCallback<void(Browser*)> callback =
      base::BindLambdaForTesting([&other_profile, &loop](Browser* browser) {
        other_profile = browser->profile();
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
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  other_identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_info.account_id, signin::ConsentLevel::kSync);

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();
  int other_original_tab_count = other_browser->tab_strip_model()->count();

  // Start the interception.
  GetInterceptorDelegate(GetProfile())
      ->set_expected_interception_type(
          WebSigninInterceptor::SigninInterceptionType::kProfileSwitch);
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(GetProfile());
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
  EXPECT_EQ(
      other_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch);
  // First run experience was not shown.
  EXPECT_EQ(GetInterceptorDelegate(other_profile)->fre_browser(), nullptr);
  EXPECT_EQ(GetInterceptorDelegate(GetProfile())->fre_browser(), nullptr);
}

// Close the source tab during the interception and check that the NTP is opened
// in the new profile (regression test for https://crbug.com/1153321).
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest, CloseSourceTab) {
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
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  ProfileWaiter profile_waiter;
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(
          Profile::FromBrowserContext(contents->GetBrowserContext()));
  interceptor->MaybeInterceptWebSignin(contents, account_info.account_id,
                                       /*is_new_account=*/true,
                                       /*is_sync_signin=*/false);
  // Close the source tab during the profile creation.
  contents->Close();
  // Wait for the interception to be complete.
  Profile* new_profile = profile_waiter.WaitForProfileAdded();
  ASSERT_TRUE(new_profile);
  signin::IdentityManager* new_identity_manager =
      IdentityManagerFactory::GetForProfile(new_profile);
  EXPECT_TRUE(new_identity_manager->HasAccountWithRefreshToken(
      account_info.account_id));

  // Add the account to the cookies (simulates the account reconcilor).
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  signin::SetCookieAccounts(new_identity_manager, test_url_loader_factory(),
                            {{account_info.email, account_info.gaia}});

  // A browser has been created for the new profile on the new tab page.
  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
  Browser* added_browser = BrowserList::GetInstance()->get(1);
  ASSERT_TRUE(added_browser);
  EXPECT_EQ(added_browser->profile(), new_profile);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count - 1);
  EXPECT_EQ(
      added_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      GURL("chrome://newtab/"));
}

// WebApps do not trigger interception. Regression test for
// https://crbug.com/1414988
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest,
                       WebAppNoInterception) {
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
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  SetupGaiaResponses();

  // Install web app
  Profile* profile = browser()->profile();
  const GURL kWebAppURL("http://www.webapp.com");
  auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
  web_app_info->start_url = kWebAppURL;
  web_app_info->scope = kWebAppURL.GetWithoutFilename();
  web_app_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  web_app_info->title = u"A Web App";
  web_app::AppId app_id =
      web_app::test::InstallWebApp(profile, std::move(web_app_info));

  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);

  ASSERT_NE(app_browser, nullptr);
  ASSERT_EQ(app_browser->type(), Browser::Type::TYPE_APP);

  // Trigger signin interception in the web app.
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(profile);
  interceptor->MaybeInterceptWebSignin(
      app_browser->tab_strip_model()->GetActiveWebContents(),
      account_info.account_id,
      /*is_new_account=*/true,
      /*is_sync_signin=*/false);

  // Check that the interception was aborted.
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.HeuristicOutcome",
      SigninInterceptionHeuristicOutcome::kAbortNoSupportedBrowser, 1);
}

// Tests the complete interception flow including profile and browser creation.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest,
                       ForcedEnterpriseInterceptionTestNoForcedInterception) {
  base::HistogramTester histogram_tester;

  AccountInfo primary_account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  primary_account_info.full_name = "fullname";
  primary_account_info.given_name = "givenname";
  primary_account_info.hosted_domain = "example.com";
  primary_account_info.locale = "en";
  primary_account_info.picture_url = "https://example.com";
  DCHECK(primary_account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);
  IdentityManagerFactory::GetForProfile(GetProfile())
      ->GetPrimaryAccountMutator()
      ->SetPrimaryAccount(primary_account_info.account_id,
                          signin::ConsentLevel::kSync);

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Enforce enterprise profile sepatation.
  GetProfile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                      "none");
  DiceWebSigninInterceptorFactory::GetForProfile(GetProfile())
      ->SetInterceptedAccountProfileSeparationPoliciesForTesting(
          policy::ProfileSeparationPolicies(""));

  SetupGaiaResponses();

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise);
  Profile* new_profile =
      InterceptAndWaitProfileCreation(web_contents, account_info.account_id);
  EXPECT_FALSE(
      chrome::enterprise_util::UserAcceptedAccountManagement(new_profile));
  ASSERT_TRUE(new_profile);
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_shown());
  signin::IdentityManager* new_identity_manager =
      IdentityManagerFactory::GetForProfile(new_profile);
  EXPECT_TRUE(new_identity_manager->HasAccountWithRefreshToken(
      account_info.account_id));

  FakeDiceWebSigninInterceptorDelegate* new_interceptor_delegate =
      GetInterceptorDelegate(new_profile);
  new_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise);

  IdentityTestEnvironmentProfileAdaptor adaptor(new_profile);
  adaptor.identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

  // Check the profile name.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(new_profile->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_EQ("example.com", base::UTF16ToUTF8(entry->GetLocalProfileName()));
  // Check the profile color.
  if (features::IsChromeWebuiRefresh2023()) {
    EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                    ->GetUserColor()
                    .has_value());
  } else {
    EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                    ->UsingAutogeneratedTheme());
  }

  // A browser has been created for the new profile and the tab was moved there.
  Browser* added_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(added_browser);
  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_EQ(added_browser->profile(), new_profile);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count - 1);
  EXPECT_EQ(
      added_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptEnterprise);

  // First run experience UI was shown exactly once in the new profile.
  EXPECT_EQ(new_interceptor_delegate->fre_browser(), added_browser);
  EXPECT_EQ(new_interceptor_delegate->fre_account_id(),
            account_info.account_id);
  EXPECT_EQ(source_interceptor_delegate->fre_browser(), nullptr);
}

// Tests the complete interception flow including profile and browser creation.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest,
                       EnterpriseInterceptionDeclined) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  AccountInfo primary_account_info =
      identity_test_env()->MakeAccountAvailable("bob@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  primary_account_info.full_name = "fullname";
  primary_account_info.given_name = "givenname";
  primary_account_info.hosted_domain = "example.com";
  primary_account_info.locale = "en";
  primary_account_info.picture_url = "https://example.com";
  DCHECK(primary_account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(primary_account_info);

  IdentityManagerFactory::GetForProfile(GetProfile())
      ->GetPrimaryAccountMutator()
      ->SetPrimaryAccount(primary_account_info.account_id,
                          signin::ConsentLevel::kSignin);

  // Enforce enterprise profile sepatation.
  GetProfile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                      "none");

  SetupGaiaResponses();

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise);
  source_interceptor_delegate->set_expected_interception_result(
      SigninInterceptionResult::kDeclined);

  // Start the interception.
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(GetProfile());
  interceptor->MaybeInterceptWebSignin(web_contents, account_info.account_id,
                                       /*is_new_account=*/true,
                                       /*is_sync_signin=*/false);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  EXPECT_FALSE(
      chrome::enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_destroyed());
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptEnterprise);
}

// Tests the complete interception flow including profile and browser creation.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest,
                       ForcedEnterpriseInterceptionTestAccountLevelPolicy) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Enforce enterprise profile sepatation.
  GetProfile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                      "none");
  DiceWebSigninInterceptorFactory::GetForProfile(GetProfile())
      ->SetInterceptedAccountProfileSeparationPoliciesForTesting(
          policy::ProfileSeparationPolicies("primary_account"));

  SetupGaiaResponses();

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced);
  Profile* new_profile =
      InterceptAndWaitProfileCreation(web_contents, account_info.account_id);
  EXPECT_TRUE(
      chrome::enterprise_util::UserAcceptedAccountManagement(new_profile));
  ASSERT_TRUE(new_profile);
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_shown());
  signin::IdentityManager* new_identity_manager =
      IdentityManagerFactory::GetForProfile(new_profile);
  EXPECT_TRUE(new_identity_manager->HasAccountWithRefreshToken(
      account_info.account_id));

  FakeDiceWebSigninInterceptorDelegate* new_interceptor_delegate =
      GetInterceptorDelegate(new_profile);
  new_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced);

  IdentityTestEnvironmentProfileAdaptor adaptor(new_profile);
  adaptor.identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

  // Check the profile name.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(new_profile->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_EQ("example.com", base::UTF16ToUTF8(entry->GetLocalProfileName()));
  // Check the profile color.
  if (features::IsChromeWebuiRefresh2023()) {
    EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                    ->GetUserColor()
                    .has_value());
  } else {
    EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                    ->UsingAutogeneratedTheme());
  }

  // A browser has been created for the new profile and the tab was moved there.
  Browser* added_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(added_browser);
  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_EQ(added_browser->profile(), new_profile);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count - 1);
  EXPECT_EQ(
      added_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(
      histogram_tester,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);

  // First run experience UI was shown exactly once in the new profile.
  EXPECT_EQ(new_interceptor_delegate->fre_browser(), added_browser);
  EXPECT_EQ(new_interceptor_delegate->fre_account_id(),
            account_info.account_id);
  EXPECT_EQ(source_interceptor_delegate->fre_browser(), nullptr);
}

// Tests the complete interception flow including profile and browser creation.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorBrowserTest,
    ForcedEnterpriseInterceptionTestAccountLevelPolicyDeclined) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Enforce enterprise profile sepatation.
  GetProfile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                      "none");
  DiceWebSigninInterceptorFactory::GetForProfile(GetProfile())
      ->SetInterceptedAccountProfileSeparationPoliciesForTesting(
          policy::ProfileSeparationPolicies("primary_account"));

  SetupGaiaResponses();

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced);
  source_interceptor_delegate->set_expected_interception_result(
      SigninInterceptionResult::kDeclined);

  // Start the interception.
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(GetProfile());
  interceptor->MaybeInterceptWebSignin(web_contents, account_info.account_id,
                                       /*is_new_account=*/true,
                                       /*is_sync_signin=*/false);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  EXPECT_FALSE(
      chrome::enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_destroyed());
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(
      histogram_tester,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

// Tests the complete interception flow including profile and browser creation.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorBrowserTest,
    ForcedEnterpriseInterceptionTestAccountLevelPolicyStrictDeclined) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Enforce enterprise profile sepatation.
  GetProfile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                      "none");
  DiceWebSigninInterceptorFactory::GetForProfile(GetProfile())
      ->SetInterceptedAccountProfileSeparationPoliciesForTesting(
          policy::ProfileSeparationPolicies("primary_account_strict"));

  SetupGaiaResponses();

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced);
  source_interceptor_delegate->set_expected_interception_result(
      SigninInterceptionResult::kDeclined);

  // Start the interception.
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(GetProfile());
  interceptor->MaybeInterceptWebSignin(web_contents, account_info.account_id,
                                       /*is_new_account=*/true,
                                       /*is_sync_signin=*/false);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(GetProfile());
  EXPECT_FALSE(
      chrome::enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_destroyed());
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(
      histogram_tester,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

// Tests the complete interception flow including profile and browser creation.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest,
                       ForcedEnterpriseInterceptionTest) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Enforce enterprise profile separation.
  GetProfile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                      "primary_account_strict");

  SetupGaiaResponses();

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced);
  Profile* new_profile =
      InterceptAndWaitProfileCreation(web_contents, account_info.account_id);
  EXPECT_TRUE(
      chrome::enterprise_util::UserAcceptedAccountManagement(new_profile));
  ASSERT_TRUE(new_profile);
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_shown());
  signin::IdentityManager* new_identity_manager =
      IdentityManagerFactory::GetForProfile(new_profile);
  EXPECT_TRUE(new_identity_manager->HasAccountWithRefreshToken(
      account_info.account_id));

  FakeDiceWebSigninInterceptorDelegate* new_interceptor_delegate =
      GetInterceptorDelegate(new_profile);
  new_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced);

  IdentityTestEnvironmentProfileAdaptor adaptor(new_profile);
  adaptor.identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

  // Check the profile name.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(new_profile->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_EQ("example.com", base::UTF16ToUTF8(entry->GetLocalProfileName()));
  // Check the profile color.
  if (features::IsChromeWebuiRefresh2023()) {
    EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                    ->GetUserColor()
                    .has_value());
  } else {
    EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                    ->UsingAutogeneratedTheme());
  }

  // A browser has been created for the new profile and the tab was moved there.
  Browser* added_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(added_browser);
  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);
  EXPECT_EQ(added_browser->profile(), new_profile);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count - 1);
  EXPECT_EQ(
      added_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(
      histogram_tester,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);

  // First run experience UI was shown exactly once in the new profile.
  EXPECT_EQ(new_interceptor_delegate->fre_browser(), added_browser);
  EXPECT_EQ(new_interceptor_delegate->fre_account_id(),
            account_info.account_id);
  EXPECT_EQ(source_interceptor_delegate->fre_browser(), nullptr);
}

// Tests the complete interception flow for a reauth of the primary account of a
// non-syncing profile.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorBrowserTest,
    ForcedEnterpriseInterceptionPrimaryACcountReauthSyncDisabledTest) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  IdentityManagerFactory::GetForProfile(GetProfile())
      ->GetPrimaryAccountMutator()
      ->SetPrimaryAccount(account_info.account_id,
                          signin::ConsentLevel::kSignin);

  // Enforce enterprise profile separation.
  GetProfile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                      "primary_account_strict");

  SetupGaiaResponses();

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced);

  EXPECT_FALSE(
      chrome::enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  // Start the interception.
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(GetProfile());
  interceptor->MaybeInterceptWebSignin(web_contents, account_info.account_id,
                                       /*is_new_account=*/false,
                                       /*is_sync_signin=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      chrome::enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  // Interception bubble was closed.
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_destroyed());
  EXPECT_TRUE(IdentityManagerFactory::GetForProfile(GetProfile())
                  ->HasAccountWithRefreshToken(account_info.account_id));

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(
      histogram_tester,
      SigninInterceptionHeuristicOutcome::kInterceptEnterpriseForced);
}

// Tests the complete interception flow for a reauth of the primary account of a
// syncing profile.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorBrowserTest,
    ForcedEnterpriseInterceptionPrimaryACcountReauthSyncEnabledTest) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  IdentityManagerFactory::GetForProfile(GetProfile())
      ->GetPrimaryAccountMutator()
      ->SetPrimaryAccount(account_info.account_id, signin::ConsentLevel::kSync);

  // Enforce enterprise profile separation.
  GetProfile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                      "primary_account_strict");

  SetupGaiaResponses();

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kEnterpriseForced);

  EXPECT_FALSE(
      chrome::enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  // Start the interception.
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(GetProfile());
  interceptor->MaybeInterceptWebSignin(web_contents, account_info.account_id,
                                       /*is_new_account=*/false,
                                       /*is_sync_signin=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(
      chrome::enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  // Interception bubble was closed.
  EXPECT_FALSE(source_interceptor_delegate->intercept_bubble_shown());
  EXPECT_FALSE(source_interceptor_delegate->intercept_bubble_destroyed());
  EXPECT_TRUE(IdentityManagerFactory::GetForProfile(GetProfile())
                  ->HasAccountWithRefreshToken(account_info.account_id));

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_EQ(browser()->tab_strip_model()->count(), original_tab_count);
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kAbortAccountNotNew);
}

// Tests the complete profile switch flow when the profile is not loaded.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest,
                       EnterpriseSwitchAndLoad) {
  base::HistogramTester histogram_tester;
  // Enforce enterprise profile separation.
  GetProfile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                      "primary_account_strict");
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  // Add a profile in the cache (simulate the profile on disk).
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage* profile_storage =
      &profile_manager->GetProfileAttributesStorage();
  const base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = u"TestProfileName";
  params.gaia_id = account_info.gaia;
  params.user_name = base::UTF8ToUTF16(account_info.email);
  profile_storage->AddProfile(std::move(params));
  ProfileAttributesEntry* entry =
      profile_storage->GetProfileAttributesWithPath(profile_path);
  ASSERT_TRUE(entry);
  ASSERT_EQ(entry->GetGAIAId(), account_info.gaia);

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced);
  Profile* new_profile =
      InterceptAndWaitProfileCreation(web_contents, account_info.account_id);
  ASSERT_TRUE(new_profile);
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_shown());
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
  EXPECT_EQ(
      added_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::
                      kInterceptEnterpriseForcedProfileSwitch);

  // Interception bubble was closed.
  EXPECT_TRUE(source_interceptor_delegate->intercept_bubble_destroyed());

  // First run experience was not shown.
  EXPECT_EQ(GetInterceptorDelegate(new_profile)->fre_browser(), nullptr);
  EXPECT_EQ(source_interceptor_delegate->fre_browser(), nullptr);
}

// Tests the complete profile switch flow when the profile is already loaded.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest,
                       EnterpriseSwitchAlreadyOpen) {
  base::HistogramTester histogram_tester;
  // Enforce enterprise profile separation.
  GetProfile()->GetPrefs()->SetString(prefs::kManagedAccountsSigninRestriction,
                                      "primary_account_strict");
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("alice@example.com");
  // Fill the account info, in particular for the hosted_domain field.
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = "example.com";
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);
  // Create another profile with a browser window.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  base::RunLoop loop;
  Profile* other_profile = nullptr;
  base::OnceCallback<void(Browser*)> callback =
      base::BindLambdaForTesting([&other_profile, &loop](Browser* browser) {
        other_profile = browser->profile();
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
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  other_identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_info.account_id, signin::ConsentLevel::kSync);

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();
  int other_original_tab_count = other_browser->tab_strip_model()->count();

  // Start the interception.
  GetInterceptorDelegate(GetProfile())
      ->set_expected_interception_type(
          WebSigninInterceptor::SigninInterceptionType::kProfileSwitchForced);
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(GetProfile());
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
  EXPECT_EQ(
      other_browser->tab_strip_model()->GetActiveWebContents()->GetVisibleURL(),
      intercepted_url);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::
                      kInterceptEnterpriseForcedProfileSwitch);
  // First run experience was not shown.
  EXPECT_EQ(GetInterceptorDelegate(other_profile)->fre_browser(), nullptr);
  EXPECT_EQ(GetInterceptorDelegate(GetProfile())->fre_browser(), nullptr);
}
