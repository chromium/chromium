// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_web_view_delegate_impl.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/caption_buttons/caption_button_model.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_types.h"
#include "ui/views/window/non_client_view.h"

namespace ash {

namespace {

class AssistantWebContainerCaptionButtonModel : public CaptionButtonModel {
 public:
  AssistantWebContainerCaptionButtonModel() = default;
  ~AssistantWebContainerCaptionButtonModel() override = default;

  // CaptionButtonModel:
  bool IsVisible(views::CaptionButtonIcon type) const override {
    switch (type) {
      case views::CAPTION_BUTTON_ICON_CLOSE:
        return true;

      case views::CAPTION_BUTTON_ICON_BACK:
        return back_button_visibility_;

      case views::CAPTION_BUTTON_ICON_MINIMIZE:
      case views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE:
      case views::CAPTION_BUTTON_ICON_LEFT_SNAPPED:
      case views::CAPTION_BUTTON_ICON_RIGHT_SNAPPED:
      case views::CAPTION_BUTTON_ICON_MENU:
      case views::CAPTION_BUTTON_ICON_ZOOM:
      case views::CAPTION_BUTTON_ICON_LOCATION:
      case views::CAPTION_BUTTON_ICON_COUNT:
        return false;
    }
  }

  bool IsEnabled(views::CaptionButtonIcon type) const override { return true; }

  bool InZoomMode() const override { return false; }

  void set_back_button_visibility(bool visibility) {
    back_button_visibility_ = visibility;
  }

 private:
  bool back_button_visibility_ = false;

  DISALLOW_COPY_AND_ASSIGN(AssistantWebContainerCaptionButtonModel);
};

}  // namespace

AssistantWebViewDelegateImpl::AssistantWebViewDelegateImpl() = default;

AssistantWebViewDelegateImpl::~AssistantWebViewDelegateImpl() = default;

void AssistantWebViewDelegateImpl::UpdateBackButtonVisibility(
    views::Widget* widget,
    bool visibility) {
  auto caption_button_model =
      std::make_unique<AssistantWebContainerCaptionButtonModel>();
  caption_button_model->set_back_button_visibility(visibility);

  auto* non_client_view = widget->non_client_view();
  DCHECK_EQ(NonClientFrameViewAsh::kViewClassName,
            non_client_view->GetClassName());
  auto* frame_view_ash =
      static_cast<NonClientFrameViewAsh*>(non_client_view->frame_view());
  frame_view_ash->SetCaptionButtonModel(std::move(caption_button_model));
}

}  // namespace ash
