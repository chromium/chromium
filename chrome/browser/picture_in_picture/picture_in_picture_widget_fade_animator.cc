// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_widget_fade_animator.h"

#include "ui/views/widget/widget.h"

PictureInPictureWidgetFadeAnimator::PictureInPictureWidgetFadeAnimator() =
    default;
PictureInPictureWidgetFadeAnimator::~PictureInPictureWidgetFadeAnimator() =
    default;

void PictureInPictureWidgetFadeAnimator::AnimateShowWindow(
    views::Widget* widget,
    PictureInPictureWidgetFadeAnimator::WidgetShowType show_type) {
  CancelAndReset();

  // Prevent re-animating the widget if it is already visible. This is done
  // because on some platforms (e.g. Linux), changing the theme can cause the
  // widget to be re-added.
  if (widget->IsVisible()) {
    widget->SetOpacity(1.0f);
    return;
  }

  widget->SetVisibilityChangedAnimationsEnabled(false);

  fade_animator_ = std::make_unique<views::WidgetFadeAnimator>(widget);
  fade_animator_->set_fade_in_duration(base::Milliseconds(kFadeInDurationMs));
  fade_animator_->set_show_type(show_type);

  fade_animator_->FadeIn();
  ++fade_in_calls_count_;
}

void PictureInPictureWidgetFadeAnimator::CancelAndReset() {
  if (!fade_animator_) {
    return;
  }
  fade_animator_->CancelFadeIn();
  fade_animator_->CancelFadeOut();
  fade_animator_.reset();
}
