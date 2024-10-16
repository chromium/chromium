// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_metrics.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct ProfileConfig {
  std::u16string gaia_name;
  bool active = true;
  bool sync_consent = false;
  bool supervised = false;
  bool managed = false;
};

struct ProfileMetricsParam {
  std::vector<ProfileConfig> profiles;
  int expected_managed_profile_count = 0;
  int expected_sync_consent_profile_count = 0;
  int expected_active_profile_count = 0;
  std::vector<base::Bucket> expected_name_share_samples;
};

const ProfileMetricsParam profile_metrics_test_params[] = {
    // Single signed out active profile
    {.profiles = {ProfileConfig()}, .expected_active_profile_count = 1},
    // Inactive profile.
    {.profiles = {{.active = false}}},
    // Supervised profile.
    {.profiles = {{.supervised = true}},
     .expected_managed_profile_count = 1,
     .expected_active_profile_count = 1},
    // Syncing in profile.
    {.profiles = {{.sync_consent = true}},
     .expected_sync_consent_profile_count = 1,
     .expected_active_profile_count = 1},
    // With Gaia name.
    {.profiles = {{.gaia_name = u"FirstName"}},
     .expected_active_profile_count = 1,
     .expected_name_share_samples = {base::Bucket(
         ProfileMetrics::GaiaNameShareStatus::kNotShared,
         1)}},
    // Multiple profiles with various states.
    {.profiles = {{.active = false},
                  {.sync_consent = true, .supervised = true},
                  {.gaia_name = u"Name1", .active = false},
                  {.gaia_name = u"Name2"}},
     .expected_managed_profile_count = 1,
     .expected_sync_consent_profile_count = 1,
     .expected_active_profile_count = 2,
     .expected_name_share_samples = {base::Bucket(
         ProfileMetrics::GaiaNameShareStatus::kNotShared,
         2)}},
    // Duplicate Gaia name (non-managed).
    {.profiles = {{.gaia_name = u"Name"}, {.gaia_name = u"Name"}},
     .expected_active_profile_count = 2,
     .expected_name_share_samples = {base::Bucket(
         ProfileMetrics::GaiaNameShareStatus::kSharedNonManaged,
         1)}},
    // Duplicate Gaia name (managed).
    {.profiles = {{.gaia_name = u"Name"},
                  {.gaia_name = u"Name", .managed = true}},
     .expected_active_profile_count = 2,
     .expected_name_share_samples = {base::Bucket(
         ProfileMetrics::GaiaNameShareStatus::kSharedManaged,
         1)}},
    // Duplicate Gaia name: non-managed takes priority over managed.
    {.profiles = {{.gaia_name = u"Name"},
                  {.gaia_name = u"Name"},
                  {.gaia_name = u"Name", .managed = true}},
     .expected_active_profile_count = 3,
     .expected_name_share_samples = {base::Bucket(
         ProfileMetrics::GaiaNameShareStatus::kSharedNonManaged,
         1)}},
    // Multiple names with mixed sharing status.
    {.profiles = {{ProfileConfig()},
                  {.gaia_name = u"Name1"},
                  {.gaia_name = u"Name1"},
                  {.gaia_name = u"Name2", .managed = true},
                  {.gaia_name = u"Name3"},
                  {.gaia_name = u"Name2", .managed = true},
                  {.gaia_name = u"Name4"},
                  {.gaia_name = u"Name1"}},
     .expected_active_profile_count = 8,
     .expected_name_share_samples =
         {base::Bucket(ProfileMetrics::GaiaNameShareStatus::kNotShared, 2),
          base::Bucket(ProfileMetrics::GaiaNameShareStatus::kSharedNonManaged,
                       1),
          base::Bucket(ProfileMetrics::GaiaNameShareStatus::kSharedManaged,
                       1)}},
};

}  // namespace

class ProfileMetricsTest : public testing::TestWithParam<ProfileMetricsParam> {
 public:
  void CheckProfileCountHistogram(const base::HistogramTester& histogram_tester,
                                  const char* histogram,
                                  int expected_count) {
    histogram_tester.ExpectUniqueSample(histogram, expected_count, 1);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
};

TEST_P(ProfileMetricsTest, LogNumberOfProfiles) {
  ProfileAttributesStorage::RegisterPrefs(prefs_.registry());
  const ProfileMetricsParam test_param = GetParam();
  base::FilePath user_data_dir = base::FilePath(FILE_PATH_LITERAL("/Foo"));
  ProfileAttributesStorage storage(&prefs_, user_data_dir);

  int i = 0;
  for (const ProfileConfig& profile_config : test_param.profiles) {
    ProfileAttributesInitParams profile_init_params;
    base::FilePath profile_path = user_data_dir.AppendASCII(base::ToString(i));
    profile_init_params.profile_path = profile_path;
    profile_init_params.profile_name = u"profile name";
    storage.AddProfile(std::move(profile_init_params));
    ProfileAttributesEntry* entry =
        storage.GetProfileAttributesWithPath(profile_path);
    CHECK(entry);
    entry->SetGAIAGivenName(profile_config.gaia_name);
    if (profile_config.active) {
      entry->SetActiveTimeToNow();
    }
    if (profile_config.supervised) {
      entry->SetSupervisedUserId("supervised id");
    }
    if (profile_config.sync_consent) {
      entry->SetAuthInfo("gaia", u"email", true);
    }
    entry->SetUserAcceptedAccountManagement(profile_config.managed);
    ++i;
  }

  base::HistogramTester histogram_tester;
  ProfileMetrics::LogNumberOfProfiles(&storage);

  EXPECT_THAT(histogram_tester.GetAllSamples("Profile.GaiaNameShareStatus"),
              base::BucketsAre(test_param.expected_name_share_samples));
  histogram_tester.ExpectUniqueSample("Profile.NumberOfProfiles",
                                      test_param.profiles.size(), 1);
  histogram_tester.ExpectUniqueSample(
      "Profile.NumberOfSignedInProfiles",
      test_param.expected_sync_consent_profile_count, 1);
  histogram_tester.ExpectUniqueSample("Profile.NumberOfManagedProfiles",
                                      test_param.expected_managed_profile_count,
                                      1);
#if BUILDFLAG(IS_ANDROID)
  // All profiles are considered active on Android.
  histogram_tester.ExpectUniqueSample("Profile.NumberOfActiveProfiles",
                                      test_param.profiles.size(), 1);
  histogram_tester.ExpectUniqueSample("Profile.NumberOfUnusedProfiles", 0, 1);
#else
  histogram_tester.ExpectUniqueSample("Profile.NumberOfActiveProfiles",
                                      test_param.expected_active_profile_count,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Profile.NumberOfUnusedProfiles",
      test_param.profiles.size() - test_param.expected_active_profile_count, 1);
#endif  // BUILDFLAG(IS_ANDROID)
}

INSTANTIATE_TEST_SUITE_P(,
                         ProfileMetricsTest,
                         testing::ValuesIn(profile_metrics_test_params));
