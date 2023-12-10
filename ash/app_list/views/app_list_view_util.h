// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_VIEW_UTIL_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_VIEW_UTIL_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "ui/gfx/animation/tween.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace views {
class View;
class AnimationSequenceBlock;
}  // namespace views

namespace ash {

// Prepares for a layer animation on `view`. Returns whether a layer is created
// for `view`.
bool PrepareForLayerAnimation(views::View* view);

// Starts a vertical slide animation for `view` with `vertical_offset` as the
// initial offset. The view must already have a layer. Runs the `end_callback`
// when the animation ends or aborts.
void StartSlideInAnimation(views::View* view,
                           int vertical_offset,
                           const base::TimeDelta& time_delta,
                           gfx::Tween::Type tween_type,
                           base::RepeatingClosure end_callback);

// Similar to the method above. But use a specified animation sequence block
// instead of creating a new one. Sets the animation duration if `time_delta`
// is meaningful. NOTE: `sequence_block` can only set duration once.
void SlideViewIntoPositionWithSequenceBlock(
    views::View* view,
    int vertical_offset,
    const std::optional<base::TimeDelta>& time_delta,
    gfx::Tween::Type tween_type,
    views::AnimationSequenceBlock* sequence_block);

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_VIEW_UTIL_H_
