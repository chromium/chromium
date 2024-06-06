// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/product_specifications/product_specifications_service_factory.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProductSpecificationsServiceFactoryTest : public testing::Test {
 public:
  ProductSpecificationsServiceFactoryTest() = default;
  ProductSpecificationsServiceFactoryTest(
      const ProductSpecificationsServiceFactoryTest&) = delete;
  ProductSpecificationsServiceFactoryTest& operator=(
      const ProductSpecificationsServiceFactoryTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(different_profile_dir_.CreateUniqueTempDir());

    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(profile_dir_.GetPath());
    profile_ = profile_builder.Build();

    TestingProfile::Builder different_profile_builder;
    different_profile_builder.SetPath(different_profile_dir_.GetPath());
    different_profile_ = different_profile_builder.Build();
  }

  void enable_product_spec_flag() {
    scoped_feature_list_.InitAndEnableFeature(commerce::kProductSpecifications);
  }

  void disable_product_spec_flag() {
    scoped_feature_list_.InitAndDisableFeature(
        commerce::kProductSpecifications);
  }

  Profile* profile() { return profile_.get(); }
  Profile* different_profile() { return different_profile_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir profile_dir_;
  base::ScopedTempDir different_profile_dir_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfile> different_profile_;
};

TEST_F(ProductSpecificationsServiceFactoryTest, TestIncognitoProfile) {
  EXPECT_EQ(nullptr,
            commerce::ProductSpecificationsServiceFactory::GetInstance()
                ->GetForBrowserContext(profile()->GetPrimaryOTRProfile(
                    /*create_if_needed=*/true)));
}

TEST_F(ProductSpecificationsServiceFactoryTest,
       TestRegularProfileProductSpecSyncFlagOn) {
  enable_product_spec_flag();
  EXPECT_NE(nullptr,
            commerce::ProductSpecificationsServiceFactory::GetInstance()
                ->GetForBrowserContext(profile()));
}

TEST_F(ProductSpecificationsServiceFactoryTest,
       TestRegularProfileProductSpecSyncFlagOff) {
  disable_product_spec_flag();
  EXPECT_EQ(nullptr,
            commerce::ProductSpecificationsServiceFactory::GetInstance()
                ->GetForBrowserContext(profile()));
}

TEST_F(ProductSpecificationsServiceFactoryTest, TestSameProfile) {
  enable_product_spec_flag();
  EXPECT_EQ(commerce::ProductSpecificationsServiceFactory::GetInstance()
                ->GetForBrowserContext(profile()),
            commerce::ProductSpecificationsServiceFactory::GetInstance()
                ->GetForBrowserContext(profile()));
}

TEST_F(ProductSpecificationsServiceFactoryTest, TestDifferentProfile) {
  enable_product_spec_flag();
  EXPECT_NE(commerce::ProductSpecificationsServiceFactory::GetInstance()
                ->GetForBrowserContext(different_profile()),
            commerce::ProductSpecificationsServiceFactory::GetInstance()
                ->GetForBrowserContext(profile()));
}
