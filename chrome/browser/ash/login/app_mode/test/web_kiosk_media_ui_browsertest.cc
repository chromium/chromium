// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_tray.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using WebKioskMediaUITest = WebKioskBaseTest;

IN_PROC_BROWSER_TEST_F(WebKioskMediaUITest, MediaTrayStaysPinnedInKiosk) {
  InitializeRegularOnlineKiosk();
  SelectFirstBrowser();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple_page.html")));

  ash::MediaTray::SetPinnedToShelf(false);
  ASSERT_FALSE(ash::MediaTray::IsPinnedToShelf());

  auto presentation_context =
      std::make_unique<media_router::StartPresentationContext>(
          content::PresentationRequest(browser()
                                           ->tab_strip_model()
                                           ->GetActiveWebContents()
                                           ->GetPrimaryMainFrame()
                                           ->GetGlobalId(),
                                       {GURL(), GURL()}, url::Origin()),
          base::DoNothing(), base::DoNothing());

  MediaNotificationServiceFactory::GetForProfile(GetProfile())
      ->ShowDialogAsh(std::move(presentation_context));

  EXPECT_TRUE(ash::MediaTray::IsPinnedToShelf());
}
}  // namespace ash
