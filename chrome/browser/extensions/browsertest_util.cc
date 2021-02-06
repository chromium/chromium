// Copyright 2013 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_paths.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#endif

namespace extensions {
namespace browsertest_util {

void CreateAndInitializeLocalCache() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::FilePath extension_cache_dir;
  CHECK(base::PathService::Get(chromeos::DIR_DEVICE_EXTENSION_LOCAL_CACHE,
                               &extension_cache_dir));
  base::FilePath cache_init_file = extension_cache_dir.Append(
      extensions::LocalExtensionCache::kCacheReadyFlagFileName);
  EXPECT_TRUE(base::WriteFile(cache_init_file, ""));
#endif
}

Browser* LaunchAppBrowser(Profile* profile, const Extension* extension_app) {
  EXPECT_TRUE(
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParams(apps::AppLaunchParams(
              extension_app->id(), LaunchContainer::kLaunchContainerWindow,
              WindowOpenDisposition::CURRENT_TAB,
              AppLaunchSource::kSourceTest)));

  Browser* browser = chrome::FindLastActive();
  bool is_correct_app_browser =
      browser && web_app::GetAppIdFromApplicationName(browser->app_name()) ==
                     extension_app->id();
  EXPECT_TRUE(is_correct_app_browser);

  return is_correct_app_browser ? browser : nullptr;
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

}  // namespace browsertest_util
}  // namespace extensions
