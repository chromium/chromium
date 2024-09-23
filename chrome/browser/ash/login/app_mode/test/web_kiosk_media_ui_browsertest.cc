// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_tray.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/media_ui_ash.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
class WebKioskMediaUITest : public WebKioskBaseTest {
 public:
  WebKioskMediaUITest() = default;

  static crosapi::MediaUIAsh* media_ui_ash() {
    return crosapi::CrosapiManager::Get()->crosapi_ash()->media_ui_ash();
  }
};

IN_PROC_BROWSER_TEST_F(WebKioskMediaUITest, MediaTrayStaysPinnedInKiosk) {
  InitializeRegularOnlineKiosk();
  SelectFirstBrowser();

  ash::MediaTray::SetPinnedToShelf(false);
  ASSERT_FALSE(ash::MediaTray::IsPinnedToShelf());

  media_ui_ash()->ShowDevicePicker("test_item_id");
  EXPECT_TRUE(ash::MediaTray::IsPinnedToShelf());
}
}  // namespace ash
