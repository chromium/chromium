// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browsertest_util.h"

#include <memory>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_paths.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#endif

namespace extensions::browsertest_util {

void CreateAndInitializeLocalCache() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath extension_cache_dir;
  CHECK(base::PathService::Get(ash::DIR_DEVICE_EXTENSION_LOCAL_CACHE,
                               &extension_cache_dir));
  base::FilePath cache_init_file = extension_cache_dir.Append(
      extensions::LocalExtensionCache::kCacheReadyFlagFileName);
  EXPECT_TRUE(base::WriteFile(cache_init_file, ""));
#endif
}

Browser* LaunchAppBrowser(Profile* profile, const Extension* extension_app) {
  ui_test_utils::BrowserChangeObserver browser_change_observer(
      /*browser=*/nullptr,
      ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);

  EXPECT_TRUE(apps::AppServiceProxyFactory::GetForProfile(profile)
                  ->BrowserAppLauncher()
                  ->LaunchAppWithParamsForTesting(apps::AppLaunchParams(
                      extension_app->id(),
                      apps::LaunchContainer::kLaunchContainerWindow,
                      WindowOpenDisposition::CURRENT_TAB,
                      apps::LaunchSource::kFromTest)));

  Browser* const browser = browser_change_observer.Wait();
  DCHECK(browser);
  EXPECT_EQ(web_app::GetAppIdFromApplicationName(browser->app_name()),
            extension_app->id());
  return browser;
}

content::WebContents* AddTab(Browser* browser, const GURL& url) {
  int starting_tab_count = browser->tab_strip_model()->count();
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  int tab_count = browser->tab_strip_model()->count();
  EXPECT_EQ(starting_tab_count + 1, tab_count);
  return browser->tab_strip_model()->GetActiveWebContents();
}

size_t GetWindowControllerCountInProfile(Profile* profile) {
  size_t count = 0;
  for (WindowController* window : *WindowControllerList::GetInstance()) {
    if (window->profile() == profile) {
      count++;
    }
  }
  return count;
}

bool DidChangeTitle(content::WebContents& web_contents,
                    const std::u16string& original_title,
                    const std::u16string& changed_title) {
  const std::u16string& title = web_contents.GetTitle();
  if (title == changed_title) {
    return true;
  }
  if (title == original_title) {
    return false;
  }
  ADD_FAILURE() << "Unexpected page title found:  " << title;
  return false;
}

BlockedActionWaiter::BlockedActionWaiter(ExtensionActionRunner* runner)
    : runner_(runner) {
  runner_->set_observer_for_testing(this);  // IN-TEST
}

BlockedActionWaiter::~BlockedActionWaiter() {
  runner_->set_observer_for_testing(nullptr);  // IN-TEST
}

void BlockedActionWaiter::Wait() {
  run_loop_.Run();
}

void BlockedActionWaiter::OnBlockedActionAdded() {
  run_loop_.Quit();
}

}  // namespace extensions::browsertest_util
