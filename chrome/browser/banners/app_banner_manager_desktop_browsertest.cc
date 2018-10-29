// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/banners/app_banner_manager_browsertest_base.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"

namespace {

class FakeAppBannerManagerDesktop : public banners::AppBannerManagerDesktop {
 public:
  explicit FakeAppBannerManagerDesktop(content::WebContents* web_contents)
      : AppBannerManagerDesktop(web_contents) {}

  static FakeAppBannerManagerDesktop* CreateForWebContents(
      content::WebContents* web_contents) {
    web_contents->SetUserData(
        UserDataKey(),
        std::make_unique<FakeAppBannerManagerDesktop>(web_contents));
    return static_cast<FakeAppBannerManagerDesktop*>(
        web_contents->GetUserData(UserDataKey()));
  }

  // Configures a callback to be invoked when the app banner flow finishes.
  void PrepareDone(base::OnceClosure on_done) { on_done_ = std::move(on_done); }

  State state() { return AppBannerManager::state(); }

 protected:
  void OnFinished() {
    if (on_done_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                    std::move(on_done_));
    }
  }

  void DidFinishCreatingBookmarkApp(
      const extensions::Extension* extension,
      const WebApplicationInfo& web_app_info) override {
    AppBannerManagerDesktop::DidFinishCreatingBookmarkApp(extension,
                                                          web_app_info);
    OnFinished();
  }

  void UpdateState(AppBannerManager::State state) override {
    AppBannerManager::UpdateState(state);

    if (state == AppBannerManager::State::PENDING_ENGAGEMENT ||
        state == AppBannerManager::State::PENDING_PROMPT) {
      OnFinished();
    }
  }

 private:
  base::OnceClosure on_done_;

  DISALLOW_COPY_AND_ASSIGN(FakeAppBannerManagerDesktop);
};

}  // anonymous namespace

using State = banners::AppBannerManager::State;

class AppBannerManagerDesktopBrowserTest
    : public AppBannerManagerBrowserTestBase {
 public:
  AppBannerManagerDesktopBrowserTest() : AppBannerManagerBrowserTestBase() {}

  void SetUpOnMainThread() override {
    // Trigger banners instantly.
    AppBannerSettingsHelper::SetTotalEngagementToTrigger(0);
    chrome::SetAutoAcceptPWAInstallDialogForTesting(true);

    feature_list_.InitWithFeatures(
        {features::kExperimentalAppBanners, features::kDesktopPWAWindowing},
        {});

    AppBannerManagerBrowserTestBase::SetUpOnMainThread();
  }

  void TearDown() override {
    chrome::SetAutoAcceptPWAInstallDialogForTesting(false);
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerDesktopBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       WebAppBannerResolvesUserChoice) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* manager =
      FakeAppBannerManagerDesktop::CreateForWebContents(web_contents);

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ui_test_utils::NavigateToURL(browser(),
                                 GetBannerURLWithAction("stash_event"));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT, manager->state());
  }

  {
    // Trigger the installation prompt and wait for installation to occur.
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());
    ExecuteScript(browser(), "callStashedPrompt();", true /* with_gesture */);
    run_loop.Run();
    EXPECT_EQ(State::COMPLETE, manager->state());
  }

  // Ensure that the userChoice promise resolves.
  const base::string16 title = base::ASCIIToUTF16("Got userChoice: accepted");
  content::TitleWatcher watcher(web_contents, title);
  EXPECT_EQ(title, watcher.WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerDesktopBrowserTest,
                       WebAppBannerFiresAppInstalled) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* manager =
      FakeAppBannerManagerDesktop::CreateForWebContents(web_contents);

  {
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());

    ui_test_utils::NavigateToURL(
        browser(), GetBannerURLWithAction("verify_appinstalled_stash_event"));
    run_loop.Run();
    EXPECT_EQ(State::PENDING_PROMPT, manager->state());
  }

  {
    // Trigger the installation prompt and wait for installation to occur.
    base::RunLoop run_loop;
    manager->PrepareDone(run_loop.QuitClosure());
    ExecuteScript(browser(), "callStashedPrompt();", true /* with_gesture */);
    run_loop.Run();
    EXPECT_EQ(State::COMPLETE, manager->state());
  }

  // Ensure that the appinstalled event fires.
  const base::string16 title =
      base::ASCIIToUTF16("Got appinstalled: listener, attr");
  content::TitleWatcher watcher(web_contents, title);
  EXPECT_EQ(title, watcher.WaitAndGetTitle());
}
