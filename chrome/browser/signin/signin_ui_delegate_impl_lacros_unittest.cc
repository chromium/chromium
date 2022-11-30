// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_delegate_impl_lacros.h"

#include "base/bind.h"
#include "base/containers/fixed_flat_map.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/account_manager_core/chromeos/fake_account_manager_ui.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin_ui_util {

namespace {

constexpr auto kPromoSuffixes = base::MakeFixedFlatMap<
    signin_metrics::PromoAction,
    base::StringPiece>(
    {{signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT, ".WithDefault"},
     {signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT, ".NotDefault"},
     {signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT,
      ".NewAccountNoExistingAccount"},
     {signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT,
      ".NewAccountExistingAccount"}});
constexpr base::StringPiece kSigninStartedHistogramBaseName =
    "Signin.SigninStartedAccessPoint";

constexpr signin_metrics::AccessPoint kAccessPoint =
    signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN;
constexpr signin_metrics::PromoAction kPromoAction =
    signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT;

class FakeAccountManagerUIDialogWaiter : public FakeAccountManagerUI::Observer {
 public:
  explicit FakeAccountManagerUIDialogWaiter(
      FakeAccountManagerUI* account_manager_ui) {
    scoped_observation_.Observe(account_manager_ui);
  }
  ~FakeAccountManagerUIDialogWaiter() override = default;

  void WaitForAddAccountDialogShown() { add_account_shown_run_loop_.Run(); }
  void WaitForReauthAccountDialogShown() { reauth_shown_run_loop_.Run(); }

  // FakeAccountManagerUI::Observer:
  void OnAddAccountDialogShown() override {
    add_account_shown_run_loop_.Quit();
  }
  void OnReauthAccountDialogShown() override { reauth_shown_run_loop_.Quit(); }

 private:
  base::RunLoop add_account_shown_run_loop_;
  base::RunLoop reauth_shown_run_loop_;
  base::ScopedObservation<FakeAccountManagerUI, FakeAccountManagerUI::Observer>
      scoped_observation_{this};
};

std::unique_ptr<KeyedService> BuildSigninManager(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<SigninManager>(
      profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile));
}

void ExpectOneSigninStartedHistograms(
    const base::HistogramTester& tester,
    signin_metrics::AccessPoint access_point,
    signin_metrics::PromoAction promo_action) {
  tester.ExpectUniqueSample(kSigninStartedHistogramBaseName, access_point, 1);
  for (const auto& [action, suffix] : kPromoSuffixes) {
    int expected_count = action == promo_action ? 1 : 0;
    tester.ExpectUniqueSample(
        base::StrCat({kSigninStartedHistogramBaseName, suffix}), access_point,
        expected_count);
  }
}

void ExpectNoSigninStartedHistograms(const base::HistogramTester& tester) {
  tester.ExpectTotalCount(kSigninStartedHistogramBaseName, 0);
  for (const auto& [action, suffix] : kPromoSuffixes) {
    tester.ExpectTotalCount(
        base::StrCat({kSigninStartedHistogramBaseName, suffix}), 0);
  }
}

}  // namespace

class SigninUiDelegateImplLacrosTest : public ::testing::TestWithParam<bool> {
 public:
  SigninUiDelegateImplLacrosTest() {
    auto fake_ui = std::make_unique<FakeAccountManagerUI>();
    fake_account_manager_ui_ = fake_ui.get();
    scoped_account_manager_ =
        std::make_unique<ScopedAshAccountManagerForTests>(std::move(fake_ui));
    auto* account_manager = MaybeGetAshAccountManagerForTests();
    account_manager->InitializeInEphemeralMode(
        test_url_loader_factory_.GetSafeWeakWrapper());

    CHECK(profile_manager_.SetUp());
    // Need to explicitly create the `SigninManager` as it usually doesn't exist
    // in tests.
    TestingProfile::TestingFactories factories = {
        {SigninManagerFactory::GetInstance(),
         base::BindRepeating(&BuildSigninManager)}};
    IdentityTestEnvironmentProfileAdaptor::
        AppendIdentityTestEnvironmentFactories(&factories);
    profile_ = profile_manager_.CreateTestingProfile("Default", factories);
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
  }

  Profile* profile() { return profile_; }

  FakeAccountManagerUI* fake_ui() { return fake_account_manager_ui_; }

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  FakeAccountManagerUI* fake_account_manager_ui_;
  std::unique_ptr<ScopedAshAccountManagerForTests> scoped_account_manager_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  Profile* profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_P(SigninUiDelegateImplLacrosTest, ShowSigninUI) {
  bool enable_sync = GetParam();
  SigninUiDelegateImplLacros signin_ui_delegate;
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  FakeAccountManagerUIDialogWaiter waiter(fake_ui());
  signin_ui_delegate.ShowSigninUI(profile(), enable_sync, kAccessPoint,
                                  kPromoAction);
  waiter.WaitForAddAccountDialogShown();
  EXPECT_TRUE(fake_ui()->IsDialogShown());
  EXPECT_EQ(1, fake_ui()->show_account_addition_dialog_calls());

  if (enable_sync) {
    ExpectOneSigninStartedHistograms(histogram_tester, kAccessPoint,
                                     kPromoAction);
  } else {
    ExpectNoSigninStartedHistograms(histogram_tester);
  }
  EXPECT_EQ(enable_sync ? 1 : 0, user_action_tester.GetActionCount(
                                     "Signin_Signin_FromAvatarBubbleSignin"));
  // TODO(https://crbug.com/1316608): test that the sync is shown after an
  // account is added.
}

TEST_P(SigninUiDelegateImplLacrosTest, ShowReauthUI) {
  bool enable_sync = GetParam();
  SigninUiDelegateImplLacros signin_ui_delegate;
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  FakeAccountManagerUIDialogWaiter waiter(fake_ui());
  signin_ui_delegate.ShowReauthUI(profile(), "test@test.com", enable_sync,
                                  kAccessPoint, kPromoAction);
  waiter.WaitForReauthAccountDialogShown();
  EXPECT_TRUE(fake_ui()->IsDialogShown());
  EXPECT_EQ(1, fake_ui()->show_account_reauthentication_dialog_calls());

  if (enable_sync) {
    ExpectOneSigninStartedHistograms(histogram_tester, kAccessPoint,
                                     kPromoAction);
  } else {
    ExpectNoSigninStartedHistograms(histogram_tester);
  }
  EXPECT_EQ(enable_sync ? 1 : 0, user_action_tester.GetActionCount(
                                     "Signin_Signin_FromAvatarBubbleSignin"));
  // TODO(https://crbug.com/1316608): test that the sync is shown after an
  // account is added.
}

INSTANTIATE_TEST_SUITE_P(EnableSync,
                         SigninUiDelegateImplLacrosTest,
                         ::testing::Bool());

}  // namespace signin_ui_util
