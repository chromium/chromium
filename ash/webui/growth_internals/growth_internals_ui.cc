// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/growth_internals/growth_internals_ui.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/growth_internals_resources.h"
#include "ash/webui/grit/growth_internals_resources_map.h"
#include "ash/webui/growth_internals/constants.h"
#include "ash/webui/growth_internals/growth_internals_page_handler.h"
#include "base/containers/span.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

bool GrowthInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsGrowthInternalsEnabled();
}

GrowthInternalsUI::GrowthInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          std::string(kGrowthInternalsHost));

  data_source->AddResourcePath("", IDR_GROWTH_INTERNALS_INDEX_HTML);
  data_source->AddResourcePaths(base::make_span(kGrowthInternalsResources,
                                                kGrowthInternalsResourcesSize));

  data_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' chrome://resources 'unsafe-inline';");

  data_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types polymer-html-literal "
      "polymer-template-event-attribute-policy;");
}

GrowthInternalsUI::~GrowthInternalsUI() = default;

void GrowthInternalsUI::BindInterface(
    mojo::PendingReceiver<growth::mojom::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<GrowthInternalsPageHandler>(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(GrowthInternalsUI)

}  // namespace ash
