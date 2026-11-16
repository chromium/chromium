// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_TEST_PERSONALIZATION_APP_WEBUI_PROVIDER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_TEST_PERSONALIZATION_APP_WEBUI_PROVIDER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "url/gurl.h"

class ApplicationLocaleStorage;
class PrefService;

namespace variations {
class VariationsService;
}  // namespace variations

namespace ash::personalization_app {

// TestPersonalizationAppWebUIProvider provides a mix of fake network fetchers
// and fake mojom providers to fake all network requests during Personalization
// App browsertests. This is designed for testing the entire app instead of
// single components.
class TestPersonalizationAppWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  // `local_state`, `application_locale_storage`, and `variations_service` must
  // not be null and must outlive `this`.
  TestPersonalizationAppWebUIProvider(
      PrefService* local_state,
      const ApplicationLocaleStorage* application_locale_storage,
      const variations::VariationsService* variations_service);

  TestPersonalizationAppWebUIProvider(
      const TestPersonalizationAppWebUIProvider&) = delete;
  TestPersonalizationAppWebUIProvider& operator=(
      const TestPersonalizationAppWebUIProvider&) = delete;

  ~TestPersonalizationAppWebUIProvider() override;

  // TestChromeWebUIControllerFactory::WebUIProvider:
  std::unique_ptr<content::WebUIController> NewWebUI(content::WebUI* web_ui,
                                                     const GURL& url) override;

 private:
  const raw_ref<PrefService> local_state_;
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
  const raw_ref<const variations::VariationsService> variations_service_;
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_TEST_PERSONALIZATION_APP_WEBUI_PROVIDER_H_
