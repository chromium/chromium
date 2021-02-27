// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/camera_pan_tilt_zoom_permission_context.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"

namespace {

struct TestConfig {
  const ContentSetting first;   // first content setting to be set
  const ContentSetting second;  // second content setting to be set
  const ContentSetting result;  // expected resulting content setting
};

}  // namespace

// Waits until a change is observed for a specific content setting type.
class ContentSettingsChangeWaiter : public content_settings::Observer {
 public:
  explicit ContentSettingsChangeWaiter(Profile* profile,
                                       ContentSettingsType content_type)
      : profile_(profile), content_type_(content_type) {
    HostContentSettingsMapFactory::GetForProfile(profile)->AddObserver(this);
  }
  ~ContentSettingsChangeWaiter() override {
    HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(
        this);
  }

  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override {
    if (content_type == content_type_)
      Proceed();
  }

  void Wait() { run_loop_.Run(); }

 private:
  void Proceed() { run_loop_.Quit(); }

  Profile* profile_;
  ContentSettingsType content_type_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsChangeWaiter);
};

class CameraPanTiltZoomPermissionContextTests
    : public ChromeRenderViewHostTestHarness,
      public testing::WithParamInterface<TestConfig> {
 public:
  CameraPanTiltZoomPermissionContextTests() = default;

  void SetContentSetting(ContentSettingsType content_settings_type,
                         ContentSetting content_setting) {
    GURL url("https://www.example.com");
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(profile());
    content_settings->SetContentSettingDefaultScope(
        url, GURL(), content_settings_type, content_setting);
  }

  ContentSetting GetContentSetting(ContentSettingsType content_settings_type) {
    GURL url("https://www.example.com");
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(profile());
    return content_settings->GetContentSetting(url.GetOrigin(), url.GetOrigin(),
                                               content_settings_type);
  }

  DISALLOW_COPY_AND_ASSIGN(CameraPanTiltZoomPermissionContextTests);
};

class CameraContentSettingTests
    : public CameraPanTiltZoomPermissionContextTests {
 public:
  CameraContentSettingTests() = default;
};

TEST_P(CameraContentSettingTests, TestResetPermissionOnCameraChange) {
  CameraPanTiltZoomPermissionContext permission_context(profile());
  ContentSettingsChangeWaiter waiter(profile(),
                                     ContentSettingsType::MEDIASTREAM_CAMERA);

  SetContentSetting(ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
                    GetParam().first);
  SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA, GetParam().second);

  waiter.Wait();
  EXPECT_EQ(GetParam().result,
            GetContentSetting(ContentSettingsType::CAMERA_PAN_TILT_ZOOM));
}

INSTANTIATE_TEST_SUITE_P(
    ResetPermissionOnCameraChange,
    CameraContentSettingTests,
    testing::Values(
        // Granted camera PTZ permission is reset if camera is blocked.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_ASK},
        // Granted camera PTZ permission is reset if camera is reset.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
                   CONTENT_SETTING_ASK},
        // Blocked camera PTZ permission is not reset if camera is granted.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW,
                   CONTENT_SETTING_BLOCK},
        // Blocked camera PTZ permission is not reset if camera is blocked.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_BLOCK}));

class CameraPanTiltZoomContentSettingTests
    : public CameraPanTiltZoomPermissionContextTests {
 public:
  CameraPanTiltZoomContentSettingTests() = default;
};

TEST_P(CameraPanTiltZoomContentSettingTests,
       TestCameraPermissionOnCameraPanTiltZoomChange) {
  CameraPanTiltZoomPermissionContext permission_context(profile());
  ContentSettingsChangeWaiter waiter(profile(),
                                     ContentSettingsType::CAMERA_PAN_TILT_ZOOM);

  SetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA, GetParam().first);
  SetContentSetting(ContentSettingsType::CAMERA_PAN_TILT_ZOOM,
                    GetParam().second);

  waiter.Wait();
  EXPECT_EQ(GetParam().result,
            GetContentSetting(ContentSettingsType::MEDIASTREAM_CAMERA));
}

INSTANTIATE_TEST_SUITE_P(
    CameraPermissionOnCameraPanTiltZoomChange,
    CameraPanTiltZoomContentSettingTests,
    testing::Values(
        // Asked camera permission is blocked if camera PTZ is blocked.
        TestConfig{CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_BLOCK},
        // Asked camera permission is granted if camera PTZ is granted.
        TestConfig{CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW,
                   CONTENT_SETTING_ALLOW},
        // Asked camera permission is unchanged if camera PTZ is reset.
        TestConfig{CONTENT_SETTING_ASK, CONTENT_SETTING_DEFAULT,
                   CONTENT_SETTING_ASK},
        // Asked camera permission is unchanged if camera PTZ is ask.
        TestConfig{CONTENT_SETTING_ASK, CONTENT_SETTING_ASK,
                   CONTENT_SETTING_ASK},
        // Allowed camera permission is blocked if camera PTZ is blocked.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_BLOCK},
        // Allowed camera permission is unchanged if camera PTZ is granted.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW,
                   CONTENT_SETTING_ALLOW},
        // Allowed camera permission is ask if camera PTZ is reset.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_DEFAULT,
                   CONTENT_SETTING_ASK},
        // Allowed camera permission is reset if camera PTZ is ask.
        TestConfig{CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK,
                   CONTENT_SETTING_ASK},
        // Blocked camera permission is unchanged if camera PTZ is blocked.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_BLOCK},
        // Blocked camera permission is allowed if camera PTZ is granted.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW,
                   CONTENT_SETTING_ALLOW},
        // Blocked camera permission is ask if camera PTZ is reset.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_DEFAULT,
                   CONTENT_SETTING_ASK},
        // Blocked camera permission is reset if camera PTZ is ask.
        TestConfig{CONTENT_SETTING_BLOCK, CONTENT_SETTING_ASK,
                   CONTENT_SETTING_ASK},
        // Default camera permission is blocked if camera PTZ is blocked.
        TestConfig{CONTENT_SETTING_DEFAULT, CONTENT_SETTING_BLOCK,
                   CONTENT_SETTING_BLOCK},
        // Default camera permission is allowed if camera PTZ is granted.
        TestConfig{CONTENT_SETTING_DEFAULT, CONTENT_SETTING_ALLOW,
                   CONTENT_SETTING_ALLOW},
        // Default camera permission is unchanged if camera PTZ is reset.
        TestConfig{CONTENT_SETTING_DEFAULT, CONTENT_SETTING_DEFAULT,
                   CONTENT_SETTING_ASK},
        // Default camera permission is ask if camera PTZ is ask.
        TestConfig{CONTENT_SETTING_DEFAULT, CONTENT_SETTING_ASK,
                   CONTENT_SETTING_ASK}));
