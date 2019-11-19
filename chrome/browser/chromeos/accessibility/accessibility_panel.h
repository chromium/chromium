// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_PANEL_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_PANEL_H_

#include <stdint.h>

#include "base/macros.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/widget/widget_delegate.h"

namespace content {
class BrowserContext;
}

namespace views {
class Widget;
}

const char EXTENSION_PREFIX[] = "chrome-extension://";

// Creates a panel onscreen on which an accessibility extension can draw a
// custom UI.
class AccessibilityPanel : public views::WidgetDelegate,
                           public content::WebContentsDelegate {
 public:
  explicit AccessibilityPanel(content::BrowserContext* browser_context,
                              std::string content_url,
                              std::string widget_name);
  ~AccessibilityPanel() override;

  // Closes the panel immediately, deleting the WebView/WebContents.
  void CloseNow();

  // Closes the panel asynchronously.
  void Close();

  // WidgetDelegate overrides.
  const views::Widget* GetWidget() const override;
  views::Widget* GetWidget() override;
  void DeleteDelegate() override;
  views::View* GetContentsView() override;

 protected:
  // Returns the web contents, so subclasses can monitor for changes.
  content::WebContents* GetWebContents();

 private:
  class AccessibilityPanelWebContentsObserver;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;

  // Indirectly invoked by the component extension.
  void DidFirstVisuallyNonEmptyPaint();

  content::WebContents* web_contents_;
  std::unique_ptr<AccessibilityPanelWebContentsObserver> web_contents_observer_;
  views::Widget* widget_ = nullptr;
  views::View* web_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityPanel);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_PANEL_H_
