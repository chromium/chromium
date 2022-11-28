// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"

namespace {

// If ash does not contain the relevant test controller functionality, then
// there's nothing to do for this test.
bool IsServiceAvailable() {
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kEnterOverviewModeMinVersion)) {
    LOG(WARNING) << "Unsupported ash version.";
    return false;
  }
  return true;
}

}  // namespace

using OverviewBrowserTest = InProcessBrowserTest;

// We enter overview mode with a single window. When we close the window,
// overview mode automatically exits. Check that there's no crash.
// TODO(https://crbug.com/1157314): This test is not safe to run in parallel
// with other lacros tests as overview mode applies to all processes.
IN_PROC_BROWSER_TEST_F(OverviewBrowserTest, NoCrashWithSingleWindow) {
  if (!IsServiceAvailable())
    return;

  // Wait for the window to be visible.
  aura::Window* window = browser()->window()->GetNativeWindow();
  std::string id =
      lacros_window_utility::GetRootWindowUniqueId(window->GetRootWindow());
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(id));

  // Enter overview mode.
  auto* lacros_service = chromeos::LacrosService::Get();
  crosapi::mojom::TestControllerAsyncWaiter waiter(
      lacros_service->GetRemote<crosapi::mojom::TestController>().get());
  waiter.EnterOverviewMode();

  // Close the window by closing all tabs and wait for it to stop existing in
  // ash.
  browser()->tab_strip_model()->CloseAllTabs();
  ASSERT_TRUE(browser_test_util::WaitForWindowDestruction(id));
}

// We enter overview mode with 2 windows. We delete 1 window during overview
// mode. Then we exit overview mode.
// TODO(https://crbug.com/1157314): This test is not safe to run in parallel
// with other lacros tests as overview mode applies to all processes.
IN_PROC_BROWSER_TEST_F(OverviewBrowserTest, NoCrashTwoWindows) {
  if (!IsServiceAvailable())
    return;

  // Wait for the window to be visible.
  aura::Window* main_window = browser()->window()->GetNativeWindow();
  std::string main_id = lacros_window_utility::GetRootWindowUniqueId(
      main_window->GetRootWindow());
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(main_id));

  // Create an incognito window and make it visible.
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      true));
  AddBlankTabAndShow(incognito_browser);
  aura::Window* incognito_window =
      incognito_browser->window()->GetNativeWindow();
  std::string incognito_id = lacros_window_utility::GetRootWindowUniqueId(
      incognito_window->GetRootWindow());
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(incognito_id));

  // Enter overview mode.
  auto* lacros_service = chromeos::LacrosService::Get();
  crosapi::mojom::TestControllerAsyncWaiter waiter(
      lacros_service->GetRemote<crosapi::mojom::TestController>().get());
  waiter.EnterOverviewMode();

  // Close the incognito window by closing all tabs and wait for it to stop
  // existing in ash.
  incognito_browser->tab_strip_model()->CloseAllTabs();
  ASSERT_TRUE(browser_test_util::WaitForWindowDestruction(incognito_id));

  // Exit overview mode.
  waiter.ExitOverviewMode();
}
