// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/sanitize_ui/sanitize_ui.h"

#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_sanitize_app_resources.h"
#include "ash/webui/grit/ash_sanitize_app_resources_map.h"
#include "ash/webui/sanitize_ui/url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/resources/grit/webui_resources.h"

namespace ash {

SanitizeDialogUI::SanitizeDialogUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUISanitizeAppHost);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  ash::EnableTrustedTypesCSP(html_source);
  html_source->UseStringsJs();
  html_source->EnableReplaceI18nInJS();

  const auto resources =
      base::make_span(kAshSanitizeAppResources, kAshSanitizeAppResourcesSize);
  html_source->AddResourcePaths(resources);
  html_source->AddResourcePath("", IDR_ASH_SANITIZE_APP_INDEX_HTML);
  html_source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  html_source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  html_source->AddResourcePath("test_loader_util.js",
                               IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);

  webui::LocalizedString kLocalizedStrings[] = {
      {"sanitizeDoneTitle", IDS_SANITIZE_DONE_HEADING},
      {"sanitizeDoneExplanation", IDS_SANITIZE_DONE_DESCRIPTION},
      {"sanitizeDoneRollback", IDS_SANITIZE_DONE_ROLLBACK},
      {"sanitizeDoneButton", IDS_SANITIZE_DONE},
      {"sanitizeDoneAccordionExtensionsTitle",
       IDS_SANITIZE_DONE_ACCORDION_EXTENSIONS_TITLE},
      {"sanitizeDoneAccordionExtensionsReenable",
       IDS_SANITIZE_DONE_ACCORDION_EXTENSIONS_REENABLE},
      {"sanitizeDoneAccordionChromeOsTitle",
       IDS_SANITIZE_DONE_ACCORDION_CHROMEOS_TITLE},
      {"sanitizeDoneAccordionChromeOsInput",
       IDS_SANITIZE_DONE_ACCORDION_CHROMEOS_INPUT},
      {"sanitizeDoneAccordionChromeOsNetwork",
       IDS_SANITIZE_DONE_ACCORDION_CHROMEOS_NETWORK},
      {"sanitizeDoneAccordionChromeTitle",
       IDS_SANITIZE_DONE_ACCORDION_CHROME_TITLE},
      {"sanitizeDoneAccordionChromeSiteContent",
       IDS_SANITIZE_DONE_ACCORDION_CHROME_SITE_CONTENT},
      {"sanitizeDoneAccordionChromeStartup",
       IDS_SANITZIE_DONE_ACCORDION_CHROME_STARTUP},
      {"sanitizeDoneAccordionChromeHomepage",
       IDS_SANITIZE_DONE_ACCORDION_CHROME_HOMEPAGE},
      {"sanitizeDoneAccordionChromeLanguages",
       IDS_SANITIZE_DONE_ACCORDION_CHROME_LANGUAGES},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

SanitizeDialogUI::~SanitizeDialogUI() {}

void SanitizeDialogUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(SanitizeDialogUI)

}  // namespace ash
