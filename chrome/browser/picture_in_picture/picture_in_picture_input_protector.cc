// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_input_protector.h"

#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

PictureInPictureInputProtector::PictureInPictureInputProtector(
    views::DialogDelegate* dialog_delegate)
    : dialog_delegate_(dialog_delegate) {
  CHECK(dialog_delegate_);
  occlusion_observation_.Observe(dialog_delegate_->GetWidget());
  widget_observation_.Observe(dialog_delegate_->GetWidget());
}

PictureInPictureInputProtector::~PictureInPictureInputProtector() = default;

void PictureInPictureInputProtector::OnWidgetDestroying(views::Widget* widget) {
  if (widget == dialog_delegate_->GetWidget()) {
    // Clear state if the widget is being destroyed.
    widget_observation_.Reset();
    dialog_delegate_ = nullptr;
  }
}

bool PictureInPictureInputProtector::OccludedByPictureInPicture() const {
  return occluded_by_picture_in_picture_;
}

void PictureInPictureInputProtector::SimulateOcclusionStateChangedForTesting(
    bool occluded) {
  OnOcclusionStateChanged(occluded);
}

void PictureInPictureInputProtector::OnOcclusionStateChanged(bool occluded) {
  // Protect from immediate input if the dialog has just become unoccluded.
  if (occluded_by_picture_in_picture_ && !occluded &&
      dialog_delegate_->GetDialogClientView()) {
    dialog_delegate_->GetDialogClientView()->TriggerInputProtection();
  }

  occluded_by_picture_in_picture_ = occluded;
}
