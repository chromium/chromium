// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/gemini_antiscam_protection/gemini_antiscam_protection_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildTestOptimizationGuideKeyedService(
    content::BrowserContext* browser_context) {
  return std::make_unique<
      testing::NiceMock<MockOptimizationGuideKeyedService>>();
}

std::unique_ptr<KeyedService> BuildTestHistoryService(
    content::BrowserContext* browser_context) {
  return history::CreateHistoryService(
      Profile::FromBrowserContext(browser_context)->GetPath(), true);
}

}  // namespace

namespace safe_browsing {

class GeminiAntiscamProtectionServiceFactoryTest : public testing::Test {
 protected:
  GeminiAntiscamProtectionServiceFactoryTest() = default;
  ~GeminiAntiscamProtectionServiceFactoryTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        kGeminiAntiscamProtectionForMetricsCollection);
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    ASSERT_TRUE(g_browser_process->profile_manager());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GeminiAntiscamProtectionServiceFactoryTest, DisabledForIncognitoMode) {
  TestingProfile* profile = TestingProfile::Builder().BuildIncognito(
      profile_manager_->CreateTestingProfile("testing_profile"));
  EXPECT_EQ(nullptr,
            GeminiAntiscamProtectionServiceFactory::GetForProfile(profile));
}

TEST_F(GeminiAntiscamProtectionServiceFactoryTest, DisabledForGuestMode) {
  Profile* profile =
      profile_manager_->CreateGuestProfile()->GetPrimaryOTRProfile(
          /*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            GeminiAntiscamProtectionServiceFactory::GetForProfile(profile));
}

TEST_F(GeminiAntiscamProtectionServiceFactoryTest,
       DisabledForStandardSafeBrowsing) {
  TestingProfile* profile = profile_manager_->CreateTestingProfile(
      "profile",
      {
          TestingProfile::TestingFactory{
              OptimizationGuideKeyedServiceFactory::GetInstance(),
              base::BindRepeating(&BuildTestOptimizationGuideKeyedService)},
      });
  profile->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  EXPECT_EQ(nullptr,
            GeminiAntiscamProtectionServiceFactory::GetForProfile(profile));
}

TEST_F(GeminiAntiscamProtectionServiceFactoryTest,
       DisabledForEnterpriseSafeBrowsing) {
  TestingProfile* profile = profile_manager_->CreateTestingProfile(
      "profile",
      {TestingProfile::TestingFactory{
           OptimizationGuideKeyedServiceFactory::GetInstance(),
           base::BindRepeating(&BuildTestOptimizationGuideKeyedService)},
       TestingProfile::TestingFactory{
           HistoryServiceFactory::GetInstance(),
           base::BindRepeating(&BuildTestHistoryService)}});
  profile->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(true));
  EXPECT_EQ(nullptr,
            GeminiAntiscamProtectionServiceFactory::GetForProfile(profile));
}

TEST_F(GeminiAntiscamProtectionServiceFactoryTest,
       EnabledForEnhancedSafeBrowsing) {
  TestingProfile* profile = profile_manager_->CreateTestingProfile(
      "profile",
      {TestingProfile::TestingFactory{
           OptimizationGuideKeyedServiceFactory::GetInstance(),
           base::BindRepeating(&BuildTestOptimizationGuideKeyedService)},
       TestingProfile::TestingFactory{
           HistoryServiceFactory::GetInstance(),
           base::BindRepeating(&BuildTestHistoryService)}});
  profile->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_NE(nullptr,
            GeminiAntiscamProtectionServiceFactory::GetForProfile(profile));
}

}  // namespace safe_browsing
