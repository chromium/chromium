// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kcer/kcer_factory.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/net/fake_nss_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/kcer/extra_instances.h"
#include "chromeos/components/kcer/kcer.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "crypto/scoped_test_system_nss_key_slot.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"
// #include "testing/gmock/include/gmock/gmock.h"
#endif

// These browser tests test KcerFactory and its specializations -
// KcerFactoryAsh, KcerFactoryLacros (on Ash and Lacros respectively). The
// factory is created outside of the tests by the code that also creates it in
// production.

namespace kcer {
namespace {

bool WeakPtrEq(const base::WeakPtr<kcer::Kcer>& v1,
               const base::WeakPtr<kcer::Kcer>& v2) {
  if (bool(v1) != bool(v2)) {
    return false;
  }
  return (v1.get() == v2.get());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class KcerFactoryNssTest : public InProcessBrowserTest {
 protected:
  KcerFactoryNssTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{}, /*disabled_features=*/{kKcerWithoutNss});

    test_system_slot_ = std::make_unique<crypto::ScopedTestSystemNSSKeySlot>(
        /*simulate_token_loader=*/false);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;
};

#elif BUILDFLAG(IS_CHROMEOS_LACROS)

class KcerFactoryNssTest : public InProcessBrowserTest {
 protected:
  KcerFactoryNssTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{}, /*disabled_features=*/{kKcerWithoutNss});

    // It's difficult to inject FakeNssService in time when CertDbInitializer is
    // created together with its profile (as it's happening in production), so
    // for these tests it's created lazily instead.
    CertDbInitializerFactory::GetInstance()->SetCreateOnDemandForTesting(true);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// Test that ExtraInstances::GetDefaultKcer() returns the instance for the
// primary profile.
IN_PROC_BROWSER_TEST_F(KcerFactoryNssTest, DefaultKcerIsPrimaryProfileKcer) {
  Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(primary_profile);

  base::WeakPtr<Kcer> default_kcer = ExtraInstances::GetDefaultKcer();

  ASSERT_TRUE(kcer);
  ASSERT_TRUE(default_kcer);
  EXPECT_EQ(kcer.get(), default_kcer.get());
}

// Test that KcerFactory can create an instance with both tokens.
IN_PROC_BROWSER_TEST_F(KcerFactoryNssTest, KcerWithBothTokensCreated) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();

  FakeNssService::InitializeForBrowserContext(testing_profile.get(),
                                              /*enable_system_slot=*/true);

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(testing_profile.get());
  ASSERT_TRUE(kcer);

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(),
            base::flat_set<Token>({Token::kUser, Token::kDevice}));
}

// Test that KcerFactory can create an instance with one token.
IN_PROC_BROWSER_TEST_F(KcerFactoryNssTest, KcerWithOneTokensCreated) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();

  FakeNssService::InitializeForBrowserContext(testing_profile.get(),
                                              /*enable_system_slot=*/false);

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(testing_profile.get());
  ASSERT_TRUE(kcer);

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({Token::kUser}));
}

// Test that KcerFactory redirects off-the-record profile to their regular
// profiles.
IN_PROC_BROWSER_TEST_F(KcerFactoryNssTest, OffTheRecordProfileIsRedirected) {
  std::unique_ptr<TestingProfile> testing_profile =
      TestingProfile::Builder().Build();
  Profile* off_the_record_profile = testing_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  FakeNssService::InitializeForBrowserContext(testing_profile.get(),
                                              /*enable_system_slot=*/false);

  base::WeakPtr<Kcer> kcer = KcerFactory::GetKcer(testing_profile.get());
  base::WeakPtr<Kcer> off_the_record_kcer =
      KcerFactory::GetKcer(off_the_record_profile);

  EXPECT_TRUE(WeakPtrEq(kcer, off_the_record_kcer));
}

// Test ExtraInstances::GetEmptyKcer() returns an instance of Kcer that
// doesn't have any tokens.
IN_PROC_BROWSER_TEST_F(KcerFactoryNssTest,
                       EmptySpecialInstanceDoesNotHaveTokens) {
  base::WeakPtr<Kcer> kcer = ExtraInstances::GetEmptyKcer();

  base::test::TestFuture<base::flat_set<Token>> tokens_waiter;
  kcer->GetAvailableTokens(tokens_waiter.GetCallback());
  EXPECT_EQ(tokens_waiter.Get(), base::flat_set<Token>({}));
}

// Test that device Kcer has correct tokens in Ash and Lacros.
IN_PROC_BROWSER_TEST_F(KcerFactoryNssTest, DeviceKcerHasCorrectTokens) {
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
