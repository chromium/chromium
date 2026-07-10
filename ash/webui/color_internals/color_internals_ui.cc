// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "ash/webui/color_internals/color_internals_ui.h"

#include "ash/webui/color_internals/url_constants.h"
#include "ash/webui/color_internals/wallpaper_colors_handler_impl.h"
#include "ash/webui/grit/ash_color_internals_resources.h"
#include "ash/webui/grit/ash_color_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/webui_util.h"

namespace ash {

ColorInternalsUI::ColorInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIColorInternalsHost);

  webui::SetupWebUIDataSource(data_source, kAshColorInternalsResources,
                              IDR_ASH_COLOR_INTERNALS_INDEX_HTML);
  data_source->AddResourcePath(
      "color_internals_tokens.json",
      IDR_WEBUI_UI_CHROMEOS_STYLES_COLOR_INTERNALS_TOKENS_JSON);
}

void ColorInternalsUI::BindInterface(
    mojo::PendingReceiver<color_internals::mojom::WallpaperColorsHandler>
        receiver) {
  wallpaper_colors_handler_ =
      std::make_unique<WallpaperColorsHandlerImpl>(std::move(receiver));
}

ColorInternalsUI::~ColorInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(ColorInternalsUI)

}  // namespace ash
