// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/media_stream_device_permission_context.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/permissions/permission_request_manager.h"
#endif

namespace {
class TestPermissionContext : public MediaStreamDevicePermissionContext {
 public:
  TestPermissionContext(Profile* profile,
                        const ContentSettingsType content_settings_type)
      : MediaStreamDevicePermissionContext(profile, content_settings_type) {}

  ~TestPermissionContext() override {}
};

}  // anonymous namespace

// TODO(raymes): many tests in MediaStreamDevicesControllerTest should be
// converted to tests in this file.
class MediaStreamDevicePermissionContextTests
    : public ChromeRenderViewHostTestHarness {
 protected:
  MediaStreamDevicePermissionContextTests() = default;

  void TestInsecureQueryingUrl(ContentSettingsType content_settings_type) {
    TestPermissionContext permission_context(profile(), content_settings_type);
    GURL insecure_url("http://www.example.com");
    GURL secure_url("https://www.example.com");

    // Check that there is no saved content settings.
    EXPECT_EQ(CONTENT_SETTING_ASK,
              HostContentSettingsMapFactory::GetForProfile(profile())
                  ->GetContentSetting(insecure_url.GetOrigin(),
                                      insecure_url.GetOrigin(),
                                      content_settings_type, std::string()));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              HostContentSettingsMapFactory::GetForProfile(profile())
                  ->GetContentSetting(secure_url.GetOrigin(),
                                      insecure_url.GetOrigin(),
                                      content_settings_type, std::string()));
    EXPECT_EQ(CONTENT_SETTING_ASK,
              HostContentSettingsMapFactory::GetForProfile(profile())
                  ->GetContentSetting(insecure_url.GetOrigin(),
                                      secure_url.GetOrigin(),
                                      content_settings_type, std::string()));

    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              permission_context
                  .GetPermissionStatus(nullptr /* render_frame_host */,
                                       insecure_url, insecure_url)
                  .content_setting);

    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              permission_context
                  .GetPermissionStatus(nullptr /* render_frame_host */,
                                       insecure_url, secure_url)
                  .content_setting);
  }

  void TestSecureQueryingUrl(ContentSettingsType content_settings_type) {
    TestPermissionContext permission_context(profile(), content_settings_type);
    GURL secure_url("https://www.example.com");

    // Check that there is no saved content settings.
    EXPECT_EQ(CONTENT_SETTING_ASK,
              HostContentSettingsMapFactory::GetForProfile(profile())
                  ->GetContentSetting(secure_url.GetOrigin(),
                                      secure_url.GetOrigin(),
                                      content_settings_type,
                                      std::string()));

    EXPECT_EQ(CONTENT_SETTING_ASK,
              permission_context
                  .GetPermissionStatus(nullptr /* render_frame_host */,
                                       secure_url, secure_url)
                  .content_setting);
  }

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined(OS_ANDROID)
    InfoBarService::CreateForWebContents(web_contents());
#else
    PermissionRequestManager::CreateForWebContents(web_contents());
#endif
  }

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDevicePermissionContextTests);
};

// MEDIASTREAM_MIC permission status should be ask for insecure origin to
// accommodate the usage case of Flash.
TEST_F(MediaStreamDevicePermissionContextTests, TestMicInsecureQueryingUrl) {
  TestInsecureQueryingUrl(ContentSettingsType::MEDIASTREAM_MIC);
}

// MEDIASTREAM_CAMERA permission status should be ask for insecure origin to
// accommodate the usage case of Flash.
TEST_F(MediaStreamDevicePermissionContextTests, TestCameraInsecureQueryingUrl) {
  TestInsecureQueryingUrl(ContentSettingsType::MEDIASTREAM_CAMERA);
}

// MEDIASTREAM_MIC permission status should be ask for Secure origin.
TEST_F(MediaStreamDevicePermissionContextTests, TestMicSecureQueryingUrl) {
  TestSecureQueryingUrl(ContentSettingsType::MEDIASTREAM_MIC);
}

// MEDIASTREAM_CAMERA permission status should be ask for Secure origin.
TEST_F(MediaStreamDevicePermissionContextTests, TestCameraSecureQueryingUrl) {
  TestSecureQueryingUrl(ContentSettingsType::MEDIASTREAM_CAMERA);
}
