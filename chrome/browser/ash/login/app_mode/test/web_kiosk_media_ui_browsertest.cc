// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/system/media/media_tray.h"
#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_factory.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

using kiosk::test::WaitKioskLaunched;

namespace {

// Returns a valid `StartPresentationContext` that does nothing.
std::unique_ptr<media_router::StartPresentationContext>
DummyPresentationContext(Browser& browser) {
  return std::make_unique<media_router::StartPresentationContext>(
      content::PresentationRequest(browser.tab_strip_model()
                                       ->GetActiveWebContents()
                                       ->GetPrimaryMainFrame()
                                       ->GetGlobalId(),
                                   /*presentation_urls=*/{GURL(), GURL()},
                                   /*frame_origin=*/url::Origin()),
      /*success_cb=*/base::DoNothing(),
      /*error_cb=*/base::DoNothing());
}

}  // namespace

class WebKioskMediaUITest : public MixinBasedInProcessBrowserTest {
 public:
  WebKioskMediaUITest() = default;

  WebKioskMediaUITest(const WebKioskMediaUITest&) = delete;
  WebKioskMediaUITest& operator=(const WebKioskMediaUITest&) = delete;

  ~WebKioskMediaUITest() override = default;

  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/
      KioskMixin::Config{/*name=*/{},
                         KioskMixin::AutoLaunchAccount{
                             KioskMixin::SimpleWebAppOption().account_id},
                         {KioskMixin::SimpleWebAppOption()}}};
};

IN_PROC_BROWSER_TEST_F(WebKioskMediaUITest, MediaTrayStaysPinnedInKiosk) {
  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  ASSERT_TRUE(WaitKioskLaunched());
  SetBrowser(browser_created_observer.Wait());

  ash::MediaTray::SetPinnedToShelf(false);
  ASSERT_FALSE(ash::MediaTray::IsPinnedToShelf());

  MediaNotificationServiceFactory::GetForProfile(GetProfile())
      ->ShowDialogAsh(DummyPresentationContext(CHECK_DEREF(browser())));

  EXPECT_TRUE(ash::MediaTray::IsPinnedToShelf());
}

}  // namespace ash
