// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_timeline.h"

#include <memory>
#include <vector>

#include "cc/animation/animation_id_provider.h"
#include "cc/animation/worklet_animation.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

namespace {
bool IsVertical(ScrollTimeline::ScrollDirection direction) {
  return direction == ScrollTimeline::ScrollUp ||
         direction == ScrollTimeline::ScrollDown;
}

bool IsReverse(ScrollTimeline::ScrollDirection direction) {
  return direction == ScrollTimeline::ScrollUp ||
         direction == ScrollTimeline::ScrollLeft;
}

bool ValidateScrollOffsets(const std::vector<double>& scroll_offsets) {
  return scroll_offsets.empty() || scroll_offsets.size() >= 2.0;
}

}  // namespace

template double ComputeProgress<std::vector<double>>(
    double,
    const std::vector<double>&);

ScrollTimeline::ScrollTimeline(base::Optional<ElementId> scroller_id,
                               ScrollDirection direction,
                               const std::vector<double> scroll_offsets,
                               double time_range,
                               int animation_timeline_id)
    : AnimationTimeline(animation_timeline_id),
      pending_id_(scroller_id),
      direction_(direction),
      scroll_offsets_(scroll_offsets),
      time_range_(time_range) {
  DCHECK(ValidateScrollOffsets(scroll_offsets_));
}

ScrollTimeline::~ScrollTimeline() = default;

scoped_refptr<ScrollTimeline> ScrollTimeline::Create(
    base::Optional<ElementId> scroller_id,
    ScrollTimeline::ScrollDirection direction,
    const std::vector<double> scroll_offsets,
    double time_range) {
  return base::WrapRefCounted(
      new ScrollTimeline(scroller_id, direction, scroll_offsets, time_range,
                         AnimationIdProvider::NextTimelineId()));
}

scoped_refptr<AnimationTimeline> ScrollTimeline::CreateImplInstance() const {
  return base::WrapRefCounted(new ScrollTimeline(
      pending_id_, direction_, scroll_offsets_, time_range_, id()));
}

bool ScrollTimeline::IsActive(const ScrollTree& scroll_tree,
                              bool is_active_tree) const {
  // Blink passes empty scroll offsets when the timeline is inactive.
  if (scroll_offsets_.empty()) {
    return false;
  }
  // If pending tree with our scroller hasn't been activated, or the scroller
  // has been removed (e.g. if it is no longer composited).
  if ((is_active_tree && !active_id_) || (!is_active_tree && !pending_id_))
    return false;

  ElementId scroller_id =
      is_active_tree ? active_id_.value() : pending_id_.value();
  // The scroller is not in the ScrollTree if it is not currently scrollable
  // (e.g. has overflow: visible). In this case the timeline is not active.
  return scroll_tree.FindNodeFromElementId(scroller_id);
}

base::Optional<base::TimeTicks> ScrollTimeline::CurrentTime(
    const ScrollTree& scroll_tree,
    bool is_active_tree) const {
  // If the timeline is not active return unresolved value by the spec.
  // https://github.com/WICG/scroll-animations/issues/31
  // https://wicg.github.io/scroll-animations/#current-time-algorithm
  if (!IsActive(scroll_tree, is_active_tree))
    return base::nullopt;

  ElementId scroller_id =
      is_active_tree ? active_id_.value() : pending_id_.value();
  const ScrollNode* scroll_node =
      scroll_tree.FindNodeFromElementId(scroller_id);

  gfx::ScrollOffset offset =
      scroll_tree.GetPixelSnappedScrollOffset(scroll_node->id);
  DCHECK_GE(offset.x(), 0);
  DCHECK_GE(offset.y(), 0);

  gfx::ScrollOffset scroll_dimensions =
      scroll_tree.MaxScrollOffset(scroll_node->id);

  double max_offset =
      IsVertical(direction_) ? scroll_dimensions.y() : scroll_dimensions.x();
  double current_physical_offset =
      IsVertical(direction_) ? offset.y() : offset.x();
  double current_offset = IsReverse(direction_)
                              ? max_offset - current_physical_offset
                              : current_physical_offset;
  DCHECK_GE(max_offset, 0);
  DCHECK_GE(current_offset, 0);

  DCHECK_GE(scroll_offsets_.size(), 2u);
  double resolved_start_scroll_offset = scroll_offsets_[0];
  double resolved_end_scroll_offset =
      scroll_offsets_[scroll_offsets_.size() - 1];

  // TODO(crbug.com/1060384): Once the spec has been updated to state what the
  // expected result is when startScrollOffset >= endScrollOffset, we might need
  // to add a special case here. See
  // https://github.com/WICG/scroll-animations/issues/20

  // 3. If current scroll offset is less than startScrollOffset:
  if (current_offset < resolved_start_scroll_offset) {
    return base::TimeTicks();
  }

  // 4. If current scroll offset is greater than or equal to endScrollOffset:
  if (current_offset >= resolved_end_scroll_offset) {
    return base::TimeTicks() + base::TimeDelta::FromMillisecondsD(time_range_);
  }

  // 5. Return the result of evaluating the following expression:
  //   ((current scroll offset - startScrollOffset) /
  //      (endScrollOffset - startScrollOffset)) * effective time range
  return base::TimeTicks() + base::TimeDelta::FromMillisecondsD(
                                 ComputeProgress<std::vector<double>>(
                                     current_offset, scroll_offsets_) *
                                 time_range_);
}

void ScrollTimeline::PushPropertiesTo(AnimationTimeline* impl_timeline) {
  AnimationTimeline::PushPropertiesTo(impl_timeline);
  DCHECK(impl_timeline);
  ScrollTimeline* scroll_timeline = ToScrollTimeline(impl_timeline);
  scroll_timeline->pending_id_ = pending_id_;
  // TODO(smcgruer): This leads to incorrect behavior in the current design,
  // because we end up using the pending start/end scroll offset for the active
  // tree too. Instead we need to either split these (like pending_id_ and
  // active_id_) or have a ScrollTimeline per tree.
  scroll_timeline->scroll_offsets_ = scroll_offsets_;
  DCHECK(ValidateScrollOffsets(scroll_timeline->scroll_offsets_));
}

void ScrollTimeline::ActivateTimeline() {
  active_id_ = pending_id_;
  for (auto& kv : id_to_animation_map_) {
    auto& animation = kv.second;
    if (animation->IsWorkletAnimation())
      ToWorkletAnimation(animation.get())->ReleasePendingTreeLock();
  }
}

bool ScrollTimeline::TickScrollLinkedAnimations(
    const std::vector<scoped_refptr<Animation>>& ticking_animations,
    const ScrollTree& scroll_tree,
    bool is_active_tree) {
  base::Optional<base::TimeTicks> tick_time =
      CurrentTime(scroll_tree, is_active_tree);
  if (!tick_time)
    return false;

  bool animated = false;
  // This potentially iterates over all ticking animations multiple
  // times (# of ScrollTimeline * # of ticking_animations_).
  // The alternative we have considered here was to maintain a
  // ticking_animations_ list for each timeline but at the moment we
  // have opted to avoid this complexity in favor of simpler but less
  // efficient solution.
  for (auto& animation : ticking_animations) {
    if (animation->animation_timeline() != this)
      continue;
    // Worklet animations are ticked at a later stage.
    if (animation->IsWorkletAnimation())
      continue;

    if (!animation->IsScrollLinkedAnimation())
      continue;

    animation->Tick(tick_time.value());
    animated = true;
  }
  return animated;
}

void ScrollTimeline::UpdateScrollerIdAndScrollOffsets(
    base::Optional<ElementId> pending_id,
    const std::vector<double> scroll_offsets) {
  if (pending_id_ == pending_id && scroll_offsets_ == scroll_offsets) {
    return;
  }

  // When the scroller id changes it will first be modified in the pending tree.
  // Then later (when the pending tree is promoted to active)
  // |ActivateTimeline| will be called and will set the |active_id_|.
  pending_id_ = pending_id;
  scroll_offsets_ = scroll_offsets;
  DCHECK(ValidateScrollOffsets(scroll_offsets_));

  SetNeedsPushProperties();
}

bool ScrollTimeline::IsScrollTimeline() const {
  return true;
}

}  // namespace cc
