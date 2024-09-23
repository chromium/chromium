// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_PANEL_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_PANEL_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/widget/widget_delegate.h"

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}

namespace ash {

const char EXTENSION_PREFIX[] = "chrome-extension://";

// Creates a panel onscreen on which an accessibility extension can draw a
// custom UI.
class AccessibilityPanel : public views::WidgetDelegate,
                           public content::WebContentsDelegate {
 public:
  explicit AccessibilityPanel(content::BrowserContext* browser_context,
                              const std::string& content_url,
                              const std::string& widget_name);

  AccessibilityPanel(const AccessibilityPanel&) = delete;
  AccessibilityPanel& operator=(const AccessibilityPanel&) = delete;

  ~AccessibilityPanel() override;

  // Closes the panel immediately, deleting the WebView/WebContents.
  void CloseNow();

  // Closes the panel asynchronously.
  void Close();

  // WidgetDelegate:
  const views::Widget* GetWidget() const override;
  views::Widget* GetWidget() override;
  views::View* GetContentsView() override;

 protected:
  // Returns the web contents, so subclasses can monitor for changes.
  content::WebContents* GetWebContents();

 private:
  class AccessibilityPanelWebContentsObserver;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  // Indirectly invoked by the component extension.
  void DidFirstVisuallyNonEmptyPaint();

  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<AccessibilityPanelWebContentsObserver> web_contents_observer_;
  raw_ptr<views::Widget> widget_ = nullptr;
  raw_ptr<views::View> web_view_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_PANEL_H_
