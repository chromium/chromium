// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service.h"

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_state.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_browsing_data_remover_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class DIPSServiceTest : public testing::Test {
 protected:
  void WaitOnStorage(DIPSService* service) {
    service->storage()->FlushPostedTasksForTesting();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DIPSServiceTest, CreateServiceIfFeatureEnabled) {
  ScopedInitDIPSFeature init_dips(true);

  TestingProfile profile;
  EXPECT_NE(DIPSService::Get(&profile), nullptr);
}

TEST_F(DIPSServiceTest, DontCreateServiceIfFeatureDisabled) {
  ScopedInitDIPSFeature init_dips(false);

  TestingProfile profile;
  EXPECT_EQ(DIPSService::Get(&profile), nullptr);
}

// Verifies that if database persistence is disabled via Finch, then when the
// DIPS Service is constructed, it deletes any DIPS Database files for the
// associated BrowserContext.
TEST_F(DIPSServiceTest, DeleteDbFilesIfPersistenceDisabled) {
  base::FilePath data_path = base::CreateUniqueTempDirectoryScopedToTest();
  DIPSService* service;
  std::unique_ptr<TestingProfile> profile;

  // Ensure the DIPS feature is enabled and the database is set to be persisted.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      dips::kFeature, {{"persist_database", "true"}});

  profile = TestingProfile::Builder().SetPath(data_path).Build();
  service = DIPSService::Get(profile.get());
  ASSERT_NE(service, nullptr);

  // Ensure the database files have been created and are NOT deleted since the
  // DIPS feature is enabled.
  WaitOnStorage(service);
  service->WaitForFileDeletionCompleteForTesting();
  ASSERT_TRUE(base::PathExists(GetDIPSFilePath(profile.get())));

  // Reset the feature list to set database persistence to false.
  feature_list.Reset();
  feature_list.InitAndEnableFeatureWithParameters(
      dips::kFeature, {{"persist_database", "false"}});

  // Reset the TestingProfile, then create a new instance with the same user
  // data path.
  profile.reset();
  profile = TestingProfile::Builder().SetPath(data_path).Build();

  service = DIPSService::Get(profile.get());
  ASSERT_NE(service, nullptr);

  // Ensure the database files ARE deleted since the DIPS feature is disabled.
  WaitOnStorage(service);
  service->WaitForFileDeletionCompleteForTesting();
  EXPECT_FALSE(base::PathExists(GetDIPSFilePath(profile.get())));
}

class DIPSServiceStateRemovalTest : public testing::Test {
 public:
  DIPSServiceStateRemovalTest()
      : profile_(std::make_unique<TestingProfile>()),
        cookie_settings_(
            CookieSettingsFactory::GetForProfile(GetProfile()).get()),
        service_(DIPSService::Get(profile_.get())) {}

  base::TimeDelta grace_period;
  base::TimeDelta interaction_ttl;
  base::TimeDelta tiny_delta = base::Milliseconds(1);

  void SetBlockThirdPartyCookies(bool value) {
    GetProfile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            value ? content_settings::CookieControlsMode::kBlockThirdParty
                  : content_settings::CookieControlsMode::kOff));
  }

  DIPSService* GetService() { return service_; }
  Profile* GetProfile() { return profile_.get(); }
  content_settings::CookieSettings* GetCookieSettings() {
    return cookie_settings_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::MockBrowsingDataRemoverDelegate delegate_;

  // Test setup.
  void SetUp() override {
    grace_period = dips::kGracePeriod.Get();
    interaction_ttl = dips::kInteractionTtl.Get();
    ASSERT_LT(tiny_delta, grace_period);

    GetProfile()->GetBrowsingDataRemover()->SetEmbedderDelegate(&delegate_);
    SetBlockThirdPartyCookies(true);
    ASSERT_TRUE(GetCookieSettings()->ShouldBlockThirdPartyCookies());

    DCHECK(service_);
    service_->SetStorageClockForTesting(&clock_);
    WaitOnStorage();
  }

  void TearDown() override {
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void WaitOnStorage() { service_->storage()->FlushPostedTasksForTesting(); }

  void AdvanceTimeTo(base::Time now) {
    ASSERT_GE(now, clock_.Now());
    clock_.SetNow(now);
  }

  void AdvanceTimeBy(base::TimeDelta delta) { clock_.Advance(delta); }

  void FireDIPSTimer() {
    service_->OnTimerFiredForTesting();
    WaitOnStorage();
  }

  void StateForURL(const GURL& url, StateForURLCallback callback) {
    service_->storage()
        ->AsyncCall(&DIPSStorage::Read)
        .WithArgs(url)
        .Then(std::move(callback));
  }

  absl::optional<StateValue> GetDIPSState(const GURL& url) {
    absl::optional<StateValue> state;
    StateForURL(url, base::BindLambdaForTesting([&](DIPSState loaded_state) {
                  if (loaded_state.was_loaded()) {
                    state = loaded_state.ToStateValue();
                  }
                }));
    WaitOnStorage();

    return state;
  }

 private:
  base::SimpleTestClock clock_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<content_settings::CookieSettings> cookie_settings_ = nullptr;
  raw_ptr<DIPSService> service_ = nullptr;
};

// NOTE: The use of a MockBrowsingDataRemoverDelegate in this test fixture
// means that when DIPS deletion is enabled, the row for 'url' is not actually
// removed from the DIPS db since 'delegate_' doesn't actually carryout the
// removal task.
TEST_F(DIPSServiceStateRemovalTest, BrowsingDataDeletion_Enabled) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      dips::kFeature, {{"delete", "true"}, {"triggering_action", "bounce"}});

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromDoubleT(2);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordBounce)
      .WithArgs(url, bounce, false);
  WaitOnStorage();
  EXPECT_TRUE(GetDIPSState(url).has_value());

  // Set the current time to just after the bounce happened.
  AdvanceTimeTo(bounce + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify a removal task was not posted to the BrowsingDataRemover(Delegate).
  delegate_.VerifyAndClearExpectations();

  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddRegisterableDomain(GetSiteForDIPS(url));
  delegate_.ExpectCall(
      base::Time::Min(), base::Time::Max(),
      chrome_browsing_data_remover::FILTERABLE_DATA_TYPES |
          content::BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
          content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
      filter_builder.get());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify that a removal task was posted to the BrowsingDataRemover(Delegate)
  // for 'url'.
  delegate_.VerifyAndClearExpectations();
  // Because this test fixture uses a MockBrowsingDataRemoverDelegate the DIPS
  // entry should not actually be removed. However, in practice it would be.
  EXPECT_TRUE(GetDIPSState(url).has_value());

  EXPECT_THAT(ukm_recorder,
              EntryUrlsAre("DIPS.Deletion", {"http://example.com/"}));
}

TEST_F(DIPSServiceStateRemovalTest, BrowsingDataDeletion_Disabled) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      dips::kFeature, {{"delete", "false"}, {"triggering_action", "bounce"}});

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromDoubleT(2);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordBounce)
      .WithArgs(url, bounce, false);
  WaitOnStorage();
  EXPECT_TRUE(GetDIPSState(url).has_value());

  // Set the current time to just after the bounce happened.
  AdvanceTimeTo(bounce + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify the DIPS entry was not removed and a removal task was not posted to
  // the BrowsingDataRemover(Delegate).
  delegate_.VerifyAndClearExpectations();
  EXPECT_TRUE(GetDIPSState(url).has_value());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify that the site's DIPS entry WAS removed, but a removal task was NOT
  // posted to the BrowsingDataRemover(Delegate) since `dips::kDeletionEnabled`
  // is false.
  delegate_.VerifyAndClearExpectations();
  EXPECT_FALSE(GetDIPSState(url).has_value());

  EXPECT_THAT(ukm_recorder,
              EntryUrlsAre("DIPS.Deletion", {"http://example.com/"}));
}

TEST_F(DIPSServiceStateRemovalTest,
       BrowsingDataDeletion_Respects3PCExceptions) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      dips::kFeature, {{"delete", "true"}, {"triggering_action", "bounce"}});

  GURL excepted_3p_url("https://excepted-as-3p.com");
  GURL excepted_1p_url("https://excepted-as-1p.com");
  GURL non_excepted_url("https://not-excepted.com");

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());

  // Add exception to third-party cookie blocking rule for
  // 'excepted_3p_url' in third-part context.
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("[*.]" + excepted_3p_url.host()),
      ContentSettingsPattern::Wildcard(), ContentSettingsType::COOKIES,
      ContentSetting::CONTENT_SETTING_ALLOW);

  // Add exception to third-party cookie blocking rule for third-parties
  // embedded by 'excepted_1p_url'.
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]" + excepted_1p_url.host()),
      ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_ALLOW);

  // Verify settings.
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetCookieSettings()->GetCookieSetting(
                                       excepted_3p_url, GURL(),
                                       net::CookieSettingOverrides(), nullptr));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetCookieSettings()->GetCookieSetting(
                                       GURL(), excepted_3p_url,
                                       net::CookieSettingOverrides(), nullptr));

  EXPECT_EQ(CONTENT_SETTING_BLOCK, GetCookieSettings()->GetCookieSetting(
                                       excepted_1p_url, GURL(),
                                       net::CookieSettingOverrides(), nullptr));
  EXPECT_EQ(CONTENT_SETTING_ALLOW, GetCookieSettings()->GetCookieSetting(
                                       GURL(), excepted_1p_url,
                                       net::CookieSettingOverrides(), nullptr));

  // Record bounces for sites.
  base::Time bounce = base::Time::FromDoubleT(2);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordBounce)
      .WithArgs(excepted_3p_url, bounce, false);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordBounce)
      .WithArgs(excepted_1p_url, bounce, false);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordBounce)
      .WithArgs(non_excepted_url, bounce, false);
  WaitOnStorage();
  EXPECT_TRUE(GetDIPSState(excepted_3p_url).has_value());
  EXPECT_TRUE(GetDIPSState(excepted_1p_url).has_value());
  EXPECT_TRUE(GetDIPSState(non_excepted_url).has_value());

  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddRegisterableDomain(GetSiteForDIPS(non_excepted_url));
  delegate_.ExpectCall(
      base::Time::Min(), base::Time::Max(),
      chrome_browsing_data_remover::FILTERABLE_DATA_TYPES |
          content::BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
          content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
      filter_builder.get());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify that a removal task was posted to the BrowsingDataRemover(Delegate)
  // for 'non_excepted_url'.
  delegate_.VerifyAndClearExpectations();
  // Because this test fixture uses a MockBrowsingDataRemoverDelegate the DIPS
  // entry should not actually be removed. However, in practice it would be.
  EXPECT_TRUE(GetDIPSState(non_excepted_url).has_value());
  // The DIPS entries for 'excepted_3p_url' and 'excepted_1p_url' should be
  // removed, since only DIPS state is cleared for sites with a cookie exception
  // and the BrowsingDataRemover(Delegate) isn't relied on for that kind of
  // deletion.
  EXPECT_FALSE(GetDIPSState(excepted_3p_url).has_value());
  EXPECT_FALSE(GetDIPSState(excepted_1p_url).has_value());

  // All 3 sites should be reported to UKM. It doesn't matter whether the URL
  // was excepted or not.
  EXPECT_THAT(ukm_recorder,
              EntryUrlsAre("DIPS.Deletion", {"http://excepted-as-3p.com/",
                                             "http://excepted-as-1p.com/",
                                             "http://not-excepted.com/"}));
}

// A test class that verifies DIPSService state deletion metrics collection
// behavior.
class DIPSServiceHistogramTest : public DIPSServiceStateRemovalTest {
 public:
  DIPSServiceHistogramTest() = default;

  const base::HistogramTester& histograms() const { return histogram_tester_; }

 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(DIPSServiceHistogramTest, DeletionLatency) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      dips::kFeature, {{"delete", "false"}, {"triggering_action", "bounce"}});

  // Verify the histogram starts empty
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency", 0);

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromDoubleT(2);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordBounce)
      .WithArgs(url, bounce, false);
  WaitOnStorage();

  // Set the current time to just after the bounce happened.
  AdvanceTimeTo(bounce + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify deletion latency metrics were NOT emitted and the DIPS entry was NOT
  // removed.
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency", 0);
  EXPECT_TRUE(GetDIPSState(url).has_value());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion latency metric was emitted and the DIPS entry was
  // removed.
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency", 1);
  EXPECT_FALSE(GetDIPSState(url).has_value());
}
