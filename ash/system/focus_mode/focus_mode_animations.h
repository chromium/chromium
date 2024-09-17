// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_ANIMATIONS_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_ANIMATIONS_H_

#include <vector>

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

// Performs animating the container views (currently we only have one
// `do_not_disturb_view_`) below the task view container shifting up/down when
// we shrink/expand the task view container. `animatable_views` is passed
// here with all the container views below the task view container.
// `shift_height` is the height we will move the `animatable_views` up/down
// during the animation.
void PerformViewsVerticalShitfAnimation(
    const std::vector<views::View*>& animatable_views,
    const int shift_height);

// Performs animating the `resized_container_layer` shrink/expand when
// selecting/deselecting (editing) a task. `old_bounds_height` is the height of
// the task container view before resizing it, or the alternate view changed in
// `FocusModeSoundsView`.
void PerformTaskContainerViewResizeAnimation(ui::Layer* resized_container_layer,
                                             const int old_bounds_height);

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_ANIMATIONS_H_
