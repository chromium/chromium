// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/embedded_browser.h"

#include "ash/public/cpp/app_types.h"
#include "ash/wm/window_state.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace ash {
namespace shell {

namespace {

class BrowserWidgetDelegateView : public views::WidgetDelegateView {
 public:
  BrowserWidgetDelegateView(content::BrowserContext* context, const GURL& url) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    auto* webview = new views::WebView(context);
    AddChildView(webview);
    Layout();
    webview->LoadInitialURL(url);
  }
  ~BrowserWidgetDelegateView() override = default;

  // views::WidgetDelegateView:
  base::string16 GetWindowTitle() const override {
    const static base::string16 title = base::ASCIIToUTF16("WebView Browser");
    return title;
  }
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool CanMinimize() const override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserWidgetDelegateView);
};

}  // namespace

EmbeddedBrowser::EmbeddedBrowser(content::BrowserContext* context,
                                 const GURL& url,
                                 const gfx::Rect& bounds)
    : widget_(new views::Widget) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = bounds;
  params.delegate = new BrowserWidgetDelegateView(context, url);
  widget_->Init(std::move(params));
  WindowState::Get(widget_->GetNativeWindow())->SetWindowPositionManaged(true);
  widget_->Show();
}

EmbeddedBrowser::~EmbeddedBrowser() = default;

aura::Window* EmbeddedBrowser::GetWindow() {
  return widget_->GetNativeView();
}

// static
aura::Window* EmbeddedBrowser::Create(content::BrowserContext* context,
                                      const GURL& url,
                                      base::Optional<gfx::Rect> bounds) {
  static const gfx::Rect default_bounds(20, 20, 800, 600);

  // EmbeddedBrowser deletes itself when the widget is closed.
  aura::Window* browser_window =
      (new EmbeddedBrowser(context, url, bounds ? *bounds : default_bounds))
          ->GetWindow();
  browser_window->SetProperty(aura::client::kAppType,
                              static_cast<int>(ash::AppType::BROWSER));
  return browser_window;
}

void EmbeddedBrowser::OnUnembed() {
  delete this;
}

}  // namespace shell
}  // namespace ash
