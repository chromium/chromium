// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/internals/contextual_cueing_internals_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_cueing/internals/contextual_cueing_internals_page_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/contextual_cueing_internals_resources.h"
#include "chrome/grit/contextual_cueing_internals_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace contextual_cueing_internals {

bool ContextualCueingInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(contextual_cueing::kContextualCueingV2);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ContextualCueingInternalsUI)

ContextualCueingInternalsUI::ContextualCueingInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIContextualCueingInternalsHost);

  webui::SetupWebUIDataSource(
      source, kContextualCueingInternalsResources,
      IDR_CONTEXTUAL_CUEING_INTERNALS_CONTEXTUAL_CUEING_INTERNALS_HTML);
}
ContextualCueingInternalsUI::~ContextualCueingInternalsUI() = default;

void ContextualCueingInternalsUI::BindInterface(
    mojo::PendingReceiver<contextual_cueing_internals::mojom::PageHandler>
        receiver) {
  page_handler_ = std::make_unique<ContextualCueingInternalsPageHandler>(
      std::move(receiver), Profile::FromWebUI(web_ui()));
}

}  // namespace contextual_cueing_internals
