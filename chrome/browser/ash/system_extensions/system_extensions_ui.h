// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_UI_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_UI_H_

#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_config.h"

// Generic config for System Extensions. Each installed System Extension
// register a WebUIConfig to load its resources.
class SystemExtensionsWebUIConfig : public ui::WebUIConfig {
 public:
  explicit SystemExtensionsWebUIConfig(
      const SystemExtension& system_extension_id);
  ~SystemExtensionsWebUIConfig() override;

  // ui::WebUIConfig
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;

 private:
  const SystemExtensionId system_extension_id_;
  const GURL system_extension_base_url_;
};

// Generic WebUIController for System Extensions. Each installed System
// Extension has a corresponding WebUIController.
class SystemExtensionsWebUIController : public ui::UntrustedWebUIController {
 public:
  explicit SystemExtensionsWebUIController(
      content::WebUI* web_ui,
      const SystemExtensionId& extension_id,
      const GURL& system_extension_base_url);
  SystemExtensionsWebUIController(const SystemExtensionsWebUIController&) =
      delete;
  SystemExtensionsWebUIController& operator=(const UntrustedWebUIController&) =
      delete;
  ~SystemExtensionsWebUIController() override = default;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_UI_H_
