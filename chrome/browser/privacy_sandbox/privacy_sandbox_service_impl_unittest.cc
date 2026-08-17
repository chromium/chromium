// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_impl.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
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
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_entry_override.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kFirstPartySetsStateHistogram[] =
    "Settings.FirstPartySets.State";

const base::Version& GetRelatedWebsiteSetsVersion() {
  static const base::NoDestructor<base::Version> kVersion("1.2.3");
  return *kVersion;
}

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
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    first_party_sets_policy_service_ =
        std::make_unique<first_party_sets::FirstPartySetsPolicyService>(
            profile()->GetOriginalProfile());
  }

  void SetUp() override {
    CreateService();

    base::RunLoop run_loop;
    first_party_sets_policy_service_->WaitForFirstInitCompleteForTesting(
        run_loop.QuitClosure());
    run_loop.Run();
    first_party_sets_policy_service_->ResetForTesting();
  }

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
  first_party_sets::ScopedMockFirstPartySetsHandler&
  mock_first_party_sets_handler() {
    return mock_first_party_sets_handler_;
  }
  first_party_sets::FirstPartySetsPolicyService*
  first_party_sets_policy_service() {
    return first_party_sets_policy_service_.get();
  }

 private:
  std::unique_ptr<PrivacySandboxServiceImpl> BuildTestService(
      content::BrowserContext* context) {
    return std::make_unique<PrivacySandboxServiceImpl>(
        privacy_sandbox_settings(), cookie_settings(), profile()->GetPrefs(),
        GetProfileType(), first_party_sets_policy_service());
  }

  content::BrowserTaskEnvironment browser_task_environment_;
  TestingProfile profile_;
  profile_metrics::BrowserProfileType profile_type_ =
      profile_metrics::BrowserProfileType::kRegular;

  first_party_sets::ScopedMockFirstPartySetsHandler
      mock_first_party_sets_handler_;
  std::unique_ptr<first_party_sets::FirstPartySetsPolicyService>
      first_party_sets_policy_service_;

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

TEST_F(PrivacySandboxServiceTest,
       GetRelatedWebsiteSetOwner_SimulatedRwsData_DisabledWhen3pcAllowed) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global RWS with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets =
      net::GlobalFirstPartySets::CreateForTesting(
          GetRelatedWebsiteSetsVersion(),
          {
              {primary_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kPrimary)}},
              {associate1_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kAssociated)}},
          },
          {});

  // Simulate 3PC are allowed while RWS pref is enabled
  CreateService();
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kCookieControlsMode,
                       std::make_unique<base::Value>(static_cast<int>(
                           content_settings::CookieControlsMode::kOff)));
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();
  // We shouldn't get associate1's owner since RWS is disabled.
  EXPECT_EQ(
      privacy_sandbox_service()->GetRelatedWebsiteSetOwner(associate1_gurl),
      std::nullopt);
}

TEST_F(
    PrivacySandboxServiceTest,
    GetRelatedWebsiteSetOwner_SimulatedRwsData_DisabledWhenAllCookiesBlocked) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global RWS with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets =
      net::GlobalFirstPartySets::CreateForTesting(
          GetRelatedWebsiteSetsVersion(),
          {
              {primary_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kPrimary)}},
              {associate1_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kAssociated)}},
          },
          {});

  // Simulate all cookies are blocked while RWS pref is enabled
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  cookie_settings()->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);
  CreateService();
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();
  // We shouldn't get associate1's owner since RWS is disabled.
  EXPECT_EQ(
      privacy_sandbox_service()->GetRelatedWebsiteSetOwner(associate1_gurl),
      std::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       GetRelatedWebsiteSetOwner_SimulatedRwsData_DisabledByRwsPref) {
  GURL associate1_gurl("https://associate1.test");
  net::SchemefulSite primary_site(GURL("https://primary.test"));
  net::SchemefulSite associate1_site(associate1_gurl);

  // Create Global RWS with the following set:
  // { primary: "https://primary.test",
  // associatedSites: ["https://associate1.test"}
  net::GlobalFirstPartySets global_sets =
      net::GlobalFirstPartySets::CreateForTesting(
          GetRelatedWebsiteSetsVersion(),
          {
              {primary_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kPrimary)}},
              {associate1_site,
               {net::FirstPartySetEntry(primary_site,
                                        net::SiteType::kAssociated)}},
          },
          {});

  // Simulate RWS pref disabled while 3PC are being blocked
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(false));

  mock_first_party_sets_handler().SetGlobalSets(global_sets.Clone());

  first_party_sets_policy_service()->InitForTesting();

  // We shouldn't get associate1's owner since RWS is disabled.
  EXPECT_EQ(
      privacy_sandbox_service()->GetRelatedWebsiteSetOwner(associate1_gurl),
      std::nullopt);
}

TEST_F(PrivacySandboxServiceTest,
       SimulatedRwsData_RwsEnabled_WithoutGlobalSets) {
  GURL primary_gurl("https://primary.test");
  GURL associate1_gurl("https://associate1.test");
  GURL associate2_gurl("https://associate2.test");
  net::SchemefulSite primary_site(primary_gurl);
  net::SchemefulSite associate1_site(associate1_gurl);
  net::SchemefulSite associate2_site(associate2_gurl);

  // Set up state for the RWS UI: block 3PC and enable the RWS pref.
  prefs()->SetUserPref(
      prefs::kCookieControlsMode,
      std::make_unique<base::Value>(static_cast<int>(
          content_settings::CookieControlsMode::kBlockThirdParty)));
  CreateService();
  ClearRwsUserPrefs(prefs());
  prefs()->SetUserPref(prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
                       std::make_unique<base::Value>(true));

  // Verify `GetRelatedWebsiteSetOwner` returns empty if RWS is enabled but the
  // Global sets are not ready yet.
  EXPECT_EQ(
      privacy_sandbox_service()->GetRelatedWebsiteSetOwner(associate1_gurl),
      std::nullopt);
  EXPECT_EQ(
      privacy_sandbox_service()->GetRelatedWebsiteSetOwner(associate2_gurl),
      std::nullopt);
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


