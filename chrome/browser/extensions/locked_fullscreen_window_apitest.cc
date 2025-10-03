// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/wm/window_pin_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chromeos/ui/base/window_pin_type.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/aura/window.h"
#endif

using ::testing::NotNull;

namespace extensions {
namespace {

class LockedFullscreenWindowApiTestBase : public ExtensionApiTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

#if !BUILDFLAG(IS_CHROMEOS)
using LockedFullscreenWindowApiTestNonChromeOS =
    LockedFullscreenWindowApiTestBase;

// Loading an extension requiring the 'lockWindowFullscreenPrivate' permission
// on non Chrome OS platforms should always fail since the API is available only
// on Chrome OS.
IN_PROC_BROWSER_TEST_F(LockedFullscreenWindowApiTestNonChromeOS,
                       OpenLockedFullscreenWindow) {
  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("locked_fullscreen/with_permission"),
      {.ignore_manifest_warnings = true});
  ASSERT_TRUE(extension);
  ASSERT_EQ(1u, extension->install_warnings().size());
  EXPECT_EQ(std::string("'lockWindowFullscreenPrivate' "
                        "is not allowed for specified platform."),
            extension->install_warnings()[0].message);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
class LockedFullscreenWindowApiTestChromeOS
    : public LockedFullscreenWindowApiTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  LockedFullscreenWindowApiTestChromeOS() {
    // TODO(crbug.com/438844429): Remove `kBoca` and `kBocaConsumer` feature
    // flags once Boca SWA is installed even when Class Tools policy is not set.
    if (IsLockedQuizMigrationEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{ash::features::kBocaOnTaskLockedQuizMigration,
                                ash::features::kBoca,
                                ash::features::kBocaConsumer},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{ash::features::kBoca,
                                ash::features::kBocaConsumer},
          /*disabled_features=*/{
              ash::features::kBocaOnTaskLockedQuizMigration});
    }
  }

  void SetUpOnMainThread() override {
    LockedFullscreenWindowApiTestBase::SetUpOnMainThread();
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();
  }

  // Launch Boca app and wait for it to open.
  void LaunchBocaAppAndWait() {
    content::TestNavigationObserver observer(
        (GURL(ash::boca::kChromeBocaAppUntrustedIndexURL)));
    observer.StartWatchingNewWebContents();
    ash::LaunchSystemWebAppAsync(browser()->profile(),
                                 ash::SystemWebAppType::BOCA);
    observer.Wait();
  }

  chromeos::WindowPinType GetCurrentWindowPinType() {
    chromeos::WindowPinType type = ash::GetWindowPinType(GetCurrentWindow());
    return type;
  }

  void SetCurrentWindowPinType(chromeos::WindowPinType type) {
    if (type == chromeos::WindowPinType::kNone) {
      ash::UnpinWindow(GetCurrentWindow());
    } else {
      ash::PinWindow(GetCurrentWindow(), /*trusted=*/true);
    }
  }

  Browser* FindBocaSystemWebAppBrowser() {
    return ash::FindSystemWebAppBrowser(browser()->profile(),
                                        ash::SystemWebAppType::BOCA);
  }

  bool IsLockedQuizMigrationEnabled() const { return GetParam(); }

 private:
  aura::Window* GetCurrentWindow() {
    extensions::WindowController* controller = nullptr;
    for (extensions::WindowController* window :
         *extensions::WindowControllerList::GetInstance()) {
      if (window->window()->IsActive()) {
        controller = window;
        break;
      }
    }

    EXPECT_TRUE(controller);
    return controller->window()->GetNativeWindow();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(LockedFullscreenWindowApiTestChromeOS,
                       OpenLockedFullscreenWindow) {
  ASSERT_TRUE(RunExtensionTest("locked_fullscreen/with_permission",
                               {.custom_arg = "openLockedFullscreenWindow"}))
      << message_;

  // Make sure the newly created window is locked fullscreen mode.
  EXPECT_EQ(chromeos::WindowPinType::kLockedFullscreen,
            GetCurrentWindowPinType());
}

IN_PROC_BROWSER_TEST_P(LockedFullscreenWindowApiTestChromeOS,
                       OpenLockedFullscreenWindowWithIncorrectUrlCount) {
  if (!IsLockedQuizMigrationEnabled()) {
    GTEST_SKIP()
        << "This test is only relevant for the new SWA-based migration case.";
  }

  ASSERT_TRUE(RunExtensionTest(
      "locked_fullscreen/with_permission",
      {.custom_arg = "openLockedFullscreenWindowWithIncorrectUrlCount"}))
      << message_;

  // Make sure no new windows get created (so only the one created by default
  // exists) since the call to chrome.windows.create fails on the javascript
  // side.
  EXPECT_EQ(1u, extensions::WindowControllerList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_P(LockedFullscreenWindowApiTestChromeOS,
                       UpdateWindowToLockedFullscreen) {
  if (IsLockedQuizMigrationEnabled()) {
    LaunchBocaAppAndWait();
  }

  ASSERT_TRUE(
      RunExtensionTest("locked_fullscreen/with_permission",
                       {.custom_arg = "updateWindowToLockedFullscreen"}))
      << message_;

  // Make sure the current window is put into the "locked fullscreen" state.
  EXPECT_EQ(chromeos::WindowPinType::kLockedFullscreen,
            GetCurrentWindowPinType());
}

IN_PROC_BROWSER_TEST_P(LockedFullscreenWindowApiTestChromeOS,
                       UpdateIncompatibleWindowToLockedFullscreen) {
  if (!IsLockedQuizMigrationEnabled()) {
    GTEST_SKIP()
        << "This test is only relevant for the new SWA-based migration case.";
  }

  ASSERT_TRUE(RunExtensionTest(
      "locked_fullscreen/with_permission",
      {.custom_arg = "updateIncompatibleWindowToLockedFullscreen"}))
      << message_;

  // chrome.windows.update call fails since the new SWA-based migration does not
  // support set locked fullscreen on regular browser window.
  EXPECT_EQ(chromeos::WindowPinType::kNone, GetCurrentWindowPinType());
}

IN_PROC_BROWSER_TEST_P(LockedFullscreenWindowApiTestChromeOS,
                       RemoveLockedFullscreenFromWindow) {
  Browser* current_browser = browser();
  if (IsLockedQuizMigrationEnabled()) {
    LaunchBocaAppAndWait();
    current_browser = FindBocaSystemWebAppBrowser();
  }
  ASSERT_THAT(current_browser, NotNull());

  // After locking the window, do a LockedFullscreenStateChanged so the
  // command_controller state catches up as well.
  SetCurrentWindowPinType(chromeos::WindowPinType::kLockedFullscreen);
  current_browser->command_controller()->LockedFullscreenStateChanged();

  ASSERT_TRUE(
      RunExtensionTest("locked_fullscreen/with_permission",
                       {.custom_arg = "removeLockedFullscreenFromWindow"}))
      << message_;

  // Make sure the current window is removed from locked-fullscreen state.
  EXPECT_EQ(chromeos::WindowPinType::kNone, GetCurrentWindowPinType());
}

IN_PROC_BROWSER_TEST_P(LockedFullscreenWindowApiTestChromeOS,
                       RemoveLockedFullscreenFromIncompatibleWindow) {
  if (!IsLockedQuizMigrationEnabled()) {
    GTEST_SKIP()
        << "This test is only relevant for the new SWA-based migration case.";
  }

  // After locking the window, do a LockedFullscreenStateChanged so the
  // command_controller state catches up as well.
  SetCurrentWindowPinType(chromeos::WindowPinType::kLockedFullscreen);
  browser()->command_controller()->LockedFullscreenStateChanged();

  ASSERT_TRUE(RunExtensionTest(
      "locked_fullscreen/with_permission",
      {.custom_arg = "removeLockedFullscreenFromIncompatibleWindow"}))
      << message_;

  // chrome.windows.update call fails since the new SWA-based migration does not
  // support set locked fullscreen on regular browser window.
  EXPECT_EQ(chromeos::WindowPinType::kLockedFullscreen,
            GetCurrentWindowPinType());
}

// Make sure that commands disabling code works in locked fullscreen mode.
IN_PROC_BROWSER_TEST_P(LockedFullscreenWindowApiTestChromeOS,
                       VerifyCommandsInLockedFullscreen) {
  Browser* current_browser = browser();
  if (IsLockedQuizMigrationEnabled()) {
    LaunchBocaAppAndWait();
    Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
    current_browser = boca_app_browser;
  }
  ASSERT_THAT(current_browser, NotNull());

  // IDC_EXIT is always enabled in regular mode so it's a perfect candidate for
  // testing.
  EXPECT_TRUE(
      current_browser->command_controller()->IsCommandEnabled(IDC_EXIT));
  ASSERT_TRUE(
      RunExtensionTest("locked_fullscreen/with_permission",
                       {.custom_arg = "updateWindowToLockedFullscreen"}))
      << message_;

  // Verify some disabled commands.
  EXPECT_FALSE(
      current_browser->command_controller()->IsCommandEnabled(IDC_EXIT));
  EXPECT_FALSE(
      current_browser->command_controller()->IsCommandEnabled(IDC_ZOOM_PLUS));

  // Verify some allowlisted commands.
  EXPECT_TRUE(
      current_browser->command_controller()->IsCommandEnabled(IDC_COPY));
  EXPECT_TRUE(
      current_browser->command_controller()->IsCommandEnabled(IDC_PASTE));

  // IDC_FIND should be disabled for the legacy locked fullscreen, but enabled
  // for new SWA-based migration locked fullscreen.
  EXPECT_EQ(current_browser->command_controller()->IsCommandEnabled(IDC_FIND),
            IsLockedQuizMigrationEnabled());
}

IN_PROC_BROWSER_TEST_P(LockedFullscreenWindowApiTestChromeOS,
                       OpenLockedFullscreenWindowWithoutPermission) {
  ASSERT_TRUE(RunExtensionTest("locked_fullscreen/without_permission",
                               {.custom_arg = "openLockedFullscreenWindow"}))
      << message_;

  // Make sure no new windows get created (so only the one created by default
  // exists) since the call to chrome.windows.create fails on the javascript
  // side.
  EXPECT_EQ(1u, extensions::WindowControllerList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_P(LockedFullscreenWindowApiTestChromeOS,
                       UpdateWindowToLockedFullscreenWithoutPermission) {
  if (IsLockedQuizMigrationEnabled()) {
    LaunchBocaAppAndWait();
  }

  ASSERT_TRUE(
      RunExtensionTest("locked_fullscreen/without_permission",
                       {.custom_arg = "updateWindowToLockedFullscreen"}))
      << message_;

  // chrome.windows.update call fails since this extension doesn't have the
  // correct permission and hence the current window has NONE as WindowPinType.
  EXPECT_EQ(chromeos::WindowPinType::kNone, GetCurrentWindowPinType());
}

IN_PROC_BROWSER_TEST_P(LockedFullscreenWindowApiTestChromeOS,
                       RemoveLockedFullscreenFromWindowWithoutPermission) {
  Browser* current_browser = browser();
  if (IsLockedQuizMigrationEnabled()) {
    LaunchBocaAppAndWait();
    current_browser = FindBocaSystemWebAppBrowser();
  }
  ASSERT_THAT(current_browser, NotNull());

  // After locking the window, do a LockedFullscreenStateChanged so the
  // command_controller state catches up as well.
  SetCurrentWindowPinType(chromeos::WindowPinType::kLockedFullscreen);
  current_browser->command_controller()->LockedFullscreenStateChanged();

  ASSERT_TRUE(
      RunExtensionTest("locked_fullscreen/without_permission",
                       {.custom_arg = "removeLockedFullscreenFromWindow"}))
      << message_;

  // The current window is still locked-fullscreen.
  EXPECT_EQ(chromeos::WindowPinType::kLockedFullscreen,
            GetCurrentWindowPinType());
}

INSTANTIATE_TEST_SUITE_P(LockedFullscreenWindowApiChromeOSTests,
                         LockedFullscreenWindowApiTestChromeOS,
                         testing::Bool());
#endif  // BUILDFLAG (IS_CHROMEOS)

}  // namespace
}  // namespace extensions
