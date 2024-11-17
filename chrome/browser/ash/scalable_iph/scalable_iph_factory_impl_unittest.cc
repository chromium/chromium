// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scalable_iph/scalable_iph_factory_impl.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/scalable_iph/mock_scalable_iph_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/scalable_iph/logger.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_factory.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

class ScalableIphFactoryImplTest : public ChromeAshTestBase {
 public:
  std::unique_ptr<scalable_iph::ScalableIphDelegate> DelegateFactory(
      Profile* profile,
      scalable_iph::Logger* logger) {
    return std::make_unique<ash::test::MockScalableIphDelegate>();
  }

 protected:
  TestingProfile testing_profile_;
};

TEST_F(ScalableIphFactoryImplTest, WaitForRefreshTokensLoad) {
  base::test::ScopedFeatureList scoped_feature_list(
      feature_engagement::kIPHScalableIphUnlockedBasedOneFeature);

  ScalableIphFactoryImpl::BuildInstance();
  ScalableIphFactoryImpl* scalable_iph_factory_impl =
      static_cast<ScalableIphFactoryImpl*>(ScalableIphFactory::GetInstance());
  scalable_iph_factory_impl->SetByPassEligiblityCheckForTesting();
  scalable_iph_factory_impl->SetDelegateFactoryForTesting(base::BindRepeating(
      &ScalableIphFactoryImplTest::DelegateFactory, base::Unretained(this)));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(&testing_profile_);
  ASSERT_TRUE(identity_manager);
  ASSERT_FALSE(identity_manager->AreRefreshTokensLoaded());
  EXPECT_FALSE(scalable_iph_factory_impl->GetRefreshTokenBarrierForTesting());

  scalable_iph_factory_impl->InitializeServiceForBrowserContext(
      &testing_profile_);
  EXPECT_FALSE(
      scalable_iph_factory_impl->GetForBrowserContext(&testing_profile_));
  EXPECT_TRUE(scalable_iph_factory_impl->GetRefreshTokenBarrierForTesting());

  scalable_iph_factory_impl->GetRefreshTokenBarrierForTesting()
      ->OnRefreshTokensLoaded();
  EXPECT_TRUE(
      scalable_iph_factory_impl->GetForBrowserContext(&testing_profile_));
  EXPECT_TRUE(scalable_iph_factory_impl->GetRefreshTokenBarrierForTesting());
}
