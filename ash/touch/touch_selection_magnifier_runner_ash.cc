// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_selection_magnifier_runner_ash.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

namespace {

// Gets the bounds of the magnifier when showing the specified point of
// interest. `point_of_interest` and returned bounds are in root window
// coordinates.
gfx::Rect GetBounds(const gfx::Point& point_of_interest) {
  const gfx::Size size = TouchSelectionMagnifierRunnerAsh::kMagnifierLayerSize;
  const gfx::Point origin(
      point_of_interest.x() - size.width() / 2,
      point_of_interest.y() - size.height() / 2 +
          TouchSelectionMagnifierRunnerAsh::kMagnifierVerticalOffset);
  return gfx::Rect(origin, size);
}

// Returns the child container in `root` that should parent the magnifier layer.
aura::Window* GetMagnifierParentContainerForRoot(aura::Window* root) {
  return root->GetChildById(kShellWindowId_ImeWindowParentContainer);
}

}  // namespace

TouchSelectionMagnifierRunnerAsh::TouchSelectionMagnifierRunnerAsh() = default;

TouchSelectionMagnifierRunnerAsh::~TouchSelectionMagnifierRunnerAsh() = default;

void TouchSelectionMagnifierRunnerAsh::ShowMagnifier(
    aura::Window* context,
    const gfx::PointF& position) {
  DCHECK(context);
  DCHECK(!current_context_ || current_context_ == context);
  if (!current_context_) {
    current_context_ = context;
  }

  aura::Window* root_window = current_context_->GetRootWindow();
  DCHECK(root_window);
  gfx::PointF position_in_root(position);
  aura::Window::ConvertPointToTarget(context, root_window, &position_in_root);

  if (!magnifier_layer_) {
    CreateMagnifierLayer(root_window, position_in_root);
  } else {
    magnifier_layer_->SetBounds(
        GetBounds(gfx::ToRoundedPoint(position_in_root)));
  }
}

void TouchSelectionMagnifierRunnerAsh::CloseMagnifier() {
  current_context_ = nullptr;
  magnifier_layer_ = nullptr;
}

bool TouchSelectionMagnifierRunnerAsh::IsRunning() const {
  return current_context_ != nullptr;
}

const aura::Window*
TouchSelectionMagnifierRunnerAsh::GetCurrentContextForTesting() const {
  return current_context_;
}

const ui::Layer* TouchSelectionMagnifierRunnerAsh::GetMagnifierLayerForTesting()
    const {
  return magnifier_layer_.get();
}

void TouchSelectionMagnifierRunnerAsh::CreateMagnifierLayer(
    aura::Window* root_window,
    const gfx::PointF& position_in_root) {
  aura::Window* parent_container =
      GetMagnifierParentContainerForRoot(root_window);

  ui::Layer* parent_layer = parent_container->layer();
  magnifier_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  magnifier_layer_->SetBounds(GetBounds(gfx::ToRoundedPoint(position_in_root)));
  magnifier_layer_->SetBackgroundZoom(kMagnifierScale, 0);
  magnifier_layer_->SetBackgroundOffset(
      gfx::Point(0, kMagnifierVerticalOffset));
  magnifier_layer_->SetFillsBoundsOpaquely(false);
  magnifier_layer_->SetRoundedCornerRadius(kMagnifierRoundedCorners);
  parent_layer->Add(magnifier_layer_.get());
}

}  // namespace ash
