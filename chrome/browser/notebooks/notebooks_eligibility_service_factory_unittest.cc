// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notebooks/notebooks_eligibility_service_factory.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/notebooks/public/features.h"
#include "components/notebooks/public/notebooks_eligibility_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notebooks {
namespace {

class NotebooksEligibilityServiceFactoryTest : public testing::Test {
 protected:
  NotebooksEligibilityServiceFactoryTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~NotebooksEligibilityServiceFactoryTest() override = default;

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

TEST_F(NotebooksEligibilityServiceFactoryTest, FeatureEnabledCreatesService) {
  InitFeature(/*enable_feature=*/true);
  TestingProfile* profile = profile_manager()->CreateTestingProfile("profile");
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(profile), "test@gmail.com",
      signin::ConsentLevel::kSignin);
  NotebooksEligibilityService* service =
      NotebooksEligibilityServiceFactory::GetForProfile(profile);
  ASSERT_NE(service, nullptr);
  EXPECT_TRUE(service->IsEligible());
}

TEST_F(NotebooksEligibilityServiceFactoryTest,
       FeatureDisabledReturnsIneligibleService) {
  InitFeature(/*enable_feature=*/false);
  TestingProfile* profile = profile_manager()->CreateTestingProfile("profile");
  NotebooksEligibilityService* service =
      NotebooksEligibilityServiceFactory::GetForProfile(profile);
  ASSERT_NE(service, nullptr);
  EXPECT_FALSE(service->IsEligible());
}

TEST_F(NotebooksEligibilityServiceFactoryTest,
       FeatureEnabledReturnsIneligibleServiceInIncognito) {
  InitFeature(/*enable_feature=*/true);
  TestingProfile* profile = profile_manager()->CreateTestingProfile("profile");
  Profile* incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  NotebooksEligibilityService* service =
      NotebooksEligibilityServiceFactory::GetForProfile(incognito_profile);
  ASSERT_NE(service, nullptr);
  EXPECT_FALSE(service->IsEligible());
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(NotebooksEligibilityServiceFactoryTest, ReturnsNullForSystemProfile) {
  InitFeature(/*enable_feature=*/true);
  TestingProfile* profile = profile_manager()->CreateSystemProfile();
  NotebooksEligibilityService* service =
      NotebooksEligibilityServiceFactory::GetForProfile(profile);
  EXPECT_EQ(service, nullptr);
}
#endif

TEST_F(NotebooksEligibilityServiceFactoryTest, ReturnsNullForGuestProfile) {
  InitFeature(/*enable_feature=*/true);
  TestingProfile* profile = profile_manager()->CreateGuestProfile();
  NotebooksEligibilityService* service =
      NotebooksEligibilityServiceFactory::GetForProfile(profile);
  EXPECT_EQ(service, nullptr);
}

}  // namespace
}  // namespace notebooks
