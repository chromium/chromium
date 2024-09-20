// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/vc_background_ui/vc_background_ui.h"

#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "ash/webui/common/sea_pen_provider.h"
#include "ash/webui/common/sea_pen_resources.h"
#include "ash/webui/common/sea_pen_resources_generated.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_vc_background_resources.h"
#include "ash/webui/grit/ash_vc_background_resources_map.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "ash/webui/vc_background_ui/url_constants.h"
#include "chromeos/crosapi/cpp/lacros_startup_state.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/manta/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::vc_background_ui {

namespace {

void AddStrings(content::WebUIDataSource* source) {
  source->AddString("vcBackgroundTitle",
                    l10n_util::GetStringUTF16(IDS_VC_BACKGROUND_APP_TITLE));

  ::ash::common::AddSeaPenStrings(source);
  ::ash::common::AddSeaPenVcBackgroundTemplateStrings(source);

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
}

void AddResources(content::WebUIDataSource* source) {
  source->AddResourcePath("", IDR_ASH_VC_BACKGROUND_INDEX_HTML);
  source->AddResourcePaths(base::make_span(kAshVcBackgroundResources,
                                           kAshVcBackgroundResourcesSize));

#if !DCHECK_IS_ON()
  // If a user goes to an invalid url and non-DCHECK mode (DHECK = debug mode)
  // is set, serve a default page so the user sees your default page instead
  // of an unexpected error. But if DCHECK is set, the user will be a
  // developer and be able to identify an error occurred.
  source->SetDefaultResource(IDR_ASH_VC_BACKGROUND_INDEX_HTML);
#endif  // !DCHECK_IS_ON()
}

}  // namespace

VcBackgroundUIConfig::VcBackgroundUIConfig(
    SystemWebAppUIConfig::CreateWebUIControllerFunc create_controller_func)
    : SystemWebAppUIConfig(kChromeUIVcBackgroundHost,
                           SystemWebAppType::VC_BACKGROUND,
                           create_controller_func) {}

bool VcBackgroundUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return SystemWebAppUIConfig::IsWebUIEnabled(browser_context) &&
         ::ash::features::IsVcBackgroundReplaceEnabled() &&
         manta::features::IsMantaServiceEnabled();
}

VcBackgroundUI::VcBackgroundUI(
    content::WebUI* web_ui,
    std::unique_ptr<::ash::common::SeaPenProvider> sea_pen_provider)
    : ui::MojoWebUIController(web_ui),
      sea_pen_provider_(std::move(sea_pen_provider)) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, std::string(kChromeUIVcBackgroundHost));

  ash::EnableTrustedTypesCSP(source);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");

  AddResources(source);
  AddStrings(source);
  AddBooleans(source);
}

VcBackgroundUI::~VcBackgroundUI() = default;

void VcBackgroundUI::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::SeaPenProvider>
        receiver) {
  sea_pen_provider_->BindInterface(std::move(receiver));
}

void VcBackgroundUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void VcBackgroundUI::AddBooleans(content::WebUIDataSource* source) {
  const bool common_sea_pen_requirements =
      sea_pen_provider_->IsEligibleForSeaPen() &&
      ::ash::features::IsVcBackgroundReplaceEnabled() &&
      manta::features::IsMantaServiceEnabled();
  source->AddBoolean("isSeaPenEnabled",
                         common_sea_pen_requirements);
  source->AddBoolean("isSeaPenTextInputEnabled",
                     common_sea_pen_requirements &&
                         ::ash::features::IsSeaPenTextInputEnabled() &&
                         sea_pen_provider_->IsEligibleForSeaPenTextInput());
  source->AddBoolean("isSeaPenUseExptTemplateEnabled",
                     common_sea_pen_requirements &&
                         ::ash::features::IsSeaPenUseExptTemplateEnabled());
  source->AddBoolean("isManagedSeaPenEnabled",
                     common_sea_pen_requirements &&
                         sea_pen_provider_->IsManagedSeaPenEnabled());
  source->AddBoolean("isManagedSeaPenFeedbackEnabled",
                     sea_pen_provider_->IsManagedSeaPenFeedbackEnabled());
  source->AddBoolean("isLacrosEnabled",
                     ::crosapi::lacros_startup_state::IsLacrosEnabled());
  source->AddBoolean("isVcResizeThumbnailEnabled",
                     ::ash::features::IsVcResizeThumbnailEnabled());
}

WEB_UI_CONTROLLER_TYPE_IMPL(VcBackgroundUI)

}  // namespace ash::vc_background_ui
