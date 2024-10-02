// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/multi_profile_credentials_filter.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/dice_web_signin_interceptor.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/sync_username_test_base.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Dummy DiceWebSigninInterceptor::Delegate that does nothing.
class TestDiceWebSigninInterceptorDelegate
    : public WebSigninInterceptor::Delegate {
 public:
  bool IsSigninInterceptionSupported(
      const content::WebContents& web_contents) override {
    return true;
  }

  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
  ShowSigninInterceptionBubble(
      content::WebContents* web_contents,
      const BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback) override {
    return nullptr;
  }
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
  ShowOidcInterceptionDialog(
      content::WebContents* web_contents,
      const BubbleParameters& bubble_parameters,
      signin::SigninChoiceWithConfirmationCallback callback,
      base::OnceClosure dialog_closed_closure,
      base::OnceClosure retry_callback) override {
    std::move(callback)
        .Then(std::move(dialog_closed_closure))
        .Run(signin::SIGNIN_CHOICE_CANCEL, base::DoNothing());
    return nullptr;
  }
  void ShowFirstRunExperienceInNewProfile(
      Browser* browser,
      const CoreAccountId& account_id,
      WebSigninInterceptor::SigninInterceptionType interception_type) override {
  }
};

class TestPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  // PasswordManagerClient:
  signin::IdentityManager* GetIdentityManager() override {
    return identity_manager_;
  }

  const syncer::SyncService* GetSyncService() const override {
    return sync_service_;
  }

  void set_identity_manager(signin::IdentityManager* manager) {
    identity_manager_ = manager;
  }

  void set_sync_service(const syncer::SyncService* sync_service) {
    sync_service_ = sync_service;
  }

 private:
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  raw_ptr<const syncer::SyncService> sync_service_ = nullptr;
};

}  // namespace

class MultiProfileCredentialsFilterTest : public BrowserWithTestWindowTest {
 public:
  MultiProfileCredentialsFilterTest()
      : sync_filter_(&test_password_manager_client_) {}

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  password_manager::PasswordManagerClient* password_manager_client() {
    return &test_password_manager_client_;
  }

  DiceWebSigninInterceptor* dice_web_signin_interceptor() {
    return dice_web_signin_interceptor_.get();
  }

  // Creates a profile, a tab and an account so that signing in this account
  // will be intercepted in the tab.
  AccountInfo SetupInterception() {
    std::string email = "bob@example.com";
    AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
    account_info.full_name = "fullname";
    account_info.given_name = "givenname";
    account_info.hosted_domain = kNoHostedDomainFound;
    account_info.locale = "en";
    account_info.picture_url = "https://example.com";
    DCHECK(account_info.IsValid());
    identity_test_env()->UpdateAccountInfoForAccount(account_info);
    Profile* profile_2 = profile_manager()->CreateTestingProfile("Profile 2");
    ProfileAttributesEntry* entry =
        profile_manager()
            ->profile_attributes_storage()
            ->GetProfileAttributesWithPath(profile_2->GetPath());
    entry->SetAuthInfo(account_info.gaia, base::UTF8ToUTF16(email),
                       /*is_consented_primary_account=*/false);
    AddTab(browser(), GURL("http://foo/1"));
    return account_info;
  }

  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);
    dice_web_signin_interceptor_ = std::make_unique<DiceWebSigninInterceptor>(
        profile(), std::make_unique<TestDiceWebSigninInterceptorDelegate>());

    test_password_manager_client_.set_identity_manager(
        identity_test_env()->identity_manager());
    test_password_manager_client_.set_sync_service(&sync_service_);

    // If features::kEnablePasswordsAccountStorage is enabled, then the browser
    // never asks to save the primary account's password. So fake-signin an
    // arbitrary primary account here, so that any follow-up signs to the Gaia
    // page aren't considered primary account sign-ins and hence trigger the
    // password save prompt.
    identity_test_env()->MakePrimaryAccountAvailable(
        "primary@example.org", signin::ConsentLevel::kSync);
  }

  void TearDown() override {
    test_password_manager_client_.set_identity_manager(nullptr);
    dice_web_signin_interceptor_->Shutdown();
    identity_test_env_profile_adaptor_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.push_back(
        {ChromeSigninClientFactory::GetInstance(),
         base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                             &test_url_loader_factory_)});
    return factories;
  }

 protected:
  network::TestURLLoaderFactory test_url_loader_factory_;
  syncer::TestSyncService sync_service_;
  TestPasswordManagerClient test_password_manager_client_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  password_manager::SyncCredentialsFilter sync_filter_;
  std::unique_ptr<DiceWebSigninInterceptor> dice_web_signin_interceptor_;
};

// Checks that MultiProfileCredentialsFilter returns false when
// SyncCredentialsFilter returns false.
TEST_F(MultiProfileCredentialsFilterTest, SyncCredentialsFilter) {
  password_manager::PasswordForm form =
      password_manager::SyncUsernameTestBase::SimpleGaiaForm(
          "user@example.org");
  form.form_data.set_is_gaia_with_skip_save_password_form(true);

  ASSERT_FALSE(sync_filter_.ShouldSave(form));
  MultiProfileCredentialsFilter multi_profile_filter(
      password_manager_client(),
      /*dice_web_signin_interceptor=*/nullptr);
  EXPECT_FALSE(multi_profile_filter.ShouldSave(form));
}

// Returns true when the interceptor is nullptr.
TEST_F(MultiProfileCredentialsFilterTest, NullInterceptor) {
  password_manager::PasswordForm form =
      password_manager::SyncUsernameTestBase::SimpleGaiaForm(
          "user@example.org");
  ASSERT_TRUE(sync_filter_.ShouldSave(form));
  MultiProfileCredentialsFilter multi_profile_filter(
      password_manager_client(),
      /*dice_web_signin_interceptor=*/nullptr);
  EXPECT_TRUE(multi_profile_filter.ShouldSave(form));
}

// Returns true for non-gaia forms.
TEST_F(MultiProfileCredentialsFilterTest, NonGaia) {
  password_manager::PasswordForm form =
      password_manager::SyncUsernameTestBase::SimpleNonGaiaForm(
          "user@example.org");
  ASSERT_TRUE(sync_filter_.ShouldSave(form));

  MultiProfileCredentialsFilter multi_profile_filter(
      password_manager_client(), dice_web_signin_interceptor());
  EXPECT_TRUE(multi_profile_filter.ShouldSave(form));
}

// Returns false for an invalid email address.
// Regression test for https://crbug.com/1401924
TEST_F(MultiProfileCredentialsFilterTest, InvalidEmail) {
  // Disallow profile creation to prevent the intercept.
  g_browser_process->local_state()->SetBoolean(prefs::kBrowserAddPersonEnabled,
                                               false);

  password_manager::PasswordForm form =
      password_manager::SyncUsernameTestBase::SimpleGaiaForm("user@");
  ASSERT_TRUE(sync_filter_.ShouldSave(form));

  MultiProfileCredentialsFilter multi_profile_filter(
      password_manager_client(), dice_web_signin_interceptor());
  EXPECT_FALSE(multi_profile_filter.ShouldSave(form));
}

// Returns true for email addresses with no domain part when sign-in is not
// intercepted.
TEST_F(MultiProfileCredentialsFilterTest, UsernameWithNoDomain) {
  // Disallow profile creation to prevent the intercept.
  g_browser_process->local_state()->SetBoolean(prefs::kBrowserAddPersonEnabled,
                                               false);

  password_manager::PasswordForm form =
      password_manager::SyncUsernameTestBase::SimpleGaiaForm("user");
  ASSERT_TRUE(sync_filter_.ShouldSave(form));

  MultiProfileCredentialsFilter multi_profile_filter(
      password_manager_client(), dice_web_signin_interceptor());
  EXPECT_TRUE(multi_profile_filter.ShouldSave(form));
}

// Returns false when interception is already in progress.
TEST_F(MultiProfileCredentialsFilterTest, InterceptInProgress) {
  password_manager::PasswordForm form =
      password_manager::SyncUsernameTestBase::SimpleGaiaForm(
          "user@example.org");
  ASSERT_TRUE(sync_filter_.ShouldSave(form));

  // Start an interception for the sign-in.
  AccountInfo account_info = SetupInterception();
  dice_web_signin_interceptor_->MaybeInterceptWebSignin(
      browser()->tab_strip_model()->GetActiveWebContents(),
      account_info.account_id,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
      /*is_new_account=*/true,
      /*is_sync_signin=*/false);
  ASSERT_TRUE(dice_web_signin_interceptor_->is_interception_in_progress());

  MultiProfileCredentialsFilter multi_profile_filter(
      password_manager_client(), dice_web_signin_interceptor());
  EXPECT_FALSE(multi_profile_filter.ShouldSave(form));
}

// Returns false when the signin is not in progress yet, but the signin will be
// intercepted.
TEST_F(MultiProfileCredentialsFilterTest, SigninIntercepted) {
  const char kFormEmail[] = "user@example.org";
  password_manager::PasswordForm form =
      password_manager::SyncUsernameTestBase::SimpleGaiaForm(kFormEmail);
  ASSERT_TRUE(sync_filter_.ShouldSave(form));
  // Setup the account for interception, but do not intercept.
  AccountInfo account_info = SetupInterception();
  ASSERT_FALSE(dice_web_signin_interceptor_->is_interception_in_progress());
  ASSERT_EQ(dice_web_signin_interceptor_->GetHeuristicOutcome(
                /*is_new_account=*/true, /*is_sync_signin=*/false,
                account_info.email),
            SigninInterceptionHeuristicOutcome::kInterceptProfileSwitch);

  MultiProfileCredentialsFilter multi_profile_filter(
      password_manager_client(), dice_web_signin_interceptor());
  EXPECT_FALSE(multi_profile_filter.ShouldSave(form));
}

// Returns false when the outcome of the interception is unknown.
TEST_F(MultiProfileCredentialsFilterTest, SigninInterceptionUnknown) {
  const char kFormEmail[] = "user@example.org";
  password_manager::PasswordForm form =
      password_manager::SyncUsernameTestBase::SimpleGaiaForm(kFormEmail);
  ASSERT_TRUE(sync_filter_.ShouldSave(form));
  // Add extra Gaia account with incomplete info, so that interception outcome
  // is unknown.
  std::string dummy_email = "bob@example.com";
  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable(dummy_email);
  ASSERT_FALSE(dice_web_signin_interceptor_->is_interception_in_progress());
  ASSERT_FALSE(dice_web_signin_interceptor_->GetHeuristicOutcome(
      /*is_new_account=*/true, /*is_sync_signin=*/false, kFormEmail));

  MultiProfileCredentialsFilter multi_profile_filter(
      password_manager_client(), dice_web_signin_interceptor());
  EXPECT_FALSE(multi_profile_filter.ShouldSave(form));
}

// Returns true when the signin is not intercepted.
TEST_F(MultiProfileCredentialsFilterTest, SigninNotIntercepted) {
  // Disallow profile creation to prevent the intercept.
  g_browser_process->local_state()->SetBoolean(prefs::kBrowserAddPersonEnabled,
                                               false);

  std::string email = "user@example.org";
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(email);
  account_info.full_name = "fullname";
  account_info.given_name = "givenname";
  account_info.hosted_domain = kNoHostedDomainFound;
  account_info.locale = "en";
  account_info.picture_url = "https://example.com";
  DCHECK(account_info.IsValid());
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  password_manager::PasswordForm form =
      password_manager::SyncUsernameTestBase::SimpleGaiaForm(email.c_str());
  ASSERT_TRUE(sync_filter_.ShouldSave(form));
  // Not interception, credentials should be saved.
  ASSERT_FALSE(dice_web_signin_interceptor_->is_interception_in_progress());
  MultiProfileCredentialsFilter multi_profile_filter(
      password_manager_client(), dice_web_signin_interceptor());
  EXPECT_TRUE(multi_profile_filter.ShouldSave(form));
}
