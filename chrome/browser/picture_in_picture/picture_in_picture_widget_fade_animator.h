// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WIDGET_FADE_ANIMATOR_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WIDGET_FADE_ANIMATOR_H_

#include <memory>

#include "ui/views/animation/widget_fade_animator.h"

namespace views {
class Widget;
}

// Fade animator used by both types of Picture-in-Picture windows (Video and
// Document). Currently only used to animate the window opening.
//
// Fade in animation is disabled for Document Picture-in-Picture on Windows. On
// Windows, resizable windows can not be translucent.
class PictureInPictureWidgetFadeAnimator {
 public:
  static constexpr int kFadeInDurationMs = 500;

  using WidgetShowType = views::WidgetFadeAnimator::WidgetShowType;
  PictureInPictureWidgetFadeAnimator();
  PictureInPictureWidgetFadeAnimator(
      const PictureInPictureWidgetFadeAnimator&) = delete;
  PictureInPictureWidgetFadeAnimator& operator=(
      const PictureInPictureWidgetFadeAnimator&) = delete;
  ~PictureInPictureWidgetFadeAnimator();

  // Applies the "show animation" to `widget`. The show widget call is
  // controlled using `show_type`. Video Picture-in-Picture windows are shown as
  // inactive, while the show type for Document picture in picture windows is
  // determined by the browser.
  void AnimateShowWindow(
      views::Widget* widget,
      PictureInPictureWidgetFadeAnimator::WidgetShowType show_type);

  // Cancels any ongoing animations and resets the `fade_animator_` if it
  // exists.
  void CancelAndReset();

  views::WidgetFadeAnimator* GetWidgetFadeAnimatorForTesting() {
    return fade_animator_.get();
  }

  int GetFadeInCallsCountForTesting() { return fade_in_calls_count_; }

 private:
  std::unique_ptr<views::WidgetFadeAnimator> fade_animator_;
  int fade_in_calls_count_ = 0;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WIDGET_FADE_ANIMATOR_H_
