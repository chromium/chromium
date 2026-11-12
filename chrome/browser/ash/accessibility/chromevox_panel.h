// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_PANEL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_PANEL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/widget/widget_delegate.h"

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}

namespace ash {

DECLARE_ELEMENT_IDENTIFIER_VALUE(kChromeVoxPanelElementId);

// Displays spoken feedback UI controls for the ChromeVox component extension.
class ChromeVoxPanel : public views::WidgetDelegate,
                       public content::WebContentsDelegate {
 public:
  explicit ChromeVoxPanel(content::BrowserContext* browser_context);

  ChromeVoxPanel(const ChromeVoxPanel&) = delete;
  ChromeVoxPanel& operator=(const ChromeVoxPanel&) = delete;

  ~ChromeVoxPanel() override;

  // Closes the panel immediately, deleting the WebView/WebContents.
  void CloseNow();

  // Closes the panel asynchronously.
  void Close();

  // WidgetDelegate:
  const views::Widget* GetWidget() const override;
  views::Widget* GetWidget() override;
  views::View* GetContentsView() override;
  void WidgetIsZombie(views::Widget* widget) override;

 private:
  class ChromeVoxPanelWebContentsObserver;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  bool ShouldAllowRendererInitiatedCrossProcessNavigation(
      bool is_outermost_main_frame_navigation) override;

  // Methods indirectly invoked by the component extension.
  void DidFirstVisuallyNonEmptyPaint();
  void EnterFullscreen();
  void ExitFullscreen();
  void Focus();

  // Sends a request to the ash window manager.
  void SetAccessibilityPanelFullscreen(bool fullscreen);

  std::string GetUrlForContent();

  // Destroys the widget and, with it, the WebView.
  void DestroyWidget();

  std::unique_ptr<ChromeVoxPanelWebContentsObserver> web_contents_observer_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> web_view_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_CHROMEVOX_PANEL_H_
