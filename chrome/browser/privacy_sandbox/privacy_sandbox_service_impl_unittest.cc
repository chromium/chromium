// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kFirstPartySetsStateHistogram[] =
    "Settings.FirstPartySets.State";

// Remove any user preference settings for Related Website Set related
// preferences, returning them to their default value.
void ClearRwsUserPrefs(
    sync_preferences::TestingPrefServiceSyncable* pref_service) {
  pref_service->RemoveUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled);
  pref_service->RemoveUserPref(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized);
}

}  // namespace

class PrivacySandboxServiceTest : public testing::Test {
 public:
  PrivacySandboxServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override { CreateService(); }

  void TearDown() override {
    if (privacy_sandbox_service_) {
      privacy_sandbox_service_->Shutdown();
      privacy_sandbox_service_ = nullptr;
    }
  }

  void CreateService() {
    if (privacy_sandbox_service_) {
      privacy_sandbox_service_->Shutdown();
      privacy_sandbox_service_ = nullptr;
    }

    privacy_sandbox_service_ =
        PrivacySandboxServiceFactory::GetInstance()
            ->SetTestingSubclassFactoryAndUse(
                profile(),
                base::BindOnce(&PrivacySandboxServiceTest::BuildTestService,
                               base::Unretained(this)));
  }

  profile_metrics::BrowserProfileType GetProfileType() { return profile_type_; }

  void SetProfileType(profile_metrics::BrowserProfileType profile_type) {
    profile_type_ = profile_type;
  }

  TestingProfile* profile() { return &profile_; }
  PrivacySandboxServiceImpl* privacy_sandbox_service() {
    return privacy_sandbox_service_.get();
  }
  privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings() {
    return PrivacySandboxSettingsFactory::GetForProfile(profile());
  }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }
  content_settings::CookieSettings* cookie_settings() {
    return CookieSettingsFactory::GetForProfile(profile()).get();
  }

 private:
  std::unique_ptr<PrivacySandboxServiceImpl> BuildTestService(
      content::BrowserContext* context) {
    return std::make_unique<PrivacySandboxServiceImpl>(
        privacy_sandbox_settings(), cookie_settings(), profile()->GetPrefs(),
        GetProfileType());
  }

  content::BrowserTaskEnvironment browser_task_environment_;
  TestingProfile profile_;
  profile_metrics::BrowserProfileType profile_type_ =
      profile_metrics::BrowserProfileType::kRegular;

  raw_ptr<PrivacySandboxServiceImpl> privacy_sandbox_service_ = nullptr;
};

TEST_F(PrivacySandboxServiceTest,
       RelatedWebsiteSetsNotRelevantMetricAllowedCookies) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_ALLOW);
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxServiceImpl::FirstPartySetsState::kFpsNotRelevant, 1);
}

TEST_F(PrivacySandboxServiceTest,
       RelatedWebsiteSetsNotRelevantMetricBlockedCookies) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxServiceImpl::FirstPartySetsState::kFpsNotRelevant, 1);
}

TEST_F(PrivacySandboxServiceTest, RelatedWebsiteSetsEnabledMetric) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxServiceImpl::FirstPartySetsState::kFpsEnabled, 1);
}

TEST_F(PrivacySandboxServiceTest, RelatedWebsiteSetsDisabledMetric) {
  base::HistogramTester histogram_tester;
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(false));
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();

  histogram_tester.ExpectUniqueSample(
      kFirstPartySetsStateHistogram,
      PrivacySandboxServiceImpl::FirstPartySetsState::kFpsDisabled, 1);
}



TEST_F(PrivacySandboxServiceTest, RwsPrefInit) {
  // Check that the init of the RWS pref occurs correctly.
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));

  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized));

  // If the UI is available, the user blocks 3PC, and the pref has not been
  // previously init, it should be.
  CreateService();
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized));

  // Once the pref has been init, it should not be re-init, and updated user
  // cookie settings should not impact it.
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));

  CreateService();
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized));

  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  EXPECT_TRUE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized));

  // Blocking all cookies should also init the RWS pref to off.
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));

  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CreateService();
  EXPECT_FALSE(
      prefs()->GetBoolean(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized));
}


