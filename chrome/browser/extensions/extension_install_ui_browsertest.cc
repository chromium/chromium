// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"

using content::WebContents;
using extensions::AppSorting;
using extensions::Extension;

class ExtensionInstallUIBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  ExtensionInstallUIBrowserTest() {}
  ~ExtensionInstallUIBrowserTest() override {}

  // Checks that a theme info bar is currently visible and issues an undo to
  // revert to the previous theme.
  void VerifyThemeInfoBarAndUndoInstall() {
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);
    ASSERT_EQ(1U, infobar_service->infobar_count());
    ConfirmInfoBarDelegate* delegate =
        infobar_service->infobar_at(0)->delegate()->AsConfirmInfoBarDelegate();
    ASSERT_TRUE(delegate);
    delegate->Cancel();
    WaitForThemeChange();
    ASSERT_EQ(0U, infobar_service->infobar_count());
  }

  // Install the given theme from the data dir and verify expected name.
  void InstallThemeAndVerify(const char* theme_name,
                             const std::string& expected_name) {
    const base::FilePath theme_path = test_data_dir_.AppendASCII(theme_name);
    const bool theme_exists = GetTheme();
    // Themes install asynchronously so we must check the number of enabled
    // extensions after theme install completes.
    size_t num_before = extensions::ExtensionRegistry::Get(profile())
                            ->enabled_extensions()
                            .size();
    ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(theme_path, 1, browser()));
    WaitForThemeChange();
    size_t num_after = extensions::ExtensionRegistry::Get(profile())
                           ->enabled_extensions()
                           .size();
    // If a theme was already installed, we're just swapping one for another, so
    // no change in extension count.
    EXPECT_EQ(num_before + (theme_exists ? 0 : 1), num_after);

    const Extension* theme = GetTheme();
    ASSERT_TRUE(theme);
    ASSERT_EQ(theme->name(), expected_name);
  }

  const Extension* GetTheme() const {
    return ThemeServiceFactory::GetThemeForProfile(browser()->profile());
  }

  void WaitForThemeChange() {
    content::WindowedNotificationObserver theme_change_observer(
        chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
        content::Source<ThemeService>(
            ThemeServiceFactory::GetForProfile(browser()->profile())));
    theme_change_observer.Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallUIBrowserTest);
};

// Fails on Linux and Windows (http://crbug.com/580907).
IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       DISABLED_TestThemeInstallUndoResetsToDefault) {
  // Install theme once and undo to verify we go back to default theme.
  base::FilePath theme_crx = PackExtension(test_data_dir_.AppendASCII("theme"));
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(theme_crx, 1, browser()));
  WaitForThemeChange();
  const Extension* theme = GetTheme();
  ASSERT_TRUE(theme);
  std::string theme_id = theme->id();
  VerifyThemeInfoBarAndUndoInstall();
  ASSERT_EQ(NULL, GetTheme());

  // Set the same theme twice and undo to verify we go back to default theme.
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(theme_crx, 0, browser()));
  WaitForThemeChange();
  theme = GetTheme();
  ASSERT_TRUE(theme);
  ASSERT_EQ(theme_id, theme->id());
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(theme_crx, 0, browser()));
  WaitForThemeChange();
  theme = GetTheme();
  ASSERT_TRUE(theme);
  ASSERT_EQ(theme_id, theme->id());
  VerifyThemeInfoBarAndUndoInstall();
  ASSERT_EQ(NULL, GetTheme());
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       TestThemeInstallUndoResetsToPreviousTheme) {
  // Install first theme.
  InstallThemeAndVerify("theme", "camo theme");
  const Extension* theme = GetTheme();
  std::string theme_id = theme->id();

  // Then install second theme.
  InstallThemeAndVerify("theme2", "snowflake theme");
  const Extension* theme2 = GetTheme();
  EXPECT_NE(theme_id, theme2->id());

  // Undo second theme will revert to first theme.
  VerifyThemeInfoBarAndUndoInstall();
  EXPECT_EQ(theme, GetTheme());
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       TestThemeReset) {
  InstallThemeAndVerify("theme", "camo theme");

  // Reset to default theme.
  ThemeServiceFactory::GetForProfile(browser()->profile())->UseDefaultTheme();
  ASSERT_FALSE(GetTheme());
}

// Flaky (http://crbug.com/851252).
IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       DISABLED_TestInstallThemeInFullScreen) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_FULLSCREEN));
  InstallThemeAndVerify("theme", "camo theme");
}
