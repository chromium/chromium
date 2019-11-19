// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

class AssistantViewDelegate;
class AssistantWebViewDelegate;
class AssistantWebView;

// The container of assistant_web_view when Assistant web container is enabled.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantWebContainerView
    : public views::WidgetDelegateView {
 public:
  AssistantWebContainerView(
      AssistantViewDelegate* assistant_view_delegate,
      AssistantWebViewDelegate* web_container_view_delegate);
  ~AssistantWebContainerView() override;

  // views::WidgetDelegateView:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;

  void OnBackButtonPressed();

  views::View* GetCaptionBarForTesting();

 private:
  void InitLayout();

  AssistantViewDelegate* const assistant_view_delegate_;
  AssistantWebViewDelegate* const web_container_view_delegate_;

  // Owned by the views hierarchy.
  AssistantWebView* assistant_web_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantWebContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_WEB_CONTAINER_VIEW_H_
