// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_ui.h"

#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/actor_overlay_resources.h"
#include "chrome/grit/actor_overlay_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace actor::ui {

bool ActorOverlayUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::kGlicActorUiOverlay.Get();
}

ActorOverlayUI::ActorOverlayUI(content::WebUI* web_ui)
    : ::ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIActorOverlayHost);
  webui::SetupWebUIDataSource(source, kActorOverlayResources,
                              IDR_ACTOR_OVERLAY_ACTOR_OVERLAY_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ActorOverlayUI)

ActorOverlayUI::~ActorOverlayUI() = default;

void ActorOverlayUI::BindInterface(
    mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver) {
  content::WebContents* web_contents = web_ui()->GetWebContents();
  tabs::TabInterface* tab_interface = webui::GetTabInterface(web_contents);
  ActorUiTabControllerInterface* actor_ui_tab_controller =
      tab_interface->GetTabFeatures()->actor_ui_tab_controller();
  CHECK(actor_ui_tab_controller);
  actor_ui_tab_controller->BindActorOverlay(std::move(receiver));
}

}  // namespace actor::ui
