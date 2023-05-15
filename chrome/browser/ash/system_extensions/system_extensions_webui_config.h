// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_WEBUI_CONFIG_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_WEBUI_CONFIG_H_

#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "content/public/browser/webui_config.h"

namespace ash {

// Generic config for System Extensions. Each installed System Extension
// register a WebUIConfig to load its resources.
class SystemExtensionsWebUIConfig : public content::WebUIConfig {
 public:
  explicit SystemExtensionsWebUIConfig(const SystemExtension& system_extension);
  ~SystemExtensionsWebUIConfig() override;

  // content::WebUIConfig
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
  void RegisterURLDataSource(content::BrowserContext* browser_context) override;

 private:
  const SystemExtensionId system_extension_id_;
  const GURL system_extension_base_url_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_WEBUI_CONFIG_H_
