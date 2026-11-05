// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_UNTRUSTED_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_UNTRUSTED_UI_CONFIG_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/webui_config.h"

class ApplicationLocaleStorage;

namespace ash {

class HelpAppUntrustedUIConfig
    : public content::WebUIConfig,
      public variations::VariationsService::Observer {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  // `variations_service` may be null only in tests (when
  // `CreateWebUIController()` is not called).
  HelpAppUntrustedUIConfig(
      const ApplicationLocaleStorage* application_locale_storage,
      variations::VariationsService* variations_service);
  HelpAppUntrustedUIConfig(const HelpAppUntrustedUIConfig& other) = delete;
  HelpAppUntrustedUIConfig& operator=(const HelpAppUntrustedUIConfig&) = delete;
  ~HelpAppUntrustedUIConfig() override;

  static bool IsCrosSwitcherEnabledForTesting(
      const variations::VariationsService& variations_service);

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

  // variations::VariationsService::Observer:
  void OnVariationsServiceDestroyed() override;

 private:
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
  raw_ptr<variations::VariationsService> variations_service_ = nullptr;
  base::ScopedObservation<variations::VariationsService,
                          variations::VariationsService::Observer>
      variations_service_observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_HELP_APP_HELP_APP_UNTRUSTED_UI_CONFIG_H_
