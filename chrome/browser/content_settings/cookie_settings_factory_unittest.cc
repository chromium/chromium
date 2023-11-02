// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using QueryReason = content_settings::CookieSettings::QueryReason;

class CookieSettingsFactoryTest : public testing::Test {
 public:
  CookieSettingsFactoryTest()
      : cookie_settings_(CookieSettingsFactory::GetForProfile(&profile_).get()),
        kBlockedSite("http://ads.thirdparty.com"),
        kAllowedSite("http://good.allays.com"),
        kFirstPartySite("http://cool.things.com"),
        kHttpsSite("https://example.com") {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  raw_ptr<content_settings::CookieSettings> cookie_settings_;
  const GURL kBlockedSite;
  const GURL kAllowedSite;
  const GURL kFirstPartySite;
  const GURL kHttpsSite;
};

TEST_F(CookieSettingsFactoryTest, IncognitoBehaviorOfBlockingRules) {
  scoped_refptr<content_settings::CookieSettings> incognito_settings =
      CookieSettingsFactory::GetForProfile(
          profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true));

  // Modify the regular cookie settings after the incognito cookie settings have
  // been instantiated.
  cookie_settings_->SetCookieSetting(kBlockedSite, CONTENT_SETTING_BLOCK);

  // The modification should apply to the regular profile and incognito profile.
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kBlockedSite, kBlockedSite, QueryReason::kSetting));
  EXPECT_FALSE(incognito_settings->IsFullCookieAccessAllowed(
      kBlockedSite, kBlockedSite, QueryReason::kSetting));

  // Modify an incognito cookie setting and check that this does not propagate
  // into regular mode.
  incognito_settings->SetCookieSetting(kHttpsSite, CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kHttpsSite, QueryReason::kSetting));
  EXPECT_FALSE(incognito_settings->IsFullCookieAccessAllowed(
      kHttpsSite, kHttpsSite, QueryReason::kSetting));
}

TEST_F(CookieSettingsFactoryTest, IncognitoBehaviorOfBlockingEverything) {
  scoped_refptr<content_settings::CookieSettings> incognito_settings =
      CookieSettingsFactory::GetForProfile(
          profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true));

  // Apply the general blocking to the regular profile.
  cookie_settings_->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  // It should be effective for regular and incognito session.
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kFirstPartySite, kFirstPartySite, QueryReason::kSetting));
  EXPECT_FALSE(incognito_settings->IsFullCookieAccessAllowed(
      kFirstPartySite, kFirstPartySite, QueryReason::kSetting));

  // A whitelisted item set in incognito mode should only apply to incognito
  // mode.
  incognito_settings->SetCookieSetting(kAllowedSite, CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(incognito_settings->IsFullCookieAccessAllowed(
      kAllowedSite, kAllowedSite, QueryReason::kSetting));
  EXPECT_FALSE(cookie_settings_->IsFullCookieAccessAllowed(
      kAllowedSite, kAllowedSite, QueryReason::kSetting));

  // A whitelisted item set in regular mode should apply to regular and
  // incognito mode.
  cookie_settings_->SetCookieSetting(kHttpsSite, CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(incognito_settings->IsFullCookieAccessAllowed(
      kHttpsSite, kHttpsSite, QueryReason::kSetting));
  EXPECT_TRUE(cookie_settings_->IsFullCookieAccessAllowed(
      kHttpsSite, kHttpsSite, QueryReason::kSetting));
}

// Android does not have guest profiles.
#if !BUILDFLAG(IS_ANDROID)

// Tests that cookie blocking is not enabled by default for guest profiles.
TEST_F(CookieSettingsFactoryTest, GuestProfile) {
  TestingProfile::Builder guest_profile_builder;
  guest_profile_builder.SetGuestSession();
  std::unique_ptr<Profile> guest_profile = guest_profile_builder.Build();
  Profile* profile_to_use =
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  scoped_refptr<content_settings::CookieSettings> guest_settings =
      CookieSettingsFactory::GetForProfile(profile_to_use);
  EXPECT_FALSE(guest_settings->ShouldBlockThirdPartyCookies());

  // OTOH, cookie blocking is default for an incognito profile.
  EXPECT_TRUE(CookieSettingsFactory::GetForProfile(
                  profile_.GetPrimaryOTRProfile(/*create_if_needed=*/true))
                  ->ShouldBlockThirdPartyCookies());
}

#endif

}  // namespace
