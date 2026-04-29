// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace {

constexpr std::string_view kDeviceOrientationScript = R"(
  new Promise((resolve, reject) => {
    window.addEventListener('deviceorientation', event => {
      if (event.alpha === null && event.beta === null &&
          event.gamma === null && event.absolute === false) {
        resolve('null_event_fired');
      } else {
        reject('non_null_event');
      }
    }, {once: true});
  });
)";

constexpr std::string_view kDeviceMotionScript = R"(
  new Promise((resolve, reject) => {
    window.addEventListener('devicemotion', event => {
      const isNull = (val) => val === null;
      const accel = event.acceleration;
      const accelG = event.accelerationIncludingGravity;
      const rot = event.rotationRate;

      const ok = accel && isNull(accel.x) && isNull(accel.y) &&
                 isNull(accel.z) && accelG && isNull(accelG.x) &&
                 isNull(accelG.y) && isNull(accelG.z) && rot &&
                 isNull(rot.alpha) && isNull(rot.beta) && isNull(rot.gamma);

      if (ok) {
        resolve('null_event_fired');
      } else {
        reject('non_null_event');
      }
    }, {once: true});
  });
)";

constexpr std::string_view kDeviceOrientationAbsoluteScript = R"(
  new Promise((resolve, reject) => {
    window.addEventListener('deviceorientationabsolute', event => {
      if (event.alpha === null && event.beta === null &&
          event.gamma === null && event.absolute === true) {
        resolve('null_event_fired');
      } else {
        reject('non_null_event');
      }
    }, {once: true});
  });
)";

}  // namespace

class DeviceSensorPermissionBrowserTest : public InProcessBrowserTest {
 public:
  DeviceSensorPermissionBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kDeviceOrientationRequestPermission);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  HostContentSettingsMap* GetHostContentSettingsMap() {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeviceSensorPermissionBrowserTest,
                       DeviceOrientationPermissionDeniedFiresNullEvent) {
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_BLOCK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ("null_event_fired",
            content::EvalJs(web_contents, kDeviceOrientationScript));
}

IN_PROC_BROWSER_TEST_F(DeviceSensorPermissionBrowserTest,
                       DeviceOrientationPermissionAskFiresNullEvent) {
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_ASK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // In the "ASK" state, adding a listener does not trigger a permission prompt.
  // Instead, it immediately falls back to the "missing hardware" behavior and
  // fires a single event with null values.
  EXPECT_EQ("null_event_fired",
            content::EvalJs(web_contents, kDeviceOrientationScript));
}

IN_PROC_BROWSER_TEST_F(DeviceSensorPermissionBrowserTest,
                       DeviceMotionPermissionDeniedFiresNullEvent) {
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_BLOCK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ("null_event_fired",
            content::EvalJs(web_contents, kDeviceMotionScript));
}

IN_PROC_BROWSER_TEST_F(DeviceSensorPermissionBrowserTest,
                       DeviceMotionPermissionAskFiresNullEvent) {
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_ASK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // In the "ASK" state, adding a listener does not trigger a permission prompt.
  // Instead, it immediately falls back to the "missing hardware" behavior and
  // fires a single event with null values.
  EXPECT_EQ("null_event_fired",
            content::EvalJs(web_contents, kDeviceMotionScript));
}

IN_PROC_BROWSER_TEST_F(
    DeviceSensorPermissionBrowserTest,
    DeviceOrientationAbsolutePermissionDeniedFiresNullEvent) {
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_BLOCK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ("null_event_fired",
            content::EvalJs(web_contents, kDeviceOrientationAbsoluteScript));
}

IN_PROC_BROWSER_TEST_F(DeviceSensorPermissionBrowserTest,
                       DeviceOrientationAbsolutePermissionAskFiresNullEvent) {
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_ASK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // In the "ASK" state, adding a listener does not trigger a permission prompt.
  // Instead, it immediately falls back to the "missing hardware" behavior and
  // fires a single event with null values.
  EXPECT_EQ("null_event_fired",
            content::EvalJs(web_contents, kDeviceOrientationAbsoluteScript));
}

IN_PROC_BROWSER_TEST_F(DeviceSensorPermissionBrowserTest,
                       DeviceOrientationRequestPermissionDeniedFiresNullEvent) {
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_ASK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::DENY_ALL);

  EXPECT_EQ("denied",
            content::EvalJs(web_contents,
                            "DeviceOrientationEvent.requestPermission()"));
  EXPECT_EQ("null_event_fired",
            content::EvalJs(web_contents, kDeviceOrientationScript));
}

IN_PROC_BROWSER_TEST_F(DeviceSensorPermissionBrowserTest,
                       DeviceMotionRequestPermissionDeniedFiresNullEvent) {
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_ASK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::DENY_ALL);

  EXPECT_EQ("denied", content::EvalJs(web_contents,
                                      "DeviceMotionEvent.requestPermission()"));
  EXPECT_EQ("null_event_fired",
            content::EvalJs(web_contents, kDeviceMotionScript));
}

IN_PROC_BROWSER_TEST_F(
    DeviceSensorPermissionBrowserTest,
    DeviceOrientationAbsoluteRequestPermissionDeniedFiresNullEvent) {
  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      url, url, ContentSettingsType::SENSORS, CONTENT_SETTING_ASK);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::DENY_ALL);

  EXPECT_EQ("denied",
            content::EvalJs(web_contents,
                            "DeviceOrientationEvent.requestPermission()"));
  EXPECT_EQ("null_event_fired",
            content::EvalJs(web_contents, kDeviceOrientationAbsoluteScript));
}
