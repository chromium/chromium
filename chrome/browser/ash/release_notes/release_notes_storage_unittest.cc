// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/release_notes/release_notes_storage.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int CurrentMilestone() {
  return version_info::GetVersion().components()[0];
}

}  // namespace

namespace ash {

class ReleaseNotesStorageTest : public testing::Test,
                                public testing::WithParamInterface<bool> {
 public:
  ReleaseNotesStorageTest(const ReleaseNotesStorageTest&) = delete;
  ReleaseNotesStorageTest& operator=(const ReleaseNotesStorageTest&) = delete;

 protected:
  ReleaseNotesStorageTest() = default;
  ~ReleaseNotesStorageTest() override = default;

  void SetUpProfile() {
    TestingProfile::Builder builder;
    if (is_guest_) {
      builder.SetGuestSession();
    } else {
      AccountId account_id_ = AccountId::FromUserEmailGaiaId(email_, "12345");
      user_manager_->AddUser(account_id_);
      builder.SetProfileName(email_);

      builder.OverridePolicyConnectorIsManagedForTesting(is_managed_);
      if (is_ephemeral_) {
        // Enabling ephemeral users passes the |IsEphemeralUserProfile| check.
        user_manager_->SetEphemeralModeConfig(
            user_manager::UserManager::EphemeralModeConfig(
                /* included_by_default= */ true,
                /* include_list= */ std::vector<AccountId>{},
                /* exclude_list= */ std::vector<AccountId>{}));
      } else if (is_unicorn_) {
        user_manager_->set_current_user_child(true);
        builder.SetIsSupervisedProfile();
      }
    }
    profile_ = builder.Build();
    release_notes_storage_ =
        std::make_unique<ReleaseNotesStorage>(profile_.get());
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kReleaseNotesNotificationAllChannels);
  }

  user_manager::TypedScopedUserManager<FakeChromeUserManager> user_manager_{
      std::make_unique<FakeChromeUserManager>()};
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage_;

  // Data members for SetUpProfile().
  std::string email_ = "test@gmail.com";
  bool is_guest_ = false;
  bool is_managed_ = false;
  bool is_ephemeral_ = false;
  bool is_unicorn_ = false;
};

// Release notes are not shown for profiles that have been created in this
// milestone.
TEST_F(ReleaseNotesStorageTest, ShouldNotShowReleaseNotesOOBE) {
  SetUpProfile();
  profile_.get()->GetPrefs()->SetString(prefs::kProfileCreatedByVersion,
                                        version_info::GetVersion().GetString());

  EXPECT_EQ(false, release_notes_storage_->ShouldNotify());
}

// Release notes are shown for profiles that have been created in an earlier
// version of chrome.
TEST_F(ReleaseNotesStorageTest, ShouldShowReleaseNotesOldProfile) {
  SetUpProfile();
  profile_.get()->GetPrefs()->SetString(prefs::kProfileCreatedByVersion,
                                        "20.0.0.0");

  EXPECT_EQ(true, release_notes_storage_->ShouldNotify());
}

// We have previously seen another notification on an earlier chrome version,
// release notes should be shown.
TEST_F(ReleaseNotesStorageTest, ShouldShowReleaseNotes) {
  SetUpProfile();
  profile_.get()->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone, 20);

  EXPECT_EQ(true, release_notes_storage_->ShouldNotify());
}

// We have already seen the notification on the current chrome version.
TEST_F(ReleaseNotesStorageTest,
       ShouldNotShowReleaseNotesIfShownInCurrentChromeVersion) {
  SetUpProfile();
  profile_.get()->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone, CurrentMilestone());

  EXPECT_EQ(false, release_notes_storage_->ShouldNotify());
}

// Release notes ShouldNotify is false after being shown once.
TEST_F(ReleaseNotesStorageTest, ReleaseNotesShouldOnlyBeNotifiedOnce) {
  SetUpProfile();

  ASSERT_EQ(true, release_notes_storage_->ShouldNotify());

  release_notes_storage_->MarkNotificationShown();

  EXPECT_NE(20, profile_.get()->GetPrefs()->GetInteger(
                    prefs::kHelpAppNotificationLastShownMilestone));
  EXPECT_EQ(false, release_notes_storage_->ShouldNotify());
}

TEST_F(ReleaseNotesStorageTest, ShouldNotShowReleaseNotesForEphemeralProfile) {
  is_ephemeral_ = true;
  SetUpProfile();
  profile_->ScopedCrosSettingsTestHelper()
      ->InstallAttributes()
      ->SetCloudManaged("test_domain", "FAKE_DEVICE_ID");

  EXPECT_EQ(false, release_notes_storage_->ShouldNotify());
}

TEST_F(ReleaseNotesStorageTest, ShouldNotShowReleaseNotesForGuestProfile) {
  is_guest_ = true;
  SetUpProfile();

  EXPECT_EQ(false, release_notes_storage_->ShouldNotify());
}

TEST_F(ReleaseNotesStorageTest, ShouldNotShowReleaseNotesForManagedProfile) {
  is_managed_ = true;
  SetUpProfile();

  EXPECT_EQ(false, release_notes_storage_->ShouldNotify());
}

TEST_F(ReleaseNotesStorageTest, ShouldShowReleaseNotesForGoogler) {
  is_managed_ = true;
  email_ = "test@google.com";
  SetUpProfile();

  EXPECT_EQ(true, release_notes_storage_->ShouldNotify());
}

TEST_F(ReleaseNotesStorageTest, ShouldShowReleaseNotesForUnicornProfile) {
  is_managed_ = true;
  is_unicorn_ = true;
  SetUpProfile();

  EXPECT_EQ(true, release_notes_storage_->ShouldNotify());
}

// Tests that when kReleaseNotesSuggestionChipTimesLeftToShow is 0,
// ReleaseNotesStorage::ShouldShowSuggestionChip returns false.
TEST_F(ReleaseNotesStorageTest, DoesNotShowReleaseNotesSuggestionChip) {
  SetUpProfile();
  profile_.get()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 0);

  EXPECT_EQ(false, release_notes_storage_->ShouldShowSuggestionChip());
}

// Tests that when kReleaseNotesSuggestionChipTimesLeftToShow is greater than 0,
// ReleaseNotesStorage::ShouldShowSuggestionChip returns true, and when
// decreased the method returns false again.
TEST_F(ReleaseNotesStorageTest, ShowReleaseNotesSuggestionChip) {
  SetUpProfile();
  profile_.get()->GetPrefs()->SetInteger(
      prefs::kReleaseNotesSuggestionChipTimesLeftToShow, 1);

  ASSERT_EQ(true, release_notes_storage_->ShouldShowSuggestionChip());

  release_notes_storage_->DecreaseTimesLeftToShowSuggestionChip();

  EXPECT_EQ(0, profile_.get()->GetPrefs()->GetInteger(
                   prefs::kReleaseNotesSuggestionChipTimesLeftToShow));
  EXPECT_EQ(false, release_notes_storage_->ShouldShowSuggestionChip());
}

}  // namespace ash
