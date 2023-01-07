// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_TEST_PERSONALIZATION_APP_MOJOM_BANNED_BROWSERTEST_FIXTURE_H_
#define ASH_WEBUI_PERSONALIZATION_APP_TEST_PERSONALIZATION_APP_MOJOM_BANNED_BROWSERTEST_FIXTURE_H_

#include <memory>

#include "chrome/test/base/mojo_web_ui_browser_test.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "url/gurl.h"

namespace ash::personalization_app {

class TestPersonalizationAppMojomBannedWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  std::unique_ptr<content::WebUIController> NewWebUI(content::WebUI* web_ui,
                                                     const GURL& url) override;
};

class PersonalizationAppMojomBannedBrowserTestFixture
    : public MojoWebUIBrowserTest {
 public:
  PersonalizationAppMojomBannedBrowserTestFixture() = default;

  PersonalizationAppMojomBannedBrowserTestFixture(
      const PersonalizationAppMojomBannedBrowserTestFixture&) = delete;
  PersonalizationAppMojomBannedBrowserTestFixture& operator=(
      const PersonalizationAppMojomBannedBrowserTestFixture&) = delete;

  ~PersonalizationAppMojomBannedBrowserTestFixture() override = default;

  void SetUpOnMainThread() override;

 private:
  TestChromeWebUIControllerFactory test_factory_;
  TestPersonalizationAppMojomBannedWebUIProvider test_web_ui_provider_;
  content::ScopedWebUIControllerFactoryRegistration
      scoped_controller_factory_registration_{&test_factory_};
};

}  // namespace ash::personalization_app

#endif  // ASH_WEBUI_PERSONALIZATION_APP_TEST_PERSONALIZATION_APP_MOJOM_BANNED_BROWSERTEST_FIXTURE_H_
