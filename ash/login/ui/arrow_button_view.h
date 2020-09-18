// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_ARROW_BUTTON_VIEW_H_
#define ASH_LOGIN_UI_ARROW_BUTTON_VIEW_H_

#include <memory>

#include "ash/login/ui/login_button.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/controls/image_view.h"

namespace gfx {
class MultiAnimation;
}

namespace ash {

// A round button with arrow icon in the middle.
// This will be used by LoginPublicAccountUserView and expanded public account
// view.
class ArrowButtonView : public LoginButton {
 public:
  ArrowButtonView(views::ButtonListener* listener, int size);
  ~ArrowButtonView() override;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  const char* GetClassName() const override;

  // Set background color of the button.
  void SetBackgroundColor(SkColor color);

  // Allows to control the loading animation (disabled by default). The
  // animation is an arc that gradually increases from a point to a full circle;
  // the animation is looped.
  void EnableLoadingAnimation(bool enabled);

 private:
  // Helper class that translates events from the loading animation events into
  // scheduling painting.
  class LoadingAnimationDelegate : public gfx::AnimationDelegate {
   public:
    explicit LoadingAnimationDelegate(ArrowButtonView* owner);
    LoadingAnimationDelegate(const LoadingAnimationDelegate&) = delete;
    LoadingAnimationDelegate& operator=(const LoadingAnimationDelegate&) =
        delete;
    ~LoadingAnimationDelegate() override;

    // views::AnimationDelegateViews:
    void AnimationProgressed(const gfx::Animation* animation) override;

   private:
    ArrowButtonView* const owner_;
  };

  int size_;
  SkColor background_color_;
  LoadingAnimationDelegate loading_animation_delegate_{this};
  std::unique_ptr<gfx::MultiAnimation> loading_animation_;

  DISALLOW_COPY_AND_ASSIGN(ArrowButtonView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_ARROW_BUTTON_VIEW_H_
