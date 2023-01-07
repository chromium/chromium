// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer_factory.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {
namespace {
constexpr char kProfile1[] = "profile-1";
constexpr char kProfile2[] = "profile-2";
constexpr char kProfile3[] = "profile-3";
}  // namespace

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

TEST_F(ExtensionRequestObserverFactoryTest,
       NoObserverForSystemAndGuestProfile) {
  ExtensionRequestObserverFactory factory_;
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());

  TestingProfile* guest_profile = profile_manager()->CreateGuestProfile();
  EXPECT_FALSE(factory_.GetObserverByProfileForTesting(guest_profile));
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  TestingProfile* system_profile = profile_manager()->CreateSystemProfile();
  EXPECT_FALSE(factory_.GetObserverByProfileForTesting(system_profile));
  EXPECT_EQ(0, factory_.GetNumberOfObserversForTesting());
#endif
}

TEST_F(ExtensionRequestObserverFactoryTest, ReportEnabledAndDisabled) {
  ExtensionRequestObserverFactory factory_;
  TestingProfile* profile1 = profile_manager()->CreateTestingProfile(kProfile1);
  EXPECT_FALSE(
      factory_.GetObserverByProfileForTesting(profile1)->IsReportEnabled());

  factory_.EnableReport(base::DoNothingAs<void(Profile*)>());
  EXPECT_TRUE(factory_.IsReportEnabled());
  EXPECT_TRUE(
      factory_.GetObserverByProfileForTesting(profile1)->IsReportEnabled());

  TestingProfile* profile2 = profile_manager()->CreateTestingProfile(kProfile2);
  EXPECT_TRUE(
      factory_.GetObserverByProfileForTesting(profile1)->IsReportEnabled());
  EXPECT_TRUE(
      factory_.GetObserverByProfileForTesting(profile2)->IsReportEnabled());

  factory_.DisableReport();
  EXPECT_FALSE(factory_.IsReportEnabled());
  EXPECT_FALSE(
      factory_.GetObserverByProfileForTesting(profile1)->IsReportEnabled());
  EXPECT_FALSE(
      factory_.GetObserverByProfileForTesting(profile2)->IsReportEnabled());

  TestingProfile* profile3 = profile_manager()->CreateTestingProfile(kProfile3);
  EXPECT_FALSE(
      factory_.GetObserverByProfileForTesting(profile1)->IsReportEnabled());
  EXPECT_FALSE(
      factory_.GetObserverByProfileForTesting(profile2)->IsReportEnabled());
  EXPECT_FALSE(
      factory_.GetObserverByProfileForTesting(profile3)->IsReportEnabled());
}

}  // namespace enterprise_reporting
