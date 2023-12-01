// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/vc_background_ui/vc_background_ui.h"

#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/ash_vc_background_resources.h"
#include "ash/webui/grit/ash_vc_background_resources_map.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/webui/system_apps/public/system_web_app_ui_config.h"
#include "ash/webui/vc_background_ui/url_constants.h"
#include "components/manta/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::vc_background_ui {

namespace {

using std::literals::string_view_literals::operator""sv;

void AddStrings(content::WebUIDataSource* source) {
  // TODO(b/311416410) real translated title.
  source->AddString("vcBackgroundTitle", u"VC Background");
}

void AddResources(content::WebUIDataSource* source) {
  source->AddResourcePath(""sv, IDR_ASH_VC_BACKGROUND_INDEX_HTML);
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

VcBackgroundUIConfig::VcBackgroundUIConfig()
    : SystemWebAppUIConfig(kChromeUIVcBackgroundHost,
                           SystemWebAppType::VC_BACKGROUND) {}

bool VcBackgroundUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return SystemWebAppUIConfig::IsWebUIEnabled(browser_context) &&
         ash::features::IsSeaPenEnabled() &&
         manta::features::IsMantaServiceEnabled();
}

VcBackgroundUI::VcBackgroundUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, std::string(kChromeUIVcBackgroundHost));

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  AddStrings(source);
  AddResources(source);
}

VcBackgroundUI::~VcBackgroundUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(VcBackgroundUI)

}  // namespace ash::vc_background_ui
