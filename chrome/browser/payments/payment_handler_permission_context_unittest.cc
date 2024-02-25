// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/payment_handler_permission_context.h"

#include <string>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/infobars/content/content_infobar_manager.h"
#else
#include "components/permissions/permission_request_manager.h"
#endif

namespace {

using PermissionStatus = blink::mojom::PermissionStatus;

class TestPermissionContext : public payments::PaymentHandlerPermissionContext {
 public:
  explicit TestPermissionContext(Profile* profile)
      : PaymentHandlerPermissionContext(profile),
        permission_set_(false),
        permission_granted_(false) {}

  ~TestPermissionContext() override {}

  bool permission_granted() { return permission_granted_; }

  bool permission_set() { return permission_set_; }

  void TrackPermissionDecision(ContentSetting content_setting) {
    permission_set_ = true;
    permission_granted_ = content_setting == CONTENT_SETTING_ALLOW;
  }

 private:
  bool permission_set_;
  bool permission_granted_;
};

}  // anonymous namespace

class PaymentHandlerPermissionContextTests
    : public ChromeRenderViewHostTestHarness {
 public:
  PaymentHandlerPermissionContextTests(
      const PaymentHandlerPermissionContextTests&) = delete;
  PaymentHandlerPermissionContextTests& operator=(
      const PaymentHandlerPermissionContextTests&) = delete;

 protected:
  PaymentHandlerPermissionContextTests() = default;

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if BUILDFLAG(IS_ANDROID)
    infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
#else
    permissions::PermissionRequestManager::CreateForWebContents(web_contents());
#endif
  }
};

// PaymentHandler permission should be denied for insecure origin.
TEST_F(PaymentHandlerPermissionContextTests, TestInsecureRequestingUrl) {
  TestPermissionContext permission_context(profile());
  GURL url("http://www.example.test");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  const permissions::PermissionRequestID id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(),
      permissions::PermissionRequestID::RequestLocalId());
  permission_context.RequestPermission(
      permissions::PermissionRequestData(&permission_context, id,
                                         /*user_gesture=*/true, url),
      base::BindOnce(&TestPermissionContext::TrackPermissionDecision,
                     base::Unretained(&permission_context)));

  EXPECT_TRUE(permission_context.permission_set());
  EXPECT_FALSE(permission_context.permission_granted());

  ContentSetting setting =
      HostContentSettingsMapFactory::GetForProfile(profile())
          ->GetContentSetting(url.DeprecatedGetOriginAsURL(),
                              url.DeprecatedGetOriginAsURL(),
                              ContentSettingsType::PAYMENT_HANDLER);
  EXPECT_EQ(CONTENT_SETTING_ALLOW, setting);
}

// PaymentHandler permission status should be denied for insecure origin.
TEST_F(PaymentHandlerPermissionContextTests, TestInsecureQueryingUrl) {
  TestPermissionContext permission_context(profile());
  GURL insecure_url("http://www.example.test");
  GURL secure_url("https://www.example.test");

  // Check that there is no saved content settings.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            HostContentSettingsMapFactory::GetForProfile(profile())
                ->GetContentSetting(insecure_url.DeprecatedGetOriginAsURL(),
                                    insecure_url.DeprecatedGetOriginAsURL(),
                                    ContentSettingsType::PAYMENT_HANDLER));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            HostContentSettingsMapFactory::GetForProfile(profile())
                ->GetContentSetting(secure_url.DeprecatedGetOriginAsURL(),
                                    insecure_url.DeprecatedGetOriginAsURL(),
                                    ContentSettingsType::PAYMENT_HANDLER));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            HostContentSettingsMapFactory::GetForProfile(profile())
                ->GetContentSetting(insecure_url.DeprecatedGetOriginAsURL(),
                                    secure_url.DeprecatedGetOriginAsURL(),
                                    ContentSettingsType::PAYMENT_HANDLER));

  EXPECT_EQ(PermissionStatus::DENIED,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, insecure_url)
                .status);

  EXPECT_EQ(PermissionStatus::DENIED,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     secure_url, insecure_url)
                .status);

  EXPECT_EQ(PermissionStatus::DENIED,
            permission_context
                .GetPermissionStatus(nullptr /* render_frame_host */,
                                     insecure_url, secure_url)
                .status);
}
