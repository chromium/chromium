// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/sanitize_ui/sanitize_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_sanitize_app_resources.h"
#include "ash/webui/grit/ash_sanitize_app_resources_map.h"
#include "ash/webui/sanitize_ui/sanitize_ui_delegate.h"
#include "ash/webui/sanitize_ui/url_constants.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/resources/grit/webui_resources.h"

namespace {

// This function chooses which view should be shown based on the url. The done
// page is only shown if the url query is set to "done".
bool ShowDone(const GURL url) {
  return url.has_query() && url.query() == "done";
}

}  // namespace
namespace ash {

bool SanitizeDialogUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  auto* session_controller = Shell::Get()->session_controller();
  bool is_managed_user = session_controller->IsActiveAccountManaged();
  bool is_child_user = session_controller->IsUserChild();
  bool is_guest_mode_active = session_controller->IsUserGuest();
  bool is_managed_device =
      !ash::InstallAttributes::Get()->IsEnterpriseManaged();
  return ChromeOSWebUIConfig::IsWebUIEnabled(browser_context) &&
         is_managed_device && !is_managed_user && !is_guest_mode_active &&
         !is_child_user &&
         base::FeatureList::IsEnabled(ash::features::kSanitize);
}

class SanitizeSettingsResetter : public sanitize_ui::mojom::SettingsResetter {
 public:
  SanitizeSettingsResetter(std::unique_ptr<SanitizeUIDelegate> delegate) {
    sanitize_ui_delegate_ = std::move(delegate);
  }

  void PerformSanitizeSettings() override {
    if (sanitize_ui_delegate_) {
      sanitize_ui_delegate_->PerformSanitizeSettings();
    }
  }

  void BindInterface(
      mojo::PendingReceiver<sanitize_ui::mojom::SettingsResetter> receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

 private:
  std::unique_ptr<SanitizeUIDelegate> sanitize_ui_delegate_;
  mojo::Receiver<sanitize_ui::mojom::SettingsResetter> receiver_{this};
};

SanitizeDialogUI::SanitizeDialogUI(
    content::WebUI* web_ui,
    std::unique_ptr<SanitizeUIDelegate> sanitize_ui_delegate)
    : ui::MojoWebDialogUI(web_ui) {
  sanitize_settings_resetter_ = std::make_unique<SanitizeSettingsResetter>(
      std::move(sanitize_ui_delegate));
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
      {"sanitizeDoneButtonExtensions", IDS_SANITIZE_DONE_BUTTON_EXTENSIONS},
      {"sanitizeDoneButtonChromeOSInput",
       IDS_SANITIZE_DONE_BUTTON_CHROMEOS_INPUT},
      {"sanitizeDoneButtonChromeOSNetwork",
       IDS_SANITIZE_DONE_BUTTON_CHROMEOS_NETWORK},
      {"sanitizeDoneButtonChromeSiteContent",
       IDS_SANITIZE_DONE_BUTTON_CHROME_SITE_CONTENT},
      {"sanitizeDoneButtonChromeStartup",
       IDS_SANITIZE_DONE_BUTTON_CHROME_STARTUP},
      {"sanitizeDoneButtonChromeHomepage",
       IDS_SANITIZE_DONE_BUTTON_CHROME_HOMEPAGE},
      {"sanitizeDoneButtonChromeLanguages",
       IDS_SANITIZE_DONE_BUTTON_CHROME_LANGUAGES},
      {"sanitizeDescription", IDS_SANITIZE_DESCRIPTION},
      {"sanitizeDialogTitle", IDS_SANITIZE_HEADING},
      {"sanitizeDialogExplanation", IDS_SANITIZE_WARNING},
      {"sanitizeDialogButton", IDS_SANITIZE},
      {"sanitizeFeedback", IDS_SANITIZE_FEEDBACK},
      {"sanitizeCancel", IDS_SANITIZE_CANCEL}};
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddBoolean("showDone",
                          ShowDone(web_ui->GetWebContents()->GetURL()));
}

SanitizeDialogUI::~SanitizeDialogUI() {}

void SanitizeDialogUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void SanitizeDialogUI::BindInterface(
    mojo::PendingReceiver<sanitize_ui::mojom::SettingsResetter> receiver) {
  sanitize_settings_resetter_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(SanitizeDialogUI)

}  // namespace ash
