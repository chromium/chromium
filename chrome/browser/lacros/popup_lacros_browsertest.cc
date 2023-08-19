// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/timer/timer.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/test/widget_test.h"

namespace {

// Waits for the window identified by |window_id| to have its ash-side window
// position in DIP screen coordinates set to |target_position|.
void WaitForWindowPositionInScreen(const std::string& window_id,
                                   const gfx::Point& target_position) {
  base::test::TestFuture<const absl::optional<gfx::Point>&> future;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    chromeos::LacrosService::Get()
        ->GetRemote<crosapi::mojom::TestController>()
        ->GetWindowPositionInScreen(window_id, future.GetCallback());
    return future.Take() == target_position;
  }));
}

void SendLongPress(const std::string& window_id,
                   const gfx::PointF& location_in_window) {
  base::test::TestFuture<void> future;

  // Generate a touch press in ash, because the bug requires lacros to receive
  // events over the Wayland connection from ash.
  auto& test_controller = chromeos::LacrosService::Get()
                              ->GetRemote<crosapi::mojom::TestController>();
  test_controller->SendTouchEvent(
      window_id, crosapi::mojom::TouchEventType::kPressed,
      /*pointer_id=*/0u, location_in_window, future.GetCallback());
  future.Get();

  // Wait long enough that the gesture recognizer will decide this is a long
  // press gesture. We cannot directly inject a long press from ash because
  // Wayland only transports press/move/release events, not gestures.
  base::RunLoop loop2;
  base::OneShotTimer timer2;
  timer2.Start(FROM_HERE, base::Seconds(1), loop2.QuitClosure());
  loop2.Run();

  // Release the touch in ash.
  test_controller->SendTouchEvent(
      window_id, crosapi::mojom::TouchEventType::kReleased,
      /*pointer_id=*/0u, location_in_window, future.GetCallback());
  future.Get();
}

using PopupBrowserTest = InProcessBrowserTest;

// Regression test for https://crbug.com/1157664. Verifying that opening a
// menu via long-press on a tab does not result in a popup window with empty
// bounds. In bug caused a Wayland protocol error and lacros crash.
IN_PROC_BROWSER_TEST_F(PopupBrowserTest, LongPressOnTabOpensNonEmptyMenu) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::TestController>());
  // This test requires the tablet mode API.
  if (lacros_service->GetInterfaceVersion<crosapi::mojom::TestController>() <
      3) {
    LOG(WARNING) << "Unsupported ash version.";
    return;
  }

  // Ensure the browser is maximized. The bug only occurs when the tab strip is
  // near the top of the screen.
  browser()->window()->Maximize();

  // Wait for the window to be created.
  aura::Window* window = browser()->window()->GetNativeWindow();
  std::string window_id =
      lacros_window_utility::GetRootWindowUniqueId(window->GetRootWindow());
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(window_id));

  // Wait for the window to be globally positioned at 0,0. It will eventually
  // have this position because it is maximized. We cannot assert the position
  // lacros-side because Wayland clients do not know the position of their
  // windows on the display.
  WaitForWindowPositionInScreen(window_id, gfx::Point(0, 0));

  // Precondition: The browser is the only open widget.
  std::set<views::Widget*> initial_widgets =
      views::test::WidgetTest::GetAllWidgets();
  ASSERT_EQ(1u, initial_widgets.size());
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  views::Widget* browser_widget = browser_view->GetWidget();
  ASSERT_EQ(browser_widget, *initial_widgets.begin());

  // Find the center position of the first tab. We cannot use "screen"
  // coordinates because the Wayland client does not know where its window is
  // located on the screen. Use widget-relative position instead.
  Tab* tab = browser_view->tabstrip()->tab_at(0);
  gfx::Point tab_center_in_widget = tab->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToWidget(tab, &tab_center_in_widget);

  SendLongPress(window_id, gfx::PointF(tab_center_in_widget));

  // Wait for the popup menu to be created and positioned on screen.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    std::set<views::Widget*> widgets = views::test::WidgetTest::GetAllWidgets();
    widgets.erase(browser_widget);
    if (widgets.size() == 0u) {
      return false;
    }
    // The popup was created.
    views::Widget* popup = *widgets.begin();
    // The popup's top edge may be off the top of the screen. Wait for it to
    // be positioned on screen. The bug involved the repositioning code. We
    // know that 0 means the top of the screen because the window is
    // maximized.
    return popup->GetRestoredBounds().y() >= 0;
  }));

  // Find the popup.
  std::set<views::Widget*> widgets = views::test::WidgetTest::GetAllWidgets();
  widgets.erase(browser_widget);
  ASSERT_EQ(1u, widgets.size());
  views::Widget* popup = *widgets.begin();

  // The popup has valid bounds.
  gfx::Rect popup_bounds = popup->GetRestoredBounds();
  EXPECT_FALSE(popup_bounds.IsEmpty()) << popup_bounds.ToString();
}

}  // namespace
