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
#include "chrome/browser/signin/chrome_signin_pref_names.h"
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
#include "chrome/browser/ui/webui/settings/people_handler.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
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
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
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
#include "url/gurl.h"

namespace {

const char kCustomSearchEngineDomain[] = "bar.com";

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

class FakeBubbleHandle final : public ScopedWebSigninInterceptionBubbleHandle {
 public:
  ~FakeBubbleHandle() override = default;

  base::WeakPtr<FakeBubbleHandle> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeBubbleHandle> weak_ptr_factory_{this};
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
      std::string_view email,
      const std::string& hosted_domain = "example.com") {
    AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
    // Fill the account info, in particular for the hosted_domain field.
    account_info.full_name = "fullname";
    account_info.given_name = "givenname";
    account_info.hosted_domain = hosted_domain;
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";

    // Fill in the required account capabilities for the sign in intercept.
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_is_subject_to_parental_controls(false);

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

  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;

  std::map<content::BrowserContext*,
           raw_ptr<FakeDiceWebSigninInterceptorDelegate, CtnExperimental>>
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
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "alice@gmail.com", signin::ConsentLevel::kSignin);

  AccountInfo account_info = MakeAccountInfoAvailableAndUpdate(
      "bob@example.com", kNoHostedDomainFound);

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
  ChromeSigninUserChoice GetChromeSigninUserChoicePref(
      const AccountInfo& account_info) {
    return SigninPrefs(*GetProfile()->GetPrefs())
        .GetChromeSigninInterceptionUserChoice(account_info.gaia);
  }

  int GetChromeSigninInterceptDismissCountPref(
      const AccountInfo& account_info) {
    return SigninPrefs(*GetProfile()->GetPrefs())
        .GetChromeSigninInterceptionDismissCount(account_info.gaia);
  }

  void Signout() { identity_test_env()->ClearPrimaryAccount(); }

  FakeDiceWebSigninInterceptorDelegate* ShowSigninBubble(
      const AccountInfo& account_info,
      std::optional<SigninInterceptionResult> expected_result) {
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

  // Attempts to show the Chrome SigninBubble, checks that it doesn't.
  void ExpectAttemptToShowChromeSigninBubbleNotToShow(const AccountInfo& info) {
    FakeDiceWebSigninInterceptorDelegate* delegate =
        ShowSigninBubble(info, /*expected_result=*/std::nullopt);
    EXPECT_FALSE(delegate->intercept_bubble_shown());
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
    enterprise_util::SetUserAcceptedAccountManagement(GetProfile(), !allow);
  }
};

// Test to sign in to Chrome from the Chrome Signin Bubble Intercept.
class DiceWebSigninInterceptorWithExplicitSigninEnabledBrowserTest
    : public DiceWebSigninInterceptorWithChromeSigninHelpersBrowserTest {
 public:
  // This function is specific to ChromeSigninDecline reprompt logic, as it does
  // not really advance time, but marks the prefs of interest in the past in
  // order to satisfy the `delta` given.
  void SimulateChromeSigninDeclinedAdvanceTime(const std::string& gaia,
                                               base::TimeDelta delta) {
    SigninPrefs signin_prefs(*GetProfile()->GetPrefs());
    std::optional<base::Time> last_bubble_decline_time =
        signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(gaia);
    if (last_bubble_decline_time.has_value()) {
      signin_prefs.SetChromeSigninInterceptionLastBubbleDeclineTime(
          gaia, last_bubble_decline_time.value() - delta);
    }
  }

  base::TimeDelta time_since_last_reprompt(const std::string& gaia) {
    return DiceWebSigninInterceptor::
        GetTimeSinceLastChromeSigninDeclineForTesting(
            SigninPrefs(*GetProfile()->GetPrefs()), gaia);
  }

  // Simulate setting the ChromeSigninUserChoice through settings explicitly to
  // Do not signin.
  void SimulateSettingExplicitChromeSigninUserChoiceToDoNotSignin(
      const std::string& email) {
    settings::PeopleHandler handler(browser()->profile());
    // The only for the value to take effect is to choose another one first.
    // Choose always ask first in case the value is already set to
    // `ChromeSigninUserChoice::kDoNotSignin`.
    handler.HandleSetChromeSigninUserChoiceForTesting(
        email, ChromeSigninUserChoice::kAlwaysAsk);
    handler.HandleSetChromeSigninUserChoiceForTesting(
        email, ChromeSigninUserChoice::kDoNotSignin);
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitSigninEnabledBrowserTest,
    ChromeSigninInterceptAccepted) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Setup account for interception.
  const std::string account_email("alice@example.com");
  AccountInfo account_info = MakeAccountInfoAvailableAndUpdate(account_email);
  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  ShowAndCompleteSigninBubbleWithResult(account_info,
                                        SigninInterceptionResult::kAccepted);

  EXPECT_TRUE(IsChromeSignedIn());

  // Check that the password account storage is enabled.
  PrefService* pref_service = GetProfile()->GetPrefs();
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      pref_service, sync_service));
  EXPECT_EQ(password_manager::features_util::GetDefaultPasswordStore(
                pref_service, sync_service),
            password_manager::PasswordForm::Store::kAccountStore);

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptChromeSignin);
  auto access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE;
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Started", access_point, 1);
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Completed", access_point,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.ChromeSignin.DismissesBeforeAccept", 0, 1);

  ChromeSigninUserChoice user_choice =
      GetChromeSigninUserChoicePref(account_info);
  // User choice is remembered.
  EXPECT_EQ(user_choice, ChromeSigninUserChoice::kSignin);

  // Attempting to show the bubble after an explicit choice.

  // Signout to attempt signing in again and show the bubble.
  Signout();
  ASSERT_FALSE(IsChromeSignedIn());
  // Make account available again.
  account_info = MakeAccountInfoAvailableAndUpdate(account_email);
  // Chrome Signin bubble should not show if the user already made a choice.
  ExpectAttemptToShowChromeSigninBubbleNotToShow(account_info);
}

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitSigninEnabledBrowserTest,
    ChromeSigninInterceptDeclined) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Setup account for interception.
  AccountInfo account_info =
      MakeAccountInfoAvailableAndUpdate("alice@example.com");
  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  SigninPrefs signin_prefs(*GetProfile()->GetPrefs());
  ASSERT_FALSE(
      signin_prefs
          .GetChromeSigninInterceptionLastBubbleDeclineTime(account_info.gaia)
          .has_value());

  ShowAndCompleteSigninBubbleWithResult(account_info,
                                        SigninInterceptionResult::kDeclined);

  EXPECT_FALSE(IsChromeSignedIn());
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile()->GetPrefs(),
      SyncServiceFactory::GetForProfile(GetProfile())));

  CheckHistograms(histogram_tester,
                  SigninInterceptionHeuristicOutcome::kInterceptChromeSignin);
  auto access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE;
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Started", access_point, 0);
  histogram_tester.ExpectUniqueSample("Signin.SignIn.Completed", access_point,
                                      0);

  // User choice is remembered and decline time is stored.
  EXPECT_EQ(GetChromeSigninUserChoicePref(account_info),
            ChromeSigninUserChoice::kDoNotSignin);
  // Bubble decline time set.
  EXPECT_TRUE(
      signin_prefs
          .GetChromeSigninInterceptionLastBubbleDeclineTime(account_info.gaia)
          .has_value());
  // But no reprompt count.
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(account_info.gaia),
            0);

  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.ChromeSignin.DismissesBeforeDecline", 0, 1);

  // Attempting to show the bubble after an explicit choice.
  // Not enough time for a reprompt yet, bubble is not shown.

  ASSERT_FALSE(IsChromeSignedIn());
  // Chrome Signin bubble should not show if the user already made a choice.
  ExpectAttemptToShowChromeSigninBubbleNotToShow(account_info);
}

// In this test, we simulate moving time forward by setting the needed pref in
// the past. This allows to have the right conditions for reprompts. Testing the
// minimum time reprompt logic here.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitSigninEnabledBrowserTest,
    ChromeSigninInterceptDeclinesAndReprompts) {
  base::HistogramTester histogram_tester;
  // Setup account for interception.
  AccountInfo info = MakeAccountInfoAvailableAndUpdate("alice@example.com");
  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  SigninPrefs signin_prefs(*GetProfile()->GetPrefs());
  ASSERT_FALSE(
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia)
          .has_value());
  ASSERT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 0);

  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);
  EXPECT_FALSE(IsChromeSignedIn());
  // Decline time pref is set.
  std::optional<base::Time> initial_decline_time =
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia);
  ASSERT_TRUE(initial_decline_time.has_value());
  // Reprompt count is 0.
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 0);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSignin.NumberOfDaysSinceLastDecline", 0);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSignin.RepromptCount", 0);

  // Immediate attempt to show the bubble should not succeed, since not enough
  // time has passed.
  ExpectAttemptToShowChromeSigninBubbleNotToShow(info);

  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(15));

  // Attempt before the minimum duration for reprompt has passed, it should
  // fail.
  ExpectAttemptToShowChromeSigninBubbleNotToShow(info);

  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(46));

  // Bubble should show as we are in the first period where the bubble can be
  // reprompted. Decline it to proceed with the reprompts.
  ASSERT_GT(time_since_last_reprompt(info.gaia), base::Days(60));
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);
  // Last bubble time pref is still set.
  std::optional<base::Time> updated_last_decline_time =
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia);
  ASSERT_TRUE(updated_last_decline_time.has_value());
  // And different from the initial decline time.
  EXPECT_NE(initial_decline_time.value(), updated_last_decline_time.value());
  // Reprompt count updated
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 1);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSignin.NumberOfDaysSinceLastDecline", 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.ChromeSignin.RepromptCount", 1, 1);

  // Move time forward with less time than the expected minimum duration for the
  // reprompt. Should not show the bubble again yet.
  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(31));

  ASSERT_LT(time_since_last_reprompt(info.gaia), base::Days(60));
  ExpectAttemptToShowChromeSigninBubbleNotToShow(info);

  // Move time forward enough to bypass the minimum duration for the reprompt.
  // Should show the bubble again now.
  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(41));

  ASSERT_GT(time_since_last_reprompt(info.gaia), base::Days(60));
  // Decline it again to keep trying later. Second reprompt decline total
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 2);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSignin.NumberOfDaysSinceLastDecline", 2);
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.ChromeSignin.RepromptCount", 2, 1);

  // Move time forward enough time to bypass the minimum reprompt duration by a
  // big margin.
  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(120));

  ASSERT_GT(time_since_last_reprompt(info.gaia), base::Days(60));
  // Decline it again to keep trying later. 3rd reprompt decline.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 3);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSignin.NumberOfDaysSinceLastDecline", 3);
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.ChromeSignin.RepromptCount", 3, 1);

  // Repeat same operation for the last allowed reprompt.

  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(120));
  ASSERT_GT(time_since_last_reprompt(info.gaia), base::Days(60));
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 4);
  histogram_tester.ExpectTotalCount(
      "Signin.Intercept.ChromeSignin.NumberOfDaysSinceLastDecline", 4);
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.ChromeSignin.RepromptCount", 4, 1);

  // Maximum reprompt count reached. Make sure that no reprompts will be made
  // regardless of the time that has passed.

  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(30));
  // Less than the minimum duration between reprompts.
  ASSERT_LT(time_since_last_reprompt(info.gaia), base::Days(60));
  ExpectAttemptToShowChromeSigninBubbleNotToShow(info);

  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(120));
  // More than the minimum duration between reprompts.
  ASSERT_GT(time_since_last_reprompt(info.gaia), base::Days(60));
  // Still no reprompt.
  ExpectAttemptToShowChromeSigninBubbleNotToShow(info);
}

// This test makes sure that the reprompts are count based and not depending one
// total time duration.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitSigninEnabledBrowserTest,
    ChromeSigninInterceptRepromptsHasNoTimeLimit) {
  // Setup account for interception.
  AccountInfo info = MakeAccountInfoAvailableAndUpdate("alice@example.com");
  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  SigninPrefs signin_prefs(*GetProfile()->GetPrefs());
  ASSERT_FALSE(
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia)
          .has_value());

  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);

  EXPECT_TRUE(
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia)
          .has_value());

  // Advance a large amount of time. Greater than the minimum duration.
  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(300));

  ASSERT_GT(time_since_last_reprompt(info.gaia), base::Days(60));
  // Reprompt should happen.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);

  // Advance even larger amount of time.
  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(300));

  // Larger than the minimum duration.
  ASSERT_GT(time_since_last_reprompt(info.gaia), base::Days(60));
  // Reprompt should happen as the max count was not reached yet. The amount of
  // time that has passed is not significant as long as it is more than the
  // minimum duration between reprompts.
  // Result is not important.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kAccepted);
}

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitSigninEnabledBrowserTest,
    ChromeSigninInterceptDeclinesRepromptAttemptWithExplicitDoNotSignin) {
  // Setup account for interception.
  AccountInfo info = MakeAccountInfoAvailableAndUpdate("alice@example.com");
  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  SigninPrefs signin_prefs(*GetProfile()->GetPrefs());
  ASSERT_FALSE(
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia)
          .has_value());

  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);

  EXPECT_TRUE(
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia)
          .has_value());

  // Simulates settings change by the user through the settings page.
  SimulateSettingExplicitChromeSigninUserChoiceToDoNotSignin(info.email);

  EXPECT_FALSE(
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia)
          .has_value());
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 0);

  // Advance a large amount of time. No reprompt is expected.
  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(100));

  // No reprompts since the choice was explicitly set through settings.
  ExpectAttemptToShowChromeSigninBubbleNotToShow(info);
}

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitSigninEnabledBrowserTest,
    ChromeSigninInterceptDeclinesRepromptsThenDismissReprompt) {
  // Setup account for interception.
  AccountInfo info = MakeAccountInfoAvailableAndUpdate("alice@example.com");
  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  // Start by dismissing the bubble 3 times, to set up for later.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDismissed);
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDismissed);
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDismissed);

  SigninPrefs signin_prefs(*GetProfile()->GetPrefs());
  ASSERT_FALSE(
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia)
          .has_value());
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);
  EXPECT_TRUE(
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia)
          .has_value());

  // Advance enough time for a reprompt.
  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(70));

  ASSERT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 0);
  // Reprompt should be successful and we dismiss it.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDismissed);
  // Reprompt count did not change, as the dismiss did not trigger a completed
  // reprompt. Only decline should do that.
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 0);

  // A followup reprompt is then allowed directly without more time passing.
  // Dismissing again, the 5th time (given the first 3 dismisses), should be
  // treated as a decline and update the the reprompt count.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDismissed);
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 1);

  // Followup attempt to show the bubble should fail, without increasing the
  // time.
  ExpectAttemptToShowChromeSigninBubbleNotToShow(info);

  // Finally increasing the time should allow for more reprompts as we did not
  // reach the limit yet.
  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(70));

  // And followup dismisses should directly be treated as declines still.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDismissed);
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 2);
}

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitSigninEnabledBrowserTest,
    ChromeSigninInterceptDeclinesRepromptsThenAcceptReprompt) {
  // Setup account for interception.
  AccountInfo info = MakeAccountInfoAvailableAndUpdate("alice@example.com");
  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  SigninPrefs signin_prefs(*GetProfile()->GetPrefs());
  ASSERT_FALSE(
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia)
          .has_value());
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);
  // Bubble last decline time is set.
  EXPECT_TRUE(
      signin_prefs.GetChromeSigninInterceptionLastBubbleDeclineTime(info.gaia)
          .has_value());
  // Choice is set impliclty.
  EXPECT_EQ(signin_prefs.GetChromeSigninInterceptionUserChoice(info.gaia),
            ChromeSigninUserChoice::kDoNotSignin);
  // No reprompt yet.
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 0);

  // Advance enough time for a reprompt.
  SimulateChromeSigninDeclinedAdvanceTime(info.gaia, base::Days(70));

  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kAccepted);
  // Implicit choice is overridden to always sign in, accepting the bubble.
  EXPECT_EQ(signin_prefs.GetChromeSigninInterceptionUserChoice(info.gaia),
            ChromeSigninUserChoice::kSignin);
  // Still no reprompt.
  EXPECT_EQ(signin_prefs.GetChromeSigninBubbleRepromptCount(info.gaia), 0);
  EXPECT_TRUE(IsChromeSignedIn());
}

// Test the memory of the user's account storage preference.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitSigninEnabledBrowserTest,
    OptOutOfAccountStorage) {
  // Setup account and accept intersection.
  const std::string email("alice@example.com");
  AccountInfo account_info = MakeAccountInfoAvailableAndUpdate(email);
  ShowAndCompleteSigninBubbleWithResult(account_info,
                                        SigninInterceptionResult::kAccepted);

  // Check that the password account storage is enabled.
  PrefService* pref_service = GetProfile()->GetPrefs();
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      pref_service, sync_service));

  // Opt out of account storage.
  password_manager::features_util::OptOutOfAccountStorageAndClearSettings(
      pref_service, sync_service);

  // Check that the password account storage is disabled.
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      pref_service, sync_service));

  Signout();

  // Check that the password account storage is false if there is no account.
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      pref_service, sync_service));

  // Log in again.
  // Force a Chrome Signin. The bubble will not be shown again.
  identity_test_env()->MakePrimaryAccountAvailable(
      email, signin::ConsentLevel::kSignin);

  // Check that the password account storage is still disabled.
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      pref_service, sync_service));
}

// Test the recording of the user entering or resolving an inconsistent state
// (sign in pending with account A, sign in to web with account B).)
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitSigninEnabledBrowserTest,
    RecordInconsistentStateResolvedAfterSignInPending) {
  base::HistogramTester histogram_tester;

  // Set up a primary account in sign in pending state and a secondary account
  // signing into the web, therefore inducing an inconsistent state.
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();
  AccountInfo secondary_account_info = MakeAccountInfoAvailableAndUpdate(
      "alice@example.com", kNoHostedDomainFound);

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);

  // Intercept.
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kMultiUser);
  source_interceptor_delegate->set_expected_interception_result(
      SigninInterceptionResult::kDismissed);
  interceptor->MaybeInterceptWebSignin(
      web_contents, secondary_account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
      /*is_new_account=*/false,
      /*is_sync_signin=*/false);

  histogram_tester.ExpectBucketCount(
      "Signin.SigninPending.InconsistentStateInvoked", true, 1);
}

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitSigninEnabledBrowserTest,
    MultiUserSigninInterception) {
  // Set up for Multi user signin interception.
  AccountInfo primary_account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          "bob@example.com", signin::ConsentLevel::kSignin);
  AccountInfo secondary_account_info = MakeAccountInfoAvailableAndUpdate(
      "alice@example.com", kNoHostedDomainFound);

  // Add a tab.
  GURL intercepted_url = embedded_test_server()->GetURL("/defaultresponse");
  content::WebContents* web_contents = AddTab(intercepted_url);

  // Intercept.
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(GetProfile());
  source_interceptor_delegate->set_expected_interception_type(
      WebSigninInterceptor::SigninInterceptionType::kMultiUser);
  source_interceptor_delegate->set_expected_interception_result(
      SigninInterceptionResult::kAccepted);
  ProfileWaiter waiter;
  interceptor->MaybeInterceptWebSignin(
      web_contents, secondary_account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
      /*is_new_account=*/true,
      /*is_sync_signin=*/false);

  // New Profile created from accepting the signin interception.
  Profile* new_profile = waiter.WaitForProfileAdded();
  // Should be signed in.
  EXPECT_TRUE(IdentityManagerFactory::GetForProfile(new_profile)
                  ->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  // ChromeSignin setting should be set.
  EXPECT_EQ(
      SigninPrefs(*new_profile->GetPrefs())
          .GetChromeSigninInterceptionUserChoice(secondary_account_info.gaia),
      ChromeSigninUserChoice::kSignin);
}

// Test to sign in to Chrome from the Chrome Signin Bubble Intercept with
// `switches::kExplicitBrowserSigninUIOnDesktop` enabled.
class DiceWebSigninInterceptorWithExplicitBrowserSigninBrowserTest
    : public DiceWebSigninInterceptorWithChromeSigninHelpersBrowserTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kExplicitBrowserSigninUIOnDesktop};
};

// This test mainly checks the combination of dismissal and the effect it has on
// the user choice. Simulating multiple accounts and checks that they do not
// affect each other:
// - Account1 dismisses the bubble twice.
// - Account2 dismisses the bubble once.
// - Account1 dismisses the bubble three more times, and make sure it is a
// decline.
// - Account2 accept the bubble.
// - Account1 changes it's pref to always ask and should show the bubble even
// after 5 dismisses.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitBrowserSigninBrowserTest,
    ChromeSigninInterceptDismissBehavior) {
  base::HistogramTester histogram_tester;

  // Setup a first account for interception.
  const std::string email1("alice1@example.com");
  AccountInfo info1 = MakeAccountInfoAvailableAndUpdate(email1);
  ASSERT_EQ(GetChromeSigninInterceptDismissCountPref(info1), 0);
  ASSERT_EQ(GetChromeSigninUserChoicePref(info1),
            ChromeSigninUserChoice::kNoChoice);

  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  // Intercept declined on account1 twice.
  ShowAndCompleteSigninBubbleWithResult(info1,
                                        SigninInterceptionResult::kDismissed);
  int expected_dismiss_count = 1;
  EXPECT_EQ(GetChromeSigninInterceptDismissCountPref(info1),
            expected_dismiss_count);
  EXPECT_EQ(GetChromeSigninUserChoicePref(info1),
            ChromeSigninUserChoice::kNoChoice);

  ShowAndCompleteSigninBubbleWithResult(info1,
                                        SigninInterceptionResult::kDismissed);
  ++expected_dismiss_count;
  EXPECT_EQ(GetChromeSigninInterceptDismissCountPref(info1),
            expected_dismiss_count);

  // Setup the second account for interception.
  AccountInfo info2 = MakeAccountInfoAvailableAndUpdate("alice2@example.com");
  ASSERT_FALSE(info2.IsEmpty());
  ASSERT_EQ(GetChromeSigninInterceptDismissCountPref(info2), 0);

  // Intercept dismissed on account2.
  ShowAndCompleteSigninBubbleWithResult(info2,
                                        SigninInterceptionResult::kDismissed);

  // Account2 pref should be affected and account1 should not.
  EXPECT_EQ(GetChromeSigninInterceptDismissCountPref(info1),
            expected_dismiss_count);
  EXPECT_EQ(GetChromeSigninInterceptDismissCountPref(info2), 1);

  // 3 more dismisses on account1:
  for (int i = 0; i < 3; ++i) {
    ShowAndCompleteSigninBubbleWithResult(info1,
                                          SigninInterceptionResult::kDismissed);
    ++expected_dismiss_count;
    EXPECT_EQ(GetChromeSigninInterceptDismissCountPref(info1),
              expected_dismiss_count);
  }
  // 5 dismiss treated as a do not sign in.
  EXPECT_EQ(GetChromeSigninUserChoicePref(info1),
            ChromeSigninUserChoice::kDoNotSignin);

  // A decline should have been recorded.
  size_t expected_decline_count = 1;
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.ChromeSignin.DismissesBeforeDecline",
      /*sample=*/5, /*expected_count=*/expected_decline_count);

  // A 6h attempt to show should fail.
  ExpectAttemptToShowChromeSigninBubbleNotToShow(info1);

  // Accepting the intercept on account2 should reset the pref and log in the
  // histogram.
  ShowAndCompleteSigninBubbleWithResult(info2,
                                        SigninInterceptionResult::kAccepted);
  // Dismiss count remains.
  EXPECT_EQ(GetChromeSigninInterceptDismissCountPref(info2), 1);
  // Record the 1 dismiss that happened before accepting the intercept.
  histogram_tester.ExpectUniqueSample(
      "Signin.Intercept.ChromeSignin.DismissesBeforeAccept",
      /*sample=*/1, /*expected_bucket_count=*/1);

  // Make sure to signout account2.
  Signout();
  // Make account1 available again.
  info1 = MakeAccountInfoAvailableAndUpdate(email1);
  // Override account1 pref to always ask.
  SigninPrefs(*GetProfile()->GetPrefs())
      .SetChromeSigninInterceptionUserChoice(
          info1.gaia, ChromeSigninUserChoice::kAlwaysAsk);
  // Showing the bubble should succeed -- result is not important, only affect
  // histogram recorded.
  ShowAndCompleteSigninBubbleWithResult(info1,
                                        SigninInterceptionResult::kDeclined);
  ++expected_decline_count;
  histogram_tester.ExpectBucketCount(
      "Signin.Intercept.ChromeSignin.DismissesBeforeDecline",
      /*sample=*/5, /*expected_count=*/expected_decline_count);
}

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitBrowserSigninBrowserTest,
    OverrideUserChoicePrefAfterAccept) {
  // Setup an account for interception.
  const std::string email("alice1@example.com");
  AccountInfo info = MakeAccountInfoAvailableAndUpdate(email);

  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kAccepted);
  // Choice is remembered.
  EXPECT_EQ(GetChromeSigninUserChoicePref(info),
            ChromeSigninUserChoice::kSignin);

  // Signout to attempt signing in again.
  Signout();
  // Make account available again.
  info = MakeAccountInfoAvailableAndUpdate(email);
  // Attempting to show the bubble again should fail since we already have a
  // user choice.
  ExpectAttemptToShowChromeSigninBubbleNotToShow(info);

  // Override account1 pref to always ask -- simulating changing it through the
  // settings.
  SigninPrefs(*GetProfile()->GetPrefs())
      .SetChromeSigninInterceptionUserChoice(
          info.gaia, ChromeSigninUserChoice::kAlwaysAsk);
  // Showing the bubble should succeed -- result is not important.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDismissed);
}

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitBrowserSigninBrowserTest,
    OverrideUserChoicePrefAfterDecline) {
  // Setup an account for interception.
  const std::string email("alice1@example.com");
  AccountInfo info = MakeAccountInfoAvailableAndUpdate(email);

  // Makes sure Chrome is not signed in to trigger the Chrome Sigin intercept
  // bubble.
  ASSERT_FALSE(IsChromeSignedIn());

  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);
  // Choice is remembered.
  EXPECT_EQ(GetChromeSigninUserChoicePref(info),
            ChromeSigninUserChoice::kDoNotSignin);

  // Attempting to show the bubble again should fail since we already have a
  // user choice.
  ExpectAttemptToShowChromeSigninBubbleNotToShow(info);

  // Override account1 pref to always ask -- simulating changing it through the
  // settings.
  SigninPrefs(*GetProfile()->GetPrefs())
      .SetChromeSigninInterceptionUserChoice(
          info.gaia, ChromeSigninUserChoice::kAlwaysAsk);
  // Showing the bubble should succeed -- result is not important.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDismissed);
}

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithExplicitBrowserSigninBrowserTest,
    ChromeSigninBubbleResultsWithAlwaysAskUserChoice) {
  // Setup an account for interception.
  const std::string email("alice1@example.com");
  AccountInfo info = MakeAccountInfoAvailableAndUpdate(email);

  // Set user choice to `ChromeSigninUserChoice::kAlwaysAsk` mode.
  SigninPrefs(*GetProfile()->GetPrefs())
      .SetChromeSigninInterceptionUserChoice(
          info.gaia, ChromeSigninUserChoice::kAlwaysAsk);

  int current_dismiss_count = GetChromeSigninInterceptDismissCountPref(info);

  // Dismiss action.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDismissed);
  // Should not alter the dismiss count when in
  // `ChromeSigninUserChoice::kAlwaysAsk` mode.
  EXPECT_EQ(current_dismiss_count,
            GetChromeSigninInterceptDismissCountPref(info));

  // Decline action.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kDeclined);
  // Choice should not be remembered when in
  // `ChromeSigninUserChoice::kAlwaysAsk` mode.
  EXPECT_EQ(GetChromeSigninUserChoicePref(info),
            ChromeSigninUserChoice::kAlwaysAsk);

  // Accept action.
  ShowAndCompleteSigninBubbleWithResult(info,
                                        SigninInterceptionResult::kAccepted);
  // Choice should not be remembered when in
  // `ChromeSigninUserChoice::kAlwaysAsk` mode.
  EXPECT_EQ(GetChromeSigninUserChoicePref(info),
            ChromeSigninUserChoice::kAlwaysAsk);
}

// Test Suite where PRE_* tests are with
// `switches::kExplicitBrowserSigninUIOnDesktop` disabled, and regular test with
// `switches::kExplicitBrowserSigninUIOnDesktop` enabled, simulating users
// transitioning in to `switches::kExplicitBrowserSigninUIOnDesktop` active.
class DiceWebSigninInterceptorWithUnoEnabledAndPREDisabledBrowserTest
    : public DiceWebSigninInterceptorWithChromeSigninHelpersBrowserTest {
 public:
  DiceWebSigninInterceptorWithUnoEnabledAndPREDisabledBrowserTest() {
    feature_list_.InitWithFeatureState(
        switches::kExplicitBrowserSigninUIOnDesktop, !content::IsPreTest());
  }

 protected:
  const std::string email_ = "alice@example.com";

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Signing in to Chrome while `switches::kExplicitBrowserSigninUIOnDesktop` is
// disabled, to simulate a signed in user prior to
// `switches::kExplicitBrowserSigninUIOnDesktop` activation, then enabling the
// feature for them.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithUnoEnabledAndPREDisabledBrowserTest,
    PRE_ChromeSignedInTransitionToUnoEnabled) {
  ASSERT_FALSE(switches::IsExplicitBrowserSigninUIOnDesktopEnabled());

  signin::AccountAvailabilityOptionsBuilder builder;
  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager(),
      builder
          .AsPrimary(signin::ConsentLevel::kSignin)
          // `ACCESS_POINT_UNKNOWN` is not explicit signin.
          .WithAccessPoint(signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN)
          .Build(email_));

  EXPECT_TRUE(IsChromeSignedIn());
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kExplicitBrowserSignin));
  // Passwords are defaulted to disabled without an explicit signin.
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile()->GetPrefs(),
      SyncServiceFactory::GetForProfile(GetProfile())));

  SetSignoutAllowed(false);
}

// Enabling `switches::kExplicitBrowserSigninUIOnDesktop`, after being signed in
// already.
IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithUnoEnabledAndPREDisabledBrowserTest,
    ChromeSignedInTransitionToUnoEnabled) {
  ASSERT_TRUE(switches::IsExplicitBrowserSigninUIOnDesktopEnabled());
  // We are still signed in from the PRE_ test.
  ASSERT_TRUE(IsChromeSignedIn());

  // Starting Chrome with a Signed in account prior to
  // `switches::kExplicitBrowserSigninUIOnDesktop` activation should not turn
  // this pref on.
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kExplicitBrowserSignin));
  // Since we did not interact with passwords before, passwords should remain
  // disabled as long as we did not explicitly sign in.
  PrefService* pref_service = GetProfile()->GetPrefs();
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(GetProfile());
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      pref_service, sync_service));

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

  // Explicit Signing in while `switches::kExplicitBrowserSigninUIOnDesktop` is
  // active should be stored.
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kExplicitBrowserSignin));
  // Signing in with `switches::kExplicitBrowserSigninUIOnDesktop` enabled,
  // should affect the passwords default.
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      pref_service, sync_service));

  // Sign out should clear the explicit signin pref.
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kExplicitBrowserSignin));
}

// Test Suite where PRE_* tests are with
// `switches::kExplicitBrowserSigninUIOnDesktop` enabled, and regular test with
// `switches::kExplicitBrowserSigninUIOnDesktop` disabled. Simulating a
// rollback.
class DiceWebSigninInterceptorWithUnoDisabledAndPREEnabledBrowserTest
    : public DiceWebSigninInterceptorWithChromeSigninHelpersBrowserTest {
 public:
  DiceWebSigninInterceptorWithUnoDisabledAndPREEnabledBrowserTest() {
    feature_list_.InitWithFeatureState(
        switches::kExplicitBrowserSigninUIOnDesktop, content::IsPreTest());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithUnoDisabledAndPREEnabledBrowserTest,
    PRE_ChromeSignedinWithUnoShouldRevertBackToDefaultWithUnoDisabled) {
  ASSERT_TRUE(switches::IsExplicitBrowserSigninUIOnDesktopEnabled());

  signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder()
          .AsPrimary(signin::ConsentLevel::kSignin)
          .WithAccessPoint(signin_metrics::AccessPoint::
                               ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE)
          .Build("alice@example.com"));

  EXPECT_TRUE(IsChromeSignedIn());
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kExplicitBrowserSignin));
  // Passwords are defaulted to enabled with an explicit sign in and
  // `switches::kExplicitBrowserSigninUIOnDesktop` active.
  EXPECT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile()->GetPrefs(),
      SyncServiceFactory::GetForProfile(GetProfile())));

  SetSignoutAllowed(false);
}

IN_PROC_BROWSER_TEST_F(
    DiceWebSigninInterceptorWithUnoDisabledAndPREEnabledBrowserTest,
    ChromeSignedinWithUnoShouldRevertBackToDefaultWithUnoDisabled) {
  ASSERT_FALSE(switches::IsExplicitBrowserSigninUIOnDesktopEnabled());

  // Disabling `switches::kExplicitBrowserSigninUIOnDesktop` should reset the
  // pref.
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kExplicitBrowserSignin));
  // Disabling `switches::kExplicitBrowserSigninUIOnDesktop` feature should
  // revert back to the previous default state, since there were no
  // interactions, defaults to disabled.
  EXPECT_FALSE(password_manager::features_util::IsOptedInForAccountStorage(
      GetProfile()->GetPrefs(),
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
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(kWebAppURL);
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
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(new_profile));
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
  EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                  ->GetUserColor()
                  .has_value());

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
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
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
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(new_profile));
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
  EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                  ->GetUserColor()
                  .has_value());

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
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
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
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
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
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(new_profile));
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
  EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                  ->GetUserColor()
                  .has_value());

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

  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  // Start the interception.
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(GetProfile());
  interceptor->MaybeInterceptWebSignin(
      web_contents, account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
      /*is_new_account=*/false,
      /*is_sync_signin=*/false);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
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
    ForcedEnterpriseInterceptionPrimaryAccountReauthSyncEnabledTest) {
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

  EXPECT_EQ(BrowserList::GetInstance()->size(), 1u);
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
  // Start the interception.
  DiceWebSigninInterceptor* interceptor =
      DiceWebSigninInterceptorFactory::GetForProfile(GetProfile());
  interceptor->MaybeInterceptWebSignin(
      web_contents, account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN,
      /*is_new_account=*/false,
      /*is_sync_signin=*/false);
  base::RunLoop().RunUntilIdle();
  // Interception bubble was closed.
  FakeDiceWebSigninInterceptorDelegate* source_interceptor_delegate =
      GetInterceptorDelegate(GetProfile());
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

// Tests the complete interception flow including profile and browser creation.
IN_PROC_BROWSER_TEST_F(DiceWebSigninInterceptorBrowserTest, InterceptionTest) {
  base::HistogramTester histogram_tester;
  // Setup profile for interception.
  identity_test_env()->MakePrimaryAccountAvailable(
      "alice@example.com", signin::ConsentLevel::kSignin);
  AccountInfo account_info = MakeAccountInfoAvailableAndUpdate(
      "bob@example.com", kNoHostedDomainFound);

  SetupGaiaResponses();

  int64_t search_engine_choice_timestamp =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();
  const char kChoiceVersion[] = "1.2.3.4";
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
  EXPECT_TRUE(ThemeServiceFactory::GetForProfile(new_profile)
                  ->GetUserColor()
                  .has_value());

  PrefService* new_pref_service = new_profile->GetPrefs();
  EXPECT_EQ(new_pref_service->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
            search_engine_choice_timestamp);
  EXPECT_EQ(new_pref_service->GetString(
                prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
            kChoiceVersion);

  TemplateURLService* new_template_url_service =
      TemplateURLServiceFactory::GetForProfile(new_profile);
  EXPECT_EQ(new_template_url_service->GetDefaultSearchProvider()->short_name(),
            base::UTF8ToUTF16(std::string(kCustomSearchEngineDomain)));

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
