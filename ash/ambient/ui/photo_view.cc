// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/photo_view.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/ui/ambient_background_image_view.h"
#include "ash/ambient/ui/ambient_slideshow_peripheral_ui.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/ui/jitter_calculator.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/metrics_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

constexpr char kPhotoTransitionSmoothness[] =
    "Ash.AmbientMode.AnimationSmoothness.PhotoTransition";

void ReportSmoothness(int value) {
  base::UmaHistogramPercentage(kPhotoTransitionSmoothness, value);
}

}  // namespace

// PhotoView ------------------------------------------------------------------
PhotoView::PhotoView(AmbientViewDelegateImpl* delegate,
                     PhotoViewConfig view_config)
    : view_config_(view_config), delegate_(delegate) {
  DCHECK(delegate_);
  SetID(AmbientViewID::kAmbientPhotoView);
  Init();
}

PhotoView::~PhotoView() = default;

void PhotoView::OnImageAdded() {
  // If NeedToAnimate() is true, will start transition animation and
  // UpdateImages() when animation completes. Otherwise, update images
  // immediately.
  if (NeedToAnimateTransition()) {
    StartTransitionAnimation();
    return;
  }

  PhotoWithDetails next_image;
  delegate_->GetAmbientBackendModel()->GetCurrentAndNextImages(
      /*current_image=*/nullptr, &next_image);
  UpdateImage(next_image);
}

void PhotoView::Init() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  for (auto*& image_view : image_views_) {
    // Creates image views. The same |glanceable_info_jitter_calculator_|
    // instance is shared between the AmbientBackgroundImageViews so that the
    // glanceable info on screen does not shift too much at once when
    // transitioning between AmbientBackgroundImageViews in
    // StartTransitionAnimation().
    image_view =
        AddChildView(std::make_unique<AmbientBackgroundImageView>(delegate_));
    // Each image view will be animated on its own layer.
    image_view->SetPaintToLayer();
    image_view->layer()->SetFillsBoundsOpaquely(false);

    image_view->SetPeripheralUiVisibility(view_config_.peripheral_ui_visible);
    image_view->SetForceResizeToFit(view_config_.force_resize_to_fit);
  }

  // Hides one image view initially for fade in animation.
  image_views_.back()->layer()->SetOpacity(0.0f);

  auto* model = delegate_->GetAmbientBackendModel();
  scoped_backend_model_observer_.Observe(model);

  // |PhotoView::Init| is called after
  // |AmbientBackendModelObserver::OnImagesReady| has been called.
  // |AmbientBackendModel| has two images ready and views should be constructed
  // for each one.
  PhotoWithDetails current_image, next_image;
  model->GetCurrentAndNextImages(&current_image, &next_image);
  UpdateImage(current_image);
  UpdateImage(next_image);
  delegate_->NotifyObserversMarkerHit(
      AmbientPhotoConfig::Marker::kUiStartRendering);
}

void PhotoView::UpdateImage(const PhotoWithDetails& next_image) {
  if (next_image.photo.isNull())
    return;

  image_views_.at(image_index_)
      ->UpdateImage(next_image.photo, next_image.related_photo,
                    next_image.is_portrait, next_image.topic_type);
  image_views_.at(image_index_)
      ->UpdateImageDetails(base::UTF8ToUTF16(next_image.details),
                           base::UTF8ToUTF16(next_image.related_details));
  image_index_ = 1 - image_index_;
  photo_refresh_timer_.Start(FROM_HERE,
                             AmbientUiModel::Get()->photo_refresh_interval(),
                             this, &PhotoView::OnImageCycleComplete);
}

void PhotoView::OnImageCycleComplete() {
  delegate_->NotifyObserversMarkerHit(
      AmbientPhotoConfig::Marker::kUiCycleEnded);
}

void PhotoView::StartTransitionAnimation() {
  ui::Layer* visible_layer = image_views_.at(image_index_)->layer();
  {
    ui::ScopedLayerAnimationSettings animation(visible_layer->GetAnimator());
    animation.SetTransitionDuration(kAnimationDuration);
    animation.SetTweenType(gfx::Tween::LINEAR);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    animation.CacheRenderSurface();

    ui::AnimationThroughputReporter reporter(
        animation.GetAnimator(),
        metrics_util::ForSmoothnessV3(base::BindRepeating(ReportSmoothness)));

    visible_layer->SetOpacity(0.0f);
  }

  ui::Layer* invisible_layer = image_views_.at(1 - image_index_)->layer();
  {
    ui::ScopedLayerAnimationSettings animation(invisible_layer->GetAnimator());
    animation.SetTransitionDuration(kAnimationDuration);
    animation.SetTweenType(gfx::Tween::LINEAR);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    animation.CacheRenderSurface();
    // For simplicity, only observe one animation.
    animation.AddObserver(this);

    ui::AnimationThroughputReporter reporter(
        animation.GetAnimator(),
        metrics_util::ForSmoothnessV3(base::BindRepeating(ReportSmoothness)));

    invisible_layer->SetOpacity(1.0f);
  }
}

void PhotoView::OnImplicitAnimationsCompleted() {
  PhotoWithDetails next_image;
  delegate_->GetAmbientBackendModel()->GetCurrentAndNextImages(
      /*current_image=*/nullptr, &next_image);
  UpdateImage(next_image);
}

bool PhotoView::NeedToAnimateTransition() const {
  // Can do transition animation if both two images in |images_unscaled_| are
  // not nullptr. Check the image index 1 is enough.
  return !image_views_.back()->GetCurrentImage().isNull();
}

gfx::ImageSkia PhotoView::GetVisibleImageForTesting() {
  return image_views_.at(image_index_)->GetCurrentImage();
}

BEGIN_METADATA(PhotoView)
END_METADATA

}  // namespace ash
