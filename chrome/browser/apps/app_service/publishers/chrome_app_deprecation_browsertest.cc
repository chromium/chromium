// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/chrome_app_deprecation.h"

#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/message_center/message_center.h"

namespace apps {

using extensions::Extension;

class ChromeAppDeprecationUserInstalledAppsBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  const Extension* LoadPlatformApp() {
    const base::FilePath& root_path = test_data_dir_;
    base::FilePath extension_path =
        root_path.AppendASCII("platform_apps/launch");

    return LoadExtension(extension_path);
  }

  void RunPlatformApp(const Extension* extension) {
    AppLaunchParams params(
        extension->id(), LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::NEW_WINDOW, LaunchSource::kFromTest);
    params.command_line = *base::CommandLine::ForCurrentProcess();

    AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->LaunchAppWithParams(std::move(params));
  }
};

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationUserInstalledAppsBrowserTest,
                       NotAllowlisted) {
  auto* center = message_center::MessageCenter::Get();
  auto notifications_count = center->GetNotifications().size();

  const Extension* app = LoadPlatformApp();
  ASSERT_TRUE(app);

  RunPlatformApp(app);
  ASSERT_TRUE(center->GetNotifications().size() == notifications_count + 1);
}

IN_PROC_BROWSER_TEST_F(ChromeAppDeprecationUserInstalledAppsBrowserTest,
                       Allowlisted) {
  auto* center = message_center::MessageCenter::Get();
  auto notifications_count = center->GetNotifications().size();

  const Extension* app = LoadPlatformApp();
  ASSERT_TRUE(app);

  chrome_app_deprecation::AddAppToAllowlistForTesting(app->id());

  RunPlatformApp(app);
  ASSERT_TRUE(center->GetNotifications().size() == notifications_count);
}

}  // namespace apps
