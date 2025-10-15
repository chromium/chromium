// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_WEB_VIEW_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_WEB_VIEW_H_

#include <optional>

#include "chrome/browser/actor/ui/actor_overlay_ui.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

// ActorOverlayWebView is used to display the WebContents of the actor overlay.
class ActorOverlayWebView : public views::WebView {
  METADATA_HEADER(ActorOverlayWebView, views::WebView)

 public:
  explicit ActorOverlayWebView(BrowserWindowInterface* browser);
  ActorOverlayWebView(const ActorOverlayWebView&) = delete;
  ActorOverlayWebView& operator=(const ActorOverlayWebView&) = delete;
  ~ActorOverlayWebView() override;

  // Creates a transparent overlay view displaying chrome://actor-overlay.
  void ShowUI(tabs::TabInterface* tab);
  // Detaches the web contents from this webview.
  // If the UI is closed, this webview may still exist as an empty container
  // until ShowUI is called again.
  void CloseUI();

  // Forwards the overlay background visibility to WebUI.
  void SetOverlayBackground(bool is_visible);

  // Forwards the border glow visibility to WebUI.
  void SetBorderGlowVisibility(bool is_visible);

 private:
  actor::ui::ActorOverlayUI* GetWebUi();
  // Manages the lifetime of the WebContents input event ignoring state.
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;

  raw_ptr<BrowserWindowInterface> browser_;

  base::WeakPtrFactory<ActorOverlayWebView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_OVERLAY_WEB_VIEW_H_
