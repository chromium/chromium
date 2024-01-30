// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kcer/kcer_factory.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/memory/weak_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/saml/lockscreen_reauth_dialog_test_helper.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/components/kcer/extra_instances.h"
#include "chromeos/components/kcer/kcer.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// These browser tests test KcerFactory and KcerFactoryAsh. The factory is
// created outside of the tests by the code that also creates it in production.

namespace kcer {
namespace {

constexpr char kUserEmail[] = "user@example.com";

bool WeakPtrEq(const base::WeakPtr<kcer::Kcer>& v1,
               const base::WeakPtr<kcer::Kcer>& v2) {
  if (bool(v1) != bool(v2)) {
    return false;
  }
  return (v1.get() == v2.get());
}

class KcerFactoryNoNssTestBase : public InProcessBrowserTest {
 protected:
  KcerFactoryNoNssTestBase() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kKcerWithoutNss}, /*disabled_features=*/{});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test ExtraInstances::GetEmptyKcer() returns an instance of Kcer that
// doesn't have any tokens.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTestBase,
                       EmptySpecialInstanceDoesNotHaveTokens) {
  base::WeakPtr<Kcer> kcer = ExtraInstances::GetEmptyKcer();

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({}));
}

// Test that device Kcer has correct tokens.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTestBase, DeviceKcerHasCorrectTokens) {
  base::WeakPtr<Kcer> kcer = ExtraInstances::GetDeviceKcer();

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());

  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({Token::kDevice}));
  // The factory is responsible for initializing HighLevelChapsClient.
  EXPECT_TRUE(KcerFactory::IsHighLevelChapsClientInitialized());
}

// Test that Kcer for the sign in profile has correct tokens.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTestBase,
                       SignInProfileGetsCorrectTokens) {
  base::WeakPtr<Kcer> expected_kcer;
  if (ash::switches::IsSigninFrameClientCertsEnabled()) {
    expected_kcer = ExtraInstances::GetDeviceKcer();
  } else {
    expected_kcer = ExtraInstances::GetEmptyKcer();
  }

  base::WeakPtr<Kcer> signin_kcer =
      KcerFactory::GetKcer(ash::ProfileHelper::GetSigninProfile());
  EXPECT_TRUE(WeakPtrEq(signin_kcer, expected_kcer));
}

IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTestBase,
                       LockScreenProfileGetsCorrectTokens) {
  ash::ScreenLockerTester locker;
  locker.Lock();
  // Showing the reauth dialog will create the lock screen profile.
  ash::LockScreenReauthDialogTestHelper::ShowDialogAndWait();

  base::WeakPtr<Kcer> expected_kcer;
  if (ash::switches::IsSigninFrameClientCertsEnabled()) {
    expected_kcer = ExtraInstances::GetDeviceKcer();
  } else {
    expected_kcer = ExtraInstances::GetEmptyKcer();
  }

  base::WeakPtr<Kcer> lockscreen_kcer =
      KcerFactory::GetKcer(ash::ProfileHelper::GetLockScreenProfile());
  EXPECT_TRUE(WeakPtrEq(lockscreen_kcer, expected_kcer));
}

class KcerFactoryNoNssTest : public KcerFactoryNoNssTestBase {
 protected:
  void SetUpOnMainThread() override {
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
  }

  void TearDownOnMainThread() override {
    user_manager_ = nullptr;
    scoped_user_manager_.reset();
  }

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  raw_ptr<ash::FakeChromeUserManager> user_manager_ = nullptr;
};

// Test that ExtraInstances::GetDefaultKcer() returns the instance for the
// primary profile.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest, DefaultKcerIsPrimaryProfileKcer) {
  // Associating a Profile with a primary User makes it a primary Profile.
  std::unique_ptr<TestingProfile> primary_profile =
      TestingProfile::Builder().Build();
  const user_manager::User* user =
      user_manager_->AddUserWithAffiliationAndTypeAndProfile(
          AccountId::FromUserEmail(kUserEmail), /*is_affiliated=*/true,
          user_manager::UserType::kRegular, primary_profile.get());
  user_manager_->LoginUser(user->GetAccountId());

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(primary_profile.get());

  base::WeakPtr<Kcer> default_kcer = ExtraInstances::GetDefaultKcer();

  ASSERT_TRUE(kcer);
  ASSERT_TRUE(default_kcer);
  EXPECT_EQ(kcer.get(), default_kcer.get());
}

// Test that KcerFactory can create an instance with both tokens.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest, KcerWithBothTokensCreated) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();

  // Affiliated users should get both tokens.
  const user_manager::User* user =
      user_manager_->AddUserWithAffiliationAndTypeAndProfile(
          AccountId::FromUserEmail(kUserEmail), /*is_affiliated=*/true,
          user_manager::UserType::kRegular, testing_profile.get());
  user_manager_->LoginUser(user->GetAccountId());

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(testing_profile.get());
  ASSERT_TRUE(kcer);

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(),
            base::flat_set<Token>({Token::kUser, Token::kDevice}));
  // The factory is responsible for initializing HighLevelChapsClient.
  EXPECT_TRUE(KcerFactory::IsHighLevelChapsClientInitialized());
}

// Test that KcerFactory can create an instance with one token.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest, KcerWithOneTokensCreated) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();

  // Unaffiliated users should get one the user token.
  const user_manager::User* user =
      user_manager_->AddUserWithAffiliationAndTypeAndProfile(
          AccountId::FromUserEmail(kUserEmail), /*is_affiliated=*/false,
          user_manager::UserType::kRegular, testing_profile.get());
  user_manager_->LoginUser(user->GetAccountId());

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(testing_profile.get());
  ASSERT_TRUE(kcer);

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({Token::kUser}));
  // The factory is responsible for initializing HighLevelChapsClient.
  EXPECT_TRUE(KcerFactory::IsHighLevelChapsClientInitialized());
}

// Test that profiles without users don't get any tokens in Ash.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest, ProfileWithoutUser) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(testing_profile.get());
  ASSERT_TRUE(kcer);

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>());
}

// Test that KcerFactory redirects off-the-record profile to their regular
// profiles.
IN_PROC_BROWSER_TEST_F(KcerFactoryNoNssTest, OffTheRecordProfileIsRedirected) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();
  Profile* off_the_record_profile = testing_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  const user_manager::User* user =
      user_manager_->AddUserWithAffiliationAndTypeAndProfile(
          AccountId::FromUserEmail(kUserEmail), /*is_affiliated=*/true,
          user_manager::UserType::kRegular, testing_profile.get());
  user_manager_->LoginUser(user->GetAccountId());

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(testing_profile.get());
  base::WeakPtr<Kcer> off_the_record_kcer =
      KcerFactory::GetKcer(off_the_record_profile);

  EXPECT_TRUE(WeakPtrEq(kcer, off_the_record_kcer));
}

}  // namespace
}  // namespace kcer
