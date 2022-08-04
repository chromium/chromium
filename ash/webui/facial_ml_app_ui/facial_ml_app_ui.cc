// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/facial_ml_app_ui/facial_ml_app_ui.h"

#include <utility>

#include "ash/webui/facial_ml_app_ui/url_constants.h"
#include "ash/webui/grit/ash_facial_ml_app_resources.h"
#include "ash/webui/grit/ash_facial_ml_app_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_allowlist.h"

namespace ash {

FacialMLAppUI::FacialMLAppUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* trusted_source =
      content::WebUIDataSource::CreateAndAdd(browser_context,
                                             kChromeUIFacialMLAppHost);
  trusted_source->AddResourcePath("", IDR_ASH_FACIAL_ML_APP_INDEX_HTML);
  trusted_source->AddResourcePaths(
      base::make_span(kAshFacialMlAppResources, kAshFacialMlAppResourcesSize));

#if !DCHECK_IS_ON()
  // Skip default page setting in the product mode, so the developers will get
  // an error page if anything is wrong.
  trusted_source->SetDefaultResource(IDR_ASH_FACIAL_ML_APP_INDEX_HTML);
#endif  // !DCHECK_IS_ON()

  // Register common permissions for chrome://facial_ml pages.
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  const url::Origin app_origin =
      url::Origin::Create(GURL(kChromeUIFacialMLAppURL));
  webui_allowlist->RegisterAutoGrantedPermissions(
      app_origin, {
                      ContentSettingsType::COOKIES,
                      ContentSettingsType::JAVASCRIPT,
                      ContentSettingsType::IMAGES,
                      ContentSettingsType::SOUND,
                  });
}

FacialMLAppUI::~FacialMLAppUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(FacialMLAppUI)

}  // namespace ash
