// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/profile_token_web_signin_interceptor.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::WithArgs;

namespace {

class MockDelegate : public ProfileTokenWebSigninInterceptor::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(bool,
              IsSigninInterceptionSupported,
              (const content::WebContents&),
              (override));
  MOCK_METHOD(std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>,
              ShowSigninInterceptionBubble,
              (content::WebContents*,
               const WebSigninInterceptor::Delegate::BubbleParameters&,
               base::OnceCallback<void(SigninInterceptionResult)>),
              (override));
  MOCK_METHOD(std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>,
              ShowOidcInterceptionDialog,
              (content::WebContents*,
               const WebSigninInterceptor::Delegate::BubbleParameters&,
               signin::SigninChoiceWithConfirmAndRetryCallback,
               base::OnceClosure,
               base::RepeatingClosure),
              (override));
  MOCK_METHOD(void,
              ShowFirstRunExperienceInNewProfile,
              (Browser*,
               const CoreAccountId&,
               WebSigninInterceptor::SigninInterceptionType),
              (override));
  MOCK_METHOD(void,
              ShowSigninError,
              (content::WebContents*, const SigninUIError&),
              (override));
};

MATCHER_P(HasSameAccountIdAs, other, "") {
  return arg.account_id == other.account_id;
}

// Matches BubbleParameters fields excepting the color. This is useful in the
// test because the color is randomly generated.
testing::Matcher<const WebSigninInterceptor::Delegate::BubbleParameters&>
MatchBubbleParameters(
    const WebSigninInterceptor::Delegate::BubbleParameters& parameters) {
  return testing::AllOf(
      testing::Field(
          "interception_type",
          &WebSigninInterceptor::Delegate::BubbleParameters::interception_type,
          parameters.interception_type),
      testing::Field("intercepted_account",
                     &WebSigninInterceptor::Delegate::BubbleParameters::
                         intercepted_account,
                     HasSameAccountIdAs(parameters.intercepted_account)),
      testing::Field(
          "primary_account",
          &WebSigninInterceptor::Delegate::BubbleParameters::primary_account,
          HasSameAccountIdAs(parameters.primary_account)),
      testing::Field("show_link_data_option",
                     &WebSigninInterceptor::Delegate::BubbleParameters::
                         show_link_data_option,
                     parameters.show_link_data_option),
      testing::Field("show_managed_disclaimer",
                     &WebSigninInterceptor::Delegate::BubbleParameters::
                         show_managed_disclaimer,
                     parameters.show_managed_disclaimer));
}

class ProfileAddedWaiter : public ProfileManagerObserver {
 public:
  ProfileAddedWaiter() {
    g_browser_process->profile_manager()->AddObserver(this);
  }
  ~ProfileAddedWaiter() override {
    g_browser_process->profile_manager()->RemoveObserver(this);
  }
  void OnProfileAdded(Profile* profile) override { future_.SetValue(profile); }
  Profile* Wait() { return future_.Get(); }

 private:
  base::test::TestFuture<Profile*> future_;
};

}  // namespace

class ProfileTokenWebSigninInterceptorTest
    : public SigninBrowserTestBase,
      public ProfileAttributesStorage::Observer {
 public:
  ProfileTokenWebSigninInterceptorTest() = default;

  ~ProfileTokenWebSigninInterceptorTest() override = default;

  void SetUpOnMainThread() override {
    SigninBrowserTestBase::SetUpOnMainThread();
    g_browser_process->profile_manager()
        ->GetProfileAttributesStorage()
        .AddObserver(this);
    auto delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    interceptor_ = std::make_unique<ProfileTokenWebSigninInterceptor>(
        browser()->profile(), std::move(delegate));
    interceptor_->SetDisableBrowserCreationAfterInterceptionForTesting(true);
  }

  void TearDownOnMainThread() override {
    g_browser_process->profile_manager()
        ->GetProfileAttributesStorage()
        .RemoveObserver(this);
    SigninBrowserTestBase::TearDownOnMainThread();
  }

  void OnProfileAdded(const base::FilePath& profile_path) override {
    auto* entry = g_browser_process->profile_manager()
                      ->GetProfileAttributesStorage()
                      .GetProfileAttributesWithPath(profile_path);
    if (entry) {
      entry->SetDasherlessManagement(true);
    }
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  std::unique_ptr<ProfileTokenWebSigninInterceptor> interceptor_;
  raw_ptr<MockDelegate> delegate_ = nullptr;  // Owned by `interceptor_`
};

IN_PROC_BROWSER_TEST_F(ProfileTokenWebSigninInterceptorTest,
                       NoInterceptionWithInvalidToken) {
  EXPECT_CALL(*delegate_, ShowSigninInterceptionBubble(_, _, _)).Times(0);
  interceptor_->MaybeInterceptSigninProfile(web_contents(), "id",
                                            /*enrollment_token=*/std::string());
}

IN_PROC_BROWSER_TEST_F(ProfileTokenWebSigninInterceptorTest,
                       NoInterceptionWithNoWebContents) {
  EXPECT_CALL(*delegate_, ShowSigninInterceptionBubble(_, _, _)).Times(0);
  interceptor_->MaybeInterceptSigninProfile(nullptr, "id", "token");
}

IN_PROC_BROWSER_TEST_F(ProfileTokenWebSigninInterceptorTest,
                       NoInterceptionWithSameProfile) {
  EXPECT_CALL(*delegate_, ShowSigninInterceptionBubble(_, _, _)).Times(0);

  auto* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(browser()->profile()->GetPath());
  entry->SetProfileManagementId("id");
  entry->SetProfileManagementEnrollmentToken("token");
  entry->SetDasherlessManagement(true);

  interceptor_->MaybeInterceptSigninProfile(web_contents(), "id", "token");
}

IN_PROC_BROWSER_TEST_F(ProfileTokenWebSigninInterceptorTest,
                       InterceptionCreatesNoProfileIfDeclined) {
  const int num_profiles_before =
      g_browser_process->profile_manager()->GetNumberOfProfiles();

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, AccountInfo(),
      AccountInfo(), SkColor(), /*show_link_data_option=*/false,
      /*show_managed_disclaimer=*/true);

  EXPECT_CALL(*delegate_, ShowSigninInterceptionBubble(
                              _, MatchBubbleParameters(expected_parameters), _))
      .Times(1)
      .WillOnce(
          [](content::WebContents*,
             const WebSigninInterceptor::Delegate::BubbleParameters&,
             base::OnceCallback<void(SigninInterceptionResult)> callback) {
            std::move(callback).Run(SigninInterceptionResult::kDeclined);
            return nullptr;
          });
  interceptor_->MaybeInterceptSigninProfile(web_contents(), "id", "token");

  base::RunLoop().RunUntilIdle();

  const int num_profiles_after =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  EXPECT_EQ(num_profiles_before, num_profiles_after);
}

// TODO(https://crbug.com/385383226): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_InterceptionCreatesNewProfileIfAccepted \
  DISABLED_InterceptionCreatesNewProfileIfAccepted
#else
#define MAYBE_InterceptionCreatesNewProfileIfAccepted \
  InterceptionCreatesNewProfileIfAccepted
#endif
IN_PROC_BROWSER_TEST_F(ProfileTokenWebSigninInterceptorTest,
                       MAYBE_InterceptionCreatesNewProfileIfAccepted) {
  const int num_profiles_before =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, AccountInfo(),
      AccountInfo(), SkColor(), /*show_link_data_option=*/false,
      /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*delegate_, ShowSigninInterceptionBubble(
                              _, MatchBubbleParameters(expected_parameters), _))
      .Times(1)
      .WillOnce(
          [](content::WebContents*,
             const WebSigninInterceptor::Delegate::BubbleParameters&,
             base::OnceCallback<void(SigninInterceptionResult)> callback) {
            std::move(callback).Run(SigninInterceptionResult::kAccepted);
            return nullptr;
          });

  ProfileAddedWaiter waiter;
  interceptor_->MaybeInterceptSigninProfile(web_contents(), "id", "token");
  waiter.Wait();

  const int num_profiles_after =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  EXPECT_EQ(num_profiles_before + 1, num_profiles_after);
}

// TODO(https://crbug.com/385383226): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_InterceptionCreatesEphemeralProfileIfAcceptedWithNoId \
  DISABLED_InterceptionCreatesEphemeralProfileIfAcceptedWithNoId
#else
#define MAYBE_InterceptionCreatesEphemeralProfileIfAcceptedWithNoId \
  InterceptionCreatesEphemeralProfileIfAcceptedWithNoId
#endif
IN_PROC_BROWSER_TEST_F(
    ProfileTokenWebSigninInterceptorTest,
    MAYBE_InterceptionCreatesEphemeralProfileIfAcceptedWithNoId) {
  const int num_profiles_before =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kEnterprise, AccountInfo(),
      AccountInfo(), SkColor(), /*show_link_data_option=*/false,
      /*show_managed_disclaimer=*/true);
  EXPECT_CALL(*delegate_, ShowSigninInterceptionBubble(
                              _, MatchBubbleParameters(expected_parameters), _))
      .Times(1)
      .WillOnce(
          [](content::WebContents*,
             const WebSigninInterceptor::Delegate::BubbleParameters&,
             base::OnceCallback<void(SigninInterceptionResult)> callback) {
            std::move(callback).Run(SigninInterceptionResult::kAccepted);
            return nullptr;
          });

  ProfileAddedWaiter waiter;
  interceptor_->MaybeInterceptSigninProfile(web_contents(), std::string(),
                                            "token");
  waiter.Wait();

  const int num_profiles_after =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  EXPECT_EQ(num_profiles_before + 1, num_profiles_after);
}

IN_PROC_BROWSER_TEST_F(ProfileTokenWebSigninInterceptorTest,
                       InterceptionSwitchesToExistingProfileIfAccepted) {
  auto* profile_manager = g_browser_process->profile_manager();
  Profile& new_profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());

  auto* entry = profile_manager->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(new_profile.GetPath());
  entry->SetProfileManagementId("id");
  entry->SetProfileManagementEnrollmentToken("token");
  entry->SetDasherlessManagement(true);

  WebSigninInterceptor::Delegate::BubbleParameters expected_parameters(
      WebSigninInterceptor::SigninInterceptionType::kProfileSwitch,
      AccountInfo(), AccountInfo(), SkColor(), /*show_link_data_option=*/false,
      /*show_managed_disclaimer=*/true);

  EXPECT_CALL(*delegate_, ShowSigninInterceptionBubble(
                              _, MatchBubbleParameters(expected_parameters), _))
      .Times(1)
      .WillOnce(
          [](content::WebContents*,
             const WebSigninInterceptor::Delegate::BubbleParameters&,
             base::OnceCallback<void(SigninInterceptionResult)> callback) {
            std::move(callback).Run(SigninInterceptionResult::kAccepted);
            return nullptr;
          });

  const int num_profiles_before =
      g_browser_process->profile_manager()->GetNumberOfProfiles();

  interceptor_->MaybeInterceptSigninProfile(web_contents(), "id", "token");

  base::RunLoop().RunUntilIdle();

  const int num_profiles_after =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  EXPECT_EQ(num_profiles_before, num_profiles_after);
}
