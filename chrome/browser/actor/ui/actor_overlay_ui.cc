// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_ui.h"

#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/actor/ui/actor_overlay_handler.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/actor_overlay_resources.h"
#include "chrome/grit/actor_overlay_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace actor::ui {

bool ActorOverlayUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kGlicActorUi) &&
         features::kGlicActorUiOverlay.Get() &&
         !browser_context->IsOffTheRecord();
}

ActorOverlayUI::ActorOverlayUI(content::WebUI* web_ui)
    : ::ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIActorOverlayHost);
  webui::SetupWebUIDataSource(source, kActorOverlayResources,
                              IDR_ACTOR_OVERLAY_ACTOR_OVERLAY_HTML);
  source->AddBoolean(
      "isMagicCursorEnabled",
      base::FeatureList::IsEnabled(features::kGlicActorUiMagicCursor));
  source->AddBoolean("isStandaloneBorderGlowEnabled",
                     features::kGlicActorUiStandaloneBorderGlow.Get());
  source->AddResourcePath("magic_cursor.svg", IDR_ACTOR_OVERLAY_MAGIC_CURSOR);
  source->AddDouble("magicCursorSpeed",
                    features::kGlicActorUiMagicCursorSpeed.Get());
  source->AddInteger("magicCursorMinDurationMs",
                     features::kGlicActorUiMagicCursorMinDuration.Get());
  source->AddInteger("magicCursorMaxDurationMs",
                     features::kGlicActorUiMagicCursorMaxDuration.Get());
}

WEB_UI_CONTROLLER_TYPE_IMPL(ActorOverlayUI)

ActorOverlayUI::~ActorOverlayUI() = default;

void ActorOverlayUI::BindInterface(
    mojo::PendingReceiver<mojom::ActorOverlayPageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void ActorOverlayUI::CreatePageHandler(
    mojo::PendingRemote<mojom::ActorOverlayPage> page,
    mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver) {
  handler_ = std::make_unique<ActorOverlayHandler>(
      std::move(page), std::move(receiver), web_ui()->GetWebContents());
  if (!handler_initialized_callbacks_.empty()) {
    for (auto& callback : handler_initialized_callbacks_) {
      std::move(callback).Run();
    }
    handler_initialized_callbacks_.clear();
  }
}

void ActorOverlayUI::SetHandlerInitializedCallback(base::OnceClosure callback) {
  // If handler is already initialized, run the callback immediately.
  if (handler_) {
    std::move(callback).Run();
    return;
  }
  handler_initialized_callbacks_.push_back(std::move(callback));
}

void ActorOverlayUI::SetOverlayBackground(bool is_visible) {
  // If handler is not initialized yet, queue the update.
  if (!handler_) {
    SetHandlerInitializedCallback(
        base::BindOnce(&ActorOverlayUI::SetOverlayBackground,
                       weak_factory_.GetWeakPtr(), is_visible));
    return;
  }
  handler_->SetOverlayBackground(is_visible);
}

void ActorOverlayUI::SetBorderGlowVisibility(bool is_visible) {
  DCHECK(handler_);
  handler_->SetBorderGlowVisibility(is_visible);
}

bool ActorOverlayUI::IsActorOverlayWebContents(
    content::WebContents* web_contents) {
  if (auto* web_ui = web_contents->GetWebUI()) {
    return web_ui->GetController() &&
           web_ui->GetController()->GetType() == &kWebUIControllerType;
  }
  return false;
}

void ActorOverlayUI::MoveCursorTo(const gfx::Point& point,
                                  base::OnceClosure callback) {
  DCHECK(handler_);
  handler_->MoveCursorTo(point, std::move(callback));
}

void ActorOverlayUI::TriggerClickAnimation(base::OnceClosure callback) {
  DCHECK(handler_);
  handler_->TriggerClickAnimation(std::move(callback));
}

}  // namespace actor::ui
