// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

void CreateAndAddUntrustedCameraAppUIHTMLSource(
    content::BrowserContext* browser_context) {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::CreateAndAdd(browser_context,
                                             kChromeUIUntrustedCameraAppURL);
  untrusted_source->AddResourcePaths(
      base::make_span(kAshCameraAppResources, kAshCameraAppResourcesSize));
  untrusted_source->AddFrameAncestor(GURL(kChromeUICameraAppURL));

  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      std::string("connect-src http://www.google-analytics.com/ 'self';"));
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      std::string("worker-src 'self';"));
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      std::string("script-src 'self' 'wasm-unsafe-eval';"));
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      std::string("trusted-types ga-js-static video-processor-js-static;"));

  // Make untrusted source cross-origin-isolated to measure memory usage.
  untrusted_source->OverrideCrossOriginOpenerPolicy("same-origin");
  untrusted_source->OverrideCrossOriginEmbedderPolicy("credentialless");
  untrusted_source->OverrideCrossOriginResourcePolicy("cross-origin");
}

}  // namespace

CameraAppUntrustedUI::CameraAppUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  CreateAndAddUntrustedCameraAppUIHTMLSource(
      web_ui->GetWebContents()->GetBrowserContext());
}

CameraAppUntrustedUI::~CameraAppUntrustedUI() = default;

}  // namespace ash
