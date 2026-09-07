// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_CONFIG_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_CONFIG_H_

#include "ash/webui/boca_ui/boca_ui.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "base/memory/raw_ref.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class ApplicationLocaleStorage;

namespace ash {
// WebUI config for Boca SWA.
class BocaUIConfig : public content::WebUIConfig {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  explicit BocaUIConfig(
      const ApplicationLocaleStorage* application_locale_storage);
  BocaUIConfig(const BocaUIConfig&) = delete;
  BocaUIConfig& operator=(const BocaUIConfig&) = delete;
  ~BocaUIConfig() override;

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
};
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_BOCA_WEB_APP_CONFIG_H_
