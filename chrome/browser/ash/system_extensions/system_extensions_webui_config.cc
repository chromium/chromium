// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_webui_config.h"

#include "base/logging.h"
#include "chrome/browser/ash/system_extensions/system_extensions_data_source.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

namespace {

void CreateAndAddURLDataSource(Profile* profile,
                               const SystemExtensionId& system_extension_id,
                               const GURL& system_extension_base_url) {
  auto data_source = std::make_unique<SystemExtensionsDataSource>(
      profile, system_extension_id, system_extension_base_url);
  content::URLDataSource::Add(profile, std::move(data_source));
}

// Generic WebUIController for System Extensions. Each installed System
// Extension has a corresponding WebUIController.
class SystemExtensionsWebUIController : public ui::UntrustedWebUIController {
 public:
  explicit SystemExtensionsWebUIController(
      content::WebUI* web_ui,
      const SystemExtensionId& system_extension_id,
      const GURL& system_extension_base_url)
      : ui::UntrustedWebUIController(web_ui) {
    auto* profile = Profile::FromWebUI(web_ui);
    CreateAndAddURLDataSource(profile, system_extension_id,
                              system_extension_base_url);
  }

  SystemExtensionsWebUIController(const SystemExtensionsWebUIController&) =
      delete;
  SystemExtensionsWebUIController& operator=(const UntrustedWebUIController&) =
      delete;
  ~SystemExtensionsWebUIController() override = default;
};

}  // namespace

SystemExtensionsWebUIConfig::SystemExtensionsWebUIConfig(
    const SystemExtension& system_extension)
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  system_extension.base_url.host()),
      system_extension_id_(system_extension.id),
      system_extension_base_url_(system_extension.base_url) {}

SystemExtensionsWebUIConfig::~SystemExtensionsWebUIConfig() = default;

std::unique_ptr<content::WebUIController>
SystemExtensionsWebUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  return std::make_unique<SystemExtensionsWebUIController>(
      web_ui, system_extension_id_, system_extension_base_url_);
}

void SystemExtensionsWebUIConfig::RegisterURLDataSource(
    content::BrowserContext* browser_context) {
  auto* profile = Profile::FromBrowserContext(browser_context);
  CreateAndAddURLDataSource(profile, system_extension_id_,
                            system_extension_base_url_);
}

}  // namespace ash
