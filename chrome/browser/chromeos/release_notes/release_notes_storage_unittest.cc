// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/release_notes/release_notes_storage.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ReleaseNotesStorageTest : public testing::Test,
                                public testing::WithParamInterface<bool> {
 protected:
  ReleaseNotesStorageTest()
      : user_manager_(new FakeChromeUserManager()),
        scoped_user_manager_(
            std::unique_ptr<FakeChromeUserManager>(user_manager_)) {}
  ~ReleaseNotesStorageTest() override {}

  std::unique_ptr<Profile> CreateProfile(std::string email) {
    AccountId account_id_ = AccountId::FromUserEmailGaiaId(email, "12345");
    user_manager_->AddUser(account_id_);
    TestingProfile::Builder builder;
    builder.SetProfileName(email);
    return builder.Build();
  }

  std::unique_ptr<Profile> SetupStandardEnvironmentAndProfile(std::string email,
                                                              bool is_managed) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kReleaseNotesNotification,
                              chromeos::features::
                                  kReleaseNotesNotificationAllChannels,
                              chromeos::features::kReleaseNotesSuggestionChip},
        /*disabled_features=*/{});
    std::unique_ptr<Profile> profile = CreateProfile(email);
    profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
        is_managed);
    return profile;
  }

  FakeChromeUserManager* user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ReleaseNotesStorageTest);
};

// Release notes are not shown for profiles that have been created in this
// milestone.
TEST_F(ReleaseNotesStorageTest, ShouldNotShowReleaseNotesOOBE) {
  std::unique_ptr<Profile> profile =
      SetupStandardEnvironmentAndProfile("test@gmail.com", false);
  profile.get()->GetPrefs()->SetString(prefs::kProfileCreatedByVersion,
                                       version_info::GetVersion().GetString());
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());

  EXPECT_EQ(false, release_notes_storage->ShouldNotify());
}

// Release notes are shown for profiles that have been created in an earlier
// version of chrome.
TEST_F(ReleaseNotesStorageTest, ShouldShowReleaseNotesOldProfile) {
  std::unique_ptr<Profile> profile =
      SetupStandardEnvironmentAndProfile("test@gmail.com", false);
  profile.get()->GetPrefs()->SetString(prefs::kProfileCreatedByVersion,
                                       "20.0.0.0");
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());

  EXPECT_EQ(true, release_notes_storage->ShouldNotify());
}

// We have previously seen another notification on an earlier chrome version,
// release notes should be shown.
TEST_F(ReleaseNotesStorageTest, ShouldShowReleaseNotes) {
  std::unique_ptr<Profile> profile =
      SetupStandardEnvironmentAndProfile("test@gmail.com", false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        20);

  EXPECT_EQ(true, release_notes_storage->ShouldNotify());
}

// We have previously seen another notification on M91, there have been no
// new release notes since then so notification should not be shown.
TEST_F(ReleaseNotesStorageTest, ShouldNotShowReleaseNotesIf91Seen) {
  std::unique_ptr<Profile> profile =
      SetupStandardEnvironmentAndProfile("test@gmail.com", false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        91);

  EXPECT_EQ(false, release_notes_storage->ShouldNotify());
}

// Release notes ShouldNotify is false after being shown once.
TEST_F(ReleaseNotesStorageTest, ReleaseNotesShouldOnlyBeNotifiedOnce) {
  std::unique_ptr<Profile> profile =
      SetupStandardEnvironmentAndProfile("test@gmail.com", false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        20);
  ASSERT_EQ(true, release_notes_storage->ShouldNotify());

  release_notes_storage->MarkNotificationShown();

  EXPECT_NE(20, profile.get()->GetPrefs()->GetInteger(
                    prefs::kReleaseNotesLastShownMilestone));
  EXPECT_EQ(false, release_notes_storage->ShouldNotify());
}

TEST_F(ReleaseNotesStorageTest, ShouldNotShowReleaseNotesForManagedProfile) {
  std::unique_ptr<Profile> profile =
      SetupStandardEnvironmentAndProfile("test@company.com", true);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        20);

  EXPECT_EQ(false, release_notes_storage->ShouldNotify());
}

TEST_F(ReleaseNotesStorageTest, ShouldShowReleaseNotesForGoogler) {
  std::unique_ptr<Profile> profile =
      SetupStandardEnvironmentAndProfile("test@google.com", true);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        20);

  EXPECT_EQ(true, release_notes_storage->ShouldNotify());
}

TEST_F(ReleaseNotesStorageTest, ShouldNotShowReleaseNotesIfFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      chromeos::features::kReleaseNotesNotification);
  std::unique_ptr<Profile> profile = CreateProfile("test@gmail.com");
  profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(prefs::kReleaseNotesLastShownMilestone,
                                        20);

  EXPECT_EQ(false, release_notes_storage->ShouldNotify());
}

// Tests that when kReleaseNotesSuggestionChipTimesLeftToShow is 0,
// ReleaseNotesStorage::ShouldShowSuggestionChip returns false.
TEST_F(ReleaseNotesStorageTest, DoesNotShowReleaseNotesSuggestionChip) {
  std::unique_ptr<Profile> profile =
      SetupStandardEnvironmentAndProfile("test@gmail.com", false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);

  EXPECT_EQ(false, release_notes_storage->ShouldShowSuggestionChip());
}

// Tests that when kReleaseNotesSuggestionChipTimesLeftToShow is greater than 0,
// ReleaseNotesStorage::ShouldShowSuggestionChip returns true, and when
// decreased the method returns false again.
TEST_F(ReleaseNotesStorageTest, ShowReleaseNotesSuggestionChip) {
  std::unique_ptr<Profile> profile =
      SetupStandardEnvironmentAndProfile("test@gmail.com", false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());
  profile.get()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 1);
  ASSERT_EQ(true, release_notes_storage->ShouldShowSuggestionChip());

  release_notes_storage->DecreaseTimesLeftToShowSuggestionChip();

  EXPECT_EQ(0, profile.get()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
  EXPECT_EQ(false, release_notes_storage->ShouldShowSuggestionChip());
}

// Tests that when we mark a notification as shown, we also show the suggestion
// chip.
TEST_F(ReleaseNotesStorageTest, ShowSuggestionChipWhenNotificationShown) {
  std::unique_ptr<Profile> profile =
      SetupStandardEnvironmentAndProfile("test@gmail.com", false);
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile.get());

  release_notes_storage->MarkNotificationShown();

  EXPECT_EQ(3, profile.get()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
  EXPECT_EQ(true, release_notes_storage->ShouldShowSuggestionChip());
}

}  // namespace chromeos
