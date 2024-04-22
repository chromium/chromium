// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_

#include "ash/public/cpp/ash_web_view.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class ClientView;
}  // namespace views

namespace ash {

class AssistantWebViewDelegate;

// The container for hosting standalone WebContents in Assistant.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantWebContainerView
    : public views::WidgetDelegateView,
      public AshWebView::Observer {
  METADATA_HEADER(AssistantWebContainerView, views::WidgetDelegateView)

 public:
  explicit AssistantWebContainerView(
      AssistantWebViewDelegate* web_container_view_delegate);

  AssistantWebContainerView(const AssistantWebContainerView&) = delete;
  AssistantWebContainerView& operator=(const AssistantWebContainerView&) =
      delete;

  ~AssistantWebContainerView() override;

  AshWebView* web_view() {
    return web_view_ptr_ ? web_view_ptr_.get() : web_view_.get();
  }

  // views::WidgetDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  void OnThemeChanged() override;

  // AssistantWebView::Observer:
  void DidStopLoading() override;
  void DidSuppressNavigation(const GURL& url,
                             WindowOpenDisposition disposition,
                             bool from_user_gesture) override;
  void DidChangeCanGoBack(bool can_go_back) override;

  // Invoke to navigate back in the embedded WebContents' navigation stack. If
  // backwards navigation is not possible, returns |false|. Otherwise |true| to
  // indicate success.
  bool GoBack();

  // Invoke to open the specified |url|.
  void OpenUrl(const GURL& url);

  void SetBackgroundRadii(const gfx::RoundedCornersF& radii);

  void SetCanGoBackForTesting(bool can_go_back);

 private:
  void InitLayout();
  void RemoveContents();
  void UpdateBackground();

  const raw_ptr<AssistantWebViewDelegate> web_container_view_delegate_;

  std::unique_ptr<AshWebView> web_view_;
  raw_ptr<AshWebView, DanglingUntriaged> web_view_ptr_ = nullptr;

  gfx::RoundedCornersF background_radii_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_
