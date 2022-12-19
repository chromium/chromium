// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_profile_observer.h"

#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace segmentation_platform {
namespace {
constexpr char kProfile1[] = "profile-1";
constexpr char kProfile2[] = "profile-2";

}  // namespace

class SegmentationPlatformProfileObserverTest : public testing::Test {
 public:
  SegmentationPlatformProfileObserverTest() = default;
  ~SegmentationPlatformProfileObserverTest() override = default;

 protected:
  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
  }

  void TearDown() override {
    segmentation_platform_profile_observer_.reset();
    testing_profile_manager_.reset();
  }

  void StartObservingProfiles() {
    segmentation_platform_profile_observer_ =
        std::make_unique<SegmentationPlatformProfileObserver>(
            &segmentation_platform_service_,
            testing_profile_manager_->profile_manager());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  MockSegmentationPlatformService segmentation_platform_service_;
  std::unique_ptr<SegmentationPlatformProfileObserver>
      segmentation_platform_profile_observer_;
};

TEST_F(SegmentationPlatformProfileObserverTest,
       StartWithDefaultProfileAndAddOneOrMoreOTRProfiles) {
  // Start with a default profile.
  TestingProfile* profile1 =
      testing_profile_manager_->CreateTestingProfile(kProfile1);

  // Start observing profiles. The service should be notified with metrics
  // enabled.
  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(true)).Times(1);
  StartObservingProfiles();

  // Create another profile. The signal collection should just continue, so no
  // further notification to the service.
  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(_)).Times(0);
  Profile* profile2 = testing_profile_manager_->CreateTestingProfile(kProfile2);

  // Start an OTR profile. The signal collection should stop.
  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(false)).Times(1);
  Profile* profile1_otr1 =
      profile1->GetPrimaryOTRProfile(/*create_if_needed*/ true);

  // Kill the OTR profile. The signal collection resumes.
  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(true)).Times(1);
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(profile1_otr1);

  // Start again another OTR profile. The signal collection should stop.
  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(false)).Times(1);
  Profile* profile1_otr2 =
      profile1->GetPrimaryOTRProfile(/*create_if_needed*/ true);

  // Add one more OTR for profile2.
  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(_)).Times(0);
  Profile* profile2_otr1 =
      profile2->GetPrimaryOTRProfile(/*create_if_needed*/ true);

  // Add a secondary OTR for profile1.
  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(_)).Times(0);
  Profile* profile1_otr2_secondary = profile1->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  // Start killing the OTR profiles one by one. The signal collection resumes
  // only after the last one is killed.
  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(_)).Times(0);
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(profile1_otr2_secondary);

  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(_)).Times(0);
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(profile1_otr2);

  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(true)).Times(1);
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(profile2_otr1);

  // Cleanup profiles.
  testing_profile_manager_->DeleteTestingProfile(kProfile1);
  testing_profile_manager_->DeleteTestingProfile(kProfile2);
}

TEST_F(SegmentationPlatformProfileObserverTest, StartWithOTRProfile) {
  // Start with a default profile and an OTR profile.
  TestingProfile* profile =
      testing_profile_manager_->CreateTestingProfile(kProfile1);
  Profile* otr = profile->GetPrimaryOTRProfile(/*create_if_needed*/ true);

  // Start observing profiles. The service should be notified with metrics
  // disabled.
  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(false)).Times(1);
  StartObservingProfiles();

  // Kill the OTR profile. The signal collection resumes.
  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(true)).Times(1);
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(otr);

  // Cleanup profiles.
  EXPECT_CALL(segmentation_platform_service_, EnableMetrics(_)).Times(0);
  testing_profile_manager_->DeleteTestingProfile(kProfile1);
}

}  // namespace segmentation_platform
