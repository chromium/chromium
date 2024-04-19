// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/app_constants/constants.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_show_state_waiter.h"
#include "ui/views/widget/widget.h"

using crosapi::mojom::ShelfItemState;

namespace {

class LacrosBrowserShelfLacrosBrowserTest : public InProcessBrowserTest {
 public:
  views::Widget* GetWidgetForBrowser(Browser* browser) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    CHECK(browser_view);
    views::Widget* widget = browser_view->GetWidget();
    CHECK(widget);
    return widget;
  }
};

// Test that clicking on Lacros icon has the expected effect of toggling between
// minimized state and visible state when there is one browser window.
IN_PROC_BROWSER_TEST_F(LacrosBrowserShelfLacrosBrowserTest, ClickOnShelf) {
  crosapi::mojom::TestController* const test_controller =
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get();
  views::Widget* widget = GetWidgetForBrowser(browser());
  views::test::WaitForWidgetActive(widget, true);
  ASSERT_TRUE(browser_test_util::WaitForShelfItemState(
      app_constants::kLacrosAppId,
      static_cast<uint32_t>(ShelfItemState::kActive)));
  ASSERT_TRUE(browser()->window()->IsVisible());
  ASSERT_FALSE(browser()->window()->IsMinimized());

  base::test::TestFuture<bool> success_future;
  test_controller->SelectItemInShelf(app_constants::kLacrosAppId,
                                     success_future.GetCallback());
  ASSERT_TRUE(success_future.Take());
  views::test::WaitForWidgetShowState(widget, ui::SHOW_STATE_MINIMIZED);
  ASSERT_FALSE(browser()->window()->IsVisible());
  ASSERT_TRUE(browser()->window()->IsMinimized());

  test_controller->SelectItemInShelf(app_constants::kLacrosAppId,
                                     success_future.GetCallback());
  ASSERT_TRUE(success_future.Take());
  views::test::WaitForWidgetActive(widget, true);
  ASSERT_TRUE(browser()->window()->IsVisible());
  ASSERT_FALSE(browser()->window()->IsMinimized());
}

}  // namespace
