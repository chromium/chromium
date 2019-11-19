// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_panel.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_animations.h"

// Monitors the contents of the accessibility panel for relevant changes
class AccessibilityPanel::AccessibilityPanelWebContentsObserver
    : public content::WebContentsObserver {
 public:
  AccessibilityPanelWebContentsObserver(content::WebContents* web_contents,
                                        AccessibilityPanel* panel)
      : content::WebContentsObserver(web_contents), panel_(panel) {}
  ~AccessibilityPanelWebContentsObserver() override = default;

  // content::WebContentsObserver overrides.
  void DidFirstVisuallyNonEmptyPaint() override {
    panel_->DidFirstVisuallyNonEmptyPaint();
  }

 private:
  AccessibilityPanel* panel_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityPanelWebContentsObserver);
};

AccessibilityPanel::AccessibilityPanel(content::BrowserContext* browser_context,
                                       std::string content_url,
                                       std::string widget_name) {
  views::WebView* web_view = new views::WebView(browser_context);
  web_contents_ = web_view->GetWebContents();
  web_contents_observer_.reset(
      new AccessibilityPanelWebContentsObserver(web_contents_, this));
  web_contents_->SetDelegate(this);
  extensions::SetViewType(web_contents_, extensions::VIEW_TYPE_COMPONENT);
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents_);
  web_view->LoadInitialURL(GURL(content_url));
  web_view_ = web_view;

  widget_ = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  // Placing the panel in the accessibility panel container allows ash to manage
  // both the window bounds and display work area.
  ash_util::SetupWidgetInitParamsForContainer(
      &params, ash::kShellWindowId_AccessibilityPanelContainer);
  params.bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  params.delegate = this;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.name = widget_name;
  params.shadow_elevation = wm::kShadowElevationInactiveWindow;
  widget_->Init(std::move(params));
}

AccessibilityPanel::~AccessibilityPanel() = default;

void AccessibilityPanel::CloseNow() {
  widget_->CloseNow();
}

void AccessibilityPanel::Close() {
  // NOTE: Close the widget asynchronously because it's not legal to delete
  // a WebView/WebContents during a DidFinishNavigation callback.
  widget_->Close();
}

const views::Widget* AccessibilityPanel::GetWidget() const {
  return widget_;
}

views::Widget* AccessibilityPanel::GetWidget() {
  return widget_;
}

content::WebContents* AccessibilityPanel::GetWebContents() {
  return web_contents_;
}

void AccessibilityPanel::DeleteDelegate() {
  delete this;
}

views::View* AccessibilityPanel::GetContentsView() {
  return web_view_;
}

bool AccessibilityPanel::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Eat all requests as context menus are disallowed.
  return true;
}

void AccessibilityPanel::DidFirstVisuallyNonEmptyPaint() {
  widget_->Show();
}
