// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/root_window_desk_switch_animator.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The space between the starting and ending desks screenshots in dips.
constexpr int kDesksSpacing = 50;

// The maximum number of times to retry taking a screenshot for either the
// starting or the ending desks. After this maximum number is reached, we ignore
// a failed screenshot request and proceed with next phases.
constexpr int kMaxScreenshotRetries = 2;

constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(300);

// The amount, by which the detached old layers of the removed desk's windows,
// is translated vertically during the for-remove desk switch animation.
constexpr int kRemovedDeskWindowYTranslation = 20;
constexpr base::TimeDelta kRemovedDeskWindowTranslationDuration =
    base::TimeDelta::FromMilliseconds(100);

// Create the layer that will be the parent of the screenshot layer, with a
// solid black color to act as the background showing behind the two
// screenshot layers in the |kDesksSpacing| region between them.
// This is the layer that will be animated.
std::unique_ptr<ui::LayerTreeOwner> CreateAnimationLayerOwner(
    aura::Window* root) {
  auto animation_layer = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  gfx::Rect layer_bounds(root->layer()->size());
  layer_bounds.set_width(2 * layer_bounds.width() + kDesksSpacing);
  animation_layer->SetBounds(layer_bounds);
  animation_layer->set_name("Desk switch animation layer");
  animation_layer->SetColor(SK_ColorBLACK);

  return std::make_unique<ui::LayerTreeOwner>(std::move(animation_layer));
}

// Takes a screenshot of the screen content. |on_screenshot_taken| will be
// triggered when the screenshot is taken.
void TakeScreenshot(
    aura::Window* root,
    viz::CopyOutputRequest::CopyOutputRequestCallback on_screenshot_taken) {
  // We don't take a screenshot of the root because that will be a screenshot of
  // a screenshot when the starting desk screenshot layer is placed on top of
  // everything. The container `kShellWindowId_ScreenRotationContainer` is
  // created for the purpose of taking screenshots of the screen content while
  // performing the screen rotation animation.
  // TODO(afakhry): Consider renaming this container.
  auto* screenshot_layer =
      root->GetChildById(kShellWindowId_ScreenRotationContainer)->layer();

  const gfx::Rect request_bounds(screenshot_layer->size());
  auto screenshot_request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
      std::move(on_screenshot_taken));
  screenshot_request->set_area(request_bounds);
  screenshot_request->set_result_selection(request_bounds);

  screenshot_layer->RequestCopyOfOutput(std::move(screenshot_request));
}

// Given a screenshot |copy_result|, creates a texture layer that contains the
// content of that screenshot.
std::unique_ptr<ui::Layer> CreateLayerFromScreenshotResult(
    std::unique_ptr<viz::CopyOutputResult> copy_result) {
  DCHECK(copy_result);
  DCHECK(!copy_result->IsEmpty());
  DCHECK_EQ(copy_result->format(), viz::CopyOutputResult::Format::RGBA_TEXTURE);

  const gfx::Size layer_size = copy_result->size();
  viz::TransferableResource transferable_resource =
      viz::TransferableResource::MakeGL(
          copy_result->GetTextureResult()->mailbox, GL_LINEAR, GL_TEXTURE_2D,
          copy_result->GetTextureResult()->sync_token, layer_size,
          /*is_overlay_candidate=*/false);
  std::unique_ptr<viz::SingleReleaseCallback> take_texture_ownership_callback =
      copy_result->TakeTextureOwnership();
  auto screenshot_layer = std::make_unique<ui::Layer>();
  screenshot_layer->SetTransferableResource(
      transferable_resource, std::move(take_texture_ownership_callback),
      layer_size);

  return screenshot_layer;
}

}  // namespace

RootWindowDeskSwitchAnimator::RootWindowDeskSwitchAnimator(
    aura::Window* root,
    const Desk* ending_desk,
    Delegate* delegate,
    bool move_left,
    bool for_remove)
    : root_window_(root),
      starting_desk_(DesksController::Get()->active_desk()),
      ending_desk_(ending_desk),
      delegate_(delegate),
      animation_layer_owner_(CreateAnimationLayerOwner(root)),
      x_translation_offset_(root->layer()->size().width() + kDesksSpacing),
      move_left_(move_left),
      for_remove_(for_remove) {
  DCHECK(root_window_);
  DCHECK(starting_desk_);
  DCHECK(ending_desk_);
  DCHECK(delegate_);
}

RootWindowDeskSwitchAnimator::~RootWindowDeskSwitchAnimator() {
  // TODO(afakhry): Determine if this is necessary, since generally this object
  // is only deleted when all animations end, but there might be situations when
  // we might need to kill the animations before they complete such as when a
  // display is removed.
  if (!attached_sequences().empty())
    StopObservingImplicitAnimations();
}

void RootWindowDeskSwitchAnimator::TakeStartingDeskScreenshot() {
  if (for_remove_) {
    // The active desk is about to be removed. Recreate and detach its old
    // layers to animate them in a jump-like animation.
    auto* desk_container =
        starting_desk_->GetDeskContainerForRoot(root_window_);
    old_windows_layer_tree_owner_ = wm::RecreateLayers(desk_container);
    root_window_->layer()->Add(old_windows_layer_tree_owner_->root());
    root_window_->layer()->StackAtTop(old_windows_layer_tree_owner_->root());

    // We don't take a screenshot of the soon-to-be-removed desk, we use an
    // empty black solid color layer.
    auto black_layer = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    black_layer->SetColor(SK_ColorBLACK);
    CompleteAnimationPhase1WithLayer(std::move(black_layer));
    return;
  }

  TakeScreenshot(
      root_window_,
      base::BindOnce(
          &RootWindowDeskSwitchAnimator::OnStartingDeskScreenshotTaken,
          weak_ptr_factory_.GetWeakPtr()));
}

void RootWindowDeskSwitchAnimator::TakeEndingDeskScreenshot() {
  DCHECK(starting_desk_screenshot_taken_);

  TakeScreenshot(
      root_window_,
      base::BindOnce(&RootWindowDeskSwitchAnimator::OnEndingDeskScreenshotTaken,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RootWindowDeskSwitchAnimator::StartAnimation() {
  DCHECK(starting_desk_screenshot_taken_);
  DCHECK(ending_desk_screenshot_taken_);
  DCHECK(!animation_finished_);

  gfx::Transform animation_layer_ending_transfrom;

  if (move_left_) {
    // Starting desk is one the left, so the ending transform of the parent
    // "animation layer" is then a translation to the left such that at the end,
    // the ending screenshot layer becomes the one visible on the screen.
    //
    //                         +-----------+
    //                         | Animation |
    //                         |  layer    |
    //                         +-----------+
    //                           /        \
    //                +------------+      +------------+
    //                | start desk |      | end desk   |
    //                | screenshot |      | screenshot |
    //                |  layer     |      |  layer     |
    //                +------------+      +------------+
    //                      ^
    //                 start here
    //
    //                |<------------------|
    //                          ^
    //               `x_translation_offset_`
    //
    animation_layer_ending_transfrom.Translate(-x_translation_offset_, 0);
  }

  // Animate the parent "animation layer" towards the ending transform.
  ui::Layer* animation_layer = animation_layer_owner_->root();
  ui::ScopedLayerAnimationSettings settings(animation_layer->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.AddObserver(this);
  settings.SetTransitionDuration(kAnimationDuration);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  animation_layer->SetTransform(animation_layer_ending_transfrom);

  if (for_remove_) {
    DCHECK(old_windows_layer_tree_owner_);
    auto* old_windows_layer = old_windows_layer_tree_owner_->root();
    DCHECK(old_windows_layer);

    // Translate the old layers of removed desk's windows back down by
    // `kRemovedDeskWindowYTranslation`.
    gfx::Transform transform = old_windows_layer->GetTargetTransform();
    ui::ScopedLayerAnimationSettings settings(old_windows_layer->GetAnimator());
    settings.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    settings.SetTransitionDuration(kRemovedDeskWindowTranslationDuration);
    settings.SetTweenType(gfx::Tween::EASE_IN);
    transform.Translate(0, kRemovedDeskWindowYTranslation);
    old_windows_layer->SetTransform(transform);
  }
}

void RootWindowDeskSwitchAnimator::OnImplicitAnimationsCompleted() {
  StopObservingImplicitAnimations();

  animation_finished_ = true;
  delegate_->OnDeskSwitchAnimationFinished();
}

void RootWindowDeskSwitchAnimator::CompleteAnimationPhase1WithLayer(
    std::unique_ptr<ui::Layer> layer) {
  DCHECK(layer);

  ui::Layer* starting_desk_screenshot_layer = layer.release();
  gfx::Rect screenshot_bounds(root_window_->layer()->size());
  gfx::Transform animation_layer_starting_transfrom;

  if (!move_left_) {
    // Starting desk is one the right, so we need to offset the screenshot layer
    // horizontally to the right by an amount equal to its width plus
    // kDesksSpacing (|x_translation_offset_|).
    //
    //                         +-----------+
    //                         | Animation |
    //                         |  layer    |
    //                         +-----------+
    //                           /        \
    //                +------------+      +------------+
    //                | end desk   |      | start desk |
    //                | screenshot |      | screenshot |
    //                |  layer     |      |  layer     |
    //                +------------+      +------------+
    //                                          ^
    //                |----------------->| start here
    //                         ^
    //               `x_translation_offset_`
    //
    screenshot_bounds.Offset(x_translation_offset_, 0);

    // However the parent "animation layer" is startingly translated by the same
    // amount in the opposite direction such that starting desk screenshot is
    // the one shown on the screen.
    animation_layer_starting_transfrom.Translate(-x_translation_offset_, 0);
  }

  starting_desk_screenshot_layer->set_name("Starting desk screenshot");
  starting_desk_screenshot_layer->SetBounds(screenshot_bounds);
  auto* animation_layer = animation_layer_owner_->root();
  animation_layer->Add(starting_desk_screenshot_layer);
  animation_layer->SetTransform(animation_layer_starting_transfrom);

  // Add the layers on top of everything, so that things that result from desk
  // activation (such as showing and hiding windows, exiting overview mode ...
  // etc.) are not visible to the user.
  auto* root_layer = root_window_->layer();
  root_layer->Add(animation_layer);

  if (for_remove_) {
    DCHECK(old_windows_layer_tree_owner_);
    auto* old_windows_layer = old_windows_layer_tree_owner_->root();
    DCHECK(old_windows_layer);
    root_layer->StackBelow(animation_layer, old_windows_layer);

    // Translate the old layers of the removed desk's windows up by
    // `kRemovedDeskWindowYTranslation`.
    gfx::Transform transform = old_windows_layer->GetTargetTransform();
    ui::ScopedLayerAnimationSettings settings(old_windows_layer->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(kRemovedDeskWindowTranslationDuration);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    transform.Translate(0, -kRemovedDeskWindowYTranslation);
    old_windows_layer->SetTransform(transform);
  } else {
    root_layer->StackAtTop(animation_layer);
  }

  starting_desk_screenshot_taken_ = true;
  delegate_->OnStartingDeskScreenshotTaken(ending_desk_);
}

void RootWindowDeskSwitchAnimator::OnStartingDeskScreenshotTaken(
    std::unique_ptr<viz::CopyOutputResult> copy_result) {
  if (!copy_result || copy_result->IsEmpty()) {
    // A frame may be activated before the screenshot requests are satisfied,
    // leading to us getting an empty |result|. Rerequest the screenshot.
    // (See viz::Surface::ActivateFrame()).
    if (++starting_desk_screenshot_retries_ <= kMaxScreenshotRetries) {
      TakeStartingDeskScreenshot();
    } else {
      LOG(ERROR) << "Received multiple empty screenshots of the starting desk.";
      NOTREACHED();
      starting_desk_screenshot_taken_ = true;
      delegate_->OnStartingDeskScreenshotTaken(ending_desk_);
    }

    return;
  }

  CompleteAnimationPhase1WithLayer(
      CreateLayerFromScreenshotResult(std::move(copy_result)));
}

void RootWindowDeskSwitchAnimator::OnEndingDeskScreenshotTaken(
    std::unique_ptr<viz::CopyOutputResult> copy_result) {
  if (!copy_result || copy_result->IsEmpty()) {
    // A frame may be activated before the screenshot requests are satisfied,
    // leading to us getting an empty |result|. Rerequest the screenshot.
    // (See viz::Surface::ActivateFrame()).
    if (++ending_desk_screenshot_retries_ <= kMaxScreenshotRetries) {
      TakeEndingDeskScreenshot();
    } else {
      LOG(ERROR) << "Received multiple empty screenshots of the ending desk.";
      NOTREACHED();
      ending_desk_screenshot_taken_ = true;
      delegate_->OnEndingDeskScreenshotTaken();
    }

    return;
  }

  ui::Layer* ending_desk_screenshot_layer =
      CreateLayerFromScreenshotResult(std::move(copy_result)).release();

  gfx::Rect screenshot_bounds(root_window_->layer()->size());

  if (move_left_) {
    // Starting desk is one the left, so we need to offset the ending desk
    // screenshot layer horizontally to the right by an amount equal to its
    // width plus kDesksSpacing (|x_translation_offset_|).
    //
    //                         +-----------+
    //                         | Animation |
    //                         |  layer    |
    //                         +-----------+
    //                           /        \
    //                +------------+      +------------+
    //                | start desk |      | end desk   |
    //                | screenshot |      | screenshot |
    //                |  layer     |      |  layer     |
    //                +------------+      +------------+
    //                      ^
    //                  start here
    //
    //                |------------------>|
    //                          ^
    //                `x_translation_offset_`
    //
    screenshot_bounds.Offset(x_translation_offset_, 0);
  }

  ending_desk_screenshot_layer->set_name("Ending desk screenshot");
  ending_desk_screenshot_layer->SetBounds(screenshot_bounds);

  auto* animation_layer = animation_layer_owner_->root();
  animation_layer->Add(ending_desk_screenshot_layer);

  ending_desk_screenshot_taken_ = true;
  delegate_->OnEndingDeskScreenshotTaken();
}

}  // namespace ash
