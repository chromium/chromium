// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_RECORDER_APP_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_RECORDER_APP_UI_CONFIG_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "content/public/browser/webui_config.h"

class ApplicationLocaleStorage;
class PrefService;

namespace ash {

// WebUIConfig for chrome://recorder-app
class RecorderAppUIConfig : public content::WebUIConfig {
 public:
  // `local_state` and `application_locale_storage` must not be null and must
  // outlive `this`.
  RecorderAppUIConfig(
      PrefService* local_state,
      const ApplicationLocaleStorage* application_locale_storage);
  RecorderAppUIConfig(const RecorderAppUIConfig&) = delete;
  RecorderAppUIConfig& operator=(const RecorderAppUIConfig&) = delete;
  ~RecorderAppUIConfig() override;

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const raw_ref<PrefService> local_state_;
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_RECORDER_APP_UI_CONFIG_H_
