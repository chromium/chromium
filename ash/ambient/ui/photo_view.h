// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_PHOTO_VIEW_H_
#define ASH_AMBIENT_UI_PHOTO_VIEW_H_

#include <array>
#include <memory>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/ui/ambient_background_image_view.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class AmbientBackgroundImageView;
class AmbientViewDelegateImpl;
class JitterCalculator;
struct PhotoWithDetails;

struct ASH_EXPORT PhotoViewConfig {
  bool peripheral_ui_visible = true;
  bool force_resize_to_fit = false;
};

// View to display photos in ambient mode.
class ASH_EXPORT PhotoView : public views::View,
                             public AmbientBackendModelObserver,
                             public ui::ImplicitAnimationObserver {
  METADATA_HEADER(PhotoView, views::View)

 public:
  explicit PhotoView(AmbientViewDelegateImpl* delegate,
                     PhotoViewConfig view_config = PhotoViewConfig());

  PhotoView(const PhotoView&) = delete;
  PhotoView& operator=(PhotoView&) = delete;
  ~PhotoView() override;

  // AmbientBackendModelObserver:
  void OnImageAdded() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  JitterCalculator* GetJitterCalculatorForTesting();

 private:
  friend class AmbientAshTestBase;

  void Init();

  void UpdateImage(const PhotoWithDetails& image);
  void OnImageCycleComplete();

  void StartTransitionAnimation();

  // Return if can start transition animation.
  bool NeedToAnimateTransition() const;

  gfx::ImageSkia GetVisibleImageForTesting();

  // PhotoView configuration allows setting the photo view related behaviors and
  // configurations.
  const PhotoViewConfig view_config_;

  // Note that we should be careful when using |delegate_|, as there is no
  // strong guarantee on the life cycle.
  const raw_ptr<AmbientViewDelegateImpl> delegate_ = nullptr;

  // Image containers used for animation. Owned by view hierarchy.
  std::array<AmbientBackgroundImageView*, 2> image_views_{nullptr, nullptr};

  // The index of |image_views_| to update the next image.
  int image_index_ = 0;

  // Fires when the next photo should be prepared, which ultimately leads to
  // it being rendered in this view.
  base::OneShotTimer photo_refresh_timer_;

  base::ScopedObservation<AmbientBackendModel, AmbientBackendModelObserver>
      scoped_backend_model_observer_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_PHOTO_VIEW_H_
