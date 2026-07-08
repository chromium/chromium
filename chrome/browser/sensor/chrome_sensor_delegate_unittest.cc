// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sensor/chrome_sensor_delegate.h"

#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeSensorDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromeSensorDelegateTest() = default;
  ~ChromeSensorDelegateTest() override = default;
};

TEST_F(ChromeSensorDelegateTest,
       SetRequestedSensorIsAvailableNoCrashWithoutSettings) {
  ChromeSensorDelegate delegate;
  // PageSpecificContentSettings is NOT created for web_contents() in this test.
  // So GetForFrame should return nullptr.
  delegate.SetRequestedSensorIsAvailable(main_rfh(), true);
  // Should not crash.
}

TEST_F(ChromeSensorDelegateTest, OnSensorBlockedNoCrashWithoutSettings) {
  ChromeSensorDelegate delegate;
  // PageSpecificContentSettings is NOT created for web_contents() in this test.
  // So GetForFrame should return nullptr.
  delegate.OnSensorBlocked(main_rfh());
  // Should not crash.
}

TEST_F(ChromeSensorDelegateTest, OnSensorStartedNoCrashWithoutSettings) {
  ChromeSensorDelegate delegate;
  delegate.OnSensorStarted(main_rfh());
  // Should not crash.
}

TEST_F(ChromeSensorDelegateTest, OnSensorStoppedNoCrashWithoutSettings) {
  ChromeSensorDelegate delegate;
  delegate.OnSensorStopped(main_rfh());
  // Should not crash.
}

TEST_F(ChromeSensorDelegateTest, SetRequestedSensorIsAvailableWithSettings) {
  ChromeSensorDelegate delegate;
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));

  auto* settings =
      content_settings::PageSpecificContentSettings::GetForFrame(main_rfh());
  ASSERT_TRUE(settings);
  EXPECT_FALSE(settings->is_any_requested_sensor_available());

  delegate.SetRequestedSensorIsAvailable(main_rfh(), true);

  EXPECT_TRUE(settings->is_any_requested_sensor_available());
}

TEST_F(ChromeSensorDelegateTest, OnSensorBlockedWithSettings) {
  ChromeSensorDelegate delegate;
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));

  auto* settings =
      content_settings::PageSpecificContentSettings::GetForFrame(main_rfh());
  ASSERT_TRUE(settings);
  EXPECT_FALSE(settings->IsContentBlocked(ContentSettingsType::SENSORS));

  delegate.OnSensorBlocked(main_rfh());

  EXPECT_TRUE(settings->IsContentBlocked(ContentSettingsType::SENSORS));
}

TEST_F(ChromeSensorDelegateTest, OnSensorStartedWithSettings) {
  ChromeSensorDelegate delegate;
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));

  auto* settings =
      content_settings::PageSpecificContentSettings::GetForFrame(main_rfh());
  ASSERT_TRUE(settings);
  EXPECT_EQ(0, settings->active_available_sensors());

  delegate.OnSensorStarted(main_rfh());

  EXPECT_EQ(1, settings->active_available_sensors());
  EXPECT_TRUE(settings->IsContentAllowed(ContentSettingsType::SENSORS));
}

TEST_F(ChromeSensorDelegateTest, OnSensorStoppedWithSettings) {
  ChromeSensorDelegate delegate;
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents(),
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));

  auto* settings =
      content_settings::PageSpecificContentSettings::GetForFrame(main_rfh());
  ASSERT_TRUE(settings);

  // Start the sensor first to avoid negative count DCHECK.
  delegate.OnSensorStarted(main_rfh());
  EXPECT_EQ(1, settings->active_available_sensors());

  delegate.OnSensorStopped(main_rfh());

  EXPECT_EQ(0, settings->active_available_sensors());
}
