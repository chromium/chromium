// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_foundation_service_monitor.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/service_process_info.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
inline constexpr char kSite1[] = "https://site1.com";
inline constexpr char kSite2[] = "https://site2.com";
inline constexpr char kSite3[] = "https://subdomain1.site1.com";
inline constexpr char kMediaFoundationServiceProcessName[] =
    "media.mojom.MediaFoundationServiceBroker";
}  // namespace

class MediaFoundationServiceMonitorTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    // Set up a testing profile manager.
    ChromeRenderViewHostTestHarness::SetUp();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());

    // Create a new temporary directory, and store the path
    const base::FilePath temp_user_data_dir =
        base::CreateUniqueTempDirectoryScopedToTest();
    ASSERT_TRUE(profile_manager_->SetUp(temp_user_data_dir));

    monitor_ = MediaFoundationServiceMonitor::GetInstance();
    CHECK(monitor_);

    // Tests could write to local state and profile. Must clear them.
    ClearLocalState();
    ClearProfile();
    // MediaFoundationServiceMonitor is a singleton and holds state between
    // test. Must clear it.
    monitor_->ResetForTesting();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  // Test the `days_since_last_disabling_date` against `disabled_dates`, both
  // of which are integer(s) number of days since an arbitrary base time.
  void TestEarliestEnableDate(std::vector<int> disabled_dates,
                              int days_since_last_disabling_date) {
    // An arbitrary base time for the tests.
    base::Time base_time;
    EXPECT_TRUE(base::Time::FromString("22 Sep 2022 12:23 GMT", &base_time));

    std::vector<base::Time> disabled_times;
    for (const auto& days : disabled_dates) {
      disabled_times.push_back(base_time + base::Days(days));
    }
    auto enable_time =
        MediaFoundationServiceMonitor::GetEarliestEnableTime(disabled_times);
    auto expected_time =
        disabled_times.back() + base::Days(days_since_last_disabling_date);

    // Expect the `enable_time` to be in a range to avoid testing rounding
    // logic.
    EXPECT_LE(enable_time, expected_time + base::Days(1));
    EXPECT_GE(enable_time, expected_time - base::Days(1));
  }

  // Creates an entry for `site` in profile.
  void InitProfileForSite(const GURL& site) {
    auto* user_prefs =
        profile_manager_->profile_manager()->GetLastUsedProfile()->GetPrefs();
    ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);

    update.Get()
        .EnsureDict(url::Origin::Create(site).Serialize())
        ->EnsureList(prefs::kHardwareSecureDecryptionDisabledTimes);
  }

  // Clears user profile.
  void ClearProfile() {
    auto* user_prefs =
        profile_manager_->profile_manager()->GetLastUsedProfile()->GetPrefs();
    ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);
    for (auto [key, value] : update.Get()) {
      ASSERT_TRUE(update.Get().Remove(key));
    }
  }

  // Clears local state.
  void ClearLocalState() {
    auto* user_prefs =
        profile_manager_->profile_manager()->GetLastUsedProfile()->GetPrefs();
    ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);
    profile_manager_->local_state()->Get()->ClearPref(
        prefs::kGlobalHardwareSecureDecryptionDisabledTimes);
  }

  bool IsAllowedForSite(GURL site) {
    return MediaFoundationServiceMonitor::
        IsHardwareSecureDecryptionAllowedForSite(site);
  }

  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<MediaFoundationServiceMonitor> monitor_;
};

TEST_F(MediaFoundationServiceMonitorTest, GetEarliestEnableTime_Default) {
  // One disabling event will cause the feature to be disabled for 30 days,
  // which is the minimum disabling days.
  TestEarliestEnableDate({0}, 30);
  TestEarliestEnableDate({1}, 30);
  TestEarliestEnableDate({10}, 30);

  // Two close disabling events will cause the feature to be disabled for 180
  // days, which is the maximum disabling days.
  TestEarliestEnableDate({10, 10}, 180);
  TestEarliestEnableDate({10, 20}, 180);
  TestEarliestEnableDate({10, 40}, 180);

  // The closer the two disabling events are, the longer the feature will be
  // disabled.
  TestEarliestEnableDate({10, 50}, 142);
  TestEarliestEnableDate({10, 100}, 80);
  TestEarliestEnableDate({10, 1000}, 34);

  // Two far apart disabling events will cause the feature to be disabled for 30
  // days, which is the minimum disabling days.
  TestEarliestEnableDate({10, 10000}, 30);

  // The third disabling event time doesn't matter.
  TestEarliestEnableDate({10, 50}, 142);
  TestEarliestEnableDate({1, 10, 50}, 142);
}

TEST_F(MediaFoundationServiceMonitorTest, GetEarliestEnableTime_Overridden) {
  // Ensure we take any base::Feature overrides into account.
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      media::kHardwareSecureDecryptionFallback,
      {{"min_disabling_days", "10"}, {"max_disabling_days", "60"}});

  // One disabling event will cause the feature to be disabled for 30 days,
  // which is the minimum disabling days.
  TestEarliestEnableDate({0}, 10);
  TestEarliestEnableDate({1}, 10);
  TestEarliestEnableDate({10}, 10);

  // Two close disabling events will cause the feature to be disabled for 60
  // days, which is the maximum disabling days.
  TestEarliestEnableDate({10, 10}, 60);
  TestEarliestEnableDate({10, 20}, 60);

  // The closer the two disabling events are, the longer the feature will be
  // disabled.
  TestEarliestEnableDate({10, 40}, 26);
  TestEarliestEnableDate({10, 50}, 22);
  TestEarliestEnableDate({10, 100}, 15);

  // Two far apart disabling events will cause the feature to be disabled for 10
  // days, which is the minimum disabling days.
  TestEarliestEnableDate({10, 1000}, 10);
  TestEarliestEnableDate({10, 10000}, 10);

  // The third disabling event time doesn't matter.
  TestEarliestEnableDate({10, 50}, 22);
  TestEarliestEnableDate({1, 10, 50}, 22);
}

TEST_F(MediaFoundationServiceMonitorTest, CrashInSiteShouldDisable) {
  content::ServiceProcessInfo crashed_info(
      kMediaFoundationServiceProcessName, GURL(kSite1),
      content::ServiceProcessId(), base::Process::Current());
  // Creation of user profile happens after media foundation cdm is created. We
  // need to force creating an entry to user profile first for testing.
  InitProfileForSite(crashed_info.site().value());
  EXPECT_TRUE(IsAllowedForSite(crashed_info.site().value()));

  // First crash still allows the site to play as first crash could be
  // transient.
  monitor_->OnServiceProcessCrashed(crashed_info);
  EXPECT_TRUE(IsAllowedForSite(crashed_info.site().value()));

  // Second crash should disallow hardware secure decryption.
  monitor_->OnServiceProcessCrashed(crashed_info);
  EXPECT_FALSE(IsAllowedForSite(crashed_info.site().value()));
}

TEST_F(MediaFoundationServiceMonitorTest, CrashInSiteShouldNotAffectOtherSite) {
  content::ServiceProcessInfo crashed_info(
      kMediaFoundationServiceProcessName, GURL(kSite1),
      content::ServiceProcessId(), base::Process::Current());
  content::ServiceProcessInfo ok_info(kMediaFoundationServiceProcessName,
                                      GURL(kSite2), content::ServiceProcessId(),
                                      base::Process::Current());
  // Creation of user profile happens after media foundation cdm is created. We
  // need to force creating an entry to user profile first for testing.
  InitProfileForSite(crashed_info.site().value());
  InitProfileForSite(ok_info.site().value());
  EXPECT_TRUE(IsAllowedForSite(crashed_info.site().value()));
  EXPECT_TRUE(IsAllowedForSite(ok_info.site().value()));

  // First crash still allows the site to play as first crash could be
  // transient.
  monitor_->OnServiceProcessCrashed(crashed_info);
  EXPECT_TRUE(IsAllowedForSite(crashed_info.site().value()));
  EXPECT_TRUE(IsAllowedForSite(ok_info.site().value()));

  // Second crash should disallow hardware secure decryption.
  monitor_->OnServiceProcessCrashed(crashed_info);
  EXPECT_FALSE(IsAllowedForSite(crashed_info.site().value()));
  EXPECT_TRUE(IsAllowedForSite(ok_info.site().value()));
}

TEST_F(MediaFoundationServiceMonitorTest, NewSiteShouldUseGlobalDisable) {
  content::ServiceProcessInfo crashed_info(
      kMediaFoundationServiceProcessName, GURL(kSite1),
      content::ServiceProcessId(), base::Process::Current());
  // Creation of user profile happens after media foundation cdm is created. We
  // need to force creating an entry to user profile first for testing.
  InitProfileForSite(crashed_info.site().value());
  EXPECT_TRUE(IsAllowedForSite(crashed_info.site().value()));

  // First crash still allows the site to play as first crash could be
  // transient.
  monitor_->OnServiceProcessCrashed(crashed_info);
  EXPECT_TRUE(IsAllowedForSite(crashed_info.site().value()));

  monitor_->OnServiceProcessCrashed(crashed_info);
  // Second crash should disallow hardware secure decryption.
  monitor_->OnServiceProcessCrashed(crashed_info);
  EXPECT_FALSE(IsAllowedForSite(crashed_info.site().value()));

  // New site should not be allowed to use hardware secure decryption.
  content::ServiceProcessInfo new_info(
      kMediaFoundationServiceProcessName, GURL(kSite2),
      content::ServiceProcessId(), base::Process::Current());
  EXPECT_FALSE(IsAllowedForSite(new_info.site().value()));
  EXPECT_TRUE(MediaFoundationServiceMonitor::
                  IsHardwareSecureDecryptionDisabledByPref());
}

TEST_F(MediaFoundationServiceMonitorTest, CrashInSiteCausesGlobalDisable) {
  content::ServiceProcessInfo crashed_info(
      kMediaFoundationServiceProcessName, GURL(kSite1),
      content::ServiceProcessId(), base::Process::Current());
  // Creation of user profile happens after media foundation cdm is created. We
  // need to force creating an entry to user profile first for testing.
  InitProfileForSite(crashed_info.site().value());
  EXPECT_TRUE(IsAllowedForSite(crashed_info.site().value()));

  // First crash still allows the site to play as first crash could be
  // transient.
  monitor_->OnServiceProcessCrashed(crashed_info);
  EXPECT_TRUE(IsAllowedForSite(crashed_info.site().value()));

  // Second crash should disallow hardware secure decryption.
  monitor_->OnServiceProcessCrashed(crashed_info);
  EXPECT_FALSE(IsAllowedForSite(crashed_info.site().value()));

  // Crash should disallow hardware secure decryption globally.
  EXPECT_TRUE(MediaFoundationServiceMonitor::
                  IsHardwareSecureDecryptionDisabledByPref());
}

TEST_F(MediaFoundationServiceMonitorTest, CrashedSiteAffectAllSameOrigins) {
  content::ServiceProcessInfo crashed_info(
      kMediaFoundationServiceProcessName, GURL(kSite1),
      content::ServiceProcessId(), base::Process::Current());
  content::ServiceProcessInfo ok_info(kMediaFoundationServiceProcessName,
                                      GURL(kSite2), content::ServiceProcessId(),
                                      base::Process::Current());
  content::ServiceProcessInfo same_origin_info(
      kMediaFoundationServiceProcessName, GURL(kSite3),
      content::ServiceProcessId(), base::Process::Current());
  // Creation of user profile happens after media foundation cdm is created. We
  // need to force creating an entry to user profile first for testing.
  InitProfileForSite(crashed_info.site().value());
  InitProfileForSite(ok_info.site().value());
  InitProfileForSite(same_origin_info.site().value());
  EXPECT_TRUE(IsAllowedForSite(crashed_info.site().value()));
  EXPECT_TRUE(IsAllowedForSite(ok_info.site().value()));
  EXPECT_TRUE(IsAllowedForSite(same_origin_info.site().value()));

  // First crash still allows the site to play as first crash could be
  // transient.
  monitor_->OnServiceProcessCrashed(crashed_info);
  EXPECT_TRUE(IsAllowedForSite(crashed_info.site().value()));

  // Second crash should disallow hardware secure decryption.
  monitor_->OnServiceProcessCrashed(crashed_info);
  EXPECT_FALSE(IsAllowedForSite(crashed_info.site().value()));

  // Site with same origin should also be disabled.
  EXPECT_FALSE(IsAllowedForSite(same_origin_info.site().value()));

  // Site with different origin should be allowed.
  EXPECT_TRUE(IsAllowedForSite(ok_info.site().value()));
}
