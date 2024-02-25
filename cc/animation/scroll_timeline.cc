// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_timeline.h"

#include <memory>
#include <vector>

#include "cc/animation/animation_id_provider.h"
#include "cc/animation/worklet_animation.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "ui/gfx/geometry/point_f.h"
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

}  // namespace

ScrollTimeline::ScrollTimeline(std::optional<ElementId> scroller_id,
                               ScrollDirection direction,
                               std::optional<ScrollOffsets> scroll_offsets,
                               int animation_timeline_id)
    : AnimationTimeline(animation_timeline_id, /* is_impl_only */ false),
      pending_id_(scroller_id),
      direction_(direction),
      pending_offsets_(scroll_offsets) {}

ScrollTimeline::~ScrollTimeline() = default;

scoped_refptr<ScrollTimeline> ScrollTimeline::Create(
    std::optional<ElementId> scroller_id,
    ScrollTimeline::ScrollDirection direction,
    std::optional<ScrollOffsets> scroll_offsets) {
  return base::WrapRefCounted(
      new ScrollTimeline(scroller_id, direction, scroll_offsets,
                         AnimationIdProvider::NextTimelineId()));
}

scoped_refptr<AnimationTimeline> ScrollTimeline::CreateImplInstance() const {
  return base::WrapRefCounted(
      new ScrollTimeline(pending_id(), direction(), pending_offsets(), id()));
}

bool ScrollTimeline::IsActive(const ScrollTree& scroll_tree,
                              bool is_active_tree) const {
  // Blink passes empty scroll offsets when the timeline is inactive.
  if ((is_active_tree && !active_offsets()) ||
      (!is_active_tree && !pending_offsets())) {
    return false;
  }

  // If pending tree with our scroller hasn't been activated, or the scroller
  // has been removed (e.g. if it is no longer composited).
  if ((is_active_tree && !active_id()) || (!is_active_tree && !pending_id())) {
    return false;
  }

  ElementId scroller_id =
      is_active_tree ? active_id().value() : pending_id().value();
  // The scroller is not in the ScrollTree if it is not currently scrollable
  // (e.g. has overflow: visible). In this case the timeline is not active.
  return scroll_tree.FindNodeFromElementId(scroller_id);
}

// https://drafts.csswg.org/scroll-animations-1/#current-time-algorithm
std::optional<base::TimeTicks> ScrollTimeline::CurrentTime(
    const ScrollTree& scroll_tree,
    bool is_active_tree) const {
  // If the timeline is not active return unresolved value by the spec.
  // https://github.com/WICG/scroll-animations/issues/31
  // https://wicg.github.io/scroll-animations/#current-time-algorithm
  if (!IsActive(scroll_tree, is_active_tree)) {
    return std::nullopt;
  }

  ElementId scroller_id =
      is_active_tree ? active_id().value() : pending_id().value();
  const ScrollNode* scroll_node =
      scroll_tree.FindNodeFromElementId(scroller_id);

  gfx::PointF offset =
      scroll_tree.GetScrollOffsetForScrollTimeline(*scroll_node);
  DCHECK_GE(offset.x(), 0);
  DCHECK_GE(offset.y(), 0);

  gfx::PointF scroll_dimensions = scroll_tree.MaxScrollOffset(scroll_node->id);

  double max_offset =
      IsVertical(direction()) ? scroll_dimensions.y() : scroll_dimensions.x();
  double current_physical_offset =
      IsVertical(direction()) ? offset.y() : offset.x();
  double current_offset = IsReverse(direction())
                              ? max_offset - current_physical_offset
                              : current_physical_offset;
  DCHECK_GE(max_offset, 0);
  DCHECK_GE(current_offset, 0);

  double start_offset = 0;
  if (is_active_tree) {
    DCHECK(active_offsets());
    start_offset = active_offsets()->start;
  } else {
    DCHECK(pending_offsets());
    start_offset = pending_offsets()->start;
  }

  int64_t progress_us = base::ClampRound((current_offset - start_offset) *
                                         kScrollTimelineMicrosecondsPerPixel);
  return base::TimeTicks() + base::Microseconds(progress_us);
}

std::optional<base::TimeTicks> ScrollTimeline::Duration(
    const ScrollTree& scroll_tree,
    bool is_active_tree) const {
  double start_offset = 0;
  double end_offset = 0;
  if (is_active_tree) {
    DCHECK(active_offsets());
    start_offset = active_offsets()->start;
    end_offset = active_offsets()->end;
  } else {
    DCHECK(pending_offsets());
    start_offset = pending_offsets()->start;
    end_offset = pending_offsets()->end;
  }
  int64_t duration_us = base::ClampRound((end_offset - start_offset) *
                                         kScrollTimelineMicrosecondsPerPixel);
  return base::TimeTicks() + base::Microseconds(duration_us);
}

void ScrollTimeline::PushPropertiesTo(AnimationTimeline* impl_timeline) {
  AnimationTimeline::PushPropertiesTo(impl_timeline);
  DCHECK(impl_timeline);
  ScrollTimeline* scroll_timeline = ToScrollTimeline(impl_timeline);
  scroll_timeline->pending_id_.Write(*this) = pending_id_.Read(*this);
  scroll_timeline->pending_offsets_.Write(*this) = pending_offsets_.Read(*this);
}

void ScrollTimeline::ActivateTimeline() {
  active_id_.Write(*this) = pending_id_.Read(*this);
  active_offsets_.Write(*this) = pending_offsets_.Read(*this);
  for (auto& kv : id_to_animation_map_.Write(*this)) {
    auto& animation = kv.second;
    if (animation->IsWorkletAnimation())
      ToWorkletAnimation(animation.get())->ReleasePendingTreeLock();
  }
}

bool ScrollTimeline::TickScrollLinkedAnimations(
    const std::vector<scoped_refptr<Animation>>& ticking_animations,
    const ScrollTree& scroll_tree,
    bool is_active_tree) {
  std::optional<base::TimeTicks> tick_time =
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
    std::optional<ElementId> pending_id,
    std::optional<ScrollOffsets> pending_offsets) {
  if (pending_id_.Read(*this) == pending_id &&
      pending_offsets_.Read(*this) == pending_offsets) {
    return;
  }

  // When the scroller id changes it will first be modified in the pending tree.
  // Then later (when the pending tree is promoted to active)
  // |ActivateTimeline| will be called and will set the |active_id_|.
  pending_id_.Write(*this) = pending_id;
  pending_offsets_.Write(*this) = pending_offsets;

  SetNeedsPushProperties();
}

bool ScrollTimeline::IsScrollTimeline() const {
  return true;
}

bool ScrollTimeline::IsLinkedToScroller(ElementId scroller) const {
  auto& id = active_id();
  return id && id.value() == scroller;
}

}  // namespace cc
