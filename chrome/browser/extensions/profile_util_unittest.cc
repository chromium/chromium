// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/profile_util.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service_user_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using extensions::profile_util::ProfileCanUseNonComponentExtensions;

namespace extensions {

class ProfileUtilUnitTest : public ExtensionServiceUserTestBase {
 public:
  void SetUp() override {
    ExtensionServiceUserTestBase::SetUp();
    InitializeEmptyExtensionService();
  }
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfileUtilUnitTest, ProfileCanUseNonComponentExtensions_RegularUser) {
  ASSERT_NO_FATAL_FAILURE(LoginChromeOSAshUser(
      GetFakeUserManager()->AddUser(account_id_), account_id_));

  EXPECT_TRUE(ProfileCanUseNonComponentExtensions(testing_profile()));
}

TEST_F(ProfileUtilUnitTest, ProfileCanUseNonComponentExtensions_ChildUser) {
  const user_manager::User* user =
      GetFakeUserManager()->AddChildUser(account_id_);
  ASSERT_NO_FATAL_FAILURE(LoginChromeOSAshUser(user, account_id_));

  EXPECT_TRUE(ProfileCanUseNonComponentExtensions(testing_profile()));
}

TEST_F(ProfileUtilUnitTest, ProfileCannotUseNonComponentExtensions_GuestUser) {
  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(/*is_guest=*/true));

  EXPECT_FALSE(ProfileCanUseNonComponentExtensions(testing_profile()));
}

// TODO(crbug.com/40878021): Test a signin, lockscreen, or lockscreen app
// profile. `FakeChromeUserManager` doesn't have one currently. Worst case could
// mock the `Profile` path to do this.
TEST_F(ProfileUtilUnitTest,
       DISABLED_ProfileCannotUseNonComponentExtensions_NotAUserProfile) {}

TEST_F(ProfileUtilUnitTest,
       ProfileCannotUseNonComponentExtensions_KioskAppUser) {
  ASSERT_NO_FATAL_FAILURE(LoginChromeOSAshUser(
      GetFakeUserManager()->AddKioskAppUser(account_id_), account_id_));

  EXPECT_FALSE(ProfileCanUseNonComponentExtensions(testing_profile()));
}

TEST_F(ProfileUtilUnitTest,
       ProfileCannotUseNonComponentExtensions_WebKioskAppUser) {
  ASSERT_NO_FATAL_FAILURE(LoginChromeOSAshUser(
      GetFakeUserManager()->AddWebKioskAppUser(account_id_), account_id_));

  EXPECT_FALSE(ProfileCanUseNonComponentExtensions(testing_profile()));
}

TEST_F(ProfileUtilUnitTest, ProfileCannotUseNonComponentExtensions_PublicUser) {
  ASSERT_NO_FATAL_FAILURE(LoginChromeOSAshUser(
      GetFakeUserManager()->AddPublicAccountUser(account_id_), account_id_));

  EXPECT_FALSE(ProfileCanUseNonComponentExtensions(testing_profile()));
}
#else
TEST_F(ProfileUtilUnitTest,
       ProfileCanUseNonComponentExtensions_RegularProfile) {
  // testing_profile() defaults to a regular profile.
  EXPECT_TRUE(ProfileCanUseNonComponentExtensions(testing_profile()));
}

TEST_F(ProfileUtilUnitTest,
       Browser_ProfileCannotUseNonComponentExtensions_NoProfile) {
  EXPECT_FALSE(ProfileCanUseNonComponentExtensions(/*profile=*/nullptr));
}

TEST_F(ProfileUtilUnitTest,
       ProfileCannotUseNonComponentExtensions_GuestProfile) {
  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(/*is_guest=*/true));
  EXPECT_FALSE(ProfileCanUseNonComponentExtensions(testing_profile()));
}

TEST_F(ProfileUtilUnitTest,
       Browser_ProfileCannotUseNonComponentExtensions_IncognitoProfile) {
  TestingProfile* incognito_test_profile =
      TestingProfile::Builder().BuildIncognito(testing_profile());
  ASSERT_TRUE(incognito_test_profile);
  EXPECT_FALSE(ProfileCanUseNonComponentExtensions(incognito_test_profile));
}

TEST_F(ProfileUtilUnitTest,
       Browser_ProfileCannotUseNonComponentExtensions_OTRProfile) {
  TestingProfile* otr_test_profile =
      TestingProfile::Builder().BuildOffTheRecord(
          testing_profile(), Profile::OTRProfileID::CreateUniqueForTesting());
  ASSERT_TRUE(otr_test_profile);
  EXPECT_FALSE(ProfileCanUseNonComponentExtensions(otr_test_profile));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions
