// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/color_internals/color_internals_ui.h"

#include "ash/webui/color_internals/url_constants.h"
#include "ash/webui/grit/ash_color_internals_resources.h"
#include "ash/webui/grit/ash_color_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

ColorInternalsUI::ColorInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  auto data_source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUIColorInternalsHost));

  data_source->AddResourcePath("", IDR_ASH_COLOR_INTERNALS_INDEX_HTML);
  data_source->AddResourcePaths(base::make_span(
      kAshColorInternalsResources, kAshColorInternalsResourcesSize));

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, data_source.release());
}

ColorInternalsUI::~ColorInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ColorInternalsUI)

}  // namespace ash
