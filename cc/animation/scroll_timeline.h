// Copyright 2017 The Chromium Authors
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

  struct ScrollOffsets {
    ScrollOffsets(double start_offset, double end_offset) {
      start = start_offset;
      end = end_offset;
    }
    bool operator==(const ScrollOffsets& other) const {
      return start == other.start && end == other.end;
    }
    bool operator!=(const ScrollOffsets& other) const {
      return !(*this == other);
    }

    double start = 0;
    double end = 0;
  };

  // 100% is represented as 100s or 100000ms. We store it here in Milliseconds
  // because that is the time unit returned by functions like CurrentTime.
  static constexpr double kScrollTimelineDurationMs = 100000;

  ScrollTimeline(absl::optional<ElementId> scroller_id,
                 ScrollDirection direction,
                 absl::optional<ScrollOffsets> scroll_offsets,
                 int animation_timeline_id);

  static scoped_refptr<ScrollTimeline> Create(
      absl::optional<ElementId> scroller_id,
      ScrollDirection direction,
      absl::optional<ScrollOffsets> scroll_offsets);

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
      absl::optional<ScrollOffsets> scroll_offsets);

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
    if (!pending_offsets_)
      return absl::nullopt;
    return pending_offsets_->start;
  }
  absl::optional<double> GetEndScrollOffsetForTest() const {
    if (!pending_offsets_)
      return absl::nullopt;
    return pending_offsets_->end;
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

  absl::optional<ScrollOffsets> pending_offsets_;
  absl::optional<ScrollOffsets> active_offsets_;
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

}  // namespace cc

#endif  // CC_ANIMATION_SCROLL_TIMELINE_H_
