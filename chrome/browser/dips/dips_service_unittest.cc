// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_service.h"

#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_file_util.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_redirect_info.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_state.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_browsing_data_remover_delegate.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"

class DIPSServiceTest : public testing::Test {
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
      features::kDIPS, {{"persist_database", "true"}});

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
      features::kDIPS, {{"persist_database", "false"}});

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

// Verifies that when an OTR profile is opened, the DIPS database file for
// the underlying regular profile is NOT deleted.
TEST_F(DIPSServiceTest, PreserveRegularProfileDbFiles) {
  base::FilePath data_path = base::CreateUniqueTempDirectoryScopedToTest();

  // Ensure the DIPS feature is enabled and the database is set to be persisted.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS, {{"persist_database", "true"}});

  // Build a regular profile.
  std::unique_ptr<TestingProfile> profile =
      TestingProfile::Builder().SetPath(data_path).Build();
  DIPSService* service = DIPSService::Get(profile.get());
  ASSERT_NE(service, nullptr);

  // Ensure the regular profile's database files have been created since the
  // DIPS feature and persistence are enabled.
  WaitOnStorage(service);
  service->WaitForFileDeletionCompleteForTesting();
  ASSERT_TRUE(base::PathExists(GetDIPSFilePath(profile.get())));

  // Build an off-the-record profile based on `profile`.
  TestingProfile* otr_profile =
      TestingProfile::Builder().SetPath(data_path).BuildIncognito(
          profile.get());
  DIPSService* otr_service = DIPSService::Get(otr_profile);
  ASSERT_NE(otr_service, nullptr);

  // Ensure the OTR profile's database has been initialized and any file
  // deletion tasks have finished (although there shouldn't be any).
  WaitOnStorage(otr_service);
  otr_service->WaitForFileDeletionCompleteForTesting();

  // Ensure the regular profile's database files were NOT deleted.
  EXPECT_TRUE(base::PathExists(GetDIPSFilePath(profile.get())));
}

TEST_F(DIPSServiceTest, EmptySiteEventsIgnored) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kDIPS);
  std::unique_ptr<TestingProfile> profile = std::make_unique<TestingProfile>();
  DIPSService* service = DIPSService::Get(profile.get());

  // Record a bounce for an empty URL.
  GURL url;
  base::Time bounce = base::Time::FromDoubleT(2);
  service->RecordBounceForTesting(
      url, GURL("https://initial.com"), GURL("https://final.com"), bounce,
      false, base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(service);

  // Verify that an entry is not returned when querying for an empty URL,
  StateForURLCallback callback = base::BindLambdaForTesting(
      [&](DIPSState state) { EXPECT_FALSE(state.was_loaded()); });
  service->storage()
      ->AsyncCall(&DIPSStorage::Read)
      .WithArgs(url)
      .Then(std::move(callback));
  WaitOnStorage(service);
}

class DIPSServiceStateRemovalTest : public testing::Test {
 public:
  DIPSServiceStateRemovalTest()
      : profile_(std::make_unique<TestingProfile>()),
        service_(DIPSService::Get(GetProfile())),
        cookie_settings_(
            CookieSettingsFactory::GetForProfile(GetProfile()).get()) {}

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

  Profile* GetProfile() { return profile_.get(); }
  DIPSService* GetService() { return service_; }
  content_settings::CookieSettings* GetCookieSettings() {
    return cookie_settings_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::MockBrowsingDataRemoverDelegate delegate_;

  // Test setup.
  void SetUp() override {
    grace_period = features::kDIPSGracePeriod.Get();
    interaction_ttl = features::kDIPSInteractionTtl.Get();
    ASSERT_LT(tiny_delta, grace_period);

    GetProfile()->GetBrowsingDataRemover()->SetEmbedderDelegate(&delegate_);
    SetBlockThirdPartyCookies(true);
    ASSERT_TRUE(GetCookieSettings()->ShouldBlockThirdPartyCookies());

    DCHECK(service_);
    service_->SetStorageClockForTesting(&clock_);
    WaitOnStorage(GetService());
  }

  void TearDown() override {
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void AdvanceTimeTo(base::Time now) {
    ASSERT_GE(now, clock_.Now());
    clock_.SetNow(now);
  }

  base::Time Now() { return clock_.Now(); }
  void SetNow(base::Time now) { clock_.SetNow(now); }

  void AdvanceTimeBy(base::TimeDelta delta) { clock_.Advance(delta); }

  void FireDIPSTimer() {
    service_->OnTimerFiredForTesting();
    WaitOnStorage(GetService());
  }

  // Add an exception to the third-party cookie blocking rule for
  // |third_party_url| embedded by |first_party_url|. If |first_party_url| is
  // not provided, |third_party_url| is allowed when embedded by any site. If
  // |third_party_url| is not provided, any sites embedded under
  // |first_party_url| are excepted.
  void Add3PCException(const absl::optional<GURL>& first_party_url,
                       const absl::optional<GURL>& third_party_url) {
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(GetProfile());

    ContentSettingsPattern first_party_pattern =
        first_party_url.has_value() ? ContentSettingsPattern::FromString(
                                          "[*.]" + first_party_url->host())
                                    : ContentSettingsPattern::Wildcard();

    ContentSettingsPattern third_party_pattern =
        third_party_url.has_value() ? ContentSettingsPattern::FromString(
                                          "[*.]" + third_party_url->host())
                                    : ContentSettingsPattern::Wildcard();

    map->SetContentSettingCustomScope(third_party_pattern, first_party_pattern,
                                      ContentSettingsType::COOKIES,
                                      ContentSetting::CONTENT_SETTING_ALLOW);

    EXPECT_EQ(
        CONTENT_SETTING_BLOCK,
        GetCookieSettings()->GetCookieSetting(
            first_party_url.value_or(GURL()), third_party_url.value_or(GURL()),
            net::CookieSettingOverrides(), nullptr));
    EXPECT_EQ(
        CONTENT_SETTING_ALLOW,
        GetCookieSettings()->GetCookieSetting(
            third_party_url.value_or(GURL()), first_party_url.value_or(GURL()),
            net::CookieSettingOverrides(), nullptr));
  }

 private:
  base::SimpleTestClock clock_;

  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<DIPSService, DanglingUntriaged> service_ = nullptr;
  raw_ptr<content_settings::CookieSettings, DanglingUntriaged>
      cookie_settings_ = nullptr;
};

TEST_F(DIPSServiceStateRemovalTest,
       CompleteChain_NotifiesRedirectChainObservers) {
  GetService()->SetStorageClockForTesting(base::DefaultClock::GetInstance());
  auto observer = std::make_unique<RedirectChainObserver>(
      GetService(), /*final_url=*/GURL("http://c.test/"));

  std::vector<DIPSRedirectInfoPtr> complete_redirects;
  complete_redirects.push_back(std::make_unique<DIPSRedirectInfo>(
      /*url=*/GURL("http://b.test/"),
      /*redirect_type=*/DIPSRedirectType::kServer,
      /*access_type=*/SiteDataAccessType::kNone,
      /*source_id=*/ukm::SourceId(),
      /*time=*/Now()));
  auto complete_chain = std::make_unique<DIPSRedirectChainInfo>(
      /*initial_url=*/GURL("http://a.test/"),
      /*final_url=*/GURL("http://c.test/"),
      /*length=*/1, /*is_partial_chain=*/false);

  GetService()->HandleRedirectChain(
      std::move(complete_redirects), std::move(complete_chain),
      base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());
  // Expect one call to Observer.OnChainHandled when handling a complete chain.
  EXPECT_EQ(observer->handle_call_count, 1u);
}

TEST_F(DIPSServiceStateRemovalTest,
       PartialChain_DoesNotNotifyRedirectChainObservers) {
  GetService()->SetStorageClockForTesting(base::DefaultClock::GetInstance());
  auto observer = std::make_unique<RedirectChainObserver>(
      GetService(), /*final_url=*/GURL("http://c.test/"));

  std::vector<DIPSRedirectInfoPtr> partial_redirects;
  partial_redirects.push_back(std::make_unique<DIPSRedirectInfo>(
      /*url=*/GURL("http://b.test/"),
      /*redirect_type=*/DIPSRedirectType::kServer,
      /*access_type=*/SiteDataAccessType::kNone,
      /*source_id=*/ukm::SourceId(),
      /*time=*/Now()));
  auto partial_chain = std::make_unique<DIPSRedirectChainInfo>(
      /*initial_url=*/GURL("http://a.test/"),
      /*final_url=*/GURL("http://c.test/"),
      /*length=*/1, /*is_partial_chain=*/true);

  GetService()->HandleRedirectChain(
      std::move(partial_redirects), std::move(partial_chain),
      base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());
  // Expect no calls to Observer.OnChainHandled when handling a partial chain.
  EXPECT_EQ(observer->handle_call_count, 0u);
}

// NOTE: The use of a MockBrowsingDataRemoverDelegate in this test fixture
// means that when DIPS deletion is enabled, the row for 'url' is not actually
// removed from the DIPS db since 'delegate_' doesn't actually carryout the
// removal task.
TEST_F(DIPSServiceStateRemovalTest, BrowsingDataDeletion_Enabled) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS, {{"delete", "true"}, {"triggering_action", "bounce"}});

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromDoubleT(2);
  GetService()->RecordBounceForTesting(
      url, GURL("https://initial.com"), GURL("https://final.com"), bounce,
      false, base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());
  EXPECT_TRUE(GetDIPSState(GetService(), url).has_value());

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
  EXPECT_TRUE(GetDIPSState(GetService(), url).has_value());

  EXPECT_THAT(ukm_recorder,
              EntryUrlsAre("DIPS.Deletion", {"http://example.com/"}));
}

TEST_F(DIPSServiceStateRemovalTest, BrowsingDataDeletion_Disabled) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS, {{"delete", "false"}, {"triggering_action", "bounce"}});

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromDoubleT(2);
  GetService()->RecordBounceForTesting(
      url, GURL("https://initial.com"), GURL("https://final.com"), bounce,
      false, base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());
  EXPECT_TRUE(GetDIPSState(GetService(), url).has_value());

  // Set the current time to just after the bounce happened.
  AdvanceTimeTo(bounce + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify the DIPS entry was not removed and a removal task was not posted to
  // the BrowsingDataRemover(Delegate).
  delegate_.VerifyAndClearExpectations();
  EXPECT_TRUE(GetDIPSState(GetService(), url).has_value());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify that the site's DIPS entry WAS removed, but a removal task was NOT
  // posted to the BrowsingDataRemover(Delegate) since
  // `features::kDIPSDeletionEnabled` is false.
  delegate_.VerifyAndClearExpectations();
  EXPECT_FALSE(GetDIPSState(GetService(), url).has_value());

  EXPECT_THAT(ukm_recorder,
              EntryUrlsAre("DIPS.Deletion", {"http://example.com/"}));
}

TEST_F(DIPSServiceStateRemovalTest,
       BrowsingDataDeletion_Respects3PExceptionsFor3PC) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS, {{"delete", "true"}, {"triggering_action", "bounce"}});

  GURL excepted_3p_url("https://excepted-as-3p.com");
  GURL non_excepted_url("https://not-excepted.com");

  Add3PCException(absl::nullopt, excepted_3p_url);

  int stateful_bounce_count = 0;
  base::RepeatingCallback<void(const GURL&)> increment_bounce =
      base::BindLambdaForTesting(
          [&](const GURL& final_url) { stateful_bounce_count++; });

  // Bounce through both tracking sites.
  base::Time bounce = base::Time::FromDoubleT(2);
  GetService()->RecordBounceForTesting(
      excepted_3p_url, GURL("https://initial.com"), GURL("https://final.com"),
      bounce, true, increment_bounce);
  GetService()->RecordBounceForTesting(
      non_excepted_url, GURL("https://initial.com"), GURL("https://final.com"),
      bounce, true, increment_bounce);
  WaitOnStorage(GetService());

  // Verify that the bounce was not recorded for the excepted 3P URL.
  EXPECT_FALSE(GetDIPSState(GetService(), excepted_3p_url).has_value());
  EXPECT_TRUE(GetDIPSState(GetService(), non_excepted_url).has_value());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Only the non-excepted site should be reported to UKM.
  EXPECT_THAT(ukm_recorder,
              EntryUrlsAre("DIPS.Deletion", {"http://not-excepted.com/"}));

  // Expect one recorded bounce, for the stateful redirect through the
  // non-excepted site.
  EXPECT_EQ(stateful_bounce_count, 1);
}

TEST_F(DIPSServiceStateRemovalTest,
       BrowsingDataDeletion_Respects1PExceptionsFor3PC) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS, {{"delete", "true"}, {"triggering_action", "bounce"}});

  GURL excepted_1p_url("https://excepted-as-1p.com");
  GURL scoped_excepted_1p_url("https://excepted-as-1p-with-3p.com");
  GURL non_excepted_url("https://not-excepted.com");
  GURL redirect_url_1("https://redirect-1.com");
  GURL redirect_url_2("https://redirect-2.com");
  GURL redirect_url_3("https://redirect-3.com");

  Add3PCException(excepted_1p_url, absl::nullopt);
  Add3PCException(scoped_excepted_1p_url, redirect_url_1);

  int stateful_bounce_count = 0;
  base::RepeatingCallback<void(const GURL&)> increment_bounce =
      base::BindLambdaForTesting(
          [&](const GURL& final_url) { stateful_bounce_count++; });

  base::Time bounce = base::Time::FromDoubleT(2);
  // Record a bounce through redirect_url_1 that starts on an excepted
  // URL.
  GetService()->RecordBounceForTesting(redirect_url_1, excepted_1p_url,
                                       non_excepted_url, bounce, true,
                                       increment_bounce);
  // Record a bounce through redirect_url_1 that ends on an excepted
  // URL.
  GetService()->RecordBounceForTesting(redirect_url_1, non_excepted_url,
                                       excepted_1p_url, bounce, true,
                                       increment_bounce);
  // Record a bounce through redirect_url_1 that ends on a URL with an exception
  // scoped to redirect_url_1.
  GetService()->RecordBounceForTesting(redirect_url_1, non_excepted_url,
                                       scoped_excepted_1p_url, bounce, true,
                                       increment_bounce);
  // Record a bounce through redirect_url_2 that does not start or
  // end on an excepted URL.
  GetService()->RecordBounceForTesting(redirect_url_2, non_excepted_url,
                                       non_excepted_url, bounce, true,
                                       increment_bounce);
  // Record a bounce through redirect_url_3 that does not start or
  // end on an excepted URL. Record an interaction on this URL as well.
  GetService()->RecordBounceForTesting(redirect_url_3, non_excepted_url,
                                       non_excepted_url, bounce, true,
                                       increment_bounce);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordInteraction)
      .WithArgs(redirect_url_3, bounce, GetService()->GetCookieMode());
  WaitOnStorage(GetService());

  // Expect no recorded DIPSState for redirect_url_1, since every
  // recorded bounce started or ended on an excepted site.
  EXPECT_FALSE(GetDIPSState(GetService(), redirect_url_1).has_value());
  EXPECT_TRUE(GetDIPSState(GetService(), redirect_url_2).has_value());
  EXPECT_TRUE(GetDIPSState(GetService(), redirect_url_3).has_value());

  // Record a bounce through redirect_url_2 that starts on an
  // excepted URL. This should clear the DB entry for redirect_url_2.
  GetService()->RecordBounceForTesting(redirect_url_2, excepted_1p_url,
                                       non_excepted_url, bounce, true,
                                       increment_bounce);
  EXPECT_FALSE(GetDIPSState(GetService(), redirect_url_2).has_value());

  // Record a bounce through redirect_url_3 that starts on an
  // excepted URL. This should not clear the DB entry for redirect_url_3 as it
  // has a recorded interaction.
  GetService()->RecordBounceForTesting(redirect_url_3, excepted_1p_url,
                                       non_excepted_url, bounce, true,
                                       increment_bounce);
  EXPECT_TRUE(GetDIPSState(GetService(), redirect_url_3).has_value());

  // Expect two non-excepted stateful redirects: the first bounces through
  // redirect_url_2 and redirect_url_3.
  EXPECT_EQ(stateful_bounce_count, 2);
}

TEST_F(DIPSServiceStateRemovalTest,
       BrowsingDataDeletion_RespectsStorageAccessGrantExceptions) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::vector<base::test::FeatureRefAndParams> enabled_features;
  enabled_features.push_back(
      {features::kDIPS, {{"delete", "true"}, {"triggering_action", "bounce"}}});
  enabled_features.push_back({blink::features::kStorageAccessAPI, {}});
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(enabled_features, {});

  GURL storage_access_grant_url("https://storage-access-granted.com");
  GURL top_level_storage_access_grant_url(
      "https://top-level-storage-access-granted.com");
  GURL no_grant_url("https://no-storage-access-grant.com");
  GURL redirect_url_1("https://redirect-1.com");
  GURL redirect_url_2("https://redirect-2.com");
  GURL redirect_url_3("https://redirect-3.com");

  // Create Storage Access grants for the required sites.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]" +
                                         storage_access_grant_url.host()),
      ContentSettingsType::STORAGE_ACCESS, CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString(
          "[*.]" + top_level_storage_access_grant_url.host()),
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, CONTENT_SETTING_ALLOW);

  int stateful_bounce_count = 0;
  base::RepeatingCallback<void(const GURL&)> increment_bounce =
      base::BindLambdaForTesting(
          [&](const GURL& final_url) { stateful_bounce_count++; });

  base::Time bounce = base::Time::FromDoubleT(2);
  // Record a bounce through redirect_url_1 that starts on a URL with an SA
  // grant.
  GetService()->RecordBounceForTesting(redirect_url_1, storage_access_grant_url,
                                       no_grant_url, bounce, true,
                                       increment_bounce);
  // Record a bounce through redirect_url_1 that ends on a URL with a top-level
  // SA grant.
  GetService()->RecordBounceForTesting(redirect_url_1, no_grant_url,
                                       top_level_storage_access_grant_url,
                                       bounce, true, increment_bounce);
  // Record a bounce through redirect_url_2 that does not start or
  // end on a URL with an SA grant.
  GetService()->RecordBounceForTesting(redirect_url_2, no_grant_url,
                                       no_grant_url, bounce, true,
                                       increment_bounce);
  // Record a bounce through redirect_url_3 that does not start or
  // end on a URL with an SA grant. Record an interaction on this URL as well.
  GetService()->RecordBounceForTesting(redirect_url_3, no_grant_url,
                                       no_grant_url, bounce, true,
                                       increment_bounce);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordInteraction)
      .WithArgs(redirect_url_3, bounce, GetService()->GetCookieMode());
  WaitOnStorage(GetService());

  // Expect no recorded DIPSState for redirect_url_1, since every
  // recorded bounce started or ended on a site with an SA grant.
  EXPECT_FALSE(GetDIPSState(GetService(), redirect_url_1).has_value());
  EXPECT_TRUE(GetDIPSState(GetService(), redirect_url_2).has_value());
  EXPECT_TRUE(GetDIPSState(GetService(), redirect_url_3).has_value());

  // Record a bounce through redirect_url_2 that starts on a URL with an SA
  // grant. This should clear the DB entry for redirect_url_2.
  GetService()->RecordBounceForTesting(redirect_url_2, storage_access_grant_url,
                                       no_grant_url, bounce, true,
                                       increment_bounce);
  EXPECT_FALSE(GetDIPSState(GetService(), redirect_url_2).has_value());

  // Record a bounce through redirect_url_3 that starts on a URL with an SA
  // grant. This should not clear the DB entry for redirect_url_3 as it has a
  // recorded interaction.
  GetService()->RecordBounceForTesting(redirect_url_3, storage_access_grant_url,
                                       no_grant_url, bounce, true,
                                       increment_bounce);
  EXPECT_TRUE(GetDIPSState(GetService(), redirect_url_3).has_value());

  // Expect two non-SA stateful redirects: the first bounces through
  // redirect_url_2 and redirect_url_3.
  EXPECT_EQ(stateful_bounce_count, 2);
}

// When third-party cookies are globally allowed, bounces should be recorded for
// sites which have an exception to block 3PC, but not by default.
TEST_F(
    DIPSServiceStateRemovalTest,
    BrowsingDataDeletion_Respects1PExceptionsForBlocking3PCWhenDefaultAllowed) {
  SetBlockThirdPartyCookies(false);

  ukm::TestAutoSetUkmRecorder ukm_recorder;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS, {{"delete", "true"}, {"triggering_action", "bounce"}});

  GURL blocked_1p_url("https://excepted-as-1p.com");
  GURL scoped_blocked_1p_url("https://excepted-as-1p-with-3p.com");
  GURL non_blocked_url("https://not-excepted.com");
  GURL redirect_url_1("https://redirect-1.com");
  GURL redirect_url_2("https://redirect-2.com");
  GURL redirect_url_3("https://redirect-3.com");
  GURL redirect_url_4("https://redirect-4.com");

  // Exceptions to block third-party cookies.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(),
      ContentSettingsPattern::FromString("[*.]" + blocked_1p_url.host()),
      ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_BLOCK);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("[*.]" + redirect_url_1.host()),
      ContentSettingsPattern::FromString("[*.]" + scoped_blocked_1p_url.host()),
      ContentSettingsType::COOKIES, ContentSetting::CONTENT_SETTING_BLOCK);

  int stateful_bounce_count = 0;
  base::RepeatingCallback<void(const GURL&)> increment_bounce =
      base::BindLambdaForTesting(
          [&](const GURL& final_url) { stateful_bounce_count++; });

  base::Time bounce = base::Time::FromDoubleT(2);
  // Record a bounce through redirect_url_1 that starts and ends on blocked
  // URLs.
  GetService()->RecordBounceForTesting(redirect_url_1, blocked_1p_url,
                                       scoped_blocked_1p_url, bounce, true,
                                       increment_bounce);
  // Record a bounce through redirect_url_2 that starts and ends on blocked
  // URLs. Record an interaction on this URL as well.
  GetService()->RecordBounceForTesting(redirect_url_2, blocked_1p_url,
                                       blocked_1p_url, bounce, true,
                                       increment_bounce);
  GetService()
      ->storage()
      ->AsyncCall(&DIPSStorage::RecordInteraction)
      .WithArgs(redirect_url_2, bounce, GetService()->GetCookieMode());
  WaitOnStorage(GetService());
  // Record a bounce through redirect_url_3 that starts on a non-blocked URL.
  GetService()->RecordBounceForTesting(redirect_url_3, non_blocked_url,
                                       blocked_1p_url, bounce, true,
                                       increment_bounce);
  // Record a bounce through redirect_url_4 that ends on a non-blocked URL.
  GetService()->RecordBounceForTesting(redirect_url_4, blocked_1p_url,
                                       non_blocked_url, bounce, true,
                                       increment_bounce);

  // Expect a recorded DIPSState for redirect_url_1 and redirect_url_2, since
  // they were bounced through with blocking exceptions on both the initial and
  // final URL. The other two trackers were only bounced through from
  // default-allowed sites.
  EXPECT_TRUE(GetDIPSState(GetService(), redirect_url_1).has_value());
  EXPECT_TRUE(GetDIPSState(GetService(), redirect_url_2).has_value());
  EXPECT_FALSE(GetDIPSState(GetService(), redirect_url_3).has_value());
  EXPECT_FALSE(GetDIPSState(GetService(), redirect_url_4).has_value());

  // Record a bounce through redirect_url_1 that starts on a non-blocked URL.
  // This should clear the DB entry for redirect_url_1.
  GetService()->RecordBounceForTesting(redirect_url_1, non_blocked_url,
                                       blocked_1p_url, bounce, true,
                                       increment_bounce);
  EXPECT_FALSE(GetDIPSState(GetService(), redirect_url_1).has_value());

  // Record a bounce through redirect_url_2 that starts on a
  // blocked URL. This should not clear the DB entry for redirect_url_2 as it
  // has a recorded interaction.
  GetService()->RecordBounceForTesting(redirect_url_2, non_blocked_url,
                                       blocked_1p_url, bounce, true,
                                       increment_bounce);
  EXPECT_TRUE(GetDIPSState(GetService(), redirect_url_2).has_value());

  // Expect two recorded stateful redirects: the first bounces through
  // redirect_url_1 and redirect_url_2.
  EXPECT_EQ(stateful_bounce_count, 2);
}

TEST_F(DIPSServiceStateRemovalTest, ImmediateEnforcement) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS, {{"delete", "true"}, {"triggering_action", "bounce"}});
  SetNow(base::Time::FromDoubleT(2));

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = Now();
  GetService()->RecordBounceForTesting(
      url, GURL("https://initial.com"), GURL("https://final.com"), bounce,
      false, base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());
  EXPECT_TRUE(GetDIPSState(GetService(), url).has_value());

  // Set the current time to just after the bounce happened and simulate firing
  // the DIPS timer.
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

  // Perform immediate enforcement of deletion, without regard for grace period
  // and verify `url` is returned the `DeletedSitesCallback`.
  base::RunLoop run_loop;
  base::OnceCallback<void(const std::vector<std::string>& sites)> callback =
      base::BindLambdaForTesting(
          [&](const std::vector<std::string>& deleted_sites) {
            EXPECT_THAT(deleted_sites,
                        testing::UnorderedElementsAre(GetSiteForDIPS(url)));
            run_loop.Quit();
          });
  GetService()->DeleteEligibleSitesImmediately(std::move(callback));
  task_environment_.RunUntilIdle();
  run_loop.Run();

  // Verify that a removal task was posted to the BrowsingDataRemover(Delegate)
  // for 'url'.
  delegate_.VerifyAndClearExpectations();
}

// A test class that verifies DIPSService state deletion metrics collection
// behavior.
class DIPSServiceHistogramTest : public DIPSServiceStateRemovalTest {
 public:
  DIPSServiceHistogramTest() = default;

  const base::HistogramTester& histograms() const { return histogram_tester_; }

 protected:
  const std::string kBlock3PC = "Block3PC";
  const std::string kUmaHistogramDeletionPrefix = "Privacy.DIPS.Deletion.";

  base::HistogramTester histogram_tester_;
};

TEST_F(DIPSServiceHistogramTest, DeletionLatency) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS, {{"delete", "false"}, {"triggering_action", "bounce"}});

  // Verify the histogram starts empty
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency2", 0);

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce = base::Time::FromDoubleT(2);
  GetService()->RecordBounceForTesting(
      url, GURL("https://initial.com"), GURL("https://final.com"), bounce,
      false, base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());

  // Set the current time to just after the bounce happened.
  AdvanceTimeTo(bounce + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify deletion latency metrics were NOT emitted and the DIPS entry was NOT
  // removed.
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency2", 0);
  EXPECT_TRUE(GetDIPSState(GetService(), url).has_value());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion latency metric was emitted and the DIPS entry was
  // removed.
  histograms().ExpectTotalCount("Privacy.DIPS.DeletionLatency2", 1);
  EXPECT_FALSE(GetDIPSState(GetService(), url).has_value());
}

TEST_F(DIPSServiceHistogramTest, Deletion_Disallowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS,
      {{"delete", "false"}, {"triggering_action", "stateful_bounce"}});

  // Verify the histogram is initially empty.
  EXPECT_TRUE(histograms()
                  .GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix)
                  .empty());

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce_time = base::Time::FromDoubleT(2);
  GetService()->RecordBounceForTesting(
      url, GURL("https://initial.com"), GURL("https://final.com"), bounce_time,
      true, base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce_time + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion metric was emitted and the DIPS entry was removed.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[kUmaHistogramDeletionPrefix + kBlock3PC] = 1;
  EXPECT_THAT(histograms().GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix),
              testing::ContainerEq(expected_counts));
  histograms().ExpectUniqueSample(kUmaHistogramDeletionPrefix + kBlock3PC,
                                  DIPSDeletionAction::kDisallowed, 1);
  EXPECT_FALSE(GetDIPSState(GetService(), url).has_value());
}

TEST_F(DIPSServiceHistogramTest, Deletion_ExceptedAs1P) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS,
      {{"delete", "true"}, {"triggering_action", "stateful_bounce"}});

  // Verify the histogram is initially empty.
  EXPECT_TRUE(histograms()
                  .GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix)
                  .empty());

  // Record a bounce.
  GURL url("https://example.com");
  GURL excepted_1p_url("https://initial.com");
  Add3PCException(excepted_1p_url, absl::nullopt);
  base::Time bounce_time = base::Time::FromDoubleT(2);
  GetService()->RecordBounceForTesting(
      url, excepted_1p_url, GURL("https://final.com"), bounce_time, true,
      base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce_time + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion metric was emitted and the DIPS entry was removed.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[kUmaHistogramDeletionPrefix + kBlock3PC] = 1;
  EXPECT_THAT(histograms().GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix),
              testing::ContainerEq(expected_counts));
  histograms().ExpectUniqueSample(kUmaHistogramDeletionPrefix + kBlock3PC,
                                  DIPSDeletionAction::kExcepted, 1);
  EXPECT_FALSE(GetDIPSState(GetService(), url).has_value());
}

TEST_F(DIPSServiceHistogramTest, Deletion_ExceptedAs3P) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS,
      {{"delete", "true"}, {"triggering_action", "stateful_bounce"}});

  // Verify the histogram is initially empty.
  EXPECT_TRUE(histograms()
                  .GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix)
                  .empty());

  // Record a bounce.
  GURL excepted_3p_url("https://example.com");
  Add3PCException(absl::nullopt, excepted_3p_url);
  base::Time bounce_time = base::Time::FromDoubleT(2);
  GetService()->RecordBounceForTesting(
      excepted_3p_url, GURL("https://initial.com"), GURL("https://final.com"),
      bounce_time, true, base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce_time + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion metric was emitted and the DIPS entry was removed.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[kUmaHistogramDeletionPrefix + kBlock3PC] = 1;
  EXPECT_THAT(histograms().GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix),
              testing::ContainerEq(expected_counts));
  histograms().ExpectUniqueSample(kUmaHistogramDeletionPrefix + kBlock3PC,
                                  DIPSDeletionAction::kExcepted, 1);
  EXPECT_FALSE(GetDIPSState(GetService(), excepted_3p_url).has_value());
}

TEST_F(DIPSServiceHistogramTest, Deletion_Enforced) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kDIPS,
      {{"delete", "true"}, {"triggering_action", "stateful_bounce"}});

  // Verify the histogram is initially empty.
  EXPECT_TRUE(histograms()
                  .GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix)
                  .empty());

  // Record a bounce.
  GURL url("https://example.com");
  base::Time bounce_time = base::Time::FromDoubleT(2);
  GetService()->RecordBounceForTesting(
      url, GURL("https://initial.com"), GURL("https://final.com"), bounce_time,
      true, base::BindRepeating([](const GURL& final_url) {}));
  WaitOnStorage(GetService());

  // Time-travel to after the grace period has ended for the bounce.
  AdvanceTimeTo(bounce_time + grace_period + tiny_delta);
  FireDIPSTimer();
  task_environment_.RunUntilIdle();

  // Verify a deletion metric was emitted and the DIPS entry was not removed.
  base::HistogramTester::CountsMap expected_counts;
  expected_counts[kUmaHistogramDeletionPrefix + kBlock3PC] = 1;
  EXPECT_THAT(histograms().GetTotalCountsForPrefix(kUmaHistogramDeletionPrefix),
              testing::ContainerEq(expected_counts));
  histograms().ExpectUniqueSample(kUmaHistogramDeletionPrefix + kBlock3PC,
                                  DIPSDeletionAction::kEnforced, 1);
  EXPECT_TRUE(GetDIPSState(GetService(), url).has_value());
}
