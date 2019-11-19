// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage/durable_storage_permission_context.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using bookmarks::BookmarkModel;

namespace {

class TestDurablePermissionContext : public DurableStoragePermissionContext {
 public:
  explicit TestDurablePermissionContext(Profile* profile)
      : DurableStoragePermissionContext(profile),
        permission_set_count_(0),
        last_permission_set_persisted_(false),
        last_permission_set_setting_(CONTENT_SETTING_DEFAULT) {}

  int permission_set_count() const { return permission_set_count_; }
  bool last_permission_set_persisted() const {
    return last_permission_set_persisted_;
  }
  ContentSetting last_permission_set_setting() const {
    return last_permission_set_setting_;
  }

  ContentSetting GetContentSettingFromMap(const GURL& url_a,
                                          const GURL& url_b) {
    return HostContentSettingsMapFactory::GetForProfile(profile())
        ->GetContentSetting(url_a.GetOrigin(), url_b.GetOrigin(),
                            ContentSettingsType::DURABLE_STORAGE,
                            std::string());
  }

 private:
  // NotificationPermissionContext:
  void NotifyPermissionSet(const PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedder_origin,
                           BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting) override {
    permission_set_count_++;
    last_permission_set_persisted_ = persist;
    last_permission_set_setting_ = content_setting;
    DurableStoragePermissionContext::NotifyPermissionSet(
        id, requesting_origin, embedder_origin, std::move(callback), persist,
        content_setting);
  }

  int permission_set_count_;
  bool last_permission_set_persisted_;
  ContentSetting last_permission_set_setting_;
};

}  // namespace


class DurableStoragePermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void MakeOriginImportant(const GURL& origin) {
    ImportantSitesUtil::MarkOriginAsImportantForTesting(profile(), origin);
  }
};

TEST_F(DurableStoragePermissionContextTest, Bookmarked) {
  TestDurablePermissionContext permission_context(profile());
  GURL url("https://www.google.com");
  MakeOriginImportant(url);
  NavigateAndCommit(url);

  const PermissionRequestID id(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(), -1);

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.DecidePermission(
      web_contents(), id, url, url, true /* user_gesture */, base::DoNothing());
  // Success.
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_TRUE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_context.last_permission_set_setting());
}

TEST_F(DurableStoragePermissionContextTest, BookmarkAndIncognitoMode) {
  TestDurablePermissionContext permission_context(
      profile()->GetOffTheRecordProfile());
  GURL url("https://www.google.com");
  MakeOriginImportant(url);
  NavigateAndCommit(url);

  const PermissionRequestID id(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(), -1);

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.DecidePermission(
      web_contents(), id, url, url, true /* user_gesture */, base::DoNothing());
  // Success.
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_TRUE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            permission_context.last_permission_set_setting());
}

TEST_F(DurableStoragePermissionContextTest, NoBookmark) {
  TestDurablePermissionContext permission_context(profile());
  GURL url("https://www.google.com");
  NavigateAndCommit(url);

  const PermissionRequestID id(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(), -1);

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.DecidePermission(
      web_contents(), id, url, url, true /* user_gesture */, base::DoNothing());

  // We shouldn't be granted.
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_FALSE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());
}

TEST_F(DurableStoragePermissionContextTest, CookiesNotAllowed) {
  TestDurablePermissionContext permission_context(profile());
  GURL url("https://www.google.com");
  MakeOriginImportant(url);
  NavigateAndCommit(url);

  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(profile());

  cookie_settings->SetCookieSetting(url, CONTENT_SETTING_BLOCK);

  const PermissionRequestID id(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(), -1);

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.DecidePermission(
      web_contents(), id, url, url, true /* user_gesture */, base::DoNothing());
  // We shouldn't be granted.
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_FALSE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());
}

TEST_F(DurableStoragePermissionContextTest, EmbeddedFrame) {
  TestDurablePermissionContext permission_context(profile());
  GURL url("https://www.google.com");
  GURL requesting_url("https://www.youtube.com");
  MakeOriginImportant(url);
  NavigateAndCommit(url);

  const PermissionRequestID id(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(), -1);

  ASSERT_EQ(0, permission_context.permission_set_count());
  ASSERT_FALSE(permission_context.last_permission_set_persisted());
  ASSERT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());

  permission_context.DecidePermission(web_contents(), id, requesting_url, url,
                                      true /* user_gesture */,
                                      base::DoNothing());
  // We shouldn't be granted.
  EXPECT_EQ(1, permission_context.permission_set_count());
  EXPECT_FALSE(permission_context.last_permission_set_persisted());
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            permission_context.last_permission_set_setting());
}

TEST_F(DurableStoragePermissionContextTest, NonsecureOrigin) {
  TestDurablePermissionContext permission_context(profile());
  GURL url("http://www.google.com");

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */, url, url)
                .content_setting);
}
