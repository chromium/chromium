// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BOOTING_BOOTING_ANIMATION_VIEW_H_
#define ASH_BOOTING_BOOTING_ANIMATION_VIEW_H_

#include "ash/ash_export.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace views {
class AnimatedImageView;
}  // namespace views

namespace ash {

class ASH_EXPORT BootingAnimationView : public views::View {
 public:
  BootingAnimationView();
  BootingAnimationView(const BootingAnimationView&) = delete;
  BootingAnimationView& operator=(const BootingAnimationView&) = delete;
  ~BootingAnimationView() override;

  void Play();

  void SetAnimatedImage(const std::string& animation_data);
  lottie::Animation* GetAnimatedImage();

 private:
  raw_ptr<views::AnimatedImageView> animation_ = nullptr;
};

}  // namespace ash

#endif  // ASH_BOOTING_BOOTING_ANIMATION_VIEW_H_
