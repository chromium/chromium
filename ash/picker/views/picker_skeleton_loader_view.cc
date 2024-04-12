// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_skeleton_loader_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_abort_handle.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ash {

PickerSkeletonLoaderView::PickerSkeletonLoaderView() {
  views::Builder<views::ImageView>(this)
      .SetImage(ui::ImageModel::FromVectorIcon(
          kPickerSkeletonLoaderIcon, cros_tokens::kCrosSysSystemOnBase, 260))
      .SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(12, 16, 0, 16)))
      .SetPaintToLayer()
      .CustomConfigure(base::BindOnce([](views::ImageView* view) {
        view->SetPaintToLayer();
        view->layer()->SetFillsBoundsOpaquely(false);
        view->layer()->SetOpacity(0.0f);
      }))
      .BuildChildren();
}

PickerSkeletonLoaderView::~PickerSkeletonLoaderView() = default;

void PickerSkeletonLoaderView::StartAnimationAfter(
    base::TimeDelta initial_delay) {
  animation_start_timer_.Start(FROM_HERE, initial_delay, this,
                               &PickerSkeletonLoaderView::StartAnimation);
}

void PickerSkeletonLoaderView::StopAnimation() {
  animation_start_timer_.Stop();
  abort_handle_.reset();
  layer()->SetOpacity(0.0f);
}

void PickerSkeletonLoaderView::StartAnimation() {
  // TODO: b/333729037 - Replace this with a Lottie animation.
  layer()->SetOpacity(1.0f);
  views::AnimationBuilder builder;
  abort_handle_ = builder.GetAbortHandle();
  builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Repeatedly()
      .SetDuration(base::Milliseconds(500))
      .SetOpacity(layer(), 0.5f, gfx::Tween::ACCEL_30_DECEL_20_85)
      .At(base::Milliseconds(500))
      .SetDuration(base::Milliseconds(500))
      .SetOpacity(layer(), 1.0f, gfx::Tween::LINEAR);
}

bool PickerSkeletonLoaderView::HasStartedAnimationForTesting() const {
  return animation_start_timer_.IsRunning() || abort_handle_ != nullptr;
}

BEGIN_METADATA(PickerSkeletonLoaderView)
END_METADATA

}  // namespace ash
