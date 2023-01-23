// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/intent_helper/custom_tab_session_impl.h"

#include "ash/components/arc/test/arc_util_test_support.h"
#include "base/containers/contains.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/arc/intent_helper/custom_tab.h"
#include "components/exo/shell_surface.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/aura/window.h"

class CustomTabSessionImplTest : public InProcessBrowserTest,
                                 public content::WebContentsObserver {
 public:
  CustomTabSessionImplTest() {}
  ~CustomTabSessionImplTest() override {}
  CustomTabSessionImplTest(const CustomTabSessionImplTest&) = delete;
  CustomTabSessionImplTest& operator=(const CustomTabSessionImplTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    wm_helper_ = std::make_unique<exo::WMHelper>();
    DCHECK(exo::WMHelper::HasInstance());
  }

  void TearDownOnMainThread() override {
    DCHECK(exo::WMHelper::HasInstance());
    wm_helper_.reset();
  }

  // WebContentsObserver
  void WebContentsDestroyed() override {
    if (web_contents_destroyed_closure_)
      std::move(web_contents_destroyed_closure_).Run();
  }

  content::WebContents* GetWebContents() { return web_contents(); }

 protected:
  void CreateAndDestroyCustomTabSession(
      std::unique_ptr<arc::CustomTab> custom_tab,
      Browser* browser) {
    CustomTabSessionImpl(std::move(custom_tab), browser);
  }

  void DestroyCustomTabSession() { delete custom_tab_session_; }

  base::RepeatingClosure web_contents_destroyed_closure_;

 private:
  std::unique_ptr<exo::WMHelper> wm_helper_;
  CustomTabSessionImpl* custom_tab_session_ = nullptr;
};

// Calls |callback| when |browser| is removed from BrowserList.
class BrowserRemovalObserver final : public BrowserListObserver {
 public:
  BrowserRemovalObserver(Browser* browser, base::OnceClosure callback)
      : browser_(browser), callback_(std::move(callback)) {
    BrowserList::AddObserver(this);
  }
  ~BrowserRemovalObserver() override = default;

  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_) {
      BrowserList::RemoveObserver(this);
      std::move(callback_).Run();
    }
  }

 private:
  Browser* browser_;
  base::OnceClosure callback_;
};

IN_PROC_BROWSER_TEST_F(CustomTabSessionImplTest,
                       WebContentsAndBrowserDestroyedWithCustomTabSession) {
  std::unique_ptr<exo::ShellSurface> test_window =
      exo::test::ShellSurfaceBuilder({640, 480})
          .SetCentered()
          .BuildShellSurface();
  aura::Window* aura_window = test_window->GetWidget()->GetNativeWindow();
  ASSERT_TRUE(aura_window);

  auto custom_tab = std::make_unique<arc::CustomTab>(aura_window);

  auto web_contents = arc::CreateArcCustomTabWebContents(browser()->profile(),
                                                         GURL("http://foo/"));
  Observe(web_contents.get());

  // The base class has already attached a WebContents with the Browser object.
  // We replace it, to simulate the single-tab ARC custom tab browser.
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  browser()->tab_strip_model()->ReplaceWebContentsAt(0,
                                                     std::move(web_contents));
  base::RunLoop webcontents_run_loop, browser_run_loop;
  web_contents_destroyed_closure_ = webcontents_run_loop.QuitClosure();
  BrowserRemovalObserver removed_waiter(browser(),
                                        browser_run_loop.QuitClosure());
  CreateAndDestroyCustomTabSession(std::move(custom_tab), browser());
  webcontents_run_loop.Run();
  EXPECT_FALSE(GetWebContents());
  browser_run_loop.Run();
  ASSERT_FALSE(base::Contains(*BrowserList::GetInstance(), browser()));
}
