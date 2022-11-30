// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_util.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class PermissionManagerTestingProfile : public TestingProfile {
 public:
  PermissionManagerTestingProfile() = default;
  ~PermissionManagerTestingProfile() override = default;
  PermissionManagerTestingProfile(const PermissionManagerTestingProfile&) =
      delete;
  PermissionManagerTestingProfile& operator=(
      const PermissionManagerTestingProfile&) = delete;

  permissions::PermissionManager* GetPermissionControllerDelegate() override {
    return PermissionManagerFactory::GetForProfile(this);
  }
};
}  // namespace

class ChromePermissionManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  permissions::PermissionManager* GetPermissionControllerDelegate() {
    return profile_->GetPermissionControllerDelegate();
  }

 private:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile_ = std::make_unique<PermissionManagerTestingProfile>();
  }

  void TearDown() override {
    profile_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<PermissionManagerTestingProfile> profile_;
};

TEST_F(ChromePermissionManagerTest, GetCanonicalOriginSearch) {
  const GURL google_com("https://www.google.com");
  const GURL google_de("https://www.google.de");
  const GURL other_url("https://other.url");
  const GURL google_base = GURL(UIThreadSearchTermsData().GoogleBaseURLValue())
                               .DeprecatedGetOriginAsURL();
  const GURL remote_ntp = GURL(std::string("chrome-search://") +
                               chrome::kChromeSearchRemoteNtpHost);
  const GURL other_chrome_search = GURL("chrome-search://not-local-ntp");
  const GURL top_level_ntp(chrome::kChromeUINewTabURL);
  const GURL webui_ntp = GURL(chrome::kChromeUINewTabPageURL);

  // "Normal" URLs are not affected by GetCanonicalOrigin.
  EXPECT_EQ(google_com,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_com, google_com));
  EXPECT_EQ(google_de,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_de, google_de));
  EXPECT_EQ(other_url,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, other_url, other_url));
  EXPECT_EQ(google_base,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_base, google_base));

  // The WebUI NTP URL gets mapped to the Google base URL.
  EXPECT_EQ(google_base,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, webui_ntp, top_level_ntp));

  // chrome-search://remote-ntp and other URLs are not affected.
  EXPECT_EQ(remote_ntp,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, remote_ntp, top_level_ntp));
  EXPECT_EQ(google_com,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, google_com, top_level_ntp));
  EXPECT_EQ(other_chrome_search,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, other_chrome_search,
                top_level_ntp));
}

TEST_F(ChromePermissionManagerTest, GetCanonicalOriginPermissionDelegation) {
  const GURL requesting_origin("https://www.requesting.com");
  const GURL embedding_origin("https://www.google.de");

  // The embedding origin should be returned except in the case of notifications
  // and, if they're enabled, extensions.
  EXPECT_EQ(embedding_origin, permissions::PermissionUtil::GetCanonicalOrigin(
                                  ContentSettingsType::GEOLOCATION,
                                  requesting_origin, embedding_origin));
  EXPECT_EQ(requesting_origin, permissions::PermissionUtil::GetCanonicalOrigin(
                                   ContentSettingsType::NOTIFICATIONS,
                                   requesting_origin, embedding_origin));
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const GURL extensions_requesting_origin(
      "chrome-extension://abcdefghijklmnopqrstuvxyz");
  EXPECT_EQ(extensions_requesting_origin,
            permissions::PermissionUtil::GetCanonicalOrigin(
                ContentSettingsType::GEOLOCATION, extensions_requesting_origin,
                embedding_origin));
#endif
}
