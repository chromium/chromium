// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_SYSTEM_WEB_APP_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_SYSTEM_WEB_APP_BROWSERTEST_BASE_H_

#include <memory>

#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/profile_test_helper.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"

class KeyedService;

namespace apps {
struct AppLaunchParams;
}

namespace content {
class WebContents;
}

namespace ash {

enum class SystemWebAppType;
class SystemWebAppManager;

class SystemWebAppBrowserTestBase
    : public InteractiveBrowserTestT<MixinBasedInProcessBrowserTest> {
 public:
  // Subclasses should call |SetSystemWebAppInstallation| in their constructor
  // to specify a test system web app to install.
  //
  // Otherwise, this test fixture installs all default-enabled system web apps.
  SystemWebAppBrowserTestBase();
  SystemWebAppBrowserTestBase(const SystemWebAppBrowserTestBase&) = delete;
  SystemWebAppBrowserTestBase& operator=(const SystemWebAppBrowserTestBase&) =
      delete;

  ~SystemWebAppBrowserTestBase() override;

  // Returns the SystemWebAppManager for browser()->profile(). For incognito
  // profiles, this will be the SystemWebAppManager of the original profile.
  SystemWebAppManager& GetManager();

  // Returns SystemWebAppType of the installed app.
  SystemWebAppType GetAppType();

  // Returns the start URL based on the given |params|.
  GURL GetStartUrl(const apps::AppLaunchParams& params);

  // Returns the default start url.
  GURL GetStartUrl();

  // Returns the URL for a installed system web app type.
  GURL GetStartUrl(SystemWebAppType type);

  void WaitForTestSystemAppInstall();

  // Creates a default AppLaunchParams for |system_app_type|. Launches a window.
  // Uses kSourceTest as the AppLaunchSource.
  apps::AppLaunchParams LaunchParamsForApp(SystemWebAppType system_app_type);

  // Launch the given System App from |params|, and wait for the application to
  // finish loading. If |browser| is not nullptr, it will store the Browser*
  // that hosts the launched application.
  content::WebContents* LaunchApp(apps::AppLaunchParams&& params,
                                  Browser** browser = nullptr);

  // Launch the given System App |type| with default AppLaunchParams, and wait
  // for the application to finish loading. If |browser| is not nullptr, it will
  // store the Browser* that hosts the launched application.
  content::WebContents* LaunchApp(SystemWebAppType type,
                                  Browser** browser = nullptr);

  // Launch the given System App from |params|, without waiting for the
  // application to finish loading. If |browser| is not nullptr, it will store
  // the Browser* that hosts the launched application.
  content::WebContents* LaunchAppWithoutWaiting(apps::AppLaunchParams&& params,
                                                Browser** browser = nullptr);

  // Launch the given System App |type| with default AppLaunchParams, without
  // waiting for the application to finish loading. If |browser| is not nullptr,
  // it will store the Browser* that hosts the launched application.
  content::WebContents* LaunchAppWithoutWaiting(SystemWebAppType type,
                                                Browser** browser = nullptr);

  // Returns number of system web app browser windows matching |type|.
  size_t GetSystemWebAppBrowserCount(SystemWebAppType type);

 protected:
  // Subclasses can use this method to specify the test system web app it
  // intends to install. This method should only be called in a subclass
  // constructor at most once.
  void SetSystemWebAppInstallation(
      std::unique_ptr<TestSystemWebAppInstallation> installation);

 private:
  std::unique_ptr<KeyedService> CreateWebAppProvider(Profile* profile);

  // Invokes OpenApplication() using the test's Profile. If |wait_for_load| is
  // true, returns after the application finishes loading. Otherwise, returns
  // immediately. If |browser| is not nullptr, it will store the Browser* that
  // hosts the launched application.
  content::WebContents* LaunchApp(apps::AppLaunchParams&& params,
                                  bool wait_for_load,
                                  Browser** out_browser);

  std::unique_ptr<TestSystemWebAppInstallation> installation_ = nullptr;
};

class SystemWebAppManagerBrowserTest
    : public TestProfileTypeMixin<SystemWebAppBrowserTestBase> {
 public:
  // Installs a single-windowed mock system web app.
  SystemWebAppManagerBrowserTest();

  SystemWebAppManagerBrowserTest(const SystemWebAppManagerBrowserTest&) =
      delete;
  SystemWebAppManagerBrowserTest& operator=(
      const SystemWebAppManagerBrowserTest&) = delete;
  ~SystemWebAppManagerBrowserTest() override = default;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_TEST_SUPPORT_SYSTEM_WEB_APP_BROWSERTEST_BASE_H_
