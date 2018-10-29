// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/macros.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_utils.h"

namespace {

// The toolbar color specified in the theme.
const SkColor kThemeToolbarColor = 0xFFCFDDC0;

bool UsingCustomTheme(const ThemeService& theme_service) {
  return !theme_service.UsingSystemTheme() &&
         !theme_service.UsingDefaultTheme();
}

class ThemeServiceBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  ThemeServiceBrowserTest() {
  }
  ~ThemeServiceBrowserTest() override {}

  void SetUp() override {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    extensions::ExtensionBrowserTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThemeServiceBrowserTest);
};

// Test that the theme is recreated from the extension when the data pack is
// unavailable or invalid (such as when the theme pack version is incremented).
// The PRE_ part of the test installs the theme and changes where Chrome looks
// for the theme data pack to make sure that Chrome does not find it.
IN_PROC_BROWSER_TEST_F(ThemeServiceBrowserTest, PRE_ThemeDataPackInvalid) {
  Profile* profile = browser()->profile();
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile);
  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile);

  // Test initial state.
  EXPECT_FALSE(UsingCustomTheme(*theme_service));
  EXPECT_NE(kThemeToolbarColor,
            theme_provider.GetColor(ThemeProperties::COLOR_TOOLBAR));
  EXPECT_EQ(base::FilePath(),
            profile->GetPrefs()->GetFilePath(prefs::kCurrentThemePackFilename));

  content::WindowedNotificationObserver theme_change_observer(
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(theme_service));
  InstallExtension(test_data_dir_.AppendASCII("theme"), 1);
  theme_change_observer.Wait();

  // Check that the theme was installed.
  EXPECT_TRUE(UsingCustomTheme(*theme_service));
  EXPECT_EQ(kThemeToolbarColor,
            theme_provider.GetColor(ThemeProperties::COLOR_TOOLBAR));
  EXPECT_NE(base::FilePath(),
            profile->GetPrefs()->GetFilePath(prefs::kCurrentThemePackFilename));

  // Change the theme data pack path to an invalid location such that second
  // part of the test is forced to recreate the theme pack when the theme
  // service is initialized.
  profile->GetPrefs()->SetFilePath(prefs::kCurrentThemePackFilename,
                                   base::FilePath());
}

IN_PROC_BROWSER_TEST_F(ThemeServiceBrowserTest, ThemeDataPackInvalid) {
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(
      browser()->profile());
  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(browser()->profile());
  EXPECT_TRUE(UsingCustomTheme(*theme_service));
  EXPECT_EQ(kThemeToolbarColor,
            theme_provider.GetColor(ThemeProperties::COLOR_TOOLBAR));
}

}  // namespace
