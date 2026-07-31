// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notebooks/notebooks_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/notebooks/public/features.h"
#include "components/notebooks/public/notebooks_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notebooks {
namespace {

class NotebooksServiceFactoryTest : public testing::Test {
 protected:
  NotebooksServiceFactoryTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~NotebooksServiceFactoryTest() override = default;

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  void InitFeature(bool enable_feature) {
    if (enable_feature) {
      scoped_feature_list_.InitAndEnableFeature(features::kNotebooks);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kNotebooks);
    }
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(NotebooksServiceFactoryTest, FeatureEnabledUsesRealService) {
  InitFeature(/*enable_feature=*/true);
  TestingProfile* profile = profile_manager()->CreateTestingProfile("profile");
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile), "test@gmail.com",
      signin::ConsentLevel::kSignin);
  NotebooksService* service = NotebooksServiceFactory::GetForProfile(profile);
  ASSERT_NE(service, nullptr);
  EXPECT_FALSE(service->IsEmptyForTesting());
}

TEST_F(NotebooksServiceFactoryTest, FeatureDisabledUsesEmptyService) {
  InitFeature(/*enable_feature=*/false);
  TestingProfile* profile = profile_manager()->CreateTestingProfile("profile");
  NotebooksService* service = NotebooksServiceFactory::GetForProfile(profile);
  ASSERT_NE(service, nullptr);
  EXPECT_TRUE(service->IsEmptyForTesting());
}

TEST_F(NotebooksServiceFactoryTest, FeatureEnabledUsesEmptyServiceInIncognito) {
  InitFeature(/*enable_feature=*/true);
  TestingProfile* profile = profile_manager()->CreateTestingProfile("profile");
  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  NotebooksService* service =
      NotebooksServiceFactory::GetForProfile(incognito_profile);
  ASSERT_NE(service, nullptr);
  EXPECT_TRUE(service->IsEmptyForTesting());
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(NotebooksServiceFactoryTest, ReturnsNullForSystemProfile) {
  InitFeature(/*enable_feature=*/true);
  TestingProfile* profile = profile_manager()->CreateSystemProfile();
  NotebooksService* service = NotebooksServiceFactory::GetForProfile(profile);
  EXPECT_EQ(service, nullptr);
}
#endif

TEST_F(NotebooksServiceFactoryTest, ReturnsNullForGuestProfile) {
  InitFeature(/*enable_feature=*/true);
  TestingProfile* profile = profile_manager()->CreateGuestProfile();
  NotebooksService* service = NotebooksServiceFactory::GetForProfile(profile);
  EXPECT_EQ(service, nullptr);
}

}  // namespace
}  // namespace notebooks
