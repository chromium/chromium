// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

GlanceablesTimeManagementBubbleView::GlanceablesTimeManagementBubbleView() =
    default;
GlanceablesTimeManagementBubbleView::~GlanceablesTimeManagementBubbleView() =
    default;

void GlanceablesTimeManagementBubbleView::ChildPreferredSizeChanged(
    View* child) {
  PreferredSizeChanged();
}

void GlanceablesTimeManagementBubbleView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);
  if (error_message_) {
    error_message_->UpdateBoundsToContainer(GetLocalBounds());
  }
}

void GlanceablesTimeManagementBubbleView::MaybeDismissErrorMessage() {
  if (!error_message_.get()) {
    return;
  }

  RemoveChildViewT(std::exchange(error_message_, nullptr));
}

void GlanceablesTimeManagementBubbleView::ShowErrorMessage(
    const std::u16string& error_message,
    views::Button::PressedCallback callback,
    GlanceablesErrorMessageView::ButtonActionType type) {
  MaybeDismissErrorMessage();

  error_message_ = AddChildView(std::make_unique<GlanceablesErrorMessageView>(
      std::move(callback), error_message, type));
  error_message_->SetProperty(views::kViewIgnoredByLayoutKey, true);
}

void GlanceablesTimeManagementBubbleView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GlanceablesTimeManagementBubbleView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

BEGIN_METADATA(GlanceablesTimeManagementBubbleView)
END_METADATA

}  // namespace ash
