// Copyright 2021 The Chromium Authors
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
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/resources/grit/webui_generated_resources.h"
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

void AddLocalizedStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"appTitle", IDS_SHORTCUT_CUSTOMIZATION_APP_TITLE},
      {"keyboardSettings", IDS_SHORTCUT_CUSTOMIZATION_KEYBOARD_SETTINGS},
      {"categoryTabsAndWindows",
       IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_TABS_AND_WINDOWS},
      {"categoryPageAndWebBrowser",
       IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_PAGE_AND_WEB_BROWSER},
      {"categorySystemAndDisplaySettings",
       IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_SYSTEM_AND_DISPLAY_SETTINGS},
      {"categoryTextEditing", IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_TEXT_EDITING},
      {"categoryAccessibility",
       IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_ACCESSIBILITY},
      {"categoryDebug", IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_DEBUG},
      {"categoryDeveloper", IDS_SHORTCUT_CUSTOMIZATION_CATEGORY_DEVELOPER},
      {"subcategoryGeneral", IDS_SHORTCUT_CUSTOMIZATION_SUBCATEGORY_GENERAL},
      {"subcategorySystemApps",
       IDS_SHORTCUT_CUSTOMIZATION_SUBCATEGORY_SYSTEM_APPS},
      {"subcategorySystemControls",
       IDS_SHORTCUT_CUSTOMIZATION_SUBCATEGORY_SYSTEM_CONTROLS},
  };

  source->AddLocalizedStrings(kLocalizedStrings);
  source->UseStringsJs();
}

void AddFeatureFlags(content::WebUIDataSource* html_source) {
  html_source->AddBoolean("isCustomizationEnabled",
                          features::IsShortcutCustomizationEnabled());
}

}  // namespace

ShortcutCustomizationAppUI::ShortcutCustomizationAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIShortcutCustomizationAppHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test chrome://webui-test "
      "'self';");

  source->DisableTrustedTypesCSP();

  const auto resources =
      base::make_span(kAshShortcutCustomizationAppResources,
                      kAshShortcutCustomizationAppResourcesSize);
  SetUpWebUIDataSource(source, resources,
                       IDR_ASH_SHORTCUT_CUSTOMIZATION_APP_INDEX_HTML);
  AddLocalizedStrings(source);

  AddFeatureFlags(source);

  provider_ = std::make_unique<shortcut_ui::AcceleratorConfigurationProvider>();
}

ShortcutCustomizationAppUI::~ShortcutCustomizationAppUI() = default;

void ShortcutCustomizationAppUI::BindInterface(
    mojo::PendingReceiver<
        shortcut_customization::mojom::AcceleratorConfigurationProvider>
        receiver) {
  provider_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ShortcutCustomizationAppUI)
}  // namespace ash
