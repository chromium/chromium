// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_handler.h"

#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/color/color_provider.h"
#include "ui/native_theme/native_theme.h"

namespace actor::ui {

ActorOverlayHandler::ActorOverlayHandler(
    mojo::PendingRemote<mojom::ActorOverlayPage> page,
    mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver,
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      theme_service_(ThemeServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))),
      page_(std::move(page)),
      receiver_{this, std::move(receiver)} {
  native_theme_observation_.Observe(
      ::ui::NativeTheme::GetInstanceForNativeUi());
  theme_service_observation_.Observe(theme_service_);

  // Trigger the initial theme color.
  OnThemeChanged();
}

ActorOverlayHandler::~ActorOverlayHandler() = default;

void ActorOverlayHandler::OnHoverStatusChanged(bool is_hovering) {
  if (is_hovering_ == is_hovering) {
    return;
  }
  is_hovering_ = is_hovering;
  if (auto* tab_controller = ActorUiTabControllerInterface::From(
          webui::GetTabInterface(web_contents_))) {
    tab_controller->OnOverlayHoverStatusChanged(is_hovering);
  }
}

void ActorOverlayHandler::GetCurrentBorderGlowVisibility(
    GetCurrentBorderGlowVisibilityCallback callback) {
  if (auto* tab_controller = ActorUiTabControllerInterface::From(
          webui::GetTabInterface(web_contents_))) {
    std::move(callback).Run(tab_controller->GetCurrentUiTabState()
                                .actor_overlay.border_glow_visible);
  } else {
    std::move(callback).Run(false);
  }
}

void ActorOverlayHandler::SetOverlayBackground(bool is_visible) {
  page_->SetScrimBackground(is_visible);
}

void ActorOverlayHandler::SetBorderGlowVisibility(bool is_visible) {
  page_->SetBorderGlowVisibility(is_visible);
}

void ActorOverlayHandler::OnNativeThemeUpdated(
    ::ui::NativeTheme* observed_theme) {
  OnThemeChanged();
}

void ActorOverlayHandler::OnThemeChanged() {
  if (base::FeatureList::IsEnabled(features::kActorUiThemed)) {
    auto theme = mojom::Theme::New();
    const ::ui::ColorProvider& color_provider =
        web_contents_->GetColorProvider();
    std::vector<SkColor> scrim_colors;
    // Hex: 0x3D is 0.24 opacity
    scrim_colors = {
        SkColorSetA(color_provider.GetColor(kColorActorUiScrimStart), 0x3D),
        SkColorSetA(color_provider.GetColor(kColorActorUiScrimMiddle), 0x3D),
        SkColorSetA(color_provider.GetColor(kColorActorUiScrimEnd), 0x3D)};
    theme->scrim_colors = scrim_colors;
    theme->border_color =
        web_contents_->GetColorProvider().GetColor(kColorActorUiOverlayBorder);
    theme->border_glow_color = web_contents_->GetColorProvider().GetColor(
        kColorActorUiOverlayBorderGlow);
    page_->SetTheme(std::move(theme));
  }
}

}  // namespace actor::ui
