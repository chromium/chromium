// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/guest_profile_creation_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class GuestProfileCreationLoggerTest : public ::testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  TestingProfileManager& profile_manager() { return profile_manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
};

TEST_F(GuestProfileCreationLoggerTest, SingleParentGuestCreation) {
  TestingProfile* guest_parent = profile_manager().CreateGuestProfile();
  base::HistogramTester histograms;

  // Recording the parent creation. It should be reported on the histogram.
  profile::RecordGuestParentCreation(guest_parent);
  EXPECT_THAT(histograms.GetAllSamples("Profile.Guest.TypeCreated"),
              ::testing::ElementsAre(base::Bucket(0, 1)));

  // Recording the first child creation. It should be reported on the histogram.
  Profile* guest_child =
      guest_parent->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  profile::MaybeRecordGuestChildCreation(guest_child);
  EXPECT_THAT(histograms.GetAllSamples("Profile.Guest.TypeCreated"),
              ::testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));

  guest_parent->DestroyOffTheRecordProfile(guest_child);
  ASSERT_FALSE(guest_parent->HasAnyOffTheRecordProfile());
  TestingProfile::Builder off_the_record_builder;
  off_the_record_builder.SetGuestSession();
  Profile* guest_child_2 = off_the_record_builder.BuildIncognito(guest_parent);

  // Recording the second child creation. It should NOT be reported.
  profile::MaybeRecordGuestChildCreation(guest_child_2);
  EXPECT_THAT(histograms.GetAllSamples("Profile.Guest.TypeCreated"),
              ::testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}

TEST_F(GuestProfileCreationLoggerTest, DualParentGuestCreation) {
  Profile* guest_parent = profile_manager().CreateGuestProfile();
  base::HistogramTester histograms;

  // Recording the parent creation. It should be reported on the histogram.
  profile::RecordGuestParentCreation(guest_parent);
  EXPECT_THAT(histograms.GetAllSamples("Profile.Guest.TypeCreated"),
              ::testing::ElementsAre(base::Bucket(0, 1)));

  // Recording the first child creation. It should be reported on the histogram.
  Profile* guest_child =
      guest_parent->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  profile::MaybeRecordGuestChildCreation(guest_child);
  EXPECT_THAT(histograms.GetAllSamples("Profile.Guest.TypeCreated"),
              ::testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));

  profile_manager().DeleteGuestProfile();
  Profile* guest_parent_2 = profile_manager().CreateGuestProfile();

  // Recording the second parent creation. It should be reported.
  profile::RecordGuestParentCreation(guest_parent_2);
  EXPECT_THAT(histograms.GetAllSamples("Profile.Guest.TypeCreated"),
              ::testing::ElementsAre(base::Bucket(0, 2), base::Bucket(1, 1)));

  // Recording the second child creation. It should be reported.
  Profile* guest_child_2 =
      guest_parent_2->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  profile::MaybeRecordGuestChildCreation(guest_child_2);
  EXPECT_THAT(histograms.GetAllSamples("Profile.Guest.TypeCreated"),
              ::testing::ElementsAre(base::Bucket(0, 2), base::Bucket(1, 2)));
}

}  // namespace
