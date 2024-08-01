// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_MOCHA_TEST_BASE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_MOCHA_TEST_BASE_H_

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/test_personalization_app_webui_provider.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"

namespace ash::personalization_app {

class PersonalizationAppMochaTestBase : public WebUIMochaBrowserTest {
 public:
  PersonalizationAppMochaTestBase();

  PersonalizationAppMochaTestBase(
      const PersonalizationAppMochaTestBase&) = delete;
  PersonalizationAppMochaTestBase& operator=(
      const PersonalizationAppMochaTestBase&) = delete;

  ~PersonalizationAppMochaTestBase() override;

  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

 private:
  void CreateDefaultWallpapers();

  base::test::ScopedFeatureList scoped_feature_list_;
  TestChromeWebUIControllerFactory test_factory_;
  TestPersonalizationAppWebUIProvider test_webui_provider_;
  content::ScopedWebUIControllerFactoryRegistration
      scoped_controller_factory_registration_{&test_factory_};
  base::ScopedTempDir default_wallpaper_dir_;
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_MOCHA_TEST_BASE_H_
