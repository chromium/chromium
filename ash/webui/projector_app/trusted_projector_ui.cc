// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/trusted_projector_ui.h"

#include "ash/public/cpp/projector/projector_annotator_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/grit/ash_projector_app_trusted_resources.h"
#include "ash/webui/grit/ash_projector_app_trusted_resources_map.h"
#include "ash/webui/projector_app/projector_message_handler.h"
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

content::WebUIDataSource* CreateProjectorHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIProjectorAppHost);

  source->AddResourcePaths(base::make_span(
      kAshProjectorAppTrustedResources, kAshProjectorAppTrustedResourcesSize));

  source->AddResourcePath("", IDR_ASH_PROJECTOR_APP_TRUSTED_APP_EMBEDDER_HTML);
  source->AddLocalizedString("appTitle", IDS_ASH_PROJECTOR_DISPLAY_SOURCE);

  std::string csp =
      std::string("frame-src ") + kChromeUIUntrustedProjectorAppUrl + ";";

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);

  return source;
}

}  // namespace

TrustedProjectorUI::TrustedProjectorUI(content::WebUI* web_ui,
                                       const GURL& url,
                                       PrefService* pref_service)
    : MojoBubbleWebUIController(web_ui, /*enable_chrome_send=*/true) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, CreateProjectorHTMLSource());

  // The selfie cam doesn't have any dependencies on WebUIMessageHandlers;
  // it also doesn't embed chrome-untrusted:// resources. Therefore, return
  // early.
  if (url == GURL(kChromeUITrustedProjectorSelfieCamUrl))
    return;

  // The Annotator and Projector SWA embed contents in a sandboxed
  // chrome-untrusted:// iframe.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  // The requested WebUI is hosting the Projector SWA.
  web_ui->AddMessageHandler(
      std::make_unique<ProjectorMessageHandler>(pref_service));
}

TrustedProjectorUI::~TrustedProjectorUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(TrustedProjectorUI)

}  // namespace ash
