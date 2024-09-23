// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_panel.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/utility/wm_util.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

// Monitors the contents of the accessibility panel for relevant changes
class AccessibilityPanel::AccessibilityPanelWebContentsObserver
    : public content::WebContentsObserver {
 public:
  AccessibilityPanelWebContentsObserver(content::WebContents* web_contents,
                                        AccessibilityPanel* panel)
      : content::WebContentsObserver(web_contents), panel_(panel) {}

  AccessibilityPanelWebContentsObserver(
      const AccessibilityPanelWebContentsObserver&) = delete;
  AccessibilityPanelWebContentsObserver& operator=(
      const AccessibilityPanelWebContentsObserver&) = delete;

  ~AccessibilityPanelWebContentsObserver() override = default;

  // content::WebContentsObserver overrides.
  void DidFirstVisuallyNonEmptyPaint() override {
    panel_->DidFirstVisuallyNonEmptyPaint();
  }

 private:
  raw_ptr<AccessibilityPanel> panel_;
};

AccessibilityPanel::AccessibilityPanel(content::BrowserContext* browser_context,
                                       const std::string& content_url,
                                       const std::string& widget_name) {
  SetOwnedByWidget(true);

  views::WebView* web_view = new views::WebView(browser_context);
  web_contents_ = web_view->GetWebContents();
  web_contents_observer_ =
      std::make_unique<AccessibilityPanelWebContentsObserver>(web_contents_,
                                                              this);
  web_contents_->SetDelegate(this);
  extensions::SetViewType(web_contents_,
                          extensions::mojom::ViewType::kComponent);
  web_view->LoadInitialURL(GURL(content_url));
  web_view_ = web_view;

  widget_ = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  // Placing the panel in the accessibility panel container allows ash to manage
  // both the window bounds and display work area.
  // The AccessibilityPanel is only shown in the primary root window.
  ash_util::SetupWidgetInitParamsForContainerInPrimary(
      &params, ShellWindowId::kShellWindowId_AccessibilityPanelContainer);
  params.bounds = display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  params.delegate = this;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.name = widget_name;
  params.shadow_elevation = wm::kShadowElevationInactiveWindow;
  widget_->Init(std::move(params));
  // We rely on being able to set the bounds of the panel to control when it
  // captures input rather than hiding the widget because we need to continue to
  // receive key events. crbug.com/1251129.
  widget_->GetNativeWindow()->layer()->SetMasksToBounds(true);
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

views::View* AccessibilityPanel::GetContentsView() {
  return web_view_;
}

bool AccessibilityPanel::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Eat all requests as context menus are disallowed.
  return true;
}

void AccessibilityPanel::DidFirstVisuallyNonEmptyPaint() {
  widget_->Show();
}

}  // namespace ash
