// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile_testing_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

using ProfileTest = testing::Test;

TEST_F(ProfileTest, AllowsBrowserWindows) {
  ProfileTestingHelper helper;
  helper.SetUp();

  EXPECT_TRUE(helper.regular_profile()->AllowsBrowserWindows());
  EXPECT_TRUE(helper.incognito_profile()->AllowsBrowserWindows());

  EXPECT_FALSE(helper.guest_profile()->AllowsBrowserWindows());
  EXPECT_TRUE(helper.guest_profile_otr()->AllowsBrowserWindows());

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(helper.system_profile()->AllowsBrowserWindows());
  EXPECT_FALSE(helper.system_profile_otr()->AllowsBrowserWindows());
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(helper.signin_profile()->AllowsBrowserWindows());
  EXPECT_FALSE(helper.signin_profile_otr()->AllowsBrowserWindows());

  EXPECT_FALSE(helper.lockscreen_profile()->AllowsBrowserWindows());
  EXPECT_FALSE(helper.lockscreen_profile_otr()->AllowsBrowserWindows());

  EXPECT_FALSE(helper.shimless_rma_app_profile()->AllowsBrowserWindows());
  EXPECT_FALSE(helper.shimless_rma_app_profile_otr()->AllowsBrowserWindows());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(ProfileTest, IsIncognitoBranded) {
  ProfileTestingHelper helper;
  helper.SetUp();

  EXPECT_FALSE(helper.regular_profile()->IsOffTheRecord());
  EXPECT_FALSE(helper.regular_profile()->IsIncognitoBranded());

  EXPECT_TRUE(helper.incognito_profile()->IsOffTheRecord());
  EXPECT_TRUE(helper.incognito_profile()->IsIncognitoProfile());
  EXPECT_TRUE(helper.incognito_profile()->IsIncognitoBranded());

  EXPECT_FALSE(helper.guest_profile()->IsIncognitoBranded());
  // Primary off-the-record profiles, but not Incognito.
  EXPECT_FALSE(helper.guest_profile_otr()->IsIncognitoBranded());
  EXPECT_FALSE(helper.isolated_mode_profile()->IsIncognitoBranded());

#if BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(Profile::OTRProfileID::PrimaryID().IsIncognitoCct());
  EXPECT_FALSE(
      Profile::OTRProfileID::CreateUniqueForTesting().IsIncognitoCct());

  Profile::OTRProfileID incognito_cct_id =
      Profile::OTRProfileID::CreateUniqueForIncognitoCct();
  EXPECT_TRUE(incognito_cct_id.IsIncognitoCct());
  Profile* incognito_cct_profile =
      helper.regular_profile()->GetOffTheRecordProfile(
          incognito_cct_id, /*create_if_needed=*/true);
  ASSERT_TRUE(incognito_cct_profile);
  EXPECT_TRUE(incognito_cct_profile->IsOffTheRecord());
  EXPECT_FALSE(incognito_cct_profile->IsIncognitoProfile());
  EXPECT_TRUE(incognito_cct_profile->IsIncognitoBranded());

  Profile::OTRProfileID ephemeral_cct_id =
      Profile::OTRProfileID::CreateUnique("CCT:Ephemeral");
  EXPECT_FALSE(ephemeral_cct_id.IsIncognitoCct());
  Profile* ephemeral_cct_profile =
      helper.regular_profile()->GetOffTheRecordProfile(
          ephemeral_cct_id, /*create_if_needed=*/true);
  ASSERT_TRUE(ephemeral_cct_profile);
  EXPECT_TRUE(ephemeral_cct_profile->IsOffTheRecord());
  EXPECT_FALSE(ephemeral_cct_profile->IsIncognitoProfile());
  EXPECT_FALSE(ephemeral_cct_profile->IsIncognitoBranded());
#endif  // BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(ProfileTest, GetJavaObjectDuringDestruction) {
  ProfileTestingHelper helper;
  helper.SetUp();

  Profile* otr_profile = helper.regular_profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  ASSERT_TRUE(otr_profile);

  otr_profile->MaybeSendDestroyedNotification();
  EXPECT_EQ(Profile::FromJavaObject(otr_profile->GetJavaObject()), otr_profile);
  helper.regular_profile()->DestroyOffTheRecordProfile(otr_profile);
}
#endif  // BUILDFLAG(IS_ANDROID)
