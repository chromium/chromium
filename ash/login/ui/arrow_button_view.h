// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_ARROW_BUTTON_VIEW_H_
#define ASH_LOGIN_UI_ARROW_BUTTON_VIEW_H_

#include <memory>
#include <optional>

#include "ash/login/ui/login_button.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/controls/image_view.h"

namespace gfx {
class MultiAnimation;
}

namespace ash {

// A round button with arrow icon in the middle.
// This will be used by LoginPublicAccountUserView and expanded public account
// view.
class ASH_EXPORT ArrowButtonView : public LoginButton {
  METADATA_HEADER(ArrowButtonView, LoginButton)

 public:
  ArrowButtonView(PressedCallback callback, int size);
  ArrowButtonView(const ArrowButtonView&) = delete;
  ArrowButtonView& operator=(const ArrowButtonView&) = delete;
  ~ArrowButtonView() override;

  // LoginButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // Causes the icon to transform bigger and smaller repeatedly to draw user
  // attention to click.
  void RunTransformAnimation();

  // Stops any existing animation.
  void StopAnimating();

  // Allows to control the loading animation (disabled by default). The
  // animation is an arc that gradually increases from a point to a full circle;
  // the animation is looped.
  void EnableLoadingAnimation(bool enabled);

  void SetBackgroundColorId(ui::ColorId color_id);

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
    const raw_ptr<ArrowButtonView> owner_;
  };

  LoadingAnimationDelegate loading_animation_delegate_{this};
  std::unique_ptr<gfx::MultiAnimation> loading_animation_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_ARROW_BUTTON_VIEW_H_
