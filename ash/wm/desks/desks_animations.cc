// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_animations.h"

#include <memory>
#include <utility>

#include "ash/shell.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace desks_animations {

namespace {

// Returns the transform that can translate |window| offscreen in the direction
// indicated by |going_left|.
gfx::Transform GetWindowEndTransform(aura::Window* window, bool going_left) {
  gfx::Transform transform;
  auto* root = window->GetRootWindow();
  const auto root_bounds = root->bounds();
  const auto window_bounds = window->GetBoundsInRootWindow();
  const int x_translation = going_left
                                ? -window_bounds.right()
                                : (root_bounds.right() - window_bounds.x());
  transform.Translate(x_translation, 0);
  return transform;
}

// A self-deleting object, which recreates the layer tree of the given |window|,
// and animates the old layer tree offscreen towards the direction of the
// target desk indicated by |going_left|. When the animation is over, this
// object deletes itself and the window's old layer tree.
class WindowMoveToDeskAnimation : public ui::ImplicitAnimationObserver {
 public:
  WindowMoveToDeskAnimation(aura::Window* window, bool going_left)
      : old_window_layer_tree_(::wm::RecreateLayers(window)) {
    ui::Layer* layer = old_window_layer_tree_->root();
    ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
    constexpr base::TimeDelta kDuration = base::Milliseconds(200);
    settings.SetTransitionDuration(kDuration);
    settings.SetTweenType(gfx::Tween::EASE_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.AddObserver(this);
    layer->SetTransform(GetWindowEndTransform(window, going_left));
  }

  WindowMoveToDeskAnimation(const WindowMoveToDeskAnimation&) = delete;
  WindowMoveToDeskAnimation& operator=(const WindowMoveToDeskAnimation&) =
      delete;

  ~WindowMoveToDeskAnimation() override = default;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override { delete this; }

 private:
  std::unique_ptr<ui::LayerTreeOwner> old_window_layer_tree_;
};

}  // namespace

void PerformHitTheWallAnimation(aura::Window* root, bool going_left) {
  DCHECK(root->IsRootWindow());

  // Start and end the animation using the root layer's target transform, since
  // the layer might have a different transform other than identity due to, for
  // example, display rotation.
  ui::Layer* layer = root->layer();
  const gfx::Transform end_transform = layer->GetTargetTransform();
  gfx::Transform begin_transform = end_transform;
  // |root| will be translated out horizontally by kEdgePaddingRatio times its
  // width and then translated back to its original transform.
  const float displacement_factor = kEdgePaddingRatio * (going_left ? 1 : -1);
  begin_transform.Translate(displacement_factor * root->bounds().width(), 0);

  // Prepare two animation elements, one for the outgoing translation:
  //      |     |
  //      |<----|
  //      |     |
  // and another for the incoming translation:
  //      |     |
  //      |---->|
  //      |     |
  constexpr base::TimeDelta kDuration = base::Milliseconds(150);
  auto outgoing_transition = ui::LayerAnimationElement::CreateTransformElement(
      begin_transform, kDuration);
  outgoing_transition->set_tween_type(gfx::Tween::EASE_OUT);
  auto sequence = std::make_unique<ui::LayerAnimationSequence>();
  sequence->AddElement(std::move(outgoing_transition));
  auto incoming_transition = ui::LayerAnimationElement::CreateTransformElement(
      end_transform, kDuration);
  incoming_transition->set_tween_type(gfx::Tween::EASE_IN);
  sequence->AddElement(std::move(incoming_transition));

  // Use `REPLACE_QUEUED_ANIMATIONS` since the user may press the shortcut many
  // times repeatedly, and we don't want to keep animating the root layer
  // endlessly.
  ui::ScopedLayerAnimationSettings settings{layer->GetAnimator()};
  settings.SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  layer->GetAnimator()->StartAnimation(sequence.release());
}

void PerformWindowMoveToDeskAnimation(aura::Window* window, bool going_left) {
  DCHECK(!Shell::Get()->overview_controller()->InOverviewSession());
  DCHECK(!desks_util::IsWindowVisibleOnAllWorkspaces(window));

  // The entire transient window tree should appear to animate together towards
  // the target desk.
  for (auto* transient_window : GetTransientTreeIterator(window)) {
    // This is a self-deleting object.
    new WindowMoveToDeskAnimation(transient_window, going_left);
  }
}

}  // namespace desks_animations

}  // namespace ash
