// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/shortcut_customization_app_ui.h"

#include <memory>
#include <utility>

#include "ash/webui/grit/ash_shortcut_customization_app_resources.h"
#include "ash/webui/grit/ash_shortcut_customization_app_resources_map.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

namespace {

void SetUpWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  source->AddResourcePaths(resources);
  source->SetDefaultResource(default_resource);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
}

}  // namespace

ShortcutCustomizationAppUI::ShortcutCustomizationAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIShortcutCustomizationAppHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  source->DisableTrustedTypesCSP();

  const auto resources =
      base::make_span(kAshShortcutCustomizationAppResources,
                      kAshShortcutCustomizationAppResourcesSize);
  SetUpWebUIDataSource(source, resources,
                       IDR_ASH_SHORTCUT_CUSTOMIZATION_APP_INDEX_HTML);

  provider_ = std::make_unique<shortcut_ui::AcceleratorConfigurationProvider>();
}

ShortcutCustomizationAppUI::~ShortcutCustomizationAppUI() = default;

void ShortcutCustomizationAppUI::BindInterface(
    mojo::PendingReceiver<
        shortcut_customization::mojom::AcceleratorConfigurationProvider>
        receiver) {
  provider_->BindInterface(std::move(receiver));
}

}  // namespace ash