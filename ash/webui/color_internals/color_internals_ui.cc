// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/color_internals/color_internals_ui.h"

#include "ash/webui/color_internals/url_constants.h"
#include "ash/webui/color_internals/wallpaper_colors_handler_impl.h"
#include "ash/webui/grit/ash_color_internals_resources.h"
#include "ash/webui/grit/ash_color_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace ash {

ColorInternalsUI::ColorInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIColorInternalsHost);
  data_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  data_source->AddResourcePath("", IDR_ASH_COLOR_INTERNALS_INDEX_HTML);
  data_source->AddResourcePath(
      "color_internals_tokens.json",
      IDR_WEBUI_UI_CHROMEOS_STYLES_COLOR_INTERNALS_TOKENS_JSON);
  data_source->AddResourcePaths(base::make_span(
      kAshColorInternalsResources, kAshColorInternalsResourcesSize));
}

void ColorInternalsUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
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
