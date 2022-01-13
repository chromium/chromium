// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ANIMATION_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_ANIMATION_VIEW_H_

#include <memory>

#include "ash/ambient/model/ambient_animation_photo_provider.h"
#include "ash/ash_export.h"
#include "ui/lottie/animation_observer.h"
#include "ui/views/view.h"

namespace views {
class AnimatedImageView;
}  // namespace views

namespace ash {

class AmbientAnimationStaticResources;
class AmbientBackendModel;
class AmbientViewEventHandler;

class ASH_EXPORT AmbientAnimationView : public views::View,
                                        public lottie::AnimationObserver {
 public:
  METADATA_HEADER(AmbientAnimationView);

  AmbientAnimationView(
      const AmbientBackendModel* model,
      AmbientViewEventHandler* event_handler,
      std::unique_ptr<const AmbientAnimationStaticResources> static_resources);
  AmbientAnimationView(const AmbientAnimationView&) = delete;
  AmbientAnimationView& operator=(AmbientAnimationView&) = delete;
  ~AmbientAnimationView() override;

 private:
  void Init();

  void AnimationWillStartPlaying(const lottie::Animation* animation) override;
  void AnimationCycleEnded(const lottie::Animation* animation) override;

  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  AmbientViewEventHandler* const event_handler_;

  const std::unique_ptr<const AmbientAnimationStaticResources>
      static_resources_;
  AmbientAnimationPhotoProvider animation_photo_provider_;

  views::AnimatedImageView* animated_image_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ANIMATION_VIEW_H_
