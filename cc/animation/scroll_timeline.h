// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_TIMELINE_H_
#define CC_ANIMATION_SCROLL_TIMELINE_H_

#include <vector>
#include "base/optional.h"
#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_model.h"
#include "cc/paint/element_id.h"

namespace cc {

class ScrollTree;

// A ScrollTimeline is an animation timeline that bases its current time on the
// progress of scrolling in some scroll container.
//
// This is the compositor-side representation of the web concept expressed in
// https://wicg.github.io/scroll-animations/#scrolltimeline-interface.
class CC_ANIMATION_EXPORT ScrollTimeline : public AnimationTimeline {
 public:
  // cc does not know about writing modes. The ScrollDirection below is
  // converted using blink::scroll_timeline_util::ConvertOrientation which takes
  // the spec-compliant ScrollDirection enumeration.
  // https://drafts.csswg.org/scroll-animations/#scrolldirection-enumeration
  enum ScrollDirection {
    ScrollUp,
    ScrollDown,
    ScrollLeft,
    ScrollRight,
  };

  ScrollTimeline(base::Optional<ElementId> scroller_id,
                 ScrollDirection direction,
                 const std::vector<double> scroll_offsets,
                 double time_range,
                 int animation_timeline_id);

  static scoped_refptr<ScrollTimeline> Create(
      base::Optional<ElementId> scroller_id,
      ScrollDirection direction,
      const std::vector<double> scroll_offsets,
      double time_range);

  // Create a copy of this ScrollTimeline intended for the impl thread in the
  // compositor.
  scoped_refptr<AnimationTimeline> CreateImplInstance() const override;

  // ScrollTimeline is active if the scroll node exists in active or pending
  // scroll tree.
  virtual bool IsActive(const ScrollTree& scroll_tree,
                        bool is_active_tree) const;

  // Calculate the current time of the ScrollTimeline. This is either a
  // base::TimeTicks value or base::nullopt if the current time is unresolved.
  // The internal calculations are performed using doubles and the result is
  // converted to base::TimeTicks. This limits the precision to 1us.
  virtual base::Optional<base::TimeTicks> CurrentTime(
      const ScrollTree& scroll_tree,
      bool is_active_tree) const;

  void UpdateScrollerIdAndScrollOffsets(
      base::Optional<ElementId> scroller_id,
      const std::vector<double> scroll_offsets);

  void PushPropertiesTo(AnimationTimeline* impl_timeline) override;
  void ActivateTimeline() override;

  bool TickScrollLinkedAnimations(
      const std::vector<scoped_refptr<Animation>>& ticking_animations,
      const ScrollTree& scroll_tree,
      bool is_active_tree) override;

  base::Optional<ElementId> GetActiveIdForTest() const { return active_id_; }
  base::Optional<ElementId> GetPendingIdForTest() const { return pending_id_; }
  ScrollDirection GetDirectionForTest() const { return direction_; }
  base::Optional<double> GetStartScrollOffsetForTest() const {
    if (scroll_offsets_.empty())
      return base::nullopt;
    return scroll_offsets_[0];
  }
  base::Optional<double> GetEndScrollOffsetForTest() const {
    if (scroll_offsets_.empty())
      return base::nullopt;
    return scroll_offsets_[1];
  }
  double GetTimeRangeForTest() const { return time_range_; }

  bool IsScrollTimeline() const override;

 protected:
  ~ScrollTimeline() override;

 private:
  // The scroller which this ScrollTimeline is based on. The same underlying
  // scroll source may have different ids in the pending and active tree (see
  // http://crbug.com/847588).
  base::Optional<ElementId> active_id_;
  base::Optional<ElementId> pending_id_;

  // The direction of the ScrollTimeline indicates which axis of the scroller
  // it should base its current time on, and where the origin point is.
  ScrollDirection direction_;

  // This defines scroll ranges of the scroller that the ScrollTimeline is
  // active within. If no ranges are defined the timeline is inactive.
  std::vector<double> scroll_offsets_;

  // A ScrollTimeline maps from the scroll offset in the scroller to a time
  // value based on a 'time range'. See the implementation of CurrentTime or the
  // spec for details.
  double time_range_;
};

inline ScrollTimeline* ToScrollTimeline(AnimationTimeline* timeline) {
  DCHECK(timeline->IsScrollTimeline());
  return static_cast<ScrollTimeline*>(timeline);
}

template <typename T>
double ComputeProgress(double current_offset, const T& resolved_offsets) {
  DCHECK_GE(resolved_offsets.size(), 2u);
  // When start offset is greater than end offset, current time is calculated
  // outside of this method.
  DCHECK(resolved_offsets[0] < resolved_offsets[resolved_offsets.size() - 1]);
  DCHECK(current_offset < resolved_offsets[resolved_offsets.size() - 1]);
  // Traverse scroll offsets from the back to find first interval that
  // contains the current offset. In case of overlapping offsets, last matching
  // interval in the list is used to calculate the current time. The rational
  // for choosing last matching offset is to be consistent with CSS property
  // overrides.
  unsigned int offset_id;
  for (offset_id = resolved_offsets.size() - 1;
       offset_id > 0 && !(resolved_offsets[offset_id - 1] <= current_offset &&
                          current_offset < resolved_offsets[offset_id]);
       offset_id--) {
  }
  DCHECK_GE(offset_id, 1u);
  // Weight of each offset within time range is distributed equally.
  double offset_distance = 1.0 / (resolved_offsets.size() - 1);
  // Progress of the current offset within its offset range.
  double p = (current_offset - resolved_offsets[offset_id - 1]) /
             (resolved_offsets[offset_id] - resolved_offsets[offset_id - 1]);
  return (offset_id - 1 + p) * offset_distance;
}

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_TIMELINE_H_
