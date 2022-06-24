// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_SCROLL_TIMELINE_H_
#define CC_ANIMATION_SCROLL_TIMELINE_H_

#include <vector>
#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_model.h"
#include "cc/paint/element_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // 100% is represented as 100s or 100000ms. We store it here in Milliseconds
  // because that is the time unit returned by functions like CurrentTime.
  static constexpr double kScrollTimelineDurationMs = 100000;

  ScrollTimeline(absl::optional<ElementId> scroller_id,
                 ScrollDirection direction,
                 const std::vector<double> scroll_offsets,
                 int animation_timeline_id);

  static scoped_refptr<ScrollTimeline> Create(
      absl::optional<ElementId> scroller_id,
      ScrollDirection direction,
      const std::vector<double> scroll_offsets);

  // Create a copy of this ScrollTimeline intended for the impl thread in the
  // compositor.
  scoped_refptr<AnimationTimeline> CreateImplInstance() const override;

  // ScrollTimeline is active if the scroll node exists in active or pending
  // scroll tree.
  virtual bool IsActive(const ScrollTree& scroll_tree,
                        bool is_active_tree) const;

  // Calculate the current time of the ScrollTimeline. This is either a
  // base::TimeTicks value or absl::nullopt if the current time is unresolved.
  // The internal calculations are performed using doubles and the result is
  // converted to base::TimeTicks. This limits the precision to 1us.
  virtual absl::optional<base::TimeTicks> CurrentTime(
      const ScrollTree& scroll_tree,
      bool is_active_tree) const;

  void UpdateScrollerIdAndScrollOffsets(
      absl::optional<ElementId> scroller_id,
      const std::vector<double> scroll_offsets);

  void PushPropertiesTo(AnimationTimeline* impl_timeline) override;
  void ActivateTimeline() override;

  bool TickScrollLinkedAnimations(
      const std::vector<scoped_refptr<Animation>>& ticking_animations,
      const ScrollTree& scroll_tree,
      bool is_active_tree) override;

  absl::optional<ElementId> GetActiveIdForTest() const { return active_id_; }
  absl::optional<ElementId> GetPendingIdForTest() const { return pending_id_; }
  ScrollDirection GetDirectionForTest() const { return direction_; }
  absl::optional<double> GetStartScrollOffsetForTest() const {
    if (scroll_offsets_.empty())
      return absl::nullopt;
    return scroll_offsets_[0];
  }
  absl::optional<double> GetEndScrollOffsetForTest() const {
    if (scroll_offsets_.empty())
      return absl::nullopt;
    return scroll_offsets_[1];
  }

  bool IsScrollTimeline() const override;

 protected:
  ~ScrollTimeline() override;

 private:
  // The scroller which this ScrollTimeline is based on. The same underlying
  // scroll source may have different ids in the pending and active tree (see
  // http://crbug.com/847588).
  absl::optional<ElementId> active_id_;
  absl::optional<ElementId> pending_id_;

  // The direction of the ScrollTimeline indicates which axis of the scroller
  // it should base its current time on, and where the origin point is.
  ScrollDirection direction_;

  // This defines scroll ranges of the scroller that the ScrollTimeline is
  // active within. If no ranges are defined the timeline is inactive.
  std::vector<double> scroll_offsets_;
};

inline ScrollTimeline* ToScrollTimeline(AnimationTimeline* timeline) {
  DCHECK(timeline->IsScrollTimeline());
  return static_cast<ScrollTimeline*>(timeline);
}

inline const ScrollTimeline* ToScrollTimeline(
    const AnimationTimeline* timeline) {
  DCHECK(timeline->IsScrollTimeline());
  return static_cast<const ScrollTimeline*>(timeline);
}

// https://drafts.csswg.org/scroll-animations-1/#progress-calculation-algorithm
template <typename T>
double ComputeProgress(double current_offset, const T& resolved_offsets) {
  // 1. Let scroll offsets be the result of applying the procedure to resolve
  // scroll timeline offsets for scrollOffsets.
  DCHECK_GE(resolved_offsets.size(), 2u);
  // When start offset is greater than end offset, current time is calculated
  // outside of this method.
  DCHECK_LT(resolved_offsets[0], resolved_offsets[resolved_offsets.size() - 1]);
  // When animation is in before or after phase, current time is calculated
  // outside of this method.
  DCHECK_GE(current_offset, resolved_offsets[0]);
  DCHECK_LT(current_offset, resolved_offsets[resolved_offsets.size() - 1]);
  // Traverse scroll offsets from the back to find first interval that
  // contains the current offset. In case of overlapping offsets, last matching
  // interval in the list is used to calculate the current time. The rational
  // for choosing last matching offset is to be consistent with CSS property
  // overrides.

  // 2. Let offset index correspond to the position of the last offset in scroll
  // offsets whose value is less than or equal to offset and the value at the
  // following position greater than offset
  int offset_index;
  for (offset_index = resolved_offsets.size() - 2;
       offset_index > 0 && resolved_offsets[offset_index] > current_offset;
       offset_index--) {
    DCHECK_LT(current_offset, resolved_offsets[offset_index + 1]);
  }
  // 3. Let start offset be the offset value at position offset index in
  // scroll offsets.
  double start_offset = resolved_offsets[offset_index];
  // 4. Let end offset be the value of next offset in scroll offsets after
  // start offset.
  double end_offset = resolved_offsets[offset_index + 1];
  // 5. Let size be the number of offsets in scroll offsets.
  unsigned int size = resolved_offsets.size();
  // 6. Let offset weight be the result of evaluating 1 / (size - 1).
  double offset_weight = 1.0 / (size - 1);
  // 7. Let interval progress be the result of evaluating
  // (offset - start offset) / (end offset - start offset).
  double interval_progress =
      (current_offset - start_offset) / (end_offset - start_offset);
  // 8. Return the result of evaluating
  // (offset index + interval progress) × offset weight.
  return (offset_index + interval_progress) * offset_weight;
}

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_TIMELINE_H_
