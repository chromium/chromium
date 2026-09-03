// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_CONFIG_H_

#include <memory>

#include "ash/webui/projector_app/untrusted_projector_ui.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "base/memory/raw_ref.h"

class ApplicationLocaleStorage;
class GURL;

namespace content {
class WebUIDataSource;
class WebUI;
class WebUIController;
}  // namespace content

// Implementation of the chromeos::UntrustedProjectorUIDelegate to expose some
// //chrome functions to //chromeos.
class ChromeUntrustedProjectorUIDelegate
    : public ash::UntrustedProjectorUIDelegate {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  explicit ChromeUntrustedProjectorUIDelegate(
      const ApplicationLocaleStorage* application_locale_storage);
  ChromeUntrustedProjectorUIDelegate(
      const ChromeUntrustedProjectorUIDelegate&) = delete;
  ChromeUntrustedProjectorUIDelegate& operator=(
      const ChromeUntrustedProjectorUIDelegate&) = delete;
  ~ChromeUntrustedProjectorUIDelegate();

  // ash::UntrustedProjectorUIDelegate:
  void PopulateLoadTimeData(content::WebUIDataSource* source) override;

 private:
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
};

// A webui config for the chrome-untrusted:// part of Projector.
class UntrustedProjectorUIConfig
    : public ash::SystemWebAppUntrustedUIConfig<ash::UntrustedProjectorUI> {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  explicit UntrustedProjectorUIConfig(
      const ApplicationLocaleStorage* application_locale_storage);
  UntrustedProjectorUIConfig(const UntrustedProjectorUIConfig& other) = delete;
  UntrustedProjectorUIConfig& operator=(const UntrustedProjectorUIConfig&) =
      delete;
  ~UntrustedProjectorUIConfig() override;

  // ash::SystemWebAppUntrustedUIConfig:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_UI_CONFIG_H_
