// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animations_impl.h"

#include <utility>
#include <vector>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/element_animations.h"
#include "cc/animation/scroll_offset_animation_curve_factory.h"
#include "cc/base/features.h"
#include "cc/paint/element_id.h"
#include "ui/gfx/animation/keyframe/timing_function.h"

namespace cc {

ScrollOffsetAnimationImpl::ScrollOffsetAnimationImpl(AnimationHost* host)
    : animation_host_(host),
      scroll_offset_timeline_(
          AnimationTimeline::Create(AnimationIdProvider::NextTimelineId(),
                                    /* is_impl_only */ true)),
      scroll_offset_animation_(
          Animation::Create(AnimationIdProvider::NextAnimationId())) {
  scroll_offset_animation_->set_animation_delegate(this);
  animation_host_->AddAnimationTimeline(scroll_offset_timeline_.get());
  scroll_offset_timeline_->AttachAnimation(scroll_offset_animation_.get());
}

ScrollOffsetAnimationImpl::~ScrollOffsetAnimationImpl() {
  scroll_offset_timeline_->DetachAnimation(scroll_offset_animation_.get());
  animation_host_->RemoveAnimationTimeline(scroll_offset_timeline_.get());
  scroll_offset_animation_->set_animation_delegate(nullptr);
  scroll_offset_animation_.reset();
}

void ScrollOffsetAnimationImpl::AutoScrollAnimationCreate(
    ElementId element_id,
    const gfx::PointF& target_offset,
    const gfx::PointF& current_offset,
    float autoscroll_velocity,
    base::TimeDelta animation_start_offset) {
  std::unique_ptr<ScrollOffsetAnimationCurve> curve =
      ScrollOffsetAnimationCurveFactory::CreateAnimation(
          target_offset,
          ScrollOffsetAnimationCurveFactory::ScrollType::kAutoScroll);
  curve->SetInitialValue(current_offset, base::TimeDelta(),
                         autoscroll_velocity);
  ScrollAnimationCreateInternal(element_id, std::move(curve),
                                animation_start_offset);
  animation_is_autoscroll_ = true;
}

void ScrollOffsetAnimationImpl::MouseWheelScrollAnimationCreate(
    ElementId element_id,
    const gfx::PointF& target_offset,
    const gfx::PointF& current_offset,
    base::TimeDelta delayed_by,
    base::TimeDelta animation_start_offset) {
  std::unique_ptr<ScrollOffsetAnimationCurve> curve =
      ScrollOffsetAnimationCurveFactory::CreateAnimation(
          target_offset,
          ScrollOffsetAnimationCurveFactory::ScrollType::kMouseWheel);

  curve->SetInitialValue(current_offset, delayed_by);
  ScrollAnimationCreateInternal(element_id, std::move(curve),
                                animation_start_offset);
  animation_is_autoscroll_ = false;
}

void ScrollOffsetAnimationImpl::ScrollAnimationCreateInternal(
    ElementId element_id,
    std::unique_ptr<gfx::AnimationCurve> curve,
    base::TimeDelta animation_start_offset) {
  TRACE_EVENT_INSTANT1("cc", "ScrollAnimationCreate", TRACE_EVENT_SCOPE_THREAD,
                       "Duration", curve->Duration().InMillisecondsF());

  std::unique_ptr<KeyframeModel> keyframe_model = KeyframeModel::Create(
      std::move(curve), AnimationIdProvider::NextKeyframeModelId(),
      AnimationIdProvider::NextGroupId(),
      KeyframeModel::TargetPropertyId(TargetProperty::SCROLL_OFFSET));
  keyframe_model->set_time_offset(animation_start_offset);
  keyframe_model->SetIsImplOnly();

  DCHECK(scroll_offset_animation_);
  DCHECK(scroll_offset_animation_->animation_timeline());

  ReattachScrollOffsetAnimationIfNeeded(element_id);
  scroll_offset_animation_->AddKeyframeModel(std::move(keyframe_model));
}

std::optional<gfx::PointF>
ScrollOffsetAnimationImpl::ScrollAnimationUpdateTarget(
    const gfx::Vector2dF& scroll_delta,
    const gfx::PointF& max_scroll_offset,
    base::TimeTicks frame_monotonic_time,
    base::TimeDelta delayed_by) {
  DCHECK(scroll_offset_animation_);
  if (!scroll_offset_animation_->element_animations()) {
    TRACE_EVENT_INSTANT0("cc", "No element animation exists",
                         TRACE_EVENT_SCOPE_THREAD);
    return std::nullopt;
  }

  KeyframeModel* keyframe_model =
      scroll_offset_animation_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET);
  if (!keyframe_model) {
    scroll_offset_animation_->DetachElement();
    TRACE_EVENT_INSTANT0("cc", "No keyframe model exists",
                         TRACE_EVENT_SCOPE_THREAD);
    return std::nullopt;
  }

  ScrollOffsetAnimationCurve* curve =
      ScrollOffsetAnimationCurve::ToScrollOffsetAnimationCurve(
          keyframe_model->curve());
  if (scroll_delta.IsZero()) {
    return curve->target_value();
  }

  gfx::PointF new_target = curve->target_value() + scroll_delta;
  new_target.SetToMax(gfx::PointF());
  new_target.SetToMin(max_scroll_offset);

  // TODO(ymalik): KeyframeModel::TrimTimeToCurrentIteration should probably
  // check for run_state == KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY.
  base::TimeDelta trimmed =
      keyframe_model->run_state() ==
              KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY
          ? base::TimeDelta()
          : keyframe_model->TrimTimeToCurrentIteration(frame_monotonic_time);

  // Re-target taking the delay into account. Note that if the duration of the
  // animation is 0, trimmed will be 0 and UpdateTarget will be called with
  // t = -delayed_by.
  trimmed -= delayed_by;

  curve->UpdateTarget(trimmed, new_target);
  TRACE_EVENT_INSTANT1("cc", "ScrollAnimationUpdateTarget",
                       TRACE_EVENT_SCOPE_THREAD, "UpdatedDuration",
                       curve->Duration().InMillisecondsF());

  return curve->target_value();
}

void ScrollOffsetAnimationImpl::ScrollAnimationApplyAdjustment(
    ElementId element_id,
    const gfx::Vector2dF& adjustment) {
  DCHECK(scroll_offset_animation_);
  if (element_id != scroll_offset_animation_->element_id()) {
    TRACE_EVENT_INSTANT0("cc", "no scroll adjustment different element_ids",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  if (!scroll_offset_animation_->element_animations()) {
    TRACE_EVENT_INSTANT0("cc", "no scroll adjustment no element animation",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  KeyframeModel* keyframe_model =
      scroll_offset_animation_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET);
  if (!keyframe_model) {
    TRACE_EVENT_INSTANT0("cc", "no scroll adjustment no keyframe model",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  std::unique_ptr<ScrollOffsetAnimationCurve> new_curve =
      ScrollOffsetAnimationCurve::ToScrollOffsetAnimationCurve(
          keyframe_model->curve())
          ->CloneToScrollOffsetAnimationCurve();
  new_curve->ApplyAdjustment(adjustment);

  std::unique_ptr<KeyframeModel> new_keyframe_model = KeyframeModel::Create(
      std::move(new_curve), AnimationIdProvider::NextKeyframeModelId(),
      AnimationIdProvider::NextGroupId(),
      KeyframeModel::TargetPropertyId(TargetProperty::SCROLL_OFFSET));
  new_keyframe_model->set_start_time(keyframe_model->start_time());
  new_keyframe_model->SetIsImplOnly();
  new_keyframe_model->set_affects_active_elements(false);

  // Abort the old animation. AutoReset here will restore the current value of
  // animation_is_autoscroll_ after ScrollAnimationAbort resets it.
  base::AutoReset<bool> autoscroll(&animation_is_autoscroll_,
                                   animation_is_autoscroll_);
  ScrollAnimationAbort(/* needs_completion */ false);

  // Start a new one with the adjusment.
  scroll_offset_animation_->AddKeyframeModel(std::move(new_keyframe_model));
  TRACE_EVENT_INSTANT0("cc", "scroll animation adjusted",
                       TRACE_EVENT_SCOPE_THREAD);
}

void ScrollOffsetAnimationImpl::ScrollAnimationAbort(bool needs_completion) {
  DCHECK(scroll_offset_animation_);
  scroll_offset_animation_->AbortKeyframeModelsWithProperty(
      TargetProperty::SCROLL_OFFSET, needs_completion);
  TRACE_EVENT_INSTANT1("cc", "ScrollAnimationAbort", TRACE_EVENT_SCOPE_THREAD,
                       "needs_completion", needs_completion);
  animation_is_autoscroll_ = false;
}

void ScrollOffsetAnimationImpl::AnimatingElementRemovedByCommit() {
  scroll_offset_animation_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET)
      ->set_affects_pending_elements(false);
}

void ScrollOffsetAnimationImpl::NotifyAnimationFinished(
    base::TimeTicks monotonic_time,
    int target_property,
    int group) {
  DCHECK_EQ(target_property, TargetProperty::SCROLL_OFFSET);
  DCHECK(animation_host_->mutator_host_client());
  animation_host_->mutator_host_client()->ScrollOffsetAnimationFinished();
  TRACE_EVENT_INSTANT0("cc", "NotifyAnimationFinished",
                       TRACE_EVENT_SCOPE_THREAD);
}

bool ScrollOffsetAnimationImpl::IsAnimating() const {
  if (!scroll_offset_animation_->element_animations())
    return false;

  KeyframeModel* keyframe_model =
      scroll_offset_animation_->GetKeyframeModel(TargetProperty::SCROLL_OFFSET);
  if (!keyframe_model)
    return false;

  switch (keyframe_model->run_state()) {
    case KeyframeModel::WAITING_FOR_TARGET_AVAILABILITY:
    case KeyframeModel::STARTING:
    case KeyframeModel::RUNNING:
    case KeyframeModel::PAUSED:
      return true;
    case KeyframeModel::WAITING_FOR_DELETION:
    case KeyframeModel::FINISHED:
    case KeyframeModel::ABORTED:
    case KeyframeModel::ABORTED_BUT_NEEDS_COMPLETION:
      return false;
  }
}

bool ScrollOffsetAnimationImpl::IsAutoScrolling() const {
  return IsAnimating() && animation_is_autoscroll_;
}

ElementId ScrollOffsetAnimationImpl::GetElementId() const {
  return scroll_offset_animation_->element_id();
}

void ScrollOffsetAnimationImpl::ReattachScrollOffsetAnimationIfNeeded(
    ElementId element_id) {
  if (scroll_offset_animation_->element_id() != element_id) {
    if (scroll_offset_animation_->element_id()) {
      TRACE_EVENT_INSTANT0("cc", "scroll offset animation detached element",
                           TRACE_EVENT_SCOPE_THREAD);
      scroll_offset_animation_->DetachElement();
    }
    if (element_id) {
      TRACE_EVENT_INSTANT0("cc", "scroll offset animation attached element",
                           TRACE_EVENT_SCOPE_THREAD);
      scroll_offset_animation_->AttachElement(element_id);
    }
  }
}

ScrollOffsetAnimationsImpl::ScrollOffsetAnimationsImpl(
    AnimationHost* animation_host)
    : animation_host_(animation_host) {
  if (!features::MultiImplOnlyScrollAnimationsSupported()) {
    // If MultiImplOnlyScrollAnimations is not supported only one impl-only
    // scroll animation can be run at a time and it is managed through the
    // singleton instantiated here.
    scroll_offset_animation_ =
        std::make_unique<ScrollOffsetAnimationImpl>(animation_host_);
  }
}

ScrollOffsetAnimationsImpl::~ScrollOffsetAnimationsImpl() = default;

void ScrollOffsetAnimationsImpl::AutoScrollAnimationCreate(
    ElementId element_id,
    const gfx::PointF& target_offset,
    const gfx::PointF& current_offset,
    float autoscroll_velocity,
    base::TimeDelta animation_start_offset) {
  if (features::MultiImplOnlyScrollAnimationsSupported()) {
    element_to_animation_map_.insert(std::pair(
        element_id,
        std::make_unique<ScrollOffsetAnimationImpl>(animation_host_)));
    std::unique_ptr<ScrollOffsetAnimationImpl>& impl_animation =
        element_to_animation_map_.at(element_id);
    impl_animation->AutoScrollAnimationCreate(
        element_id, target_offset, current_offset, autoscroll_velocity,
        animation_start_offset);
  } else {
    DCHECK(scroll_offset_animation_);
    scroll_offset_animation_->AutoScrollAnimationCreate(
        element_id, target_offset, current_offset, autoscroll_velocity,
        animation_start_offset);
  }
}

void ScrollOffsetAnimationsImpl::MouseWheelScrollAnimationCreate(
    ElementId element_id,
    const gfx::PointF& target_offset,
    const gfx::PointF& current_offset,
    base::TimeDelta delayed_by,
    base::TimeDelta animation_start_offset) {
  if (features::MultiImplOnlyScrollAnimationsSupported()) {
    element_to_animation_map_.insert(std::pair(
        element_id,
        std::make_unique<ScrollOffsetAnimationImpl>(animation_host_)));
    std::unique_ptr<ScrollOffsetAnimationImpl>& impl_animation =
        element_to_animation_map_.at(element_id);
    impl_animation->MouseWheelScrollAnimationCreate(element_id, target_offset,
                                                    current_offset, delayed_by,
                                                    animation_start_offset);
  } else {
    DCHECK(scroll_offset_animation_);
    scroll_offset_animation_->MouseWheelScrollAnimationCreate(
        element_id, target_offset, current_offset, delayed_by,
        animation_start_offset);
  }
}

std::optional<gfx::PointF>
ScrollOffsetAnimationsImpl::ScrollAnimationUpdateTarget(
    const gfx::Vector2dF& scroll_delta,
    const gfx::PointF& max_scroll_offset,
    base::TimeTicks frame_monotonic_time,
    base::TimeDelta delayed_by,
    ElementId element_id) {
  if (features::MultiImplOnlyScrollAnimationsSupported()) {
    ScrollOffsetAnimationImpl* animation = GetScrollAnimation(element_id);
    DCHECK(animation);
    return animation->ScrollAnimationUpdateTarget(
        scroll_delta, max_scroll_offset, frame_monotonic_time, delayed_by);
  } else {
    DCHECK(scroll_offset_animation_);
    return scroll_offset_animation_->ScrollAnimationUpdateTarget(
        scroll_delta, max_scroll_offset, frame_monotonic_time, delayed_by);
  }
}

void ScrollOffsetAnimationsImpl::ScrollAnimationApplyAdjustment(
    ElementId element_id,
    const gfx::Vector2dF& adjustment) {
  if (features::MultiImplOnlyScrollAnimationsSupported()) {
    if (ScrollOffsetAnimationImpl* animation = GetScrollAnimation(element_id)) {
      animation->ScrollAnimationApplyAdjustment(element_id, adjustment);
    }
  } else {
    DCHECK(scroll_offset_animation_);
    return scroll_offset_animation_->ScrollAnimationApplyAdjustment(element_id,
                                                                    adjustment);
  }
}

void ScrollOffsetAnimationsImpl::ScrollAnimationAbort(bool needs_completion,
                                                      ElementId element_id) {
  if (features::MultiImplOnlyScrollAnimationsSupported()) {
    if (ScrollOffsetAnimationImpl* animation = GetScrollAnimation(element_id)) {
      animation->ScrollAnimationAbort(needs_completion);
    }
  } else {
    DCHECK(scroll_offset_animation_);
    scroll_offset_animation_->ScrollAnimationAbort(needs_completion);
  }
}

void ScrollOffsetAnimationsImpl::HandleRemovedScrollAnimatingElements(
    bool commits_to_active) {
  if (features::MultiImplOnlyScrollAnimationsSupported()) {
    std::vector<ElementId> deleted;
    for (auto& entry : element_to_animation_map_) {
      ElementId element_id = entry.first;
      if (!animation_host_->IsElementInPropertyTrees(element_id,
                                                     commits_to_active)) {
        // We probably shouldn't need to check IsAnimating here,
        // but some bots recycle AnimationHost between tests
        // which seems to lead to referencing Animations with null
        // KeyframeModels. Checking IsAnimating guards against this and also
        // matches what was done pre-MultiImplOnlyScrollAnimationsSupported.
        if (entry.second->IsAnimating()) {
          entry.second->AnimatingElementRemovedByCommit();
        }
        deleted.push_back(element_id);
      }
    }

    for (auto& entry : deleted) {
      element_to_animation_map_.erase(entry);
    }
  } else {
    DCHECK(scroll_offset_animation_);
    if (scroll_offset_animation_->IsAnimating()) {
      if (!animation_host_->IsElementInPropertyTrees(
              scroll_offset_animation_->GetElementId(), commits_to_active)) {
        scroll_offset_animation_->AnimatingElementRemovedByCommit();
      }
    }
  }
}

bool ScrollOffsetAnimationsImpl::ElementHasImplOnlyScrollAnimation(
    ElementId element_id) const {
  if (features::MultiImplOnlyScrollAnimationsSupported()) {
    ScrollOffsetAnimationImpl* impl_animation = GetScrollAnimation(element_id);
    return impl_animation ? impl_animation->IsAnimating() : false;
  } else {
    DCHECK(scroll_offset_animation_);
    return scroll_offset_animation_->GetElementId() == element_id &&
           scroll_offset_animation_->IsAnimating();
  }
}

bool ScrollOffsetAnimationsImpl::HasImplOnlyScrollAnimatingElement() const {
  if (features::MultiImplOnlyScrollAnimationsSupported()) {
    for (auto& entry : element_to_animation_map_) {
      if (entry.second->IsAnimating()) {
        return true;
      }
    }
    return false;
  } else {
    DCHECK(scroll_offset_animation_);
    return scroll_offset_animation_ ? scroll_offset_animation_->IsAnimating()
                                    : false;
  }
}

bool ScrollOffsetAnimationsImpl::HasImplOnlyAutoScrollAnimatingElement() const {
  if (features::MultiImplOnlyScrollAnimationsSupported()) {
    for (auto& entry : element_to_animation_map_) {
      if (entry.second->IsAutoScrolling()) {
        return true;
      }
    }
    return false;
  } else {
    DCHECK(scroll_offset_animation_);
    return scroll_offset_animation_
               ? scroll_offset_animation_->IsAutoScrolling()
               : false;
  }
}

ElementId ScrollOffsetAnimationsImpl::GetElementId() const {
  DCHECK(!features::MultiImplOnlyScrollAnimationsSupported());
  return scroll_offset_animation_->GetElementId();
}

ScrollOffsetAnimationImpl* ScrollOffsetAnimationsImpl::GetScrollAnimation(
    ElementId element_id) const {
  DCHECK(features::MultiImplOnlyScrollAnimationsSupported());
  auto iter = element_to_animation_map_.find(element_id);
  if (iter != element_to_animation_map_.end()) {
    return iter->second.get();
  }
  return nullptr;
}

}  // namespace cc
