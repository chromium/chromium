// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/clipboard.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/aura/window.h"

class WebContentsCanGoBackObserverTest : public InProcessBrowserTest {
 protected:
  WebContentsCanGoBackObserverTest() = default;

  WebContentsCanGoBackObserverTest(const WebContentsCanGoBackObserverTest&) =
      delete;
  WebContentsCanGoBackObserverTest& operator=(
      const WebContentsCanGoBackObserverTest&) = delete;

  void CheckCanGoBackOnServer(const std::string& window_id,
                              bool expected_value) {
    base::RunLoop outer_loop;
    auto look_for_property_value = base::BindRepeating(
        [](base::RunLoop* outer_loop, const std::string& window_id,
           bool expected_value) {
          auto* lacros_service = chromeos::LacrosService::Get();

          base::RunLoop inner_loop(base::RunLoop::Type::kNestableTasksAllowed);
          crosapi::mojom::OptionalBoolean out_value;
          lacros_service->GetRemote<crosapi::mojom::TestController>()
              ->GetMinimizeOnBackKeyWindowProperty(
                  window_id, base::BindOnce(
                                 [](base::RunLoop* loop,
                                    crosapi::mojom::OptionalBoolean* out_value,
                                    crosapi::mojom::OptionalBoolean value) {
                                   *out_value = value;
                                   loop->Quit();
                                 },
                                 &inner_loop, &out_value));
          inner_loop.Run();

          // can-go-back and minimize-on-back-gesture are always contrary.
          if ((expected_value &&
               out_value == crosapi::mojom::OptionalBoolean::kFalse) ||
              (!expected_value &&
               out_value == crosapi::mojom::OptionalBoolean::kTrue))
            outer_loop->Quit();
        },
        &outer_loop, window_id, expected_value);
    base::RepeatingTimer timer;
    timer.Start(FROM_HERE, base::Milliseconds(1),
                std::move(look_for_property_value));
    outer_loop.Run();
  }

  ~WebContentsCanGoBackObserverTest() override = default;
};

IN_PROC_BROWSER_TEST_F(WebContentsCanGoBackObserverTest, CanGoBack_ServerSide) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::TestController>());

  aura::Window* window = BrowserView::GetBrowserViewForBrowser(browser())
                             ->frame()
                             ->GetNativeWindow();
  std::string id =
      lacros_window_utility::GetRootWindowUniqueId(window->GetRootWindow());
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(id));

  EXPECT_FALSE(chrome::CanGoBack(browser()));
  EXPECT_FALSE(chrome::CanGoForward(browser()));

  // Navigate away to any valid URL, so the back/forward list changes.
  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUIAboutURL),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_FALSE(chrome::CanGoForward(browser()));
  CheckCanGoBackOnServer(id, true /* expected_value */);

  // Tweak the back/forward list.
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  EXPECT_FALSE(chrome::CanGoBack(browser()));
  EXPECT_TRUE(chrome::CanGoForward(browser()));
  CheckCanGoBackOnServer(id, false /* expected_value */);
}

IN_PROC_BROWSER_TEST_F(WebContentsCanGoBackObserverTest,
                       CanGoBackMultipleTabs_ServerSide) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::TestController>());

  aura::Window* window = BrowserView::GetBrowserViewForBrowser(browser())
                             ->frame()
                             ->GetNativeWindow();
  std::string id =
      lacros_window_utility::GetRootWindowUniqueId(window->GetRootWindow());
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(id));

  EXPECT_FALSE(chrome::CanGoBack(browser()));
  EXPECT_FALSE(chrome::CanGoForward(browser()));

  // Navigate away to any valid URL, so the back/forward list changes.
  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUIAboutURL),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_FALSE(chrome::CanGoForward(browser()));
  CheckCanGoBackOnServer(id, true /* expected_value */);

  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUIVersionURL),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  EXPECT_FALSE(chrome::CanGoBack(browser()));
  EXPECT_FALSE(chrome::CanGoForward(browser()));
  CheckCanGoBackOnServer(id, false /* expected_value */);

  // Navigate the current (second) tab to a different URL, so we can test
  // back/forward later.
  NavigateToURLWithDisposition(browser(), GURL(chrome::kChromeUIFlagsURL),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_FALSE(chrome::CanGoForward(browser()));
  CheckCanGoBackOnServer(id, true /* expected_value */);

  // Tweak the back/forward list of the 2nd tab, and verify.
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  EXPECT_FALSE(chrome::CanGoBack(browser()));
  EXPECT_TRUE(chrome::CanGoForward(browser()));
  CheckCanGoBackOnServer(id, false /* expected_value */);

  // Switch to a different tab, and verify whether the `can go back` property
  // updates accordingly.
  browser()->tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  EXPECT_TRUE(chrome::CanGoBack(browser()));
  EXPECT_FALSE(chrome::CanGoForward(browser()));
  CheckCanGoBackOnServer(id, true /* expected_value */);
}
