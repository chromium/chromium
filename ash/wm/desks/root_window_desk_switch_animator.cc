// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/root_window_desk_switch_animator.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/utility/layer_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/animation_builder.h"
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

// Different durations is used for different animations, see more details in
// `DeskSwitchAnimationType`.
constexpr base::TimeDelta kQuickAnimationDuration = base::Milliseconds(150);
constexpr base::TimeDelta kQuickFadeInAnimationDuration =
    base::Milliseconds(100);
constexpr base::TimeDelta kContinuousAnimationDuration =
    base::Milliseconds(300);

// The ratio of root window width for quick swipe animation to translate.
constexpr float kQuickAnimationTranslationDistanceRatio = 0.25;

// The ratio of root window width used for animation layer width.
constexpr float kQuickAnimationLayerWidthRatio = 1.25;

// When ending a swipe that is deemed fast, the target desk only needs to be
// 10% shown for us to animate to that desk, compared to 50% shown for a non
// fast swipe.
constexpr float kFastSwipeVisibilityRatio = 0.1f;

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
  CHECK(root);

  auto* screenshot_layer =
      root->GetChildById(kShellWindowId_ScreenAnimationContainer)->layer();

  const gfx::Rect request_bounds(screenshot_layer->size());
  auto screenshot_request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kNativeTextures,
      std::move(on_screenshot_taken));
  screenshot_request->set_area(request_bounds);
  screenshot_request->set_result_task_runner(
      base::SequencedTaskRunner::GetCurrentDefault());
  screenshot_layer->RequestCopyOfOutput(std::move(screenshot_request));
}

std::string GetScreenshotLayerName(int index) {
  return "Desk " + base::NumberToString(index) + " screenshot layer";
}

// The values received from WmGestureHandler via DesksController are in touchpad
// units. Convert these units so that what is considered a full touchpad swipe
// shifts the animation layer one entire desk length.
float TouchpadToXTranslation(float touchpad_x, int desk_length) {
  return desk_length * touchpad_x / kTouchpadSwipeLengthForDeskChange;
}

}  // namespace

RootWindowDeskSwitchAnimator::RootWindowDeskSwitchAnimator(
    aura::Window* root,
    DeskSwitchAnimationType type,
    int starting_desk_index,
    int ending_desk_index,
    Delegate* delegate,
    bool for_remove)
    : root_window_(root),
      type_(type),
      starting_desk_index_(starting_desk_index),
      ending_desk_index_(ending_desk_index),
      delegate_(delegate),
      animation_layer_owner_(CreateAnimationLayerOwner(root)),
      root_window_size_(
          screen_util::SnapBoundsToDisplayEdge(root->bounds(), root).size()),
      x_translation_offset_(
          type_ == DeskSwitchAnimationType::kContinuousAnimation
              ? root_window_size_.width() + kDesksSpacing
              : root_window_size_.width() *
                    kQuickAnimationTranslationDistanceRatio),
      edge_padding_width_dp_(
          std::round(root_window_size_.width() * kEdgePaddingRatio)),
      for_remove_(for_remove) {
  DCHECK(root_window_);
  DCHECK_NE(starting_desk_index_, ending_desk_index_);
  DCHECK(delegate_);

  // Observe root window removals.
  Shell::Get()->AddShellObserver(this);

  screenshot_layers_.resize(desks_util::GetMaxNumberOfDesks());
}

RootWindowDeskSwitchAnimator::~RootWindowDeskSwitchAnimator() {
  // TODO(afakhry): Determine if this is necessary, since generally this object
  // is only deleted when all animations end, but there might be situations when
  // we might need to kill the animations before they complete such as when a
  // display is removed.
  if (!attached_sequences().empty())
    StopObservingImplicitAnimations();

  Shell::Get()->RemoveShellObserver(this);
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
  DCHECK(!animation_finished_);

  // Set a transform so that the ending desk will be visible.
  gfx::Transform animation_layer_ending_transform;
  animation_layer_ending_transform.Translate(
      -GetXPositionOfScreenshot(ending_desk_index_), 0);

  // Animate the parent "animation layer" towards the ending transform.
  ui::Layer* animation_layer = animation_layer_owner_->root();
  ui::ScopedLayerAnimationSettings scoped_settings(
      animation_layer->GetAnimator());
  scoped_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  scoped_settings.AddObserver(this);
  if (type_ == DeskSwitchAnimationType::kContinuousAnimation) {
    scoped_settings.SetTransitionDuration(kContinuousAnimationDuration);
    scoped_settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  } else {
    scoped_settings.SetTransitionDuration(kQuickAnimationDuration);
    scoped_settings.SetTweenType(gfx::Tween::ACCEL_20_DECEL_100);
  }
  animation_layer->SetTransform(animation_layer_ending_transform);

  // During quick animation, we fade in the ending desk during sliding
  // animation.
  if (type_ == DeskSwitchAnimationType::kQuickAnimation &&
      screenshot_layers_[ending_desk_index_]) {
    CHECK_EQ(screenshot_layers_[ending_desk_index_]->opacity(), 0.f);
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetDuration(kQuickFadeInAnimationDuration)
        .SetOpacity(screenshot_layers_[ending_desk_index_], 1.f,
                    gfx::Tween::ACCEL_20_DECEL_100);
  }
}

bool RootWindowDeskSwitchAnimator::ReplaceAnimation(int new_ending_desk_index) {
  DCHECK(!for_remove_);
  DCHECK_NE(new_ending_desk_index, ending_desk_index_);

  starting_desk_index_ = ending_desk_index_;
  ending_desk_index_ = new_ending_desk_index;

  if (!!screenshot_layers_[ending_desk_index_]) {
    // Update the ending desk opacity when replace animation happens.
    if (type_ == DeskSwitchAnimationType::kQuickAnimation) {
      screenshot_layers_[ending_desk_index_]->SetOpacity(0.f);
    }
    // Notify the caller to start an animation to |ending_desk_index_|.
    return false;
  }

  ending_desk_screenshot_retries_ = 0;
  ending_desk_screenshot_taken_ = false;

  // Notify the caller to activate the next desk and request a screenshot.
  return true;
}

std::optional<int> RootWindowDeskSwitchAnimator::UpdateSwipeAnimation(
    float scroll_delta_x) {
  if (!starting_desk_screenshot_taken_ || !ending_desk_screenshot_taken_)
    return std::nullopt;

  const float translation_delta_x =
      TouchpadToXTranslation(scroll_delta_x, x_translation_offset_);

  // The visible bounds to the user are the root window bounds which always have
  // origin of 0,0. Therefore the rightmost edge of the visible bounds will be
  // the width.
  const int visible_bounds_width = root_window_size_.width();

  // Append the new offset to the current transform. Clamp the new transform so
  // that we do not swipe past the edges.
  auto* animation_layer = animation_layer_owner_->root();
  float translation_x =
      animation_layer->transform().To2dTranslation().x() + translation_delta_x;
  translation_x =
      std::clamp(translation_x,
                 static_cast<float>(-animation_layer->bounds().width() +
                                    visible_bounds_width),
                 0.f);
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
  gfx::RectF transformed_animation_layer_bounds =
      transform.MapRect(gfx::RectF(animation_layer->bounds()));

  // `reached_edge_` becomes true if the user has scrolled `animation_layer` to
  // its limits.
  reached_edge_ =
      transformed_animation_layer_bounds.x() == 0 ||
      transformed_animation_layer_bounds.right() == root_window_size_.width();

  transformed_animation_layer_bounds.Inset(
      gfx::InsetsF::VH(0, edge_padding_width_dp_));

  const bool moving_left = scroll_delta_x < 0.f;
  const bool going_out_of_bounds =
      moving_left
          ? transformed_animation_layer_bounds.right() - visible_bounds_width <
                kMinDistanceBeforeScreenshotDp
          : transformed_animation_layer_bounds.x() >
                -kMinDistanceBeforeScreenshotDp;

  if (!going_out_of_bounds)
    return std::nullopt;

  // The upcoming desk we need to show will be an adjacent desk to the desk at
  // the visible desk index based on |moving_left|.
  const int new_desk_index =
      GetIndexOfMostVisibleDeskScreenshot() + (moving_left ? 1 : -1);

  if (new_desk_index < 0 ||
      new_desk_index >=
          static_cast<int>(DesksController::Get()->desks().size())) {
    return std::nullopt;
  }

  return new_desk_index;
}

void RootWindowDeskSwitchAnimator::PrepareForEndingDeskScreenshot(
    int new_ending_desk_index) {
  ending_desk_index_ = new_ending_desk_index;
  ending_desk_screenshot_retries_ = 0;
  ending_desk_screenshot_taken_ = false;
}

int RootWindowDeskSwitchAnimator::EndSwipeAnimation(bool is_fast_swipe) {
  // If the starting screenshot has not finished, just let our delegate know
  // that the desk animation is finished (and |this| will soon be deleted), and
  // go back to the starting desk.
  if (!starting_desk_screenshot_taken_) {
    animation_finished_ = true;
    // Notifying the delegate may delete |this|. Store the target index in a
    // local so we do not try to access a member of a deleted object.
    const int ending_desk_index = starting_desk_index_;
    delegate_->OnDeskSwitchAnimationFinished();
    return ending_desk_index;
  }

  // In tests, StartAnimation() may trigger OnDeskSwitchAnimationFinished()
  // right away which may delete |this|. Store the target index in a
  // local so we do not try to access a member of a deleted object.
  int local_ending_desk_index = -1;

  // Try animating to `ending_desk_index_` regardless of how much of it is
  // visible.
  if (is_fast_swipe) {
    local_ending_desk_index = ending_desk_index_;
    // If the ending desk screenshot is underway, it will call
    // `StartAnimation()` when finished.
    if (ending_desk_screenshot_taken_)
      StartAnimation();
    return local_ending_desk_index;
  }

  // If the ending desk screenshot has not finished,
  // GetIndexOfMostVisibleDeskScreenshot() will
  // still return a valid desk index that we can animate to, but we need to make
  // sure the ending desk screenshot callback does not get called.
  if (!ending_desk_screenshot_taken_)
    weak_ptr_factory_.InvalidateWeakPtrs();

  // If the swipe we are ending with is deemed a fast swipe, we animate to
  // |ending_desk_index_| if more than 10% of it is currently visible.
  // Otherwise, we animate to the most visible desk.
  if (is_fast_swipe) {
    ui::Layer* layer = screenshot_layers_[ending_desk_index_];
    if (layer) {
      const gfx::Transform transform =
          animation_layer_owner_->root()->transform();
      gfx::RectF screenshot_bounds =
          transform.MapRect(gfx::RectF(layer->bounds()));

      const gfx::RectF root_window_bounds(root_window_->bounds());
      const gfx::RectF intersection_rect =
          gfx::IntersectRects(screenshot_bounds, root_window_bounds);
      if (intersection_rect.width() >
          root_window_bounds.width() * kFastSwipeVisibilityRatio) {
        local_ending_desk_index = ending_desk_index_;
      }
    }
  }

  if (local_ending_desk_index == -1)
    local_ending_desk_index = GetIndexOfMostVisibleDeskScreenshot();

  ending_desk_index_ = local_ending_desk_index;
  StartAnimation();
  return local_ending_desk_index;
}

int RootWindowDeskSwitchAnimator::GetIndexOfMostVisibleDeskScreenshot() const {
  int index = -1;

  // The most visible desk is the one whose screenshot layer bounds, including
  // the transform of its parent that has its origin closest to the root window
  // origin (0, 0).
  const gfx::Transform transform = animation_layer_owner_->root()->transform();
  int min_distance = INT_MAX;
  for (int i = 0; i < static_cast<int>(screenshot_layers_.size()); ++i) {
    ui::Layer* layer = screenshot_layers_[i];
    if (!layer)
      continue;

    gfx::Rect bounds = transform.MapRect(layer->bounds());
    const int distance = std::abs(bounds.x());
    if (distance < min_distance) {
      min_distance = distance;
      index = i;
    }
  }

  // TODO(crbug.com/40151430): Convert back to DCHECK when the issue is fixed.
  CHECK_GE(index, 0);
  CHECK_LT(index, static_cast<int>(DesksController::Get()->desks().size()));
  return index;
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

void RootWindowDeskSwitchAnimator::OnRootWindowWillShutdown(
    aura::Window* root_window) {
  if (root_window != root_window_) {
    return;
  }

  // The root window we are working on is about to go away, so we must not use
  // it anymore.
  root_window_ = nullptr;
  animator_failed_ = true;
}

ui::Layer* RootWindowDeskSwitchAnimator::GetAnimationLayerForTesting() const {
  return animation_layer_owner_->root();
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
  CHECK(root_window_);
  auto* root_layer = root_window_->layer();
  root_layer->Add(animation_layer);

  if (for_remove_ && is_combine_desks_type_) {
    DCHECK(old_windows_layer_tree_owner_);
    auto* old_windows_layer = old_windows_layer_tree_owner_->root();
    DCHECK(old_windows_layer);
    root_layer->StackBelow(animation_layer, old_windows_layer);
  } else {
    root_layer->StackAtTop(animation_layer);
  }

  starting_desk_screenshot_taken_ = true;
  OnScreenshotLayerCreated();

  if (on_starting_screenshot_taken_callback_for_testing_)
    std::move(on_starting_screenshot_taken_callback_for_testing_).Run();

  delegate_->OnStartingDeskScreenshotTaken(ending_desk_index_);
}

void RootWindowDeskSwitchAnimator::OnStartingDeskScreenshotTaken(
    std::unique_ptr<viz::CopyOutputResult> copy_result) {
  if (animator_failed_) {
    delegate_->OnStartingDeskScreenshotTaken(ending_desk_index_);
    return;
  }

  if (!copy_result || copy_result->IsEmpty()) {
    // A frame may be activated before the screenshot requests are satisfied,
    // leading to us getting an empty |result|. Rerequest the screenshot.
    // (See viz::Surface::ActivateFrame()).
    base::UmaHistogramBoolean(kDeskSwitchScreenshotResultHistogramName, false);
    if (++starting_desk_screenshot_retries_ <= kMaxScreenshotRetries) {
      TakeStartingDeskScreenshot();
    } else {
      LOG(ERROR) << "Received multiple empty screenshots of the starting desk.";
      animator_failed_ = true;
      delegate_->OnStartingDeskScreenshotTaken(ending_desk_index_);
    }

    return;
  }

  base::UmaHistogramBoolean(kDeskSwitchScreenshotResultHistogramName, true);
  CompleteAnimationPhase1WithLayer(CreateLayerFromCopyOutputResult(
      std::move(copy_result), root_window_size_));
}

void RootWindowDeskSwitchAnimator::OnEndingDeskScreenshotTaken(
    std::unique_ptr<viz::CopyOutputResult> copy_result) {
  if (animator_failed_) {
    delegate_->OnStartingDeskScreenshotTaken(ending_desk_index_);
    return;
  }

  if (!copy_result || copy_result->IsEmpty()) {
    // A frame may be activated before the screenshot requests are satisfied,
    // leading to us getting an empty |result|. Rerequest the screenshot.
    // (See viz::Surface::ActivateFrame()).
    base::UmaHistogramBoolean(kDeskSwitchScreenshotResultHistogramName, false);
    if (++ending_desk_screenshot_retries_ <= kMaxScreenshotRetries) {
      TakeEndingDeskScreenshot();
    } else {
      LOG(ERROR) << "Received multiple empty screenshots of the ending desk.";
      animator_failed_ = true;
      delegate_->OnEndingDeskScreenshotTaken();
    }

    return;
  }

  base::UmaHistogramBoolean(kDeskSwitchScreenshotResultHistogramName, true);
  ui::Layer* ending_desk_screenshot_layer =
      CreateLayerFromCopyOutputResult(std::move(copy_result), root_window_size_)
          .release();
  screenshot_layers_[ending_desk_index_] = ending_desk_screenshot_layer;
  // In quick animation, the ending desk starts with 0 opacity.
  if (type_ == DeskSwitchAnimationType::kQuickAnimation) {
    screenshot_layers_[ending_desk_index_]->SetOpacity(0.f);
  }
  ending_desk_screenshot_layer->SetName(
      GetScreenshotLayerName(ending_desk_index_));
  animation_layer_owner_->root()->Add(ending_desk_screenshot_layer);

  ending_desk_screenshot_taken_ = true;
  OnScreenshotLayerCreated();

  // On ending screenshot may delete |this|.
  if (on_ending_screenshot_taken_callback_for_testing_)
    std::move(on_ending_screenshot_taken_callback_for_testing_).Run();

  delegate_->OnEndingDeskScreenshotTaken();
}

void RootWindowDeskSwitchAnimator::OnScreenshotLayerCreated() {
  // Set the layer bounds. |screenshot_layers_| always matches the order of the
  // desks, which is left to right.
  int num_screenshots = 0;
  for (ui::Layer* layer : screenshot_layers_) {
    if (!layer)
      continue;

    const int x =
        num_screenshots * x_translation_offset_ + edge_padding_width_dp_;
    layer->SetBounds(gfx::Rect(gfx::Point(x, 0), root_window_size_));
    ++num_screenshots;
  }

  // The animation layer is sized to contain all the screenshot layers,
  // |kDesksSpacing| between any two adjacent screenshot layers, and
  // |edge_padding_width_dp_| on each side.
  gfx::Rect animation_layer_bounds;
  if (type_ == DeskSwitchAnimationType::kContinuousAnimation) {
    animation_layer_bounds =
        gfx::Rect(num_screenshots * x_translation_offset_ - kDesksSpacing +
                      2 * edge_padding_width_dp_,
                  root_window_size_.height());
  } else {
    animation_layer_bounds =
        gfx::Rect(root_window_size_.width() * kQuickAnimationLayerWidthRatio +
                      2 * edge_padding_width_dp_,
                  root_window_size_.height());
  }

  auto* animation_layer = animation_layer_owner_->root();
  animation_layer->SetBounds(animation_layer_bounds);

  // Two examples of simple animations (two desks involved), one moving left and
  // one moving right. Starting desk is one the left, so we start off with no
  // offset and then slide the animation layer so that ending desk is visible
  // (target transform of -|x_translation_offset_| translation).
  //
  // Note: The `x_translation_offset_` is different between
  // `kContinuousAnimation` and `kQuickAnimation`, see more details in header
  // file.
  //
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
  DCHECK(current_transform.IsIdentityOr2dTranslation());
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
  // TODO(crbug.com/1223866): Investigate if we can prevent this higher in the
  // call stack.
  if (index < 0 || index >= static_cast<int>(screenshot_layers_.size()))
    return 0;
  ui::Layer* layer = screenshot_layers_[index];
  if (!layer)
    return 0;

  DCHECK(layer);
  return layer->bounds().x();
}

}  // namespace ash
