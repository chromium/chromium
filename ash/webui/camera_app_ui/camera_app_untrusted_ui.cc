// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/camera_app_ui/camera_app_untrusted_ui.h"

#include <string>

#include "ash/webui/camera_app_ui/url_constants.h"
#include "ash/webui/grit/ash_camera_app_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "url/gurl.h"

namespace ash {

namespace {

content::WebUIDataSource* CreateUntrustedCameraAppUIHTMLSource() {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::Create(kChromeUIUntrustedCameraAppURL);
  untrusted_source->AddResourcePaths(
      base::make_span(kAshCameraAppResources, kAshCameraAppResourcesSize));
  untrusted_source->AddFrameAncestor(GURL(kChromeUICameraAppURL));

  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      std::string("connect-src http://www.google-analytics.com/ 'self';"));
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      std::string("worker-src 'self';"));
  // TODO(crbug/948834): Replace 'wasm-eval' with 'wasm-unsafe-eval'.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      std::string("script-src 'self' 'wasm-eval';"));
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      std::string("trusted-types ga-js-static video-processor-js-static;"));

  return untrusted_source;
}

}  // namespace

CameraAppUntrustedUI::CameraAppUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* untrusted_source =
      CreateUntrustedCameraAppUIHTMLSource();

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, untrusted_source);
}

CameraAppUntrustedUI::~CameraAppUntrustedUI() = default;

}  // namespace ash
