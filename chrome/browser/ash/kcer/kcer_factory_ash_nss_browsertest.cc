// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/extra_instances.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/kcer/kcer_factory_ash.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_test.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace kcer {
namespace {

bool WeakPtrEq(const base::WeakPtr<kcer::Kcer>& v1,
               const base::WeakPtr<kcer::Kcer>& v2) {
  if (bool(v1) != bool(v2)) {
    return false;
  }
  return (v1.get() == v2.get());
}

class KcerFactoryAshNssTest : public InProcessBrowserTest {
 protected:
  KcerFactoryAshNssTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{}, /*disabled_features=*/{kKcerWithoutNss});

    test_system_slot_ = std::make_unique<crypto::ScopedTestSystemNSSKeySlot>(
        /*simulate_token_loader=*/false);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;
};

// Test that ExtraInstances::GetDefaultKcer() returns the instance for the
// primary profile.
IN_PROC_BROWSER_TEST_F(KcerFactoryAshNssTest, DefaultKcerIsPrimaryProfileKcer) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();

  base::WeakPtr<Kcer> kcer = KcerFactoryAsh::GetKcer(primary_profile);

  base::WeakPtr<Kcer> default_kcer = ExtraInstances::GetDefaultKcer();

  ASSERT_TRUE(kcer);
  ASSERT_TRUE(default_kcer);
  EXPECT_EQ(kcer.get(), default_kcer.get());
}

// Test that KcerFactory can create an instance with both tokens.
IN_PROC_BROWSER_TEST_F(KcerFactoryAshNssTest, KcerWithBothTokensCreated) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();

  FakeNssService::InitializeForBrowserContext(testing_profile.get(),
                                              /*enable_system_slot=*/true);

  base::WeakPtr<Kcer> kcer = KcerFactoryAsh::GetKcer(testing_profile.get());
  ASSERT_TRUE(kcer);

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(),
            base::flat_set<Token>({Token::kUser, Token::kDevice}));
}

// Test that KcerFactory can create an instance with one token.
IN_PROC_BROWSER_TEST_F(KcerFactoryAshNssTest, KcerWithOneTokensCreated) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();

  FakeNssService::InitializeForBrowserContext(testing_profile.get(),
                                              /*enable_system_slot=*/false);

  base::WeakPtr<Kcer> kcer = KcerFactoryAsh::GetKcer(testing_profile.get());
  ASSERT_TRUE(kcer);

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({Token::kUser}));
}

// Test that KcerFactory redirects off-the-record profile to their regular
// profiles.
IN_PROC_BROWSER_TEST_F(KcerFactoryAshNssTest, OffTheRecordProfileIsRedirected) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();
  Profile* off_the_record_profile = testing_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  FakeNssService::InitializeForBrowserContext(testing_profile.get(),
                                              /*enable_system_slot=*/false);

  base::WeakPtr<Kcer> kcer = KcerFactoryAsh::GetKcer(testing_profile.get());
  base::WeakPtr<Kcer> off_the_record_kcer =
      KcerFactoryAsh::GetKcer(off_the_record_profile);

  EXPECT_TRUE(WeakPtrEq(kcer, off_the_record_kcer));
}

// Test ExtraInstances::GetEmptyKcer() returns an instance of Kcer that
// doesn't have any tokens.
IN_PROC_BROWSER_TEST_F(KcerFactoryAshNssTest,
                       EmptySpecialInstanceDoesNotHaveTokens) {
  base::WeakPtr<Kcer> kcer = ExtraInstances::GetEmptyKcer();

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({}));
}

// Test that device Kcer has correct tokens in Ash and Lacros.
IN_PROC_BROWSER_TEST_F(KcerFactoryAshNssTest, DeviceKcerHasCorrectTokens) {
  base::WeakPtr<Kcer> kcer = ExtraInstances::GetDeviceKcer();

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({Token::kDevice}));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({}));
#endif
}

}  // namespace
}  // namespace kcer
