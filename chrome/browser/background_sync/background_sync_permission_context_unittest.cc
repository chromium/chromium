// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_permission_context.h"

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class BackgroundSyncPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  BackgroundSyncPermissionContextTest() = default;

  ~BackgroundSyncPermissionContextTest() override = default;

  void NavigateAndRequestPermission(
      const GURL& url,
      BackgroundSyncPermissionContext* permission_context) {
    content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

    base::RunLoop run_loop;

    const PermissionRequestID id(
        web_contents()->GetMainFrame()->GetProcess()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), /* request_id= */ -1);
    permission_context->RequestPermission(
        web_contents(), id, url, /* user_gesture= */ false,
        base::AdaptCallbackForRepeating(base::BindOnce(
            &BackgroundSyncPermissionContextTest::TrackPermissionDecision,
            base::Unretained(this), run_loop.QuitClosure())));

    run_loop.Run();
  }

  void TrackPermissionDecision(base::Closure done_closure,
                               ContentSetting content_setting) {
    permission_granted_ = content_setting == CONTENT_SETTING_ALLOW;
    done_closure.Run();
  }

  bool permission_granted() const { return permission_granted_; }

 private:
  bool permission_granted_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncPermissionContextTest);
};

// Background sync permission should be allowed by default for a secure origin.
TEST_F(BackgroundSyncPermissionContextTest, TestSecureRequestingUrl) {
  GURL url("https://www.example.com");
  BackgroundSyncPermissionContext permission_context(profile());

  NavigateAndRequestPermission(url, &permission_context);

  EXPECT_TRUE(permission_granted());
}

// Background sync permission should be denied for an insecure origin.
TEST_F(BackgroundSyncPermissionContextTest, TestInsecureRequestingUrl) {
  GURL url("http://example.com");
  BackgroundSyncPermissionContext permission_context(profile());

  NavigateAndRequestPermission(url, &permission_context);

  EXPECT_FALSE(permission_granted());
}

// Tests that blocking one origin does not affect the others.
TEST_F(BackgroundSyncPermissionContextTest, TestBlockOrigin) {
  GURL url1("https://www.example1.com");
  GURL url2("https://www.example2.com");
  BackgroundSyncPermissionContext permission_context(profile());
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingDefaultScope(url1, GURL(),
                                      ContentSettingsType::BACKGROUND_SYNC,
                                      std::string(), CONTENT_SETTING_BLOCK);

  NavigateAndRequestPermission(url1, &permission_context);

  EXPECT_FALSE(permission_granted());

  NavigateAndRequestPermission(url2, &permission_context);

  EXPECT_TRUE(permission_granted());
}
