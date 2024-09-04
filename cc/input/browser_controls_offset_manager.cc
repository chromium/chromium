// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager.h"

#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/types/optional_ref.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/offset_tag.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
namespace {
// These constants were chosen empirically for their visually pleasant behavior.
// Contact tedchoc@chromium.org for questions about changing these values.
const int64_t kShowHideMaxDurationMs = 200;
// TODO(sinansahin): Temporary value, pending UX guidance probably.
const int64_t kHeightChangeDurationMs = 200;
}

// static
std::unique_ptr<BrowserControlsOffsetManager>
BrowserControlsOffsetManager::Create(BrowserControlsOffsetManagerClient* client,
                                     float controls_show_threshold,
                                     float controls_hide_threshold) {
  return base::WrapUnique(new BrowserControlsOffsetManager(
      client, controls_show_threshold, controls_hide_threshold));
}

BrowserControlsOffsetManager::BrowserControlsOffsetManager(
    BrowserControlsOffsetManagerClient* client,
    float controls_show_threshold,
    float controls_hide_threshold)
    : client_(client),
      permitted_state_(BrowserControlsState::kBoth),
      accumulated_scroll_delta_(0.f),
      baseline_top_content_offset_(0.f),
      baseline_bottom_content_offset_(0.f),
      controls_show_threshold_(controls_hide_threshold),
      controls_hide_threshold_(controls_show_threshold),
      pinch_gesture_active_(false),
      constraint_changed_since_commit_(false),
      top_min_height_change_in_progress_(false),
      bottom_min_height_change_in_progress_(false),
      top_controls_min_height_offset_(0.f),
      bottom_controls_min_height_offset_(0.f) {
  CHECK(client_);
  UpdateOldBrowserControlsParams();
}

BrowserControlsOffsetManager::~BrowserControlsOffsetManager() = default;

float BrowserControlsOffsetManager::ControlsTopOffset() const {
  return ContentTopOffset() - TopControlsHeight();
}

float BrowserControlsOffsetManager::ContentTopOffset() const {
  return TopControlsHeight() > 0
      ? TopControlsShownRatio() * TopControlsHeight() : 0.0f;
}

float BrowserControlsOffsetManager::TopControlsShownRatio() const {
  return client_->CurrentTopControlsShownRatio();
}

float BrowserControlsOffsetManager::TopControlsHeight() const {
  return client_->TopControlsHeight();
}

float BrowserControlsOffsetManager::TopControlsMinHeight() const {
  return client_->TopControlsMinHeight();
}

viz::OffsetTag BrowserControlsOffsetManager::TopControlsOffsetTag() const {
  return top_controls_offset_tag_;
}

float BrowserControlsOffsetManager::TopControlsMinShownRatio() const {
  return TopControlsHeight() ? TopControlsMinHeight() / TopControlsHeight()
                             : 0.f;
}

float BrowserControlsOffsetManager::BottomControlsHeight() const {
  return client_->BottomControlsHeight();
}

float BrowserControlsOffsetManager::BottomControlsMinHeight() const {
  return client_->BottomControlsMinHeight();
}

float BrowserControlsOffsetManager::BottomControlsMinShownRatio() const {
  return BottomControlsHeight()
             ? BottomControlsMinHeight() / BottomControlsHeight()
             : 0.f;
}

float BrowserControlsOffsetManager::ContentBottomOffset() const {
  return BottomControlsHeight() > 0
      ? BottomControlsShownRatio() * BottomControlsHeight() : 0.0f;
}

float BrowserControlsOffsetManager::BottomControlsShownRatio() const {
  return client_->CurrentBottomControlsShownRatio();
}

float BrowserControlsOffsetManager::TopControlsMinHeightOffset() const {
  return top_controls_min_height_offset_;
}

float BrowserControlsOffsetManager::BottomControlsMinHeightOffset() const {
  return bottom_controls_min_height_offset_;
}

std::pair<float, float>
BrowserControlsOffsetManager::TopControlsShownRatioRange() {
  if (top_controls_animation_.IsInitialized())
    return std::make_pair(top_controls_animation_.min_value(),
                          top_controls_animation_.max_value());

  return std::make_pair(0.f, 1.f);
}

std::pair<float, float>
BrowserControlsOffsetManager::BottomControlsShownRatioRange() {
  if (bottom_controls_animation_.IsInitialized())
    return std::make_pair(bottom_controls_animation_.min_value(),
                          bottom_controls_animation_.max_value());

  return std::make_pair(0.f, 1.f);
}

void BrowserControlsOffsetManager::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate,
    base::optional_ref<const BrowserControlsOffsetTagsInfo> offset_tags_info) {
  DCHECK(!(constraints == BrowserControlsState::kShown &&
           current == BrowserControlsState::kHidden));
  DCHECK(!(constraints == BrowserControlsState::kHidden &&
           current == BrowserControlsState::kShown));

  TRACE_EVENT2("cc", "BrowserControlsOffsetManager::UpdateBrowserControlsState",
               "constraints", static_cast<int>(constraints), "current",
               static_cast<int>(current));

  // If the constraints have changed we need to inform Blink about it since
  // that'll affect main thread scrolling as well as layout.
  if (permitted_state_ != constraints) {
    constraint_changed_since_commit_ = true;
    client_->SetNeedsCommit();
  }

  permitted_state_ = constraints;

  if (offset_tags_info.has_value()) {
    top_controls_offset_tag_ = offset_tags_info.value().top_controls_offset_tag;
  }

  // Don't do anything if it doesn't matter which state the controls are in.
  if (constraints == BrowserControlsState::kBoth &&
      current == BrowserControlsState::kBoth)
    return;

  // Don't do anything if there is no change in offset.
  float final_top_shown_ratio = 1.f;
  float final_bottom_shown_ratio = 1.f;
  AnimationDirection direction = AnimationDirection::kShowingControls;
  if (constraints == BrowserControlsState::kHidden ||
      current == BrowserControlsState::kHidden) {
    final_top_shown_ratio = TopControlsMinShownRatio();
    final_bottom_shown_ratio = BottomControlsMinShownRatio();
    direction = AnimationDirection::kHidingControls;
  }
  if (final_top_shown_ratio == TopControlsShownRatio() &&
      final_bottom_shown_ratio == BottomControlsShownRatio()) {
    TRACE_EVENT_INSTANT0("cc", "Ratios Unchanged", TRACE_EVENT_SCOPE_THREAD);
    ResetAnimations();
    return;
  }

  // Don't do anything if the currently running animations end in our desired
  // state.
  float animated_top_shown_ratio = top_controls_animation_.IsInitialized()
                                       ? top_controls_animation_.FinalValue()
                                       : TopControlsShownRatio();
  float animated_bottom_shown_ratio =
      bottom_controls_animation_.IsInitialized()
          ? bottom_controls_animation_.FinalValue()
          : BottomControlsShownRatio();
  if (animated_top_shown_ratio == final_top_shown_ratio &&
      animated_bottom_shown_ratio == final_bottom_shown_ratio) {
    return;
  }

  ResetAnimations();

  // If we're about to animate the controls in, then restart the animation after
  // the scroll completes.  We don't know if a scroll is in progress, but that's
  // okay; the flag will be reset when a scroll starts next in that case.
  if (animate && direction == AnimationDirection::kShowingControls) {
    show_controls_when_scroll_completes_ = true;
  }

  if (animate)
    SetupAnimation(direction);
  else
    client_->SetCurrentBrowserControlsShownRatio(final_top_shown_ratio,
                                                 final_bottom_shown_ratio);
}

BrowserControlsState BrowserControlsOffsetManager::PullConstraintForMainThread(
    bool* out_changed_since_commit) {
  DCHECK(out_changed_since_commit);
  *out_changed_since_commit = constraint_changed_since_commit_;
  return permitted_state_;
}

void BrowserControlsOffsetManager::NotifyConstraintSyncedToMainThread() {
  constraint_changed_since_commit_ = false;
}

void BrowserControlsOffsetManager::OnBrowserControlsParamsChanged(
    bool animate_changes) {
  if (old_browser_controls_params_.top_controls_height == TopControlsHeight() &&
      old_browser_controls_params_.top_controls_min_height ==
          TopControlsMinHeight() &&
      old_browser_controls_params_.bottom_controls_height ==
          BottomControlsHeight() &&
      old_browser_controls_params_.bottom_controls_min_height ==
          BottomControlsMinHeight()) {
    return;
  }

  // We continue to update both top and bottom controls even if one has a height
  // of 0 so that animations work properly. So here, we should preserve the
  // ratios even if the controls height is 0.
  float old_top_height = old_browser_controls_params_.top_controls_height;
  float new_top_ratio =
      TopControlsHeight()
          ? TopControlsShownRatio() * old_top_height / TopControlsHeight()
          : TopControlsShownRatio();

  float old_bottom_height = old_browser_controls_params_.bottom_controls_height;
  float new_bottom_ratio = BottomControlsHeight()
                               ? BottomControlsShownRatio() *
                                     old_bottom_height / BottomControlsHeight()
                               : BottomControlsShownRatio();

  if (!animate_changes) {
    // If the min-heights changed when the controls were at the min-height, the
    // shown ratios need to be snapped to the new min-shown-ratio to keep the
    // controls at the min height. If the controls were fully shown, we want to
    // keep them fully shown even after the heights changed. For any other
    // cases, we should update the shown ratio so the visible height remains the
    // same.
    if (TopControlsShownRatio() == OldTopControlsMinShownRatio())
      new_top_ratio = TopControlsMinShownRatio();
    else if (TopControlsShownRatio() == 1.f)
      new_top_ratio = 1.f;

    if (BottomControlsShownRatio() == OldBottomControlsMinShownRatio())
      new_bottom_ratio = BottomControlsMinShownRatio();
    else if (BottomControlsShownRatio() == 1.f)
      new_bottom_ratio = 1.f;
  }

  // Browser controls height change animations
  // If the browser controls heights (and/or min-heights) changed and need to be
  // animated, the setup is done here. All the animations done in this class
  // involve changing the shown ratios smoothly.
  //
  // There are several cases to handle:
  // 1- The controls shown ratio was at the minimum ratio
  //    - If the min-height changed, we will run an animation from
  //      old-min-height / new-total-height to new-min-height / new-total-height
  //    - If the min-height didn't change, we should update the shown ratio to
  //      min-height / new-total-height so that the controls keep the same
  //      visible height and don't jump. No animation needed in this case.
  // 2- The controls shown ratio was at the highest ratio (should be 1 here)
  //    - If the total height changed, we will run an animation from
  //      old-total-height / new-total-height to 1.
  //    - If the total height didn't change, we don't need to do anything.
  // 3- The controls shown ratio is between the minimum and the maximum.
  //    - If an animation is running to the old min-height, start a new
  //      animation to min-height / total-height.
  //    - Otherwise don't start an animation. We're either animating the
  //      controls to their expanded state, in which case we can let that
  //      animation continue, or we're scrolling and no animation is needed.
  //      Update the shown ratio so the visible height remains the same (see
  //      new_{top,bottom}_ratio above).
  //
  // When this method is called as a result of a height change,
  // TopControlsHeight(), TopControlsMinHeight(), BottomControlsHeight(), and
  // BottomControlsMinHeight() will already be returning the new values.
  // However, the shown ratios aren't updated.

  bool top_controls_need_animation = animate_changes;
  bool bottom_controls_need_animation = animate_changes;

  // To handle the case where the min-height changes while we're animating to
  // the previous min-height, base our "are we at the minimum shown ratio"
  // check on the post-animation ratio if an animation is running, rather than
  // its current value.
  float effective_top_shown_ratio = TopControlsShownRatio();
  if (top_controls_animation_.IsInitialized()) {
    effective_top_shown_ratio = top_controls_animation_.FinalValue();
  }
  float effective_bottom_shown_ratio = BottomControlsShownRatio();
  if (bottom_controls_animation_.IsInitialized()) {
    effective_bottom_shown_ratio = bottom_controls_animation_.FinalValue();
  }

  float top_target_ratio;
  // We can't animate if we don't have top controls.
  if (!TopControlsHeight()) {
    top_controls_need_animation = false;

    // If the top controls height changed when they were fully shown.
  } else if (TopControlsShownRatio() == 1.f &&
             TopControlsHeight() != old_top_height) {
    top_target_ratio = 1.f;  // i.e. new_height / new_height

    // If the top controls min-height changed when they were at the minimum
    // shown ratio. For example, the min height changed from 0 to a positive
    // value while the top controls were completely hidden.
  } else if (effective_top_shown_ratio == OldTopControlsMinShownRatio() &&
             TopControlsMinHeight() !=
                 old_browser_controls_params_.top_controls_min_height) {
    top_target_ratio = TopControlsMinShownRatio();
  } else {
    top_controls_need_animation = false;
  }

  float bottom_target_ratio;
  // We can't animate if we don't have bottom controls.
  if (!BottomControlsHeight()) {
    bottom_controls_need_animation = false;

    // If the bottom controls height changed when they were fully shown.
  } else if (BottomControlsShownRatio() == 1.f &&
             BottomControlsHeight() != old_bottom_height) {
    bottom_target_ratio = 1.f;  // i.e. new_height / new_height

    // If the bottom controls min-height changed when they were at the minimum
    // shown ratio.
  } else if (effective_bottom_shown_ratio == OldBottomControlsMinShownRatio() &&
             BottomControlsMinHeight() !=
                 old_browser_controls_params_.bottom_controls_min_height) {
    bottom_target_ratio = BottomControlsMinShownRatio();
  } else {
    bottom_controls_need_animation = false;
  }

  if (top_controls_need_animation) {
    InitAnimationForHeightChange(&top_controls_animation_, new_top_ratio,
                                 top_target_ratio);
    if (old_browser_controls_params_.top_controls_min_height !=
        TopControlsMinHeight()) {
      top_controls_min_height_offset_ =
          old_browser_controls_params_.top_controls_min_height;
      top_min_height_change_in_progress_ = true;
      SetTopMinHeightOffsetAnimationRange(top_controls_min_height_offset_,
                                          TopControlsMinHeight());
    }
  } else {
    top_controls_min_height_offset_ = TopControlsMinHeight();
  }

  if (bottom_controls_need_animation) {
    InitAnimationForHeightChange(&bottom_controls_animation_, new_bottom_ratio,
                                 bottom_target_ratio);
    if (old_browser_controls_params_.bottom_controls_min_height !=
        BottomControlsMinHeight()) {
      bottom_controls_min_height_offset_ =
          old_browser_controls_params_.bottom_controls_min_height;

      int height_delta = BottomControlsHeight() - old_bottom_height;
      int min_height_delta =
          BottomControlsMinHeight() -
          old_browser_controls_params_.bottom_controls_min_height;
      // Currently, browser controls animate purely based on the change in the
      // height, not on the change in minHeight. This works fine when that
      // change is the same, but causes issues if the minHeight has been changed
      // by a different value than the height has. This is mitigated by
      // "stepping up" or "down" the starting min height offset such that the
      // effective change is the same for both the height and minHeight.
      if (min_height_delta > height_delta) {
        bottom_controls_min_height_offset_ += min_height_delta - height_delta;
      }

      bottom_min_height_change_in_progress_ = true;
      SetBottomMinHeightOffsetAnimationRange(bottom_controls_min_height_offset_,
                                             BottomControlsMinHeight());
    }
  } else {
    bottom_controls_min_height_offset_ = BottomControlsMinHeight();
  }

  // We won't run any animations if the controls are in an in-between state.
  // Examples: a show/hide animation is running, shown ratio is some value
  // between min-shown-ratio and 1 because of a scroll event.

  UpdateOldBrowserControlsParams();
  client_->SetCurrentBrowserControlsShownRatio(new_top_ratio, new_bottom_ratio);
}

void BrowserControlsOffsetManager::ScrollBegin() {
  if (pinch_gesture_active_)
    return;

  // If an animation to show the controls is in progress, re-order the animation
  // to start after the scroll completes.  This ensures that the user doesn't
  // accidentally hide the controls with a gesture that would not normally be
  // enough to hide them.
  show_controls_when_scroll_completes_ = IsAnimatingToShowControls();
  ResetAnimations();
  ResetBaseline();
}

gfx::Vector2dF BrowserControlsOffsetManager::ScrollBy(
    const gfx::Vector2dF& pending_delta) {
  // If one or both of the top/bottom controls are showing, the shown ratio
  // needs to be computed.
  if (!TopControlsHeight() && !BottomControlsHeight())
    return pending_delta;

  if (pinch_gesture_active_)
    return pending_delta;

  if ((permitted_state_ == BrowserControlsState::kShown &&
       pending_delta.y() > 0) ||
      (permitted_state_ == BrowserControlsState::kHidden &&
       pending_delta.y() < 0))
    return pending_delta;

  // Scroll the page up before expanding the browser controls if
  // OnlyExpandTopControlsAtPageTop() returns true.
  float viewport_offset_y = client_->ViewportScrollOffset().y();
  if (client_->OnlyExpandTopControlsAtPageTop() && pending_delta.y() < 0 &&
      viewport_offset_y > 0) {
    // Reset the baseline so the controls will immediately begin to scroll
    // once we're at the top.
    ResetBaseline();
    // Only scroll the controls by the amount remaining after the page contents
    // have been scrolled to the top.
    accumulated_scroll_delta_ =
        std::min(0.f, pending_delta.y() + viewport_offset_y);
  } else {
    accumulated_scroll_delta_ += pending_delta.y();
  }

  // We want to base our calculations on top or bottom controls. After consuming
  // the scroll delta, we will calculate a shown ratio for the controls. The
  // top controls have the priority because they need to visually be in sync
  // with the web contents.
  bool base_on_top_controls = TopControlsHeight();

  float old_top_offset = ContentTopOffset();
  float baseline_content_offset = base_on_top_controls
                                      ? baseline_top_content_offset_
                                      : baseline_bottom_content_offset_;
  // The top and bottom controls ratios can be calculated independently.
  // However, we want the (normalized) ratios to be equal when scrolling.
  // Having normalized ratios in this context means the top and bottom controls
  // reach the min and max ratios at the same time when scrolling or during
  // the usual show/hide animations, but they can have different shown ratios at
  // any time.
  float shown_ratio =
      (baseline_content_offset - accumulated_scroll_delta_) /
      (base_on_top_controls ? TopControlsHeight() : BottomControlsHeight());

  float min_ratio = base_on_top_controls ? TopControlsMinShownRatio()
                                         : BottomControlsMinShownRatio();
  float normalized_shown_ratio =
      (std::clamp(shown_ratio, min_ratio, 1.f) - min_ratio) / (1.f - min_ratio);
  // Even though the real shown ratios (shown height / total height) of the top
  // and bottom controls can be different, they share the same
  // relative/normalized ratio to keep them in sync.
  client_->SetCurrentBrowserControlsShownRatio(
      TopControlsMinShownRatio() +
          normalized_shown_ratio * (1.f - TopControlsMinShownRatio()),
      BottomControlsMinShownRatio() +
          normalized_shown_ratio * (1.f - BottomControlsMinShownRatio()));

  // If the controls are fully visible, treat the current position as the
  // new baseline even if the gesture didn't end.
  if (TopControlsShownRatio() == 1.f && BottomControlsShownRatio() == 1.f) {
    ResetBaseline();
    // Once the controls are fully visible, then any cancelled animation to show
    // them isn't relevant; the user definitely sees the controls and can decide
    // if they'd like to keep them.
    show_controls_when_scroll_completes_ = false;
  }

  ResetAnimations();

  // applied_delta will negate any scroll on the content if the top browser
  // controls are showing in favor of hiding the controls and resizing the
  // content. If the top controls have no height, the content should scroll
  // immediately.
  gfx::Vector2dF applied_delta(0.f, old_top_offset - ContentTopOffset());
  return pending_delta - applied_delta;
}

void BrowserControlsOffsetManager::ScrollEnd() {
  if (pinch_gesture_active_)
    return;

  // See if we should animate the top bar in, in case there was a race between
  // chrome showing the controls and the user performing a scroll. We only need
  // to animate the top control if it's not fully shown.
  if (show_controls_when_scroll_completes_ && TopControlsShownRatio() != 1.f) {
    SetupAnimation(AnimationDirection::kShowingControls);
    return;
  }

  StartAnimationIfNecessary();
}

void BrowserControlsOffsetManager::PinchBegin() {
  DCHECK(!pinch_gesture_active_);
  pinch_gesture_active_ = true;
  StartAnimationIfNecessary();
}

void BrowserControlsOffsetManager::PinchEnd() {
  DCHECK(pinch_gesture_active_);
  // Pinch{Begin,End} will always occur within the scope of Scroll{Begin,End},
  // so return to a state expected by the remaining scroll sequence.
  pinch_gesture_active_ = false;
  ScrollBegin();
}

gfx::Vector2dF BrowserControlsOffsetManager::Animate(
    base::TimeTicks monotonic_time) {
  if (!HasAnimation() || !client_->HaveRootScrollNode())
    return gfx::Vector2dF();

  float old_top_offset = ContentTopOffset();
  float old_bottom_offset = ContentBottomOffset();
  std::optional<float> new_top_ratio =
      top_controls_animation_.Tick(monotonic_time);
  if (!new_top_ratio.has_value())
    new_top_ratio = TopControlsShownRatio();
  std::optional<float> new_bottom_ratio =
      bottom_controls_animation_.Tick(monotonic_time);
  if (!new_bottom_ratio.has_value())
    new_bottom_ratio = BottomControlsShownRatio();

  client_->SetCurrentBrowserControlsShownRatio(new_top_ratio.value(),
                                               new_bottom_ratio.value());

  float top_offset_delta = ContentTopOffset() - old_top_offset;
  float bottom_offset_delta = ContentBottomOffset() - old_bottom_offset;

  if (top_min_height_change_in_progress_) {
    // The change in top offset may be larger than the min-height, resulting in
    // too low or too high |top_controls_min_height_offset_| values. So, we
    // should clamp it to a valid range.
    top_controls_min_height_offset_ =
        std::clamp(top_controls_min_height_offset_ + top_offset_delta,
                   top_min_height_offset_animation_range_->first,
                   top_min_height_offset_animation_range_->second);
    // Ticking the animation might reset it if it's at the final value.
    top_min_height_change_in_progress_ =
        top_controls_animation_.IsInitialized();
  }
  if (bottom_min_height_change_in_progress_) {
    // The change in bottom offset may be larger than the min-height, resulting
    // in too low or too high |bottom_controls_min_height_offset_| values. So,
    // we should clamp it to a valid range.
    bottom_controls_min_height_offset_ =
        std::clamp(bottom_controls_min_height_offset_ + bottom_offset_delta,
                   bottom_min_height_offset_animation_range_->first,
                   bottom_min_height_offset_animation_range_->second);
    // Ticking the animation might reset it if it's at the final value.
    bottom_min_height_change_in_progress_ =
        bottom_controls_animation_.IsInitialized();

    // When shrinking the bottom controls, there may be a remaining offset
    // mistake if the min height was decreased by more than the height was. This
    // can be fixed by simply "resetting" the offset to the final minHeight
    // value at the end of the animation. This only applies to shrinking
    // animations, since this adjustment happens at the beginning for growing
    // animations. This is done to avoid the bottom controls "lagging behind"
    // the changes to the web content and exposing a blank space right above the
    // bottom controls.
    if (!bottom_min_height_change_in_progress_) {
      bottom_controls_min_height_offset_ = BottomControlsMinHeight();
    }
  }

  gfx::Vector2dF scroll_delta(0.f, top_offset_delta);
  return scroll_delta;
}

bool BrowserControlsOffsetManager::HasAnimation() {
  return top_controls_animation_.IsInitialized() ||
         bottom_controls_animation_.IsInitialized();
}

void BrowserControlsOffsetManager::ResetAnimations() {
  // If the animation doesn't need to jump to the end, Animation::Reset() will
  // return |std::nullopt|.
  std::optional<float> top_ratio = top_controls_animation_.Reset();
  std::optional<float> bottom_ratio = bottom_controls_animation_.Reset();

  if (top_ratio.has_value() || bottom_ratio.has_value()) {
    client_->SetCurrentBrowserControlsShownRatio(
        top_ratio.has_value() ? top_ratio.value() : TopControlsShownRatio(),
        bottom_ratio.has_value() ? bottom_ratio.value()
                                 : BottomControlsShownRatio());
    if (top_min_height_change_in_progress_) {
      DCHECK(top_ratio.has_value());
      top_controls_min_height_offset_ = TopControlsMinHeight();
    }
    if (bottom_min_height_change_in_progress_) {
      DCHECK(bottom_ratio.has_value());
      bottom_controls_min_height_offset_ = BottomControlsMinHeight();
    }
  }
  top_min_height_change_in_progress_ = false;
  bottom_min_height_change_in_progress_ = false;
  top_min_height_offset_animation_range_.reset();
  bottom_min_height_offset_animation_range_.reset();
}

void BrowserControlsOffsetManager::SetupAnimation(
    AnimationDirection direction) {
  DCHECK_NE(AnimationDirection::kNoAnimation, direction);
  DCHECK(direction != AnimationDirection::kHidingControls ||
         TopControlsShownRatio() > 0.f);
  DCHECK(direction != AnimationDirection::kShowingControls ||
         TopControlsShownRatio() < 1.f);

  if (top_controls_animation_.IsInitialized() &&
      top_controls_animation_.Direction() == direction &&
      bottom_controls_animation_.IsInitialized() &&
      bottom_controls_animation_.Direction() == direction) {
    return;
  }

  if (!TopControlsHeight() && !BottomControlsHeight()) {
    float ratio = direction == AnimationDirection::kHidingControls ? 0.f : 1.f;
    client_->SetCurrentBrowserControlsShownRatio(ratio, ratio);
    return;
  }

  // Providing artificially larger/smaller stop ratios to make the animation
  // faster if the start ratio is closer to stop ratio.
  const float max_stop_ratio =
      direction == AnimationDirection::kShowingControls ? 1 : -1;
  float top_start_ratio = TopControlsShownRatio();
  float top_stop_ratio = top_start_ratio + max_stop_ratio;
  top_controls_animation_.Initialize(direction, top_start_ratio, top_stop_ratio,
                                     kShowHideMaxDurationMs,
                                     /*jump_to_end_on_reset=*/false);
  top_controls_animation_.SetBounds(TopControlsMinShownRatio(), 1.f);

  float bottom_start_ratio = BottomControlsShownRatio();
  float bottom_stop_ratio = bottom_start_ratio + max_stop_ratio;
  bottom_controls_animation_.Initialize(
      direction, bottom_start_ratio, bottom_stop_ratio, kShowHideMaxDurationMs,
      /*jump_to_end_on_reset=*/false);
  bottom_controls_animation_.SetBounds(BottomControlsMinShownRatio(), 1.f);

  client_->DidChangeBrowserControlsPosition();
}

void BrowserControlsOffsetManager::StartAnimationIfNecessary() {
  if ((TopControlsShownRatio() == TopControlsMinShownRatio() ||
       TopControlsShownRatio() == 1.f) &&
      (BottomControlsShownRatio() == BottomControlsMinShownRatio() ||
       BottomControlsShownRatio() == 1.f))
    return;

  float normalized_top_ratio =
      (TopControlsShownRatio() - TopControlsMinShownRatio()) /
      (1.f - TopControlsMinShownRatio());
  if (normalized_top_ratio >= 1.f - controls_hide_threshold_) {
    // If we're showing so much that the hide threshold won't trigger, show.
    SetupAnimation(AnimationDirection::kShowingControls);
  } else if (normalized_top_ratio <= controls_show_threshold_) {
    // If we're showing so little that the show threshold won't trigger, hide.
    SetupAnimation(AnimationDirection::kHidingControls);
  } else {
    // If we could be either showing or hiding, we determine which one to
    // do based on whether or not the total scroll delta was moving up or
    // down.
    SetupAnimation(accumulated_scroll_delta_ <= 0.f
                       ? AnimationDirection::kShowingControls
                       : AnimationDirection::kHidingControls);
  }
}

void BrowserControlsOffsetManager::ResetBaseline() {
  accumulated_scroll_delta_ = 0.f;
  baseline_top_content_offset_ = ContentTopOffset();
  baseline_bottom_content_offset_ = ContentBottomOffset();
}

void BrowserControlsOffsetManager::InitAnimationForHeightChange(
    Animation* animation,
    float start_ratio,
    float stop_ratio) {
  AnimationDirection direction = start_ratio < stop_ratio
                                     ? AnimationDirection::kShowingControls
                                     : AnimationDirection::kHidingControls;
  animation->Initialize(direction, start_ratio, stop_ratio,
                        kHeightChangeDurationMs, /*jump_to_end_on_reset=*/true);
}

float BrowserControlsOffsetManager::OldTopControlsMinShownRatio() {
  return old_browser_controls_params_.top_controls_height
             ? old_browser_controls_params_.top_controls_min_height /
                   old_browser_controls_params_.top_controls_height
             : 0.f;
}

float BrowserControlsOffsetManager::OldBottomControlsMinShownRatio() {
  return old_browser_controls_params_.bottom_controls_height
             ? old_browser_controls_params_.bottom_controls_min_height /
                   old_browser_controls_params_.bottom_controls_height
             : 0.f;
}

void BrowserControlsOffsetManager::UpdateOldBrowserControlsParams() {
  // No need to update the other two bool members as they aren't useful for this
  // class.
  old_browser_controls_params_.top_controls_height = TopControlsHeight();
  old_browser_controls_params_.top_controls_min_height = TopControlsMinHeight();
  old_browser_controls_params_.bottom_controls_height = BottomControlsHeight();
  old_browser_controls_params_.bottom_controls_min_height =
      BottomControlsMinHeight();
}

void BrowserControlsOffsetManager::SetTopMinHeightOffsetAnimationRange(
    float from,
    float to) {
  top_min_height_offset_animation_range_ =
      std::make_pair(std::min(from, to), std::max(from, to));
}

void BrowserControlsOffsetManager::SetBottomMinHeightOffsetAnimationRange(
    float from,
    float to) {
  bottom_min_height_offset_animation_range_ =
      std::make_pair(std::min(from, to), std::max(from, to));
}

double BrowserControlsOffsetManager::PredictViewportBoundsDelta(
    double current_bounds_delta,
    gfx::Vector2dF scroll_distance) {
  double adjustment = current_bounds_delta;
  if (scroll_distance.y() > 0 && adjustment > 0) {
    // We're scrolling down and started to hide controls. Let's assume they're
    // going to be fully hidden by the end of the fling.
    if (TopControlsShownRatio() < 1) {
      adjustment += ContentTopOffset();
    }
    if (BottomControlsShownRatio() < 1) {
      adjustment += ContentBottomOffset();
    }
  }
  if (scroll_distance.y() < 0 && adjustment < 0) {
    // We're scrolling up and started to show controls. Let's assume they're
    // going to be fully shown by the end of the fling.
    if (TopControlsShownRatio() > 0) {
      adjustment -= TopControlsHeight() - ContentTopOffset();
    }
    if (BottomControlsShownRatio() > 0) {
      adjustment -= BottomControlsHeight() - ContentBottomOffset();
    }
  }
  return adjustment;
}

// class Animation

BrowserControlsOffsetManager::Animation::Animation() {}

void BrowserControlsOffsetManager::Animation::Initialize(
    AnimationDirection direction,
    float start_value,
    float stop_value,
    int64_t duration,
    bool jump_to_end_on_reset) {
  direction_ = direction;
  start_value_ = start_value;
  stop_value_ = stop_value;
  duration_ = base::Milliseconds(duration);
  initialized_ = true;
  jump_to_end_on_reset_ = jump_to_end_on_reset;
  SetBounds(std::min(start_value_, stop_value_),
            std::max(start_value_, stop_value_));
}

std::optional<float> BrowserControlsOffsetManager::Animation::Tick(
    base::TimeTicks monotonic_time) {
  if (!IsInitialized())
    return std::nullopt;

  if (!started_) {
    start_time_ = monotonic_time;
    stop_time_ = start_time_ + duration_;
    started_ = true;
  }

  float value = gfx::Tween::ClampedFloatValueBetween(
      monotonic_time, start_time_, start_value_, stop_time_, stop_value_);

  if (IsComplete(value)) {
    value = FinalValue();
    Reset();
  }

  return value;
}

void BrowserControlsOffsetManager::Animation::SetBounds(float min, float max) {
  min_value_ = min;
  max_value_ = max;
}

std::optional<float> BrowserControlsOffsetManager::Animation::Reset() {
  auto ret =
      jump_to_end_on_reset_
          ? std::make_optional(std::clamp(stop_value_, min_value_, max_value_))
          : std::nullopt;

  started_ = false;
  initialized_ = false;
  start_time_ = base::TimeTicks();
  start_value_ = 0.f;
  stop_time_ = base::TimeTicks();
  stop_value_ = 0.f;
  direction_ = AnimationDirection::kNoAnimation;
  duration_ = base::TimeDelta();
  min_value_ = 0.f;
  max_value_ = 1.f;
  jump_to_end_on_reset_ = false;

  return ret;
}

bool BrowserControlsOffsetManager::Animation::IsComplete(float value) {
  return (direction_ == AnimationDirection::kShowingControls &&
          (value >= stop_value_ || value >= max_value_)) ||
         (direction_ == AnimationDirection::kHidingControls &&
          (value <= stop_value_ || value <= min_value_));
}

float BrowserControlsOffsetManager::Animation::FinalValue() {
  return std::clamp(stop_value_, min_value_, max_value_);
}

}  // namespace cc
