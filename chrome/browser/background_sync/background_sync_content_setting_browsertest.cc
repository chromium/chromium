// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/background_sync/background_sync_controller_impl.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

const char kExampleUrl[] = "https://www.example.com/foo/";

}  // namespace

class BackgroundSyncContentSettingBrowserTest : public InProcessBrowserTest {
 public:
  BackgroundSyncContentSettingBrowserTest() = default;

  BackgroundSyncContentSettingBrowserTest(
      const BackgroundSyncContentSettingBrowserTest&) = delete;
  BackgroundSyncContentSettingBrowserTest& operator=(
      const BackgroundSyncContentSettingBrowserTest&) = delete;

  ~BackgroundSyncContentSettingBrowserTest() override = default;

  // ---------------------------------------------------------------------------
  // Helper functions.

  void SetBackgroundSyncContentSetting(const GURL& url,
                                       ContentSetting setting) {
    auto* profile = browser()->profile();
    auto* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile);
    ASSERT_TRUE(host_content_settings_map);
    host_content_settings_map->SetContentSettingDefaultScope(
        /* primary_url= */ url, /* secondary_url= */ url,
        ContentSettingsType::BACKGROUND_SYNC, setting);
  }
};

IN_PROC_BROWSER_TEST_F(BackgroundSyncContentSettingBrowserTest,
                       BlockingContentSettingUnregistersPeriodicSync) {
  auto* controller = static_cast<BackgroundSyncControllerImpl*>(
      browser()->profile()->GetBackgroundSyncController());
  DCHECK(controller);

  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));

  controller->AddToTrackedOrigins(origin);
  SetBackgroundSyncContentSetting(origin.GetURL(), CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(controller->IsOriginTracked(origin));
}
