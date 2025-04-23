// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_browsertest_platform_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/test/browser_test_utils.h"

namespace extensions {

ExtensionBrowserTestPlatformDelegate::ExtensionBrowserTestPlatformDelegate(
    ExtensionBrowserTest& parent)
    : parent_(parent) {}

Profile* ExtensionBrowserTestPlatformDelegate::GetProfile() {
  if (!profile_) {
    if (parent_->browser()) {
      profile_ = parent_->browser()->profile();
    } else {
      profile_ = ProfileManager::GetLastUsedProfile();
    }
  }
  return profile_;
}

void ExtensionBrowserTestPlatformDelegate::SetUpOnMainThread() {
  content::URLDataSource::Add(GetProfile(),
                              std::make_unique<ThemeSource>(GetProfile()));
}

void ExtensionBrowserTestPlatformDelegate::OpenURL(const GURL& url,
                                                   bool open_in_incognito) {
  if (open_in_incognito) {
    parent_->OpenURLOffTheRecord(GetProfile(), url);
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(parent_->browser(), url));
  }
}

const Extension* ExtensionBrowserTestPlatformDelegate::LoadAndLaunchApp(
    const base::FilePath& path,
    bool uses_guest_view) {
  const Extension* app = parent_->LoadExtension(path);
  CHECK(app);
  content::CreateAndLoadWebContentsObserver app_loaded_observer(
      /*num_expected_contents=*/uses_guest_view ? 2 : 1);
  apps::AppLaunchParams params(
      app->id(), apps::LaunchContainer::kLaunchContainerNone,
      WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);
  params.command_line = *base::CommandLine::ForCurrentProcess();
  apps::AppServiceProxyFactory::GetForProfile(GetProfile())
      ->BrowserAppLauncher()
      ->LaunchAppWithParamsForTesting(std::move(params));
  app_loaded_observer.Wait();

  return app;
}

bool ExtensionBrowserTestPlatformDelegate::WaitForPageActionVisibilityChangeTo(
    int count) {
  // Note: It's okay if the visibility is already at `count` (i.e., that we're
  // constructing this observer "late"); the observer handles that case
  // gracefully.
  std::unique_ptr<ChromeExtensionTestNotificationObserver> observer =
      parent_->browser()
          ? std::make_unique<ChromeExtensionTestNotificationObserver>(
                parent_->browser())
          : std::make_unique<ChromeExtensionTestNotificationObserver>(
                GetProfile());
  return observer->WaitForPageActionVisibilityChangeTo(count);
}

}  // namespace extensions
