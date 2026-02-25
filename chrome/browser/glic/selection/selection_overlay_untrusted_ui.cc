// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_overlay_untrusted_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/glic_untrusted_resources.h"
#include "chrome/grit/glic_untrusted_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace glic {

bool SelectionOverlayUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return GlicEnabling::IsProfileEligible(
             Profile::FromBrowserContext(browser_context)) &&
         base::FeatureList::IsEnabled(::features::kGlicRegionSelectionNew);
}

SelectionOverlayUntrustedUI::SelectionOverlayUntrustedUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  // Set up the chrome-untrusted://glic/selection-overlay source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIGlicUntrustedURL);
  CHECK(html_source);
  webui::SetupWebUIDataSource(html_source, kGlicUntrustedResources,
                              IDR_GLIC_UNTRUSTED_SELECTION_OVERLAY_HTML);
}

SelectionOverlayUntrustedUI::~SelectionOverlayUntrustedUI() = default;

void SelectionOverlayUntrustedUI::BindInterface(
    mojo::PendingReceiver<selection::SelectionOverlayPageHandlerFactory>
        receiver) {
  selection_page_factory_receiver_.reset();
  selection_page_factory_receiver_.Bind(std::move(receiver));
}

SelectionOverlayController&
SelectionOverlayUntrustedUI::GetSelectionOverlayController() {
  SelectionOverlayController* controller =
      SelectionOverlayController::FromOverlayWebContents(
          web_ui()->GetWebContents());
  return *controller;
}

void SelectionOverlayUntrustedUI::CreatePageHandler(
    mojo::PendingReceiver<selection::SelectionOverlayPageHandler> receiver,
    mojo::PendingRemote<selection::SelectionOverlayPage> page) {
  SelectionOverlayController& controller = GetSelectionOverlayController();
  controller.BindOverlay(std::move(receiver), std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(SelectionOverlayUntrustedUI)

}  // namespace glic
