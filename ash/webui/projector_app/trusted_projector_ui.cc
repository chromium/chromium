// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/trusted_projector_ui.h"

#include "ash/public/cpp/projector/projector_annotator_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/grit/ash_projector_app_trusted_resources.h"
#include "ash/webui/grit/ash_projector_app_trusted_resources_map.h"
#include "ash/webui/grit/ash_projector_common_resources.h"
#include "ash/webui/grit/ash_projector_common_resources_map.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/gurl.h"

namespace ash {

namespace {

void CreateAndAddProjectorHTMLSource(content::WebUI* web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUIProjectorAppHost);

  source->AddResourcePaths(base::make_span(
      kAshProjectorAppTrustedResources, kAshProjectorAppTrustedResourcesSize));
  source->AddResourcePaths(base::make_span(kAshProjectorCommonResources,
                                           kAshProjectorCommonResourcesSize));
  source->AddResourcePath("", IDR_ASH_PROJECTOR_APP_TRUSTED_EMBEDDER_HTML);
  source->AddLocalizedString("appTitle", IDS_ASH_PROJECTOR_DISPLAY_SOURCE);

  std::string csp =
      std::string("frame-src ") + kChromeUIUntrustedProjectorUrl + ";";

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types polymer-html-literal "
      "polymer-template-event-attribute-policy;");
}

}  // namespace

TrustedProjectorUI::TrustedProjectorUI(content::WebUI* web_ui,
                                       const GURL& url,
                                       PrefService* pref_service)
    : MojoBubbleWebUIController(web_ui, /*enable_chrome_send=*/true) {
  CreateAndAddProjectorHTMLSource(web_ui);

  // The Annotator and Projector SWA embed contents in a sandboxed
  // chrome-untrusted:// iframe.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  // The requested WebUI is hosting the Projector SWA.
  ProjectorAppClient::Get()->NotifyAppUIActive(true);
}

TrustedProjectorUI::~TrustedProjectorUI() {
  ProjectorAppClient::Get()->NotifyAppUIActive(false);
}

WEB_UI_CONTROLLER_TYPE_IMPL(TrustedProjectorUI)

}  // namespace ash
