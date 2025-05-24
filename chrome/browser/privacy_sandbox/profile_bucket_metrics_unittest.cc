// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/profile_bucket_metrics.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct RegularProfileTestData {
  // Inputs
  int profile_num;
  // Expectations
  std::string expected_bucket_name;
  privacy_sandbox::ProfileEnabledState expected_enabled_state;
  privacy_sandbox::ProfileEnabledState expected_disabled_state;
};

class ProfileBucketMetricsTest : public testing::Test {
 public:
  ProfileBucketMetricsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    Test::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
  }

  TestingProfileManager* testing_profile_manager() {
    return &testing_profile_manager_;
  }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
};

class ProfileBucketMetricsTestRegular
    : public ProfileBucketMetricsTest,
      public testing::WithParamInterface<RegularProfileTestData> {};

TEST_P(ProfileBucketMetricsTestRegular, RegularProfile) {
  int profile_num = GetParam().profile_num;
  // set up profile creation for multi-profile case
  for (int idx = 0; idx < profile_num - 1; idx++) {
    TestingProfile* foo = testing_profile_manager()->CreateTestingProfile(
        base::StrCat({"p", base::ToString(idx)}));
    privacy_sandbox::GetProfileBucketName(foo);
  }
  TestingProfile* profile = testing_profile_manager()->CreateTestingProfile(
      base::StrCat({"p", base::ToString(profile_num)}));

  ASSERT_EQ(privacy_sandbox::GetProfileBucketName(profile),
            GetParam().expected_bucket_name);
  ASSERT_EQ(privacy_sandbox::GetProfileEnabledState(profile, true).value(),
            GetParam().expected_enabled_state);
  ASSERT_EQ(privacy_sandbox::GetProfileEnabledState(profile, false).value(),
            GetParam().expected_disabled_state);
}

INSTANTIATE_TEST_SUITE_P(
    ProfileBucketMetricsTest_RegularProfiles,
    ProfileBucketMetricsTestRegular,
    testing::Values(
        RegularProfileTestData{
            1,
            "Profile_1",
            privacy_sandbox::ProfileEnabledState::kPSProfileOneEnabled,
            privacy_sandbox::ProfileEnabledState::kPSProfileOneDisabled,
        },
        RegularProfileTestData{
            2,
            "Profile_2",
            privacy_sandbox::ProfileEnabledState::kPSProfileTwoEnabled,
            privacy_sandbox::ProfileEnabledState::kPSProfileTwoDisabled,
        },
        RegularProfileTestData{
            3,
            "Profile_3",
            privacy_sandbox::ProfileEnabledState::kPSProfileThreeEnabled,
            privacy_sandbox::ProfileEnabledState::kPSProfileThreeDisabled,
        },
        RegularProfileTestData{
            4,
            "Profile_4",
            privacy_sandbox::ProfileEnabledState::kPSProfileFourEnabled,
            privacy_sandbox::ProfileEnabledState::kPSProfileFourDisabled,
        },
        RegularProfileTestData{
            5,
            "Profile_5",
            privacy_sandbox::ProfileEnabledState::kPSProfileFivePlusEnabled,
            privacy_sandbox::ProfileEnabledState::kPSProfileFivePlusDisabled,
        },
        RegularProfileTestData{
            6,
            "Profile_6",
            privacy_sandbox::ProfileEnabledState::kPSProfileFivePlusEnabled,
            privacy_sandbox::ProfileEnabledState::kPSProfileFivePlusDisabled,
        },
        RegularProfileTestData{
            7,
            "Profile_7",
            privacy_sandbox::ProfileEnabledState::kPSProfileFivePlusEnabled,
            privacy_sandbox::ProfileEnabledState::kPSProfileFivePlusDisabled,
        },
        RegularProfileTestData{
            11,
            "Profile_11+",
            privacy_sandbox::ProfileEnabledState::kPSProfileFivePlusEnabled,
            privacy_sandbox::ProfileEnabledState::kPSProfileFivePlusDisabled,
        }));

class ProfileBucketMetricsTestNonRegular
    : public ProfileBucketMetricsTest,
      public testing::WithParamInterface<profile_metrics::BrowserProfileType> {
};

TEST_P(ProfileBucketMetricsTestNonRegular,
       ValidatingBucketName_NonRegularProfiles) {
  TestingProfile* profile =
      testing_profile_manager()->CreateTestingProfile("foo");
  profile_metrics::SetBrowserProfileType(profile, GetParam());

  ASSERT_FALSE(
      privacy_sandbox::GetProfileEnabledState(profile, true).has_value());
  ASSERT_FALSE(
      privacy_sandbox::GetProfileEnabledState(profile, false).has_value());
  ASSERT_EQ(privacy_sandbox::GetProfileBucketName(profile), "");
}

INSTANTIATE_TEST_SUITE_P(
    ProfileBucketMetricsTest_NonRegularProfiles,
    ProfileBucketMetricsTestNonRegular,
    testing::Values(profile_metrics::BrowserProfileType::kGuest,
                    profile_metrics::BrowserProfileType::kIncognito));
}  // namespace
