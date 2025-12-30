// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_HANDLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/actor/ui/actor_overlay.mojom.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace actor::ui {

class ActorOverlayHandler : public mojom::ActorOverlayPageHandler,
                            public ThemeServiceObserver,
                            public ::ui::NativeThemeObserver {
 public:
  ActorOverlayHandler(
      mojo::PendingRemote<mojom::ActorOverlayPage> page,
      mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver,
      content::WebContents* web_contents);

  ActorOverlayHandler(const ActorOverlayHandler&) = delete;
  ActorOverlayHandler& operator=(const ActorOverlayHandler&) = delete;

  ~ActorOverlayHandler() override;

  // mojom::ActorOverlayPageHandler:
  // Notifies the ActorUiTabController that the user's hovering status over the
  // overlay has changed. Called by the ActorOverlay WebUI (renderer-side).
  void OnHoverStatusChanged(bool is_hovering) override;

  // mojom::ActorOverlayPageHandler
  // Calls the ActorUiTabController to retrieve the most current BorderGlow
  // visibility state. Called by the ActorOverlay WebUi (renderer-side)
  void GetCurrentBorderGlowVisibility(
      GetCurrentBorderGlowVisibilityCallback callback) override;

  // mojom::ActorOverlayPage:
  // Forwards the scrim background visibility to WebUI.
  void SetOverlayBackground(bool is_visible);

  // Forwards the border glow visibility to WebUI.
  void SetBorderGlowVisibility(bool is_visible);

 private:
  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(::ui::NativeTheme* observed_theme) override;
  // ThemeServiceObserver:
  void OnThemeChanged() override;
  // Is the user hovering over the actor overlay.
  bool is_hovering_ = false;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<ThemeService> theme_service_;
  base::ScopedObservation<::ui::NativeTheme, ::ui::NativeThemeObserver>
      native_theme_observation_{this};
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};

  mojo::Remote<mojom::ActorOverlayPage> page_;
  mojo::Receiver<mojom::ActorOverlayPageHandler> receiver_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_HANDLER_H_
