// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_web_view_delegate_impl.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "chromeos/ui/frame/caption_buttons/caption_button_model.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_types.h"
#include "ui/views/window/frame_caption_button.h"
#include "ui/views/window/non_client_view.h"

namespace ash {

namespace {

class AssistantWebContainerCaptionButtonModel
    : public chromeos::CaptionButtonModel {
 public:
  AssistantWebContainerCaptionButtonModel() = default;

  AssistantWebContainerCaptionButtonModel(
      const AssistantWebContainerCaptionButtonModel&) = delete;
  AssistantWebContainerCaptionButtonModel& operator=(
      const AssistantWebContainerCaptionButtonModel&) = delete;

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
      case views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED:
      case views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED:
      case views::CAPTION_BUTTON_ICON_MENU:
      case views::CAPTION_BUTTON_ICON_ZOOM:
      case views::CAPTION_BUTTON_ICON_LOCATION:
      case views::CAPTION_BUTTON_ICON_CENTER:
      case views::CAPTION_BUTTON_ICON_FLOAT:
      case views::CAPTION_BUTTON_ICON_CUSTOM:
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

  DCHECK(
      views::IsViewClass<NonClientFrameViewAsh>(non_client_view->frame_view()));

  auto* frame_view_ash =
      static_cast<NonClientFrameViewAsh*>(non_client_view->frame_view());
  frame_view_ash->SetCaptionButtonModel(std::move(caption_button_model));

  if (visibility) {
    views::FrameCaptionButton* back_button =
        frame_view_ash->GetHeaderView()->GetFrameHeader()->GetBackButton();
    back_button->SetPaintAsActive(widget->IsActive());
  }
}

}  // namespace ash
