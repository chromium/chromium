// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_APP_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_APP_BROWSERTEST_UTIL_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/mixin_based_extension_apitest.h"
#include "extensions/browser/app_window/app_window.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/media_router/browser/test/mock_media_router.h"
#endif

namespace base {
class CommandLine;
}

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

class Browser;
class ExtensionTestMessageListener;

namespace extensions {
class Extension;

class PlatformAppBrowserTest : public MixinBasedExtensionApiTest {
 public:
  PlatformAppBrowserTest();
  PlatformAppBrowserTest(const PlatformAppBrowserTest&) = delete;
  PlatformAppBrowserTest& operator=(const PlatformAppBrowserTest&) = delete;
  ~PlatformAppBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  // Gets the first app window that is found for a given browser.
  static AppWindow* GetFirstAppWindowForBrowser(Browser* browser);

 protected:
  // Runs the app named |name| out of the platform_apps subdirectory. Waits
  // for the provided listener to be satisifed.
  const Extension* LoadAndLaunchPlatformApp(
      const char* name,
      ExtensionTestMessageListener* listener);

  // Runs the app named |name| out of the platform_apps subdirectory. Waits
  // until the given message is received from the app.
  const Extension* LoadAndLaunchPlatformApp(const char* name,
                                            const std::string& message);

  // Installs the app named |name| out of the platform_apps subdirectory.
  const Extension* InstallPlatformApp(const char* name);

  // Installs the sample hosted app.
  const Extension* InstallHostedApp();

  // Installs and runs the app named |name| out of the platform_apps
  // subdirectory. Waits until it is launched.
  const Extension* InstallAndLaunchPlatformApp(const char* name);

  // Launch the given platform app.
  virtual void LaunchPlatformApp(const Extension* extension);

  // Launch the given hosted app.
  virtual void LaunchHostedApp(const Extension* extension);

  // Gets the WebContents associated with the first app window that is found
  // (most tests only deal with one platform app window, so this is good
  // enough).
  content::WebContents* GetFirstAppWindowWebContents();

  // Gets the first app window that is found (most tests only deal with one
  // platform app window, so this is good enough).
  AppWindow* GetFirstAppWindow();

  // Gets the first app window for an app.
  AppWindow* GetFirstAppWindowForApp(const std::string& app_id);

  // Runs chrome.windows.getAll for the given extension and returns the number
  // of windows that the function returns.
  size_t RunGetWindowsFunctionForExtension(const Extension* extension);

  // Runs chrome.windows.get(|window_id|) for the the given extension and
  // returns whether or not a window was found.
  bool RunGetWindowFunctionForExtension(int window_id,
                                        const Extension* extension);

  // Returns the number of app windows.
  size_t GetAppWindowCount();

  // Returns the number of app windows for a specific app.
  size_t GetAppWindowCountForApp(const std::string& app_id);

  // Creates an empty app window for |extension|.
  AppWindow* CreateAppWindow(content::BrowserContext* context,
                             const Extension* extension);

  AppWindow* CreateAppWindowFromParams(content::BrowserContext* context,
                                       const Extension* extension,
                                       const AppWindow::CreateParams& params);

  // Closes |window| and waits until it's gone.
  void CloseAppWindow(AppWindow* window);

  // Call AdjustBoundsToBeVisibleOnScreen of |window|.
  void CallAdjustBoundsToBeVisibleOnScreenForAppWindow(
      AppWindow* window,
      const gfx::Rect& cached_bounds,
      const gfx::Rect& cached_screen_bounds,
      const gfx::Rect& current_screen_bounds,
      const gfx::Size& minimum_size,
      gfx::Rect* bounds);

  // Load a simple test app and create a window. The window must be closed by
  // the caller in order to terminate the test - use CloseAppWindow().
  // |window_create_options| are the options that will be passed to
  // chrome.app.window.create() in the test app.
  AppWindow* CreateTestAppWindow(const std::string& window_create_options);

  // Returns the native app window associated with |window|.
  NativeAppWindow* GetNativeAppWindowForAppWindow(AppWindow* window);

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<media_router::MockMediaRouter> media_router_;
#endif
  base::AutoReset<bool> enable_chrome_apps_;
};

class ExperimentalPlatformAppBrowserTest : public PlatformAppBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_APP_BROWSERTEST_UTIL_H_
