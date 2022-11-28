// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/lacros/browser_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/lacros/window_utility.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_scrubber_chromeos.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/aura/window.h"

namespace {

using TabScrubberBrowserTest = InProcessBrowserTest;

void AddBlankTab(Browser* browser) {
  content::WebContents* blank_tab = chrome::AddSelectedTabWithURL(
      browser, GURL(url::kAboutBlankURL), ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  content::TestNavigationObserver observer(blank_tab);
  observer.Wait();
}

TabStrip* GetTabStrip(Browser* browser) {
  aura::Window* window = browser->window()->GetNativeWindow();
  // This test depends on TabStrip impl.
  TabStrip* tab_strip =
      BrowserView::GetBrowserViewForNativeWindow(window)->tabstrip();
  DCHECK(tab_strip);
  return tab_strip;
}

// Regression test for https://crbug.com/1267344
//
// It verifies that triggering a tab scrubbing request when a lacros window
// is upfront and active, does not activates the logic in Ash, and forwards
// the call to Lacros.
// TODO(crbug.com/1298835): Flaking very badly
IN_PROC_BROWSER_TEST_F(TabScrubberBrowserTest, DISABLED_Smoke) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service->IsAvailable<crosapi::mojom::TestController>());
  // This test requires the tab scrubbing API.
  if (lacros_service->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kTriggerTabScrubbingMinVersion)) {
    return;
  }

  // Wait for the window to be created.
  aura::Window* window = browser()->window()->GetNativeWindow();
  std::string window_id =
      lacros_window_utility::GetRootWindowUniqueId(window->GetRootWindow());
  ASSERT_TRUE(browser_test_util::WaitForWindowCreation(window_id));

  // Add further 5 blank tabs.
  for (int i = 0; i < 5; ++i)
    AddBlankTab(browser());

  // Active tab should be the last one.
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 5);

  TabStrip* tab_strip = GetTabStrip(browser());
  tab_strip->StopAnimating(true);

  float x_offset = -200.f;

  // Attempt to perform a tab scrubbing through Ash, and it should bail out.
  bool scrubbing = false;
  crosapi::mojom::TestControllerAsyncWaiter waiter(
      lacros_service->GetRemote<crosapi::mojom::TestController>().get());
  waiter.TriggerTabScrubbing(x_offset, &scrubbing);
  ASSERT_FALSE(scrubbing);
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 5);

  // Perform the tab scrubbing in lacros.
  //
  // NOTE: In case it is possible to make the BrowserManager to call this
  // out in Lacros, the code below can be simplified.
  TabScrubberChromeOS::GetInstance()->SynthesizedScrollEvent(x_offset);
  ASSERT_TRUE(TabScrubberChromeOS::GetInstance()->IsActivationPending());

  // Wait 200ms, which is the default delay on tab_scribber_chromeos.cc.
  base::RunLoop loop;
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(200), loop.QuitClosure());
  loop.Run();

  ASSERT_NE(browser()->tab_strip_model()->active_index(), 5);
}

}  // namespace
