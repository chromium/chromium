// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/pulsing_block_view.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/rand_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_sequence_block.h"

namespace {

const SkColor kBlockColor = SkColorSetRGB(225, 225, 225);
const int kBlockSize = 64;

const int kAnimationDurationInMs = 600;
const float kAnimationOpacity[] = {0.4f, 0.8f, 0.4f};
const float kAnimationScale[] = {0.8f, 1.0f, 0.8f};

void SchedulePulsingAnimation(ui::Layer* layer) {
  DCHECK(layer);
  DCHECK_EQ(std::size(kAnimationOpacity), std::size(kAnimationScale));

  const gfx::Rect local_bounds(layer->bounds().size());
  views::AnimationBuilder builder;
  builder.Repeatedly();
  for (size_t i = 0; i < std::size(kAnimationOpacity); ++i) {
    builder.GetCurrentSequence()
        .SetDuration(base::Milliseconds(kAnimationDurationInMs))
        .SetOpacity(layer, kAnimationOpacity[i])
        .SetTransform(layer, gfx::GetScaleTransform(local_bounds.CenterPoint(),
                                                    kAnimationScale[i]))
        .Then();
  }
  builder.GetCurrentSequence().SetDuration(
      base::Milliseconds(kAnimationDurationInMs));
}

}  // namespace

namespace ash {

PulsingBlockView::PulsingBlockView(const gfx::Size& size, bool start_delay) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  const int max_delay = kAnimationDurationInMs * std::size(kAnimationOpacity);
  const int delay = start_delay ? base::RandInt(0, max_delay) : 0;
  start_delay_timer_.Start(FROM_HERE, base::Milliseconds(delay), this,
                           &PulsingBlockView::OnStartDelayTimer);
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
