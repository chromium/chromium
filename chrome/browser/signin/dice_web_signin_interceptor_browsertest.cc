// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_web_signin_interceptor.h"

#include <map>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/dice_intercepted_session_startup_helper.h"
#include "chrome/browser/signin/dice_web_signin_interceptor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
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
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/pref_names.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace {

#if BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
const char kCustomSearchEngineDomain[] = "bar.com";
#endif

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
  interceptor->MaybeInterceptWebSignin(
      contents, account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
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

#if BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
void SetUserSelectedDefaultSearchProvider(
    TemplateURLService* template_url_service) {
  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(std::string(kCustomSearchEngineDomain)));
  data.SetKeyword(base::UTF8ToUTF16(std::string(kCustomSearchEngineDomain)));
  data.SetURL("https://" + std::string(kCustomSearchEngineDomain) +
              "url?bar={searchTerms}");
  data.new_tab_url =
      "https://" + std::string(kCustomSearchEngineDomain) + "newtab";
  data.alternate_urls.push_back("https://" +
                                std::string(kCustomSearchEngineDomain) +
                                "alt#quux={searchTerms}");

  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
}
#endif

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

  AccountInfo MakeAccountInfoAvailableAndUpdate(
      const std::string email,
      const std::string& hosted_domain = "example.com") {
    AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
    // Fill the account info, in particular for the hosted_domain field.
    account_info.full_name = "fullname";
    account_info.given_name = "givenname";
    account_info.hosted_domain = hosted_domain;
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";
    DCHECK(account_info.IsValid());
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
    return account_info;
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

// Tests the complete profile switch flow when the profile is not loaded.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest, SwitchAndLoad) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      MakeAccountInfoAvailableAndUpdate("alice@example.com");
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
      MakeAccountInfoAvailableAndUpdate("alice@example.com");
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
  interceptor->MaybeInterceptWebSignin(
      web_contents, account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
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
      MakeAccountInfoAvailableAndUpdate("bob@example.com");

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* contents = AddTab(intercepted_url);
  int original_tab_count = browser()->tab_strip_model()->count();

  // Do the signin interception.
  ProfileWaiter profile_waiter;
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(
          Profile::FromBrowserContext(contents->GetBrowserContext()));
  interceptor->MaybeInterceptWebSignin(
      contents, account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
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

class DiceWebSigninInterceptorWithChromeSigninHelpersBrowserTest
    : public DiceWebSigninInterceptorBrowserTest {
 public:
  absl::optional<int> GetChromeSigninInterceptDeclinedCountPref(
      const AccountInfo& account_info) {
    return GetProfile()
        ->GetPrefs()
        // Content of `kChromeSigninInterceptionDeclinedPref`.
        ->GetDict("signin.ChromeSigninInterceptionDeclinedPref")
        .FindInt(DiceWebSigninInterceptor::GetPersistentEmailHash(
            account_info.email));
  }

  absl::optional<int> GetChromeSigninInterceptShownCountPref(
      const AccountInfo& account_info) {
    return GetProfile()
        ->GetPrefs()
        // Content of `kChromeSigninInterceptionShownCountPref`.
        ->GetDict("signin.ChromeSigninInterceptionShownCountPref")
        .FindInt(DiceWebSigninInterceptor::GetPersistentEmailHash(
            account_info.email));
  }

  FakeDiceWebSigninInterceptorDelegate* ShowSigninBubble(
      const AccountInfo& account_info,
      absl::optional<SigninInterceptionResult> expected_result) {
    GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
    content::WebContents* contents = AddTab(intercepted_url);

    // Set up the result expectations.
    FakeDiceWebSigninInterceptorDelegate* interceptor_delegate =
        GetInterceptorDelegate(GetProfile());
    interceptor_delegate->set_expected_interception_type(
        WebSigninInterceptor::SigninInterceptionType::kChromeSignin);
    if (expected_result.has_value()) {
      interceptor_delegate->set_expected_interception_result(
          expected_result.value());
    }

    DiceWebSigninInterceptor* interceptor =
        DiceWebSigninInterceptorFactory::GetForProfile(
            Profile::FromBrowserContext(contents->GetBrowserContext()));
    interceptor->MaybeInterceptWebSignin(
        contents, account_info.account_id,
        signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
        /*is_new_account=*/true,
        /*is_sync_signin=*/false);

    return interceptor_delegate;
  }

  void ShowAndCompleteSigninBubbleWithResult(
      const AccountInfo& account_info,
      SigninInterceptionResult expected_result) {
    FakeDiceWebSigninInterceptorDelegate* interceptor_delegate =
        ShowSigninBubble(account_info, expected_result);

    // Bubble should be shown following the intercept.
    EXPECT_TRUE(interceptor_delegate->intercept_bubble_shown());

    // The handling of the response to the bubble is done asynchronously in
    // `FakeDiceWebSigninInterceptorDelegate::ShowSigninInterceptionBubble()`.
    base::RunLoop().RunUntilIdle();

    // Following the result the bubble should have been desrtoyed.
    EXPECT_TRUE(interceptor_delegate->intercept_bubble_destroyed());
  }

  void ExpectChromeSigninBubbleShownCount(
      const base::HistogramTester& histogram_tester,
      size_t times,
      size_t count) {
    histogram_tester.ExpectBucketCount(
        "Signin.Intercept.ChromeSignin.BubbleShownCount", times, count);
  }

  void ExpectTotalChromeSigninBubbleShownCount(
      const base::HistogramTester& histogram_tester,
      size_t count) {
    histogram_tester.ExpectTotalCount(
        "Signin.Intercept.ChromeSignin.BubbleShownCount", count);
  }

  bool IsChromeSignedIn() const {
    return identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  }

  void SetSignoutAllowed(bool allow) {
    // Accepting management in order not to get signed out when restarting the
    // browser. Since this test uses the fake IdentityManager cookies will not
    // be saved on disc, therefore unable to find them back on startup which is
    // causing a startup signout. Managed accounts cannot be signed out which is
    // a workaround not to be signed out on Chrome restart.
    chrome::enterprise_util::SetUserAcceptedAccountManagement(GetProfile(),
                                                              !allow);
  }
};

// Test to sign in to Chrome from the Chrome Signin Bubble Intercept with
// `switches::kUnoDesktop` enabled.
class DiceWebSigninInterceptorWithUnoEnabledBrowserTest
    : public DiceWebSigninInterceptorWithChromeSigninHelpersBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{switches::kUnoDesktop};
};

IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorWithUnoEnabledBrowserTest,
                       ChromeSigninInterceptAccepted) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Setup account for interception.
  AccountInfo account_info =
      MakeAccountInfoAvailableAndUpdate("alice@example.com");
  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  ShowAndCompleteSigninBubbleWithResult(account_info,
                                        SigninInterceptionResult::kAccepted);

  EXPECT_TRUE(identity_test_env()->identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  // Check that the password account storage is enabled.
  PrefService* pref_service = GetProfile()->GetPrefs();
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      sync_service));
  EXPECT_EQ(password_manager::features_util::GetDefaultPasswordStore(
                pref_service, sync_service),
            password_manager::PasswordForm::Store::kAccountStore);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptChromeSignin);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.ChromeSignin.AttemptsBeforeAccept",
      /*sample=*/0, /*expected_bucket_count=*/1);
  auto access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE;
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Started", access_point, 1);
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Completed", access_point,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.ChromeSignin.AttemptsBeforeAccept", 0, 1);

  ExpectTotalChromeSigninBubbleShownCount(histogram_tester, 1);
}

IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorWithUnoEnabledBrowserTest,
                       ChromeSigninInterceptDeclined) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Setup account for interception.
  AccountInfo account_info =
      MakeAccountInfoAvailableAndUpdate("alice@example.com");
  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  // This pref should contain no data before the bubble is shown.
  ASSERT_FALSE(
      GetChromeSigninInterceptDeclinedCountPref(account_info).has_value());

  ShowAndCompleteSigninBubbleWithResult(account_info,
                                        SigninInterceptionResult::kDeclined);

  EXPECT_FALSE(IsChromeSignedIn());
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      SyncServiceFactory::GetForProfile(GetProfile())));
  // The pref should have recorded the declined action.
  EXPECT_EQ(GetChromeSigninInterceptDeclinedCountPref(account_info), 1);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptChromeSignin);
  auto access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE;
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Started", access_point, 0);
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Completed", access_point,
                                      0);

  ExpectTotalChromeSigninBubbleShownCount(histogram_tester, 1);
}

IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorWithUnoEnabledBrowserTest,
                       ChromeSigninInterceptDeclinedPrefCheck) {
  base::HistogramTester histogram_tester;

  // Setup a first account for interception.
  AccountInfo info1 = MakeAccountInfoAvailableAndUpdate("alice1@example.com");

  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  // This pref should contain no data before the bubble is shown.
  ASSERT_FALSE(GetChromeSigninInterceptDeclinedCountPref(info1).has_value());

  // Intercept declined on account1 twice.
  ShowAndCompleteSigninBubbleWithResult(info1,
                                        SigninInterceptionResult::kDeclined);
  ShowAndCompleteSigninBubbleWithResult(info1,
                                        SigninInterceptionResult::kDeclined);

  // Expect the pref to record both declines.
  int expected_info1_decline_count = 2;
  EXPECT_EQ(GetChromeSigninInterceptDeclinedCountPref(info1),
            expected_info1_decline_count);

  // Setup the second account for interception.
  AccountInfo info2 = MakeAccountInfoAvailableAndUpdate("alice2@example.com");
  ASSERT_FALSE(info2.IsEmpty());
  ASSERT_FALSE(GetChromeSigninInterceptDeclinedCountPref(info2).has_value());
  // Signout the account1 so that the account2 can get the interception.
  identity_test_env()->RemoveRefreshTokenForAccount(info1.account_id);

  // Intercept declined on account2.
  ShowAndCompleteSigninBubbleWithResult(info2,
                                        SigninInterceptionResult::kDeclined);

  // Account2 pref should be affected and account1 should not.
  EXPECT_EQ(GetChromeSigninInterceptDeclinedCountPref(info1),
            expected_info1_decline_count);
  EXPECT_EQ(GetChromeSigninInterceptDeclinedCountPref(info2), 1);

  // Accepting the intercept on account2 should reset the pref and log in the
  // histogram.
  ShowAndCompleteSigninBubbleWithResult(info2,
                                        SigninInterceptionResult::kAccepted);

  EXPECT_FALSE(GetChromeSigninInterceptDeclinedCountPref(info2).has_value());
  EXPECT_EQ(GetChromeSigninInterceptDeclinedCountPref(info1),
            expected_info1_decline_count);
  // Record the 2 declines that happened before accepting the intercept.
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.ChromeSignin.AttemptsBeforeAccept",
      /*sample=*/1, /*expected_bucket_count=*/1);

  ExpectTotalChromeSigninBubbleShownCount(histogram_tester, 4);
}

// In the following test, we show the bubble multiple times with different
// results and two different accounts to test the max number of times the bubble
// is allowed to be shown. We reach the maximum with account1 then continue
// trying with account2. The maximum is `kMaxChromeSigninInterceptionShownCount`
// (5) times. The 6th time the bubble is tried to be shown, it should fail.
// Trying with another account should not be blocking though, which is what is
// shown with account2 showing the bubble even though account1 reached the max.
// Only 1 account is allowed to be signed in at a time in order to show the
// bubble.
// Also checks the `Signin.Intercept.ChromeSignin.NumBubbleShown` histogram
// values after each time the bubble is shown.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorWithUnoEnabledBrowserTest,
                       ChromeSigninInterceptShownCount) {
  base::HistogramTester histogram_tester;

  // Setup a first account for interception.
  AccountInfo info1 = MakeAccountInfoAvailableAndUpdate("alice1@example.com");

  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  // This pref should contain no data before the bubble is shown.
  ASSERT_FALSE(GetChromeSigninInterceptShownCountPref(info1).has_value());

  // Intercept declined on account1 twice.
  ShowAndCompleteSigninBubbleWithResult(info1,
                                        SigninInterceptionResult::kDeclined);
  ExpectChromeSigninBubbleShownCount(histogram_tester, 1, 1);
  ShowAndCompleteSigninBubbleWithResult(info1,
                                        SigninInterceptionResult::kDeclined);
  ExpectChromeSigninBubbleShownCount(histogram_tester, 2, 1);
  ShowAndCompleteSigninBubbleWithResult(info1,
                                        SigninInterceptionResult::kAccepted);
  ExpectChromeSigninBubbleShownCount(histogram_tester, 3, 1);

  // Expect the pref to record all the times the bubble was shown for `info1`,
  // even when accepting.
  int expected_bubble_shown_count_info1 = 3;
  EXPECT_EQ(GetChromeSigninInterceptShownCountPref(info1),
            expected_bubble_shown_count_info1);

  // Signout the account1 so that the account2 can get the interception.
  identity_test_env()->RemoveRefreshTokenForAccount(info1.account_id);

  // Setup the second account for interception.
  AccountInfo info2 = MakeAccountInfoAvailableAndUpdate("alice2@example.com");
  ASSERT_FALSE(info2.IsEmpty());
  ASSERT_FALSE(GetChromeSigninInterceptShownCountPref(info2).has_value());

  // Intercept declined on account2.
  ShowAndCompleteSigninBubbleWithResult(info2,
                                        SigninInterceptionResult::kDeclined);
  ExpectChromeSigninBubbleShownCount(histogram_tester, 1, 2);

  // Account2 pref should be affected and account1 should not.
  EXPECT_EQ(GetChromeSigninInterceptShownCountPref(info1),
            expected_bubble_shown_count_info1);
  int expected_bubble_shown_count_info2 = 1;
  EXPECT_EQ(GetChromeSigninInterceptShownCountPref(info2),
            expected_bubble_shown_count_info2);

  // Signout account 2 and make account 1 available again.
  identity_test_env()->RemoveRefreshTokenForAccount(info2.account_id);
  info1 = MakeAccountInfoAvailableAndUpdate(info1.email);

  // Proceed with showing the bubble 2 more times (5 times overall).
  ShowAndCompleteSigninBubbleWithResult(info1,
                                        SigninInterceptionResult::kAccepted);
  ExpectChromeSigninBubbleShownCount(histogram_tester, 4, 1);

  // Sign out account 1 after accepting the bubble and resign in.
  identity_test_env()->RemoveRefreshTokenForAccount(info1.account_id);
  info1 = MakeAccountInfoAvailableAndUpdate(info1.email);
  ShowAndCompleteSigninBubbleWithResult(info1,
                                        SigninInterceptionResult::kDeclined);
  ExpectChromeSigninBubbleShownCount(histogram_tester, 5, 1);

  expected_bubble_shown_count_info1 += 2;
  EXPECT_EQ(GetChromeSigninInterceptShownCountPref(info1),
            expected_bubble_shown_count_info1);

  ExpectTotalChromeSigninBubbleShownCount(
      histogram_tester,
      expected_bubble_shown_count_info1 + expected_bubble_shown_count_info2);

  // Attempts to show a 6th time. It should not show the bubble.
  // No expected result since the bubble should be not be shown.
  FakeDiceWebSigninInterceptorDelegate* delegate =
      ShowSigninBubble(info1, /*expected_result=*/absl::nullopt);
  EXPECT_FALSE(delegate->intercept_bubble_shown());
  // Pref bubble shown count should remain the same.
  EXPECT_EQ(GetChromeSigninInterceptShownCountPref(info1),
            expected_bubble_shown_count_info1);

  // Signout account 1 and make account 2 available again.
  identity_test_env()->RemoveRefreshTokenForAccount(info1.account_id);
  info2 = MakeAccountInfoAvailableAndUpdate(info2.email);
  // Make sure that this value did not change after attempting to show the
  // bubble for the 6th time for info1.
  ExpectTotalChromeSigninBubbleShownCount(
      histogram_tester,
      expected_bubble_shown_count_info1 + expected_bubble_shown_count_info2);

  // Account 2 can still show the bubble since it didn't reach the max count
  // yet.
  ShowAndCompleteSigninBubbleWithResult(info2,
                                        SigninInterceptionResult::kDeclined);
  ExpectChromeSigninBubbleShownCount(histogram_tester, 2, 2);
  expected_bubble_shown_count_info2 += 1;
  EXPECT_EQ(GetChromeSigninInterceptShownCountPref(info2),
            expected_bubble_shown_count_info2);

  ExpectTotalChromeSigninBubbleShownCount(
      histogram_tester,
      expected_bubble_shown_count_info1 + expected_bubble_shown_count_info2);
}

// Test the memory of the user's account storage preference.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorWithUnoEnabledBrowserTest,
                       OptOutOfAccountStorage) {
  // Setup account and accept intersection.
  AccountInfo account_info =
      MakeAccountInfoAvailableAndUpdate("alice@example.com");
  ShowAndCompleteSigninBubbleWithResult(account_info,
                                        SigninInterceptionResult::kAccepted);

  // Check that the password account storage is enabled.
  PrefService* pref_service = GetProfile()->GetPrefs();
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      sync_service));

  // Opt out of account storage.
  password_manager::features_util::OptOutOfAccountStorageAndClearSettings(
      pref_service, sync_service);

  // Check that the password account storage is disabled.
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      sync_service));

  // Log out.
  identity_test_env()->ClearPrimaryAccount();

  // Check that the password account storage is false if there is no account.
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      sync_service));

  // Log in again.
  account_info = MakeAccountInfoAvailableAndUpdate("alice@example.com");
  ShowAndCompleteSigninBubbleWithResult(account_info,
                                        SigninInterceptionResult::kAccepted);

  // Check that the password account storage is still disabled.
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      sync_service));
}

// Test Suite where PRE_* tests are with `switches::kUnoDesktop` disabled, and
// regular test with `switches::kUnoDesktop` enabled, simulating users
// transitioning in to `switches::kUnoDesktop` active.
class DiceWebSigninInterceptorWithUnoEnabledAndPREDisabledBrowserTest
    : public DiceWebSigninInterceptorWithChromeSigninHelpersBrowserTest {
 public:
  DiceWebSigninInterceptorWithUnoEnabledAndPREDisabledBrowserTest() {
    if (content::IsPreTest()) {
      feature_list_.InitAndDisableFeature(switches::kUnoDesktop);
    } else {
      feature_list_.InitAndEnableFeature(switches::kUnoDesktop);
    }
  }

 protected:
  const std::string email_ = "alice@example.com";

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Signing in to Chrome while `switches::kUnoDesktop` is disabled, to simulate a
// signed in user prior to `switches::kUnoDesktop` activation, then enabling the
// feature for them.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithUnoEnabledAndPREDisabledBrowserTest,
    PRE_ChromeSignedInTransitionToUnoEnabled) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(switches::kUnoDesktop));

  signin::MakePrimaryAccountAvailable(identity_manager(), email_,
                                      signin::ConsentLevel::kSignin);

  EXPECT_TRUE(IsChromeSignedIn());
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      syncer::prefs::kExplicitBrowserSignin));
  // Passwords are defaulted to disabled without an explicit signin.
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      SyncServiceFactory::GetForProfile(GetProfile())));

  SetSignoutAllowed(false);
}

// Enabling `switches::kUnoDesktop`, after being signed in already.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithUnoEnabledAndPREDisabledBrowserTest,
    ChromeSignedInTransitionToUnoEnabled) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(switches::kUnoDesktop));
  // We are still signed in from the PRE_ test.
  ASSERT_TRUE(IsChromeSignedIn());

  // Starting Chrome with a Signed in account prior to `switches::kUnoDesktop`
  // activation should not turn this pref on.
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      syncer::prefs::kExplicitBrowserSignin));
  // Since we did not interact with passwords before, passwords should remain
  // disabled as long as we did not explicitly sign in.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(GetProfile());
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      sync_service));

  // Sign out, and sign back in.
  SetSignoutAllowed(true);
  identity_test_env()->ClearPrimaryAccount();
  ASSERT_FALSE(IsChromeSignedIn());
  signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::
                               ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE)
          .Build(email_));

  // Explicit Signing in while `switches::kUnoDesktop` is active should be
  // stored.
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      syncer::prefs::kExplicitBrowserSignin));
  // Signing in with `switches::kUnoDesktop` enabled, should affect the
  // passwords default.
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      sync_service));

  // Sign out should clear the explicit signin pref.
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      syncer::prefs::kExplicitBrowserSignin));
}

// Test Suite where PRE_* tests are with `switches::kUnoDesktop` enabled, and
// regular test with `switches::kUnoDesktop` disabled. Simulating a rollback.
class DiceWebSigninInterceptorWithUnoDisabledAndPREEnabledBrowserTest
    : public DiceWebSigninInterceptorWithChromeSigninHelpersBrowserTest {
 public:
  DiceWebSigninInterceptorWithUnoDisabledAndPREEnabledBrowserTest() {
    if (content::IsPreTest()) {
      feature_list_.InitAndEnableFeature(switches::kUnoDesktop);
    } else {
      feature_list_.InitAndDisableFeature(switches::kUnoDesktop);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithUnoDisabledAndPREEnabledBrowserTest,
    PRE_ChromeSignedinWithUnoShouldRevertBackToDefaultWithUnoDisabled) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(switches::kUnoDesktop));

  signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::
                               ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE)
          .Build("alice@example.com"));

  EXPECT_TRUE(IsChromeSignedIn());
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      syncer::prefs::kExplicitBrowserSignin));
  // Passwords are defaulted to enabled with an explicit sign in and
  // `switches::kUnoDesktop` active.
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      SyncServiceFactory::GetForProfile(GetProfile())));

  SetSignoutAllowed(false);
}

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithUnoDisabledAndPREEnabledBrowserTest,
    ChromeSignedinWithUnoShouldRevertBackToDefaultWithUnoDisabled) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(switches::kUnoDesktop));

  // Disabling `switches::kUnoDesktop` should not reset the pref.
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      syncer::prefs::kExplicitBrowserSignin));
  // Disabling `switches::kUnoDesktop` feature should revert back to the
  // previous default state, since there were no interactions, defaults to
  // disabled.
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      SyncServiceFactory::GetForProfile(GetProfile())));
}

// WebApps do not trigger interception. Regression test for
// https://crbug.com/1414988
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest,
                       WebAppNoInterception) {
  base::HistogramTester histogram_tester;
  // Setup profile for interception.
  identity_test_env()->MakeAccountAvailable("alice@example.com");
  AccountInfo account_info =
      MakeAccountInfoAvailableAndUpdate("bob@example.com");

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
  webapps::AppId app_id =
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
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
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
      MakeAccountInfoAvailableAndUpdate("bob@example.com");
  IdentityManagerFactory::GetForProfile(GetProfile())
      ->GetPrimaryAccountMutator()
      ->SetPrimaryAccount(primary_account_info.account_id,
                          signin::ConsentLevel::kSync);

  AccountInfo account_info =
      MakeAccountInfoAvailableAndUpdate("alice@example.com");

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
      MakeAccountInfoAvailableAndUpdate("alice@example.com");

  AccountInfo primary_account_info =
      MakeAccountInfoAvailableAndUpdate("bob@example.com");

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
  interceptor->MaybeInterceptWebSignin(
      web_contents, account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
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
      MakeAccountInfoAvailableAndUpdate("alice@example.com");

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
      MakeAccountInfoAvailableAndUpdate("alice@example.com");

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
  interceptor->MaybeInterceptWebSignin(
      web_contents, account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
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
      MakeAccountInfoAvailableAndUpdate("alice@example.com");

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
  interceptor->MaybeInterceptWebSignin(
      web_contents, account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
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
      MakeAccountInfoAvailableAndUpdate("alice@example.com");

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
      MakeAccountInfoAvailableAndUpdate("alice@example.com");

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
  interceptor->MaybeInterceptWebSignin(
      web_contents, account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
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
      MakeAccountInfoAvailableAndUpdate("alice@example.com");

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
  interceptor->MaybeInterceptWebSignin(
      web_contents, account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
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
      MakeAccountInfoAvailableAndUpdate("alice@example.com");

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
      MakeAccountInfoAvailableAndUpdate("alice@example.com");
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
  interceptor->MaybeInterceptWebSignin(
      web_contents, account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
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

class DiceWebSigninInterceptorParametrizedBrowserTest
    : public DiceWebSigninInterceptorBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  DiceWebSigninInterceptorParametrizedBrowserTest() {
    if (WithSearchEngineChoiceEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(switches::kSearchEngineChoice);
    } else {
      scoped_feature_list_.InitAndDisableFeature(switches::kSearchEngineChoice);
    }
  }

  bool WithSearchEngineChoiceEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests the complete interception flow including profile and browser creation.
IN_PROC_BROWSER_TEST_P(DiceWebSigninInterceptorParametrizedBrowserTest,
                       InterceptionTest) {
  base::HistogramTester histogram_tester;
  // Setup profile for interception.
  identity_test_env()->MakePrimaryAccountAvailable(
      "alice@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info = MakeAccountInfoAvailableAndUpdate(
      "bob@example.com", kNoHostedDomainFound);

  SetupGaiaResponses();

#if BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
  int64_t search_engine_choice_timestamp =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();
  const char kChoiceVersion[] = "1.2.3.4";
  if (WithSearchEngineChoiceEnabled()) {
    PrefService* pref_service = browser()->profile()->GetPrefs();
    pref_service->SetInt64(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
        search_engine_choice_timestamp);
    pref_service->SetString(
        prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
        kChoiceVersion);

    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    SetUserSelectedDefaultSearchProvider(template_url_service);
  }
#endif

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

#if BUILDFLAG(ENABLE_SEARCH_ENGINE_CHOICE)
  if (WithSearchEngineChoiceEnabled()) {
    PrefService* new_pref_service = new_profile->GetPrefs();
    EXPECT_EQ(new_pref_service->GetInt64(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
              search_engine_choice_timestamp);
    EXPECT_EQ(new_pref_service->GetString(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
              kChoiceVersion);

    TemplateURLService* new_template_url_service =
        TemplateURLServiceFactory::GetForProfile(new_profile);
    EXPECT_EQ(
        new_template_url_service->GetDefaultSearchProvider()->short_name(),
        base::UTF8ToUTF16(std::string(kCustomSearchEngineDomain)));
  }
#endif

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

INSTANTIATE_TEST_SUITE_P(All,
                         DiceWebSigninInterceptorParametrizedBrowserTest,
                         testing::Bool());
