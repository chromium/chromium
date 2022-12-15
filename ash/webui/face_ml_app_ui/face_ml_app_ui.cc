// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/face_ml_app_ui/face_ml_app_ui.h"

#include <utility>

#include "ash/webui/face_ml_app_ui/url_constants.h"
#include "ash/webui/grit/ash_face_ml_app_resources.h"
#include "ash/webui/grit/ash_face_ml_app_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {
FaceMLAppUI::FaceMLAppUI(content::WebUI* web_ui,
                         std::unique_ptr<FaceMLUserProvider> user_provider)
    : ui::MojoWebUIController(web_ui),
      user_provider_(std::move(user_provider)) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* trusted_source =
      content::WebUIDataSource::CreateAndAdd(browser_context,
                                             kChromeUIFaceMLAppHost);
  trusted_source->AddResourcePath("", IDR_ASH_FACE_ML_APP_INDEX_HTML);
  trusted_source->AddResourcePaths(
      base::make_span(kAshFaceMlAppResources, kAshFaceMlAppResourcesSize));

#if !DCHECK_IS_ON()
  // Skip default page setting in the product mode, so the developers will get
  // an error page if anything is wrong.
  trusted_source->SetDefaultResource(IDR_ASH_FACE_ML_APP_INDEX_HTML);
#endif  // !DCHECK_IS_ON()

  // We need a CSP override to use the chrome-untrusted:// scheme in the host.
  trusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      std::string("frame-src ") + kChromeUIFaceMLAppUntrustedURL + ";");

  // Add ability to request "chrome-untrusted" URLs.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  // Register common permissions for chrome-untrusted://face-ml pages.
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin origin = url::Origin::Create(GURL(kChromeUIFaceMLAppURL));
  webui_allowlist->RegisterAutoGrantedPermissions(
      origin, {
                  ContentSettingsType::COOKIES,
                  ContentSettingsType::DISPLAY_CAPTURE,
                  ContentSettingsType::JAVASCRIPT,
                  ContentSettingsType::IMAGES,
                  ContentSettingsType::MEDIASTREAM_CAMERA,
                  ContentSettingsType::SOUND,
              });
}

FaceMLAppUI::~FaceMLAppUI() = default;

void FaceMLAppUI::BindInterface(
    mojo::PendingReceiver<mojom::face_ml_app::PageHandlerFactory> factory) {
  if (face_ml_page_factory_.is_bound()) {
    face_ml_page_factory_.reset();
  }
  face_ml_page_factory_.Bind(std::move(factory));
}

void FaceMLAppUI::CreatePageHandler(
    mojo::PendingReceiver<mojom::face_ml_app::PageHandler> handler,
    mojo::PendingRemote<mojom::face_ml_app::Page> page) {
  DCHECK(page.is_valid());
  face_ml_page_handler_->BindInterface(std::move(handler), std::move(page));
}

void FaceMLAppUI::WebUIPrimaryPageChanged(content::Page& page) {
  // Create a new page handler for each document load. This avoids sharing
  // states when WebUIController is reused for same-origin navigations.
  face_ml_page_handler_ = std::make_unique<FaceMLPageHandler>(this);
}

WEB_UI_CONTROLLER_TYPE_IMPL(FaceMLAppUI)

}  // namespace ash
