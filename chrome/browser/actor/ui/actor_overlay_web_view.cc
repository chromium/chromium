// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_web_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/view_class_properties.h"

ActorOverlayWebView::ActorOverlayWebView(BrowserWindowInterface* browser)
    : browser_(browser) {
  // Required to create a new web contents if one doesn't exist.
  SetBrowserContext(browser->GetProfile());
  SetVisible(false);
  SetFocusBehavior(views::View::FocusBehavior::NEVER);
  GetViewAccessibility().SetIsIgnored(true);
}

ActorOverlayWebView::~ActorOverlayWebView() {
  CloseUI();
  SetWebContents(nullptr);
}

void ActorOverlayWebView::ShowUI(tabs::TabInterface* tab) {
  CHECK(features::kGlicActorUiOverlay.Get());
  if (!web_contents()) {
    // Creates a new web contents if one doesn't exist.
    LoadInitialURL(GURL(chrome::kChromeUIActorOverlayURL));
  }
  // Disable mouse, keyboard, and a11y input events to underlying tab
  // contents.
  scoped_ignore_input_events_ = tab->GetContents()->IgnoreInputEvents(
      std::nullopt, /*should_ignore_a11y_input=*/true);
  // Set the tab interface
  webui::SetTabInterface(web_contents(), tab);

  // Make the view background transparent so it can act as an overlay.
  content::RenderWidgetHostView* rwhv =
      web_contents()->GetRenderWidgetHostView();
  if (rwhv) {
    rwhv->SetBackgroundColor(SK_ColorTRANSPARENT);
  }

  SetVisible(true);
  web_contents()->WasShown();
}

void ActorOverlayWebView::CloseUI() {
  if (web_contents()) {
    SetVisible(false);
    // Re-enable mouse, keyboard, and a11y input events to the underlying web
    // contents by resetting the ScopedIgnoreInputEvents object.
    scoped_ignore_input_events_.reset();
    web_contents()->WasHidden();
  }
}

void ActorOverlayWebView::SetOverlayBackground(bool is_visible) {
  actor::ui::ActorOverlayUI* web_ui = GetWebUi();
  if (!web_ui) {
    return;
  }

  web_ui->SetOverlayBackground(is_visible);
}

void ActorOverlayWebView::SetBorderGlowVisibility(bool is_visible) {
  actor::ui::ActorOverlayUI* web_ui = GetWebUi();
  if (!web_ui) {
    return;
  }

  web_ui->SetBorderGlowVisibility(is_visible);
}

actor::ui::ActorOverlayUI* ActorOverlayWebView::GetWebUi() {
  if (!web_contents()) {
    return nullptr;
  }

  return web_contents()
      ->GetWebUI()
      ->GetController()
      ->GetAs<actor::ui::ActorOverlayUI>();
}

BEGIN_METADATA(ActorOverlayWebView)
END_METADATA
