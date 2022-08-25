// Copyright 2022 The Chromium Authors. All rights reserved.
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

FaceMLAppUI::FaceMLAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
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
  const url::Origin untrusted_origin =
      url::Origin::Create(GURL(kChromeUIFaceMLAppUntrustedURL));
  webui_allowlist->RegisterAutoGrantedPermissions(
      untrusted_origin, {
                            ContentSettingsType::COOKIES,
                            ContentSettingsType::JAVASCRIPT,
                            ContentSettingsType::IMAGES,
                            ContentSettingsType::SOUND,
                        });
}

FaceMLAppUI::~FaceMLAppUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(FaceMLAppUI)

}  // namespace ash
