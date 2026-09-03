// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_internals_ui.h"

#include <utility>

#include "chrome/browser/glic/host/glic_internals_page_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/glic_resources.h"
#include "chrome/grit/glic_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace glic {

GlicInternalsUI::GlicInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIGlicHost);

  webui::SetupWebUIDataSource(source, kGlicResources,
                              IDR_GLIC_INTERNALS_GLIC_INTERNALS_HTML);
  source->AddResourcePath("internals", IDR_GLIC_INTERNALS_GLIC_INTERNALS_HTML);
  source->AddResourcePath("internals/", IDR_GLIC_INTERNALS_GLIC_INTERNALS_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(GlicInternalsUI)

GlicInternalsUI::~GlicInternalsUI() = default;

void GlicInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::InternalsPageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void GlicInternalsUI::CreateInternalsPageHandler(
    mojo::PendingReceiver<mojom::InternalsPageHandler> receiver) {
  page_handler_ = std::make_unique<GlicInternalsPageHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

}  // namespace glic
