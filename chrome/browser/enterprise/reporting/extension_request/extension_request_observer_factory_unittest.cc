// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer_factory.h"

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {
constexpr char kProfile1[] = "profile-1";
constexpr char kProfile2[] = "profile-2";

class ExtensionRequestObserverFactoryTest : public ::testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }
  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
};

TEST_F(ExtensionRequestObserverFactoryTest, LoadSpecificProfile) {
  TestingProfile* profile = profile_manager()->CreateTestingProfile(kProfile1);
  ExtensionRequestObserverFactory factory_(profile);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());
}

TEST_F(ExtensionRequestObserverFactoryTest, LoadSpecificProfile_AddProfile) {
  TestingProfile* profile1 = profile_manager()->CreateTestingProfile(kProfile1);
  ExtensionRequestObserverFactory factory_(profile1);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile1));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());

  // The change of profile manager won't impact the loaded profile number in the
  // factory if there is only one specific profile is loaded in the constructor.
  TestingProfile* profile2 = profile_manager()->CreateTestingProfile(kProfile2);
  EXPECT_FALSE(factory_.GetObserverByProfileForTesting(profile2));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());
}

TEST_F(ExtensionRequestObserverFactoryTest,
       LoadSpecificProfile_OnProfileAdded_OnProfileDeletion) {
  TestingProfile* profile1 = profile_manager()->CreateTestingProfile(kProfile1);
  ExtensionRequestObserverFactory factory_(profile1);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile1));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());

  // Nothing happens if the profile to remove is not provided in the
  // constructor.
  TestingProfile* profile2 = profile_manager()->CreateTestingProfile(kProfile2);
  factory_.OnProfileMarkedForPermanentDeletion(profile2);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile1));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());

  // The specific observer is removed.
  factory_.OnProfileMarkedForPermanentDeletion(profile1);
  EXPECT_FALSE(factory_.GetObserverByProfileForTesting(profile1));
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());

  // Nothing happens if the profile to add is not provided in the constructor.
  factory_.OnProfileAdded(profile2);
  EXPECT_FALSE(factory_.GetObserverByProfileForTesting(profile2));
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());

  // The specific observer can be added back.
  factory_.OnProfileAdded(profile1);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile1));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());

  // Nothing happens if the specific observer is added twice.
  factory_.OnProfileAdded(profile1);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile1));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());
}

TEST_F(ExtensionRequestObserverFactoryTest, OnProfileWillBeDestroyed) {
  TestingProfile* profile = profile_manager()->CreateTestingProfile(kProfile1);
  ExtensionRequestObserverFactory factory_(profile);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());

  // If the profile to be destroyed is not same as the one assigned in the
  // constructor. Nothing will happen.
  TestingProfile* profile2 = profile_manager()->CreateTestingProfile(kProfile2);
  factory_.OnProfileWillBeDestroyed(profile2);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());

  // If the profile to be destroyed is same as the one assigned in the
  // constructor. The corresponding observer will be removed.
  factory_.OnProfileWillBeDestroyed(profile);
  EXPECT_FALSE(factory_.GetObserverByProfileForTesting(profile));
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());
}

TEST_F(ExtensionRequestObserverFactoryTest, LoadExistProfile) {
  TestingProfile* profile = profile_manager()->CreateTestingProfile(kProfile1);
  ExtensionRequestObserverFactory factory_;
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());
}

TEST_F(ExtensionRequestObserverFactoryTest, AddProfile) {
  ExtensionRequestObserverFactory factory_;
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());

  TestingProfile* profile1 = profile_manager()->CreateTestingProfile(kProfile1);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile1));
  EXPECT_EQ(1, factory_.GetNumberOfObserversForTesting());

  TestingProfile* profile2 = profile_manager()->CreateTestingProfile(kProfile2);
  EXPECT_TRUE(factory_.GetObserverByProfileForTesting(profile2));
  EXPECT_EQ(2, factory_.GetNumberOfObserversForTesting());
}

class GuestExtensionRequestObserverFactoryTest
    : public ExtensionRequestObserverFactoryTest,
      public ::testing::WithParamInterface<bool> {
 public:
  GuestExtensionRequestObserverFactoryTest() {
    TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
        scoped_feature_list_, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(GuestExtensionRequestObserverFactoryTest,
       NoObserverForSystemAndGuestProfile) {
  ExtensionRequestObserverFactory factory_;
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());

  TestingProfile* guest_profile = profile_manager()->CreateGuestProfile();
  EXPECT_FALSE(factory_.GetObserverByProfileForTesting(guest_profile));
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());

  TestingProfile* system_profile = profile_manager()->CreateSystemProfile();
  EXPECT_FALSE(factory_.GetObserverByProfileForTesting(system_profile));
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());
}

INSTANTIATE_TEST_SUITE_P(AllGuestTypes,
                         GuestExtensionRequestObserverFactoryTest,
                         /*is_ephemeral=*/testing::Bool());

}  // namespace enterprise_reporting
