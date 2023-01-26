// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_profile_manager.h"

#include <string>

#include "base/strings/string_piece.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kidschromemanagement_messages.pb.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::kids_chrome_management::FamilyMember;

class KidsProfileManagerTest : public ::testing::Test {
 protected:
  PrefService* pref_service() { return profile_.GetTestingPrefService(); }

  FamilyMember CreateFamilyMember(base::StringPiece display_name,
                                  base::StringPiece email,
                                  base::StringPiece user_id,
                                  base::StringPiece profile_image_url,
                                  base::StringPiece profile_url) {
    FamilyMember member;
    *member.mutable_profile()->mutable_display_name() =
        std::string(display_name);
    *member.mutable_profile()->mutable_email() = std::string(email);
    *member.mutable_user_id() = std::string(user_id);
    *member.mutable_profile()->mutable_profile_image_url() =
        std::string(profile_image_url);
    *member.mutable_profile()->mutable_profile_url() = std::string(profile_url);
    return member;
  }

  content::BrowserTaskEnvironment
      task_environment_;  // The test must run on Chrome_UIThread.
  TestingProfile profile_;
};

// For a supervised profile, checks if setting it to child account will just
// confirm the status.
TEST_F(KidsProfileManagerTest, SetChildAccountStatusOnSupervisedProfile) {
  KidsProfileManager under_test(*pref_service(), profile_);

  profile_.SetIsSupervisedProfile();
  ASSERT_TRUE(profile_.IsChild());
  ASSERT_FALSE(under_test.IsChildAccountStatusKnown());

  under_test.UpdateChildAccountStatus(true);

  EXPECT_TRUE(under_test.IsChildAccountStatusKnown());
}

// For an unsupervised profile, checks if setting it to child account sets the
// right status and whether other associated supervised values are set.
TEST_F(KidsProfileManagerTest, SetChildAccountStatusOnUnsupervisedProfile) {
  KidsProfileManager under_test(*pref_service(), profile_);

  ASSERT_FALSE(profile_.IsChild());
  ASSERT_FALSE(under_test.IsChildAccountStatusKnown());

  under_test.UpdateChildAccountStatus(true);

  EXPECT_TRUE(under_test.IsChildAccountStatusKnown());
  EXPECT_EQ(pref_service()->GetString(prefs::kSupervisedUserId),
            supervised_users::kChildAccountSUID);
}

// For a supervised profile, checks if unsetting it as child account clears
// relevant fields.
TEST_F(KidsProfileManagerTest, UnsetChildAccountStatusOnSupervisedProfile) {
  KidsProfileManager under_test(*pref_service(), profile_);

  profile_.SetIsSupervisedProfile();
  ASSERT_TRUE(profile_.IsChild());
  ASSERT_FALSE(under_test.IsChildAccountStatusKnown());

  under_test.UpdateChildAccountStatus(false);

  EXPECT_TRUE(under_test.IsChildAccountStatusKnown());
  EXPECT_EQ(pref_service()->HasPrefPath(prefs::kSupervisedUserId), false);

  EXPECT_EQ(pref_service()->HasPrefPath(prefs::kSupervisedUserCustodianName),
            false);
  EXPECT_EQ(pref_service()->HasPrefPath(prefs::kSupervisedUserCustodianEmail),
            false);
  EXPECT_EQ(pref_service()->HasPrefPath(
                prefs::kSupervisedUserCustodianObfuscatedGaiaId),
            false);
  EXPECT_EQ(
      pref_service()->HasPrefPath(prefs::kSupervisedUserCustodianProfileURL),
      false);
  EXPECT_EQ(pref_service()->HasPrefPath(
                prefs::kSupervisedUserCustodianProfileImageURL),
            false);

  EXPECT_EQ(
      pref_service()->HasPrefPath(prefs::kSupervisedUserSecondCustodianName),
      false);
  EXPECT_EQ(
      pref_service()->HasPrefPath(prefs::kSupervisedUserSecondCustodianEmail),
      false);
  EXPECT_EQ(pref_service()->HasPrefPath(
                prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId),
            false);
  EXPECT_EQ(pref_service()->HasPrefPath(
                prefs::kSupervisedUserSecondCustodianProfileURL),
            false);
  EXPECT_EQ(pref_service()->HasPrefPath(
                prefs::kSupervisedUserSecondCustodianProfileImageURL),
            false);
}

// For an usupervised profile, checks if unsetting it as child account just
// confirms its status.
TEST_F(KidsProfileManagerTest, UnsetChildAccountStatusOnUnsupervisedProfile) {
  KidsProfileManager under_test(*pref_service(), profile_);

  ASSERT_FALSE(profile_.IsChild());
  ASSERT_FALSE(under_test.IsChildAccountStatusKnown());

  under_test.UpdateChildAccountStatus(false);

  EXPECT_TRUE(under_test.IsChildAccountStatusKnown());
  EXPECT_EQ(pref_service()->HasPrefPath(prefs::kSupervisedUserId), false);
}

// Checks if primary custodian's fields are properly set and unset with
// UpdateChildAccountStatus operation.
TEST_F(KidsProfileManagerTest, SetPrimaryCustodian) {
  KidsProfileManager under_test(*pref_service(), profile_);

  profile_
      .SetIsSupervisedProfile();  // Then it will be possible to clear values.

  FamilyMember member = CreateFamilyMember("display_name", "email", "user_id",
                                           "profile_image_url", "profile_url");

  under_test.SetFirstCustodian(member);

  EXPECT_EQ(pref_service()->GetString(prefs::kSupervisedUserCustodianName),
            "display_name");
  EXPECT_EQ(pref_service()->GetString(prefs::kSupervisedUserCustodianEmail),
            "email");
  EXPECT_EQ(pref_service()->GetString(
                prefs::kSupervisedUserCustodianObfuscatedGaiaId),
            "user_id");
  EXPECT_EQ(
      pref_service()->GetString(prefs::kSupervisedUserCustodianProfileURL),
      "profile_url");
  EXPECT_EQ(
      pref_service()->GetString(prefs::kSupervisedUserCustodianProfileImageURL),
      "profile_image_url");

  under_test.UpdateChildAccountStatus(false);

  EXPECT_EQ(pref_service()->HasPrefPath(prefs::kSupervisedUserCustodianName),
            false);
  EXPECT_EQ(pref_service()->HasPrefPath(prefs::kSupervisedUserCustodianEmail),
            false);
  EXPECT_EQ(pref_service()->HasPrefPath(
                prefs::kSupervisedUserCustodianObfuscatedGaiaId),
            false);
  EXPECT_EQ(
      pref_service()->HasPrefPath(prefs::kSupervisedUserCustodianProfileURL),
      false);
  EXPECT_EQ(pref_service()->HasPrefPath(
                prefs::kSupervisedUserCustodianProfileImageURL),
            false);
}

// Checks if secondary custodian's fields are properly set and unset with
// UpdateChildAccountStatus operation.
TEST_F(KidsProfileManagerTest, SetSecondaryCustodian) {
  KidsProfileManager under_test(*pref_service(), profile_);

  profile_
      .SetIsSupervisedProfile();  // Then it will be possible to clear values.

  FamilyMember member = CreateFamilyMember("display_name", "email", "user_id",
                                           "profile_image_url", "profile_url");

  under_test.SetSecondCustodian(member);

  EXPECT_EQ(
      pref_service()->GetString(prefs::kSupervisedUserSecondCustodianName),
      "display_name");
  EXPECT_EQ(
      pref_service()->GetString(prefs::kSupervisedUserSecondCustodianEmail),
      "email");
  EXPECT_EQ(pref_service()->GetString(
                prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId),
            "user_id");
  EXPECT_EQ(pref_service()->GetString(
                prefs::kSupervisedUserSecondCustodianProfileURL),
            "profile_url");
  EXPECT_EQ(pref_service()->GetString(
                prefs::kSupervisedUserSecondCustodianProfileImageURL),
            "profile_image_url");

  under_test.UpdateChildAccountStatus(false);

  EXPECT_EQ(
      pref_service()->HasPrefPath(prefs::kSupervisedUserSecondCustodianName),
      false);
  EXPECT_EQ(
      pref_service()->HasPrefPath(prefs::kSupervisedUserSecondCustodianEmail),
      false);
  EXPECT_EQ(pref_service()->HasPrefPath(
                prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId),
            false);
  EXPECT_EQ(pref_service()->HasPrefPath(
                prefs::kSupervisedUserSecondCustodianProfileURL),
            false);
  EXPECT_EQ(pref_service()->HasPrefPath(
                prefs::kSupervisedUserSecondCustodianProfileImageURL),
            false);
}

}  // namespace
