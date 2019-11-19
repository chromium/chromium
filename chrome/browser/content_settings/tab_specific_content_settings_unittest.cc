// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/tab_specific_content_settings.h"

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/parsed_cookie.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

class MockSiteDataObserver
    : public TabSpecificContentSettings::SiteDataObserver {
 public:
  explicit MockSiteDataObserver(
      TabSpecificContentSettings* tab_specific_content_settings)
      : SiteDataObserver(tab_specific_content_settings) {
  }

  ~MockSiteDataObserver() override {}

  MOCK_METHOD0(OnSiteDataAccessed, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSiteDataObserver);
};

}  // namespace

class TabSpecificContentSettingsTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    TabSpecificContentSettings::CreateForWebContents(web_contents());
  }
};

TEST_F(TabSpecificContentSettingsTest, BlockedContent) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

  // Check that after initializing, nothing is blocked.
#if !defined(OS_ANDROID)
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::PLUGINS));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::SOUND));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  // Set a cookie, block access to images, block mediastream access and block a
  // popup.
  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      origin, "A=B", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie1);
  web_contents()->OnCookieChange(origin, origin, *cookie1, false);
#if !defined(OS_ANDROID)
  content_settings->OnContentBlocked(ContentSettingsType::IMAGES);
#endif
  content_settings->OnContentBlocked(ContentSettingsType::POPUPS);
  TabSpecificContentSettings::MicrophoneCameraState
      blocked_microphone_camera_state =
          TabSpecificContentSettings::MICROPHONE_ACCESSED |
          TabSpecificContentSettings::MICROPHONE_BLOCKED |
          TabSpecificContentSettings::CAMERA_ACCESSED |
          TabSpecificContentSettings::CAMERA_BLOCKED;
  content_settings->OnMediaStreamPermissionSet(GURL("http://google.com"),
                                               blocked_microphone_camera_state,
                                               std::string(),
                                               std::string(),
                                               std::string(),
                                               std::string());

  // Check that only the respective content types are affected.
#if !defined(OS_ANDROID)
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::PLUGINS));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::SOUND));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_TRUE(
      content_settings->IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_TRUE(content_settings->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
  web_contents()->OnCookieChange(origin, origin, *cookie1, false);

  // Block a cookie.
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      origin, "C=D", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie2);
  web_contents()->OnCookieChange(origin, origin, *cookie2, true);
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Block a javascript during a navigation.
  content_settings->OnServiceWorkerAccessed(GURL("http://google.com"),
                                            true, false);
  EXPECT_TRUE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));

  // Reset blocked content settings.
  content_settings
      ->ClearContentSettingsExceptForNavigationRelatedSettings();
#if !defined(OS_ANDROID)
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::PLUGINS));
#endif
  EXPECT_TRUE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  content_settings->ClearNavigationRelatedContentSettings();
#if !defined(OS_ANDROID)
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::IMAGES));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::PLUGINS));
#endif
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  EXPECT_FALSE(content_settings->IsContentBlocked(ContentSettingsType::POPUPS));
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::MEDIASTREAM_MIC));
  EXPECT_FALSE(content_settings->IsContentBlocked(
      ContentSettingsType::MEDIASTREAM_CAMERA));
}

TEST_F(TabSpecificContentSettingsTest, BlockedFileSystems) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

  // Access a file system.
  content_settings->OnFileSystemAccessed(GURL("http://google.com"), false);
  EXPECT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Block access to a file system.
  content_settings->OnFileSystemAccessed(GURL("http://google.com"), true);
  EXPECT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(TabSpecificContentSettingsTest, AllowedContent) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

  // Test default settings.
  ASSERT_FALSE(content_settings->IsContentAllowed(ContentSettingsType::IMAGES));
  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::MEDIASTREAM_MIC));
  ASSERT_FALSE(content_settings->IsContentAllowed(
      ContentSettingsType::MEDIASTREAM_CAMERA));

  // Record a cookie.
  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie1(net::CanonicalCookie::Create(
      origin, "A=B", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie1);
  web_contents()->OnCookieChange(origin, origin, *cookie1, false);
  ASSERT_TRUE(content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));

  // Record a blocked cookie.
  std::unique_ptr<net::CanonicalCookie> cookie2(net::CanonicalCookie::Create(
      origin, "C=D", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie2);
  web_contents()->OnCookieChange(origin, origin, *cookie2, true);
  ASSERT_TRUE(content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_TRUE(content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(TabSpecificContentSettingsTest, EmptyCookieList) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());

  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
  web_contents()->OnCookiesRead(GURL("http://google.com"),
                                GURL("http://google.com"), net::CookieList(),
                                true);
  ASSERT_FALSE(
      content_settings->IsContentAllowed(ContentSettingsType::COOKIES));
  ASSERT_FALSE(
      content_settings->IsContentBlocked(ContentSettingsType::COOKIES));
}

TEST_F(TabSpecificContentSettingsTest, SiteDataObserver) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  MockSiteDataObserver mock_observer(content_settings);
  EXPECT_CALL(mock_observer, OnSiteDataAccessed()).Times(6);

  bool blocked_by_policy = false;
  GURL origin("http://google.com");
  std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
      origin, "A=B", base::Time::Now(), base::nullopt /* server_time */));
  ASSERT_TRUE(cookie);
  web_contents()->OnCookieChange(origin, origin, *cookie, blocked_by_policy);

  net::CookieList cookie_list;
  std::unique_ptr<net::CanonicalCookie> other_cookie(
      net::CanonicalCookie::Create(GURL("http://google.com"),
                                   "CookieName=CookieValue", base::Time::Now(),
                                   base::nullopt /* server_time */));
  ASSERT_TRUE(other_cookie);

  cookie_list.push_back(*other_cookie);
  web_contents()->OnCookiesRead(GURL("http://google.com"),
                                GURL("http://google.com"), cookie_list,
                                blocked_by_policy);
  content_settings->OnFileSystemAccessed(GURL("http://google.com"),
                                              blocked_by_policy);
  content_settings->OnIndexedDBAccessed(GURL("http://google.com"),
                                        blocked_by_policy);
  content_settings->OnDomStorageAccessed(GURL("http://google.com"), true,
                                         blocked_by_policy);
  content_settings->OnWebDatabaseAccessed(GURL("http://google.com"),
                                          blocked_by_policy);
}

TEST_F(TabSpecificContentSettingsTest, LocalSharedObjectsContainer) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  bool blocked_by_policy = false;
  auto cookie = net::CanonicalCookie::Create(GURL("http://google.com"), "k=v",
                                             base::Time::Now(),
                                             base::nullopt /* server_time */);
  web_contents()->OnCookiesRead(GURL("http://google.com"),
                                GURL("http://google.com"), {*cookie},
                                blocked_by_policy);
  content_settings->OnFileSystemAccessed(GURL("https://www.google.com"),
                                         blocked_by_policy);
  content_settings->OnIndexedDBAccessed(GURL("https://localhost"),
                                        blocked_by_policy);
  content_settings->OnDomStorageAccessed(GURL("http://maps.google.com:8080"),
                                         true, blocked_by_policy);
  content_settings->OnWebDatabaseAccessed(GURL("http://192.168.0.1"),
                                          blocked_by_policy);
  content_settings->OnSharedWorkerAccessed(
      GURL("http://youtube.com/worker.js"), "worker",
      url::Origin::Create(GURL("https://youtube.com")), blocked_by_policy);

  const auto& objects = content_settings->allowed_local_shared_objects();
  EXPECT_EQ(6u, objects.GetObjectCount());
  EXPECT_EQ(3u, objects.GetObjectCountForDomain(GURL("http://google.com")));
  EXPECT_EQ(1u, objects.GetObjectCountForDomain(GURL("http://youtube.com")));
  EXPECT_EQ(1u, objects.GetObjectCountForDomain(GURL("http://localhost")));
  EXPECT_EQ(1u, objects.GetObjectCountForDomain(GURL("http://192.168.0.1")));
  // google.com, youtube.com, localhost and 192.168.0.1 should be counted as
  // domains.
  EXPECT_EQ(4u, objects.GetDomainCount());
}

TEST_F(TabSpecificContentSettingsTest, LocalSharedObjectsContainerCookie) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  bool blocked_by_policy = false;
  auto cookie1 = net::CanonicalCookie::Create(GURL("http://google.com"), "k1=v",
                                              base::Time::Now(),
                                              base::nullopt /* server_time */);
  auto cookie2 = net::CanonicalCookie::Create(
      GURL("http://www.google.com"), "k2=v; Domain=google.com",
      base::Time::Now(), base::nullopt /* server_time */);
  auto cookie3 = net::CanonicalCookie::Create(
      GURL("http://www.google.com"), "k3=v; Domain=.google.com",
      base::Time::Now(), base::nullopt /* server_time */);
  auto cookie4 = net::CanonicalCookie::Create(
      GURL("http://www.google.com"), "k4=v; Domain=.www.google.com",
      base::Time::Now(), base::nullopt /* server_time */);
  web_contents()->OnCookiesRead(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      {*cookie1, *cookie2, *cookie3, *cookie4}, blocked_by_policy);

  auto cookie5 = net::CanonicalCookie::Create(GURL("https://www.google.com"),
                                              "k5=v", base::Time::Now(),
                                              base::nullopt /* server_time */);
  web_contents()->OnCookiesRead(GURL("https://www.google.com"),
                                GURL("https://www.google.com"), {*cookie5},
                                blocked_by_policy);

  const auto& objects = content_settings->allowed_local_shared_objects();
  EXPECT_EQ(5u, objects.GetObjectCount());
  EXPECT_EQ(5u, objects.GetObjectCountForDomain(GURL("http://google.com")));
  EXPECT_EQ(1u, objects.GetDomainCount());
}
