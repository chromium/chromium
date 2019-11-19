// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/pulsing_block_view.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/rand_util.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/transform_util.h"

namespace {

const SkColor kBlockColor = SkColorSetRGB(225, 225, 225);
const int kBlockSize = 64;

const int kAnimationDurationInMs = 600;
const float kAnimationOpacity[] = {0.4f, 0.8f, 0.4f};
const float kAnimationScale[] = {0.8f, 1.0f, 0.8f};

void SchedulePulsingAnimation(ui::Layer* layer) {
  DCHECK(layer);
  DCHECK_EQ(base::size(kAnimationOpacity), base::size(kAnimationScale));

  std::unique_ptr<ui::LayerAnimationSequence> opacity_sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  std::unique_ptr<ui::LayerAnimationSequence> transform_sequence =
      std::make_unique<ui::LayerAnimationSequence>();

  // The animations loop infinitely.
  opacity_sequence->set_is_cyclic(true);
  transform_sequence->set_is_cyclic(true);

  const gfx::Rect local_bounds(layer->bounds().size());
  for (size_t i = 0; i < base::size(kAnimationOpacity); ++i) {
    opacity_sequence->AddElement(
        ui::LayerAnimationElement::CreateOpacityElement(
            kAnimationOpacity[i],
            base::TimeDelta::FromMilliseconds(kAnimationDurationInMs)));
    transform_sequence->AddElement(
        ui::LayerAnimationElement::CreateTransformElement(
            gfx::GetScaleTransform(local_bounds.CenterPoint(),
                                   kAnimationScale[i]),
            base::TimeDelta::FromMilliseconds(kAnimationDurationInMs)));
  }

  opacity_sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      ui::LayerAnimationElement::OPACITY,
      base::TimeDelta::FromMilliseconds(kAnimationDurationInMs)));

  transform_sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      ui::LayerAnimationElement::TRANSFORM,
      base::TimeDelta::FromMilliseconds(kAnimationDurationInMs)));

  std::vector<ui::LayerAnimationSequence*> animations;
  animations.push_back(opacity_sequence.release());
  animations.push_back(transform_sequence.release());
  layer->GetAnimator()->ScheduleTogether(animations);
}

}  // namespace

namespace ash {

PulsingBlockView::PulsingBlockView(const gfx::Size& size, bool start_delay) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  const int max_delay = kAnimationDurationInMs * base::size(kAnimationOpacity);
  const int delay = start_delay ? base::RandInt(0, max_delay) : 0;
  start_delay_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(delay),
                           this, &PulsingBlockView::OnStartDelayTimer);
}

PulsingBlockView::~PulsingBlockView() {}

const char* PulsingBlockView::GetClassName() const {
  return "PulsingBlockView";
}

void PulsingBlockView::OnStartDelayTimer() {
  SchedulePulsingAnimation(layer());
}

void PulsingBlockView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  rect.ClampToCenteredSize(gfx::Size(kBlockSize, kBlockSize));
  canvas->FillRect(rect, kBlockColor);
}

}  // namespace ash
