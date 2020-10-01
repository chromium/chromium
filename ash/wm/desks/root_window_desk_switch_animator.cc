// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/root_window_desk_switch_animator.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The maximum number of times to retry taking a screenshot for either the
// starting or the ending desks. After this maximum number is reached, we ignore
// a failed screenshot request and proceed with next phases.
constexpr int kMaxScreenshotRetries = 2;

// When using the touchpad to perform a continuous desk update, we may need a
// new screenshot request during the swipe. While updating the animation layer,
// if we are getting close to the edges of the animation layer by this amount,
// request a new screenshot.
constexpr int kMinDistanceBeforeScreenshotDp = 40;

constexpr base::TimeDelta kAnimationDuration =
    base::TimeDelta::FromMilliseconds(300);

// The amount, by which the detached old layers of the removed desk's windows,
// is translated vertically during the for-remove desk switch animation.
constexpr int kRemovedDeskWindowYTranslation = 20;
constexpr base::TimeDelta kRemovedDeskWindowTranslationDuration =
    base::TimeDelta::FromMilliseconds(100);

// Create the layer that will be the parent of the screenshot layer, with a
// solid black color to act as the background showing behind the two
// screenshot layers in the |kDesksSpacing| region between them. It will get
// sized as children get added to it. This is the layer that will be animated.
std::unique_ptr<ui::LayerTreeOwner> CreateAnimationLayerOwner(
    aura::Window* root) {
  auto animation_layer = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  animation_layer->SetName("Desk switch animation layer");
  animation_layer->SetColor(SK_ColorBLACK);
  return std::make_unique<ui::LayerTreeOwner>(std::move(animation_layer));
}

// Takes a screenshot of the screen content. |on_screenshot_taken| will be
// triggered when the screenshot is taken.
void TakeScreenshot(
    aura::Window* root,
    viz::CopyOutputRequest::CopyOutputRequestCallback on_screenshot_taken) {
  auto* screenshot_layer =
      root->GetChildById(kShellWindowId_ScreenAnimationContainer)->layer();

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

std::string GetScreenshotLayerName(int index) {
  return "Desk " + base::NumberToString(index) + " screenshot layer";
}

// The values received from WmGestureHandler via DesksController are in touchpad
// units. Convert these units so that what is considered a full touchpad swipe
// shifts the animation layer one entire desk length.
float TouchpadToXTranslation(float touchpad_x, int desk_length) {
  return desk_length * touchpad_x /
         RootWindowDeskSwitchAnimator::kTouchpadSwipeLengthForDeskChange;
}

}  // namespace

RootWindowDeskSwitchAnimator::RootWindowDeskSwitchAnimator(
    aura::Window* root,
    int starting_desk_index,
    int ending_desk_index,
    Delegate* delegate,
    bool for_remove)
    : root_window_(root),
      starting_desk_index_(starting_desk_index),
      ending_desk_index_(ending_desk_index),
      delegate_(delegate),
      animation_layer_owner_(CreateAnimationLayerOwner(root)),
      x_translation_offset_(root->layer()->size().width() + kDesksSpacing),
      edge_padding_width_dp_(
          std::round(root_window_->bounds().width() * kEdgePaddingRatio)),
      for_remove_(for_remove) {
  DCHECK(root_window_);
  DCHECK_NE(starting_desk_index_, ending_desk_index_);
  DCHECK(delegate_);

  screenshot_layers_.resize(desks_util::kMaxNumberOfDesks);
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
    auto* desk_container = DesksController::Get()
                               ->desks()[starting_desk_index_]
                               ->GetDeskContainerForRoot(root_window_);
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

  // Set a transform so that the ending desk will be visible.
  gfx::Transform animation_layer_ending_transform;
  animation_layer_ending_transform.Translate(
      -GetXPositionOfScreenshot(ending_desk_index_), 0);

  // Animate the parent "animation layer" towards the ending transform.
  ui::Layer* animation_layer = animation_layer_owner_->root();
  ui::ScopedLayerAnimationSettings settings(animation_layer->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.AddObserver(this);
  settings.SetTransitionDuration(kAnimationDuration);
  settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  animation_layer->SetTransform(animation_layer_ending_transform);

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

bool RootWindowDeskSwitchAnimator::ReplaceAnimation(int new_ending_desk_index) {
  DCHECK(features::IsEnhancedDeskAnimations());
  DCHECK(!for_remove_);
  DCHECK_NE(new_ending_desk_index, ending_desk_index_);

  starting_desk_index_ = ending_desk_index_;
  ending_desk_index_ = new_ending_desk_index;

  if (!!screenshot_layers_[ending_desk_index_]) {
    // Notify the caller to start an animation to |ending_desk_index_|.
    return false;
  }

  ending_desk_screenshot_retries_ = 0;
  ending_desk_screenshot_taken_ = false;

  // Notify the caller to activate the next desk and request a screenshot.
  return true;
}

bool RootWindowDeskSwitchAnimator::UpdateSwipeAnimation(float scroll_delta_x) {
  if (!starting_desk_screenshot_taken_ || !ending_desk_screenshot_taken_)
    return false;

  const float translation_delta_x =
      TouchpadToXTranslation(scroll_delta_x, x_translation_offset_);

  // The visible bounds to the user are the root window bounds which always have
  // origin of 0,0. Therefore the rightmost edge of the visible bounds will be
  // the width.
  const int visible_bounds_width =
      root_window_->GetBoundsInRootWindow().width();

  // Append the new offset to the current transform. Clamp the new transform so
  // that we do not swipe past the edges.
  auto* animation_layer = animation_layer_owner_->root();
  float translation_x =
      animation_layer->transform().To2dTranslation().x() + translation_delta_x;
  translation_x = base::ClampToRange(
      translation_x,
      float{-animation_layer->bounds().width() + visible_bounds_width}, 0.f);
  gfx::Transform transform;
  transform.Translate(translation_x, 0.f);
  base::AutoReset<bool> auto_reset(&setting_new_transform_, true);
  animation_layer->SetTransform(transform);

  // The animation layer starts with two screenshot layers as the most common
  // transition is from one desk to another adjacent desk. We may need to signal
  // the delegate to request a new screenshot if the animating layer is about to
  // slide past the bounds which are visible to the user (root window bounds).
  //
  //              moving right ---->
  //   +---+------------------------------+---+
  //   |   |               +-----------+  |   |
  //   | c |      b        |     a     |  | c |
  //   |   |               +___________+  |   |
  //   +___+______________________________+___+
  //
  //  a - root window/visible bounds - (0,0-1000x500)
  //  b - animating layer with two screenshots and edge padding - (0,0-2350x500)
  //    - current second screenshot is visible (translation (-1200, 0))
  //  c - Edge padding, equal to |kEdgePaddingRatio| x 1000 - 150 dips wide
  //  We will notify the delegate to request a new screenshot once the x of b is
  //  within |kMinDistanceBeforeScreenshotDp| of the x of a, not including the
  //  edge padding (i.e. translation of (-190, 0)).
  gfx::RectF transformed_animation_layer_bounds(animation_layer->bounds());
  transform.TransformRect(&transformed_animation_layer_bounds);
  transformed_animation_layer_bounds.Inset(edge_padding_width_dp_, 0);

  const bool moving_left = scroll_delta_x < 0.f;
  const bool going_out_of_bounds =
      moving_left
          ? transformed_animation_layer_bounds.right() - visible_bounds_width <
                kMinDistanceBeforeScreenshotDp
          : transformed_animation_layer_bounds.x() >
                -kMinDistanceBeforeScreenshotDp;

  if (!going_out_of_bounds)
    return false;

  // Get the current visible desk index. The upcoming desk we need to show will
  // be an adjacent desk based on |moving_left|.
  const int current_visible_desk_index = GetIndexOfMostVisibleDeskScreenshot();
  int new_desk_index = current_visible_desk_index + (moving_left ? 1 : -1);

  if (new_desk_index < 0 ||
      new_desk_index >= int{DesksController::Get()->desks().size()}) {
    return false;
  }

  ending_desk_index_ = new_desk_index;
  ending_desk_screenshot_retries_ = 0;
  ending_desk_screenshot_taken_ = false;
  return true;
}

void RootWindowDeskSwitchAnimator::EndSwipeAnimation() {
  ending_desk_index_ = GetIndexOfMostVisibleDeskScreenshot();
  StartAnimation();
}

void RootWindowDeskSwitchAnimator::OnImplicitAnimationsCompleted() {
  // |setting_new_transform_| is true we call SetTransform while an animation is
  // under progress. Do not notify our delegate in that case.
  if (setting_new_transform_)
    return;

  StopObservingImplicitAnimations();
  animation_finished_ = true;
  delegate_->OnDeskSwitchAnimationFinished();
}

void RootWindowDeskSwitchAnimator::CompleteAnimationPhase1WithLayer(
    std::unique_ptr<ui::Layer> layer) {
  DCHECK(layer);

  ui::Layer* starting_desk_screenshot_layer = layer.release();
  screenshot_layers_[starting_desk_index_] = starting_desk_screenshot_layer;
  starting_desk_screenshot_layer->SetName(
      GetScreenshotLayerName(starting_desk_index_));

  auto* animation_layer = animation_layer_owner_->root();
  animation_layer->Add(starting_desk_screenshot_layer);

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
  OnScreenshotLayerCreated();
  delegate_->OnStartingDeskScreenshotTaken(ending_desk_index_);
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
      delegate_->OnStartingDeskScreenshotTaken(ending_desk_index_);
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
  screenshot_layers_[ending_desk_index_] = ending_desk_screenshot_layer;
  ending_desk_screenshot_layer->SetName(
      GetScreenshotLayerName(ending_desk_index_));
  animation_layer_owner_->root()->Add(ending_desk_screenshot_layer);

  ending_desk_screenshot_taken_ = true;
  OnScreenshotLayerCreated();
  delegate_->OnEndingDeskScreenshotTaken();
}

void RootWindowDeskSwitchAnimator::OnScreenshotLayerCreated() {
  // Set the layer bounds. |screenshot_layers_| always matches the order of the
  // desks, which is left to right.
  int num_screenshots = 0;
  const gfx::Size root_window_size = root_window_->bounds().size();
  DCHECK_EQ(x_translation_offset_, root_window_size.width() + kDesksSpacing);
  for (ui::Layer* layer : screenshot_layers_) {
    if (!layer)
      continue;

    const int x =
        num_screenshots * x_translation_offset_ + edge_padding_width_dp_;
    layer->SetBounds(gfx::Rect(gfx::Point(x, 0), root_window_size));
    ++num_screenshots;
  }

  // The animation layer is sized to contain all the screenshot layers,
  // |kDesksSpacing| between any two adjacent screenshot layers, and
  // |edge_padding_width_dp_| on each side.
  const gfx::Rect animation_layer_bounds(
      num_screenshots * x_translation_offset_ - kDesksSpacing +
          2 * edge_padding_width_dp_,
      root_window_size.height());
  auto* animation_layer = animation_layer_owner_->root();
  animation_layer->SetBounds(animation_layer_bounds);

  // Two examples of simple animations (two desks involved), one moving left and
  // one moving right. Starting desk is one the left, so we start off with no
  // offset and then slide the animation layer so that ending desk is visible
  // (target transform of -|x_translation_offset_| translation).
  //
  //                         +-----------+
  //                         | Animation |
  //                         |  layer    |
  //                         +-----------+
  //                           /        \
  //                +------------+      +------------+
  //                | start desk |      | end desk   |
  //                | screenshot |      | screenshot |
  //                |  layer (1) |      |  layer (2) |
  //                +------------+      +------------+
  //                      ^
  //                  start here
  //
  //                |------------------>|
  //                          ^
  //                `x_translation_offset_`
  //
  // Starting desk is one the right, so we need to offset the animation layer
  // horizontally so that the starting desk is visible
  // (-|x_translation_offset_|) and the slide the animation layer so that the
  // ending desk is visible (target transform of 0 translation).
  //
  //                         +-----------+
  //                         | Animation |
  //                         |  layer    |
  //                         +-----------+
  //                           /        \
  //                +------------+      +------------+
  //                | end desk   |      | start desk |
  //                | screenshot |      | screenshot |
  //                |  layer (1) |      |  layer (2) |
  //                +------------+      +------------+
  //                                          ^
  //                |----------------->| start here
  //                         ^
  //               `x_translation_offset_`
  //
  // Chained animation example, we are in the middle of animating from desk 3 to
  // desk 2 (start' to end'), currently halfway through the animation. Desk 1 is
  // added, so the x position of both desk 2 and desk 3 will get shifted by
  // |x_translation_offset_|. Shift animation layer by -|x_translation_offset_|
  // so that half of desk 3 and half of desk 2 are still visible. Without this
  // shift, there will be a jump and we will see half of desk 2 and half of
  // desk 1. We then animate from start to end.
  //
  //                +---------------------------------------+
  //                |          Animation                    |
  //                |           layer                       |
  //                +---------------------------------------+
  //                    /               |                  \
  //          +------------+      +------------+      +------------+
  //          | desk 1     |      | desk 2     |      | desk 3     |
  //          | screenshot |      | screenshot |      | screenshot |
  //          |  layer     |      |  layer     |      |  layer     |
  //          +------------+      +------------+      +------------+
  //          ^                   ^       ^           ^
  //         end                 end'   start       start'

  // If there is an existing transform, continue animating from there.
  gfx::Transform current_transform = animation_layer->transform();
  DCHECK(current_transform.IsIdentityOr2DTranslation());
  if (!current_transform.IsIdentity()) {
    // If the new layer is located on the left of the prior created layers,
    // shift the animation layer transform so that the content shown to users
    // remain the same.
    if (ending_desk_index_ < starting_desk_index_) {
      // Setting a new transform will end an ongoing animation, which will
      // trigger OnImplicitAnimationsCompleted, which notifies our delegate to
      // delete us. For this case, set a flag so that
      // OnImplicitAnimationsCompleted does no notifying.
      current_transform.Translate(-x_translation_offset_, 0);
      base::AutoReset<bool> auto_reset(&setting_new_transform_, true);
      animation_layer->SetTransform(current_transform);
    }
    return;
  }

  // Otherwise, transform |animation_layer| so that starting desk screenshot
  // layer is the current visible layer.
  gfx::Transform animation_layer_starting_transform;
  animation_layer_starting_transform.Translate(
      -GetXPositionOfScreenshot(starting_desk_index_), 0);
  base::AutoReset<bool> auto_reset(&setting_new_transform_, true);
  animation_layer->SetTransform(animation_layer_starting_transform);
}

int RootWindowDeskSwitchAnimator::GetXPositionOfScreenshot(int index) {
  ui::Layer* layer = screenshot_layers_[index];
  DCHECK(layer);
  return layer->bounds().x();
}

int RootWindowDeskSwitchAnimator::GetIndexOfMostVisibleDeskScreenshot() const {
  int index = -1;

  // The most visible desk is the one whose screenshot layer bounds, including
  // the transform of its parent that has its origin closest to the root window
  // origin (0, 0).
  const gfx::Transform transform = animation_layer_owner_->root()->transform();
  int min_distance = INT_MAX;
  for (int i = 0; i < int{screenshot_layers_.size()}; ++i) {
    ui::Layer* layer = screenshot_layers_[i];
    if (!layer)
      continue;

    gfx::RectF bounds(layer->bounds());
    transform.TransformRect(&bounds);
    const int distance = std::abs(bounds.x());
    if (distance < min_distance) {
      min_distance = distance;
      index = i;
    }
  }

  DCHECK_GE(index, 0);
  DCHECK_LT(index, int{DesksController::Get()->desks().size()});
  return index;
}

}  // namespace ash
