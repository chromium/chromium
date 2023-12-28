// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PERSONALIZATION_APP_TEST_PERSONALIZATION_APP_MOJOM_BANNED_MOCHA_TEST_BASE_H_
#define ASH_WEBUI_PERSONALIZATION_APP_TEST_PERSONALIZATION_APP_MOJOM_BANNED_MOCHA_TEST_BASE_H_

#include <memory>

#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "url/gurl.h"

namespace ash::personalization_app {

// TestPersonalizationAppMojomBannedWebUIProvider provides fake mojom provider
// implementations that immediately crash if a mojom request occurs. This is
// used to test UI components in isolation.
class TestPersonalizationAppMojomBannedWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  std::unique_ptr<content::WebUIController> NewWebUI(content::WebUI* web_ui,
                                                     const GURL& url) override;
};

class PersonalizationAppMojomBannedMochaTestBase
    : public WebUIMochaBrowserTest {
 public:
  PersonalizationAppMojomBannedMochaTestBase();

  PersonalizationAppMojomBannedMochaTestBase(
      const PersonalizationAppMojomBannedMochaTestBase&) = delete;
  PersonalizationAppMojomBannedMochaTestBase& operator=(
      const PersonalizationAppMojomBannedMochaTestBase&) = delete;

  ~PersonalizationAppMojomBannedMochaTestBase() override = default;

  void SetUpOnMainThread() override;

 private:
  TestChromeWebUIControllerFactory test_factory_;
  TestPersonalizationAppMojomBannedWebUIProvider test_web_ui_provider_;
  content::ScopedWebUIControllerFactoryRegistration
      scoped_controller_factory_registration_{&test_factory_};
};

}  // namespace ash::personalization_app

#endif  // ASH_WEBUI_PERSONALIZATION_APP_TEST_PERSONALIZATION_APP_MOJOM_BANNED_MOCHA_TEST_BASE_H_
