// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_SNAP_DATA_H_
#define CC_INPUT_SCROLL_SNAP_DATA_H_

#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "cc/cc_export.h"
#include "cc/paint/element_id.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/range/range_f.h"

namespace cc {

class SnapSelectionStrategy;

// See https://www.w3.org/TR/css-scroll-snap-1/#snap-axis
enum class SnapAxis : unsigned {
  kBoth,
  kX,
  kY,
  kBlock,
  kInline,
};

// A helper enum to specify the the axis when doing calculations.
enum class SearchAxis : unsigned { kX, kY };

// See https://www.w3.org/TR/css-scroll-snap-1/#snap-strictness
enum class SnapStrictness : unsigned { kProximity, kMandatory };

// See https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-align
enum class SnapAlignment : unsigned { kNone, kStart, kEnd, kCenter };

struct ScrollSnapType {
  ScrollSnapType()
      : is_none(true),
        axis(SnapAxis::kBoth),
        strictness(SnapStrictness::kProximity) {}

  ScrollSnapType(bool snap_type_none, SnapAxis axis, SnapStrictness strictness)
      : is_none(snap_type_none), axis(axis), strictness(strictness) {}

  bool operator==(const ScrollSnapType& other) const {
    return is_none == other.is_none && axis == other.axis &&
           strictness == other.strictness;
  }

  bool operator!=(const ScrollSnapType& other) const {
    return !(*this == other);
  }

  // Represents whether the scroll-snap-type is none.
  bool is_none;

  SnapAxis axis;
  SnapStrictness strictness;
};

struct ScrollSnapAlign {
  ScrollSnapAlign()
      : alignment_block(SnapAlignment::kNone),
        alignment_inline(SnapAlignment::kNone) {}

  explicit ScrollSnapAlign(SnapAlignment alignment)
      : alignment_block(alignment), alignment_inline(alignment) {}

  ScrollSnapAlign(SnapAlignment b, SnapAlignment i)
      : alignment_block(b), alignment_inline(i) {}

  bool operator==(const ScrollSnapAlign& other) const {
    return alignment_block == other.alignment_block &&
           alignment_inline == other.alignment_inline;
  }

  bool operator!=(const ScrollSnapAlign& other) const {
    return !(*this == other);
  }

  SnapAlignment alignment_block;
  SnapAlignment alignment_inline;
};

// This struct represents a snap area that is considered to be a viable
// alternative to the snap area that was selected for the associated
// SnapSearchResult.
// The snap area is a viable alternative because it:
// - is a snap target in both axes, and
// - is aligned with its associated SnapSearchResult's snap area in the
//     main axis of that SnapSearchResult.
// This alternative may be considered to be a better choice if it is also
// aligned with the SnapSearchResult of the cross axis.
struct SnapSearchResultAlternative {
  explicit SnapSearchResultAlternative(const ElementId id,
                                       gfx::RectF rect,
                                       float area_cross_axis_snap_offset) {
    element_id = id;
    area_rect = rect;
    cross_axis_snap_offset = area_cross_axis_snap_offset;
  }
  // The ElementId of the snap area considered to be a viable alternative to
  // the snap area selected for the associated SnapSearchResult.
  ElementId element_id;

  // The rect of the snap area considered to be a viable alternative to
  // the snap area selected for the associated SnapSearchResult relative to
  // its snap container.
  gfx::RectF area_rect;

  // The offset in the cross axis of the associated SnapSearchResult at which
  // the snapport of the snap container is aligned with the snap area
  // represented by this SnapSearchResultAlternative.
  float cross_axis_snap_offset;
};

// This class includes snap offset and visible range needed to perform a snap
// operation on one axis for a specific area. The data can be used to determine
// whether this snap area provides a valid snap position for the current scroll.
class SnapSearchResult {
 public:
  SnapSearchResult() {}
  SnapSearchResult(float offset,
                   SearchAxis axis,
                   gfx::RangeF snapport_visible_range,
                   float max_visible);
  // Clips the |snap_offset| between 0 and |max_snap|.
  void Clip(float max_snap);

  // Union the rect of the two SnapSearchResult if they represent two snap areas
  // that are both covering the snapport at the current offset. The
  // |element_id_| of this is arbitrarily chosen because both snap areas cover
  // the snapport and are therefore both valid.
  void Union(const SnapSearchResult& other);

  float snap_offset() const { return snap_offset_; }
  void set_snap_offset(float offset) { snap_offset_ = offset; }

  // |visible_range()| returns the range of scroll positions at which the area
  // generating this SnapSearchResult intersects (i.e is visible within)
  // its snapport in the cross axis for this result.
  gfx::RangeF visible_range() const {
    if (!rect_) {
      return gfx::RangeF(0, snapport_max_visible_);
    }
    const float rect_start =
        axis_ == SearchAxis::kX ? rect_.value().y() : rect_.value().x();
    const float rect_end = axis_ == SearchAxis::kX ? rect_.value().bottom()
                                                   : rect_.value().right();
    return gfx::RangeF(std::clamp(rect_start - snapport_visible_range_.end(),
                                  0.0f, snapport_max_visible_),
                       std::clamp(rect_end - snapport_visible_range_.start(),
                                  0.0f, snapport_max_visible_));
  }

  ElementId element_id() const { return element_id_; }
  void set_element_id(ElementId id) { element_id_ = id; }

  std::optional<gfx::RangeF> covered_range() const { return covered_range_; }
  void set_covered_range(const gfx::RangeF& range) { covered_range_ = range; }

  bool has_focus_within() const { return has_focus_within_; }
  void set_has_focus_within(bool has_focus_within) {
    has_focus_within_ = has_focus_within;
  }

  SearchAxis axis() const { return axis_; }
  void set_axis(const SearchAxis& axis) { axis_ = axis; }
  void set_snapport_visible_range(const gfx::RangeF& range) {
    snapport_visible_range_ = range;
  }
  void set_snapport_max_visible(float position) {
    snapport_max_visible_ = position;
  }

  std::optional<gfx::RectF> rect() const { return rect_; }
  void set_rect(const gfx::RectF& rect) { rect_ = rect; }

  std::optional<SnapSearchResultAlternative> alternative() const {
    return alternative_;
  }
  void set_alternative(const ElementId& id,
                       const gfx::RectF& rect,
                       float alt_cross_snap_offset) {
    alternative_ = SnapSearchResultAlternative(id, rect, alt_cross_snap_offset);
  }

 private:
  // Scroll offset corresponding to this snap position. If covered_range_ is set
  // then this will be a position inside the range. In the covered case, the
  // result from FindClosestValidArea has a snap_offset_ equal to the
  // intended_position() of the SnapSelectionStrategy.
  // TODO(crbug.com/40278621): With refactoring it may be possible to replace
  // snap_offset_ and covered_range_ with a single range field with start == end
  // for "aligned" snap positions.
  float snap_offset_;

  // The ElementId of the snap area that corresponds to this SnapSearchResult.
  ElementId element_id_;

  // Whether the snap area generating this result has focus or has a descendant
  // element which has focus.
  bool has_focus_within_;

  // This is set if the validity of this result derives from the fact that the
  // snap area covers the viewport, as described in the spec section on
  // "Snapping Boxes that Overflow the Scrollport":
  // https://drafts.csswg.org/css-scroll-snap-1/#snap-overflow
  //
  // If set, indicates the range of scroll offsets for which the snap area
  // covers the viewport. The snap_offset_ will be a point within this range.
  std::optional<gfx::RangeF> covered_range_;

  // The axis for which the result was generated.
  SearchAxis axis_;

  // The range (in the cross axis of this result) of the rect of the snap
  // container which snaps to the area generating this search result .
  gfx::RangeF snapport_visible_range_;

  // The max scroll offset (in the cross axis of this result) of the snap
  // container which snaps to the area generating this search result .
  float snapport_max_visible_;

  // This is the rect of the SnapArea generating this result relative to its
  // snap container.
  std::optional<gfx::RectF> rect_;

  // This represents a snap area that is aligned with the snap area generating
  // this result. We may end up selecting the alternative if it turns out to
  // also be an aligned candidate in the cross axis.
  std::optional<SnapSearchResultAlternative> alternative_;
};

// Snap area is a bounding box that could be snapped to when a scroll happens in
// its scroll container.
// This data structure describes the data needed for SnapCoordinator if we want
// to snap to this snap area.
struct SnapAreaData {
  // kInvalidScrollOffset is used to mark that the snap_position on a specific
  // axis is not applicable, thus should not be considered when snapping on that
  // axis. This is because the snap area has SnapAlignmentNone on that axis.
  static const int kInvalidScrollPosition = -1;

  SnapAreaData() {}

  SnapAreaData(const ScrollSnapAlign& align,
               const gfx::RectF& rec,
               bool msnap,
               bool has_focus_within,
               ElementId id)
      : scroll_snap_align(align),
        rect(rec),
        must_snap(msnap),
        has_focus_within(has_focus_within),
        element_id(id) {}

  bool operator==(const SnapAreaData& other) const {
    return (other.element_id == element_id) &&
           (other.scroll_snap_align == scroll_snap_align) &&
           (other.rect == rect) && (other.must_snap == must_snap) &&
           (other.has_focus_within == has_focus_within);
  }

  bool operator!=(const SnapAreaData& other) const { return !(*this == other); }

  // Specifies how the snap area should be aligned with its snap container when
  // snapped. The alignment_inline and alignment_block represent the alignments
  // on x axis and y axis repectively.
  ScrollSnapAlign scroll_snap_align;

  // The snap area rect relative to its snap container's boundary
  gfx::RectF rect;

  // Whether this area has scroll-snap-stop: always.
  // See https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-stop
  bool must_snap;

  // Whether this area has focus or has a descendant element which has focus.
  bool has_focus_within = false;

  // ElementId of the corresponding snap area.
  ElementId element_id;
};

struct TargetSnapAreaElementIds {
  TargetSnapAreaElementIds() = default;
  TargetSnapAreaElementIds(ElementId x_id, ElementId y_id) : x(x_id), y(y_id) {}
  bool operator==(const TargetSnapAreaElementIds& other) const {
    return (other.x == x) && (other.y == y);
  }

  bool operator!=(const TargetSnapAreaElementIds& other) const {
    return !(*this == other);
  }

  // Note that the same element can be snapped to on both the x and y axes.
  ElementId x;
  ElementId y;
};

typedef std::vector<SnapAreaData> SnapAreaList;

// Represents the result of a call to SnapContainerData::FindSnapPosition.
struct SnapPositionData {
  enum class Type { kNone, kAligned, kCovered };

  // What kind of snap position (if any) was found.
  Type type = Type::kNone;

  // The scroll offset of the snap position.
  gfx::PointF position;

  // The elements generating the snap areas on both axes.
  TargetSnapAreaElementIds target_element_ids;

  std::optional<gfx::RangeF> covered_range_x;
  std::optional<gfx::RangeF> covered_range_y;
};

// Snap container is a scroll container that at least one snap area assigned to
// it.  If the snap-type is not 'none', then it can be snapped to one of its
// snap areas when a scroll happens.
// This data structure describes the data needed for SnapCoordinator to perform
// snapping in the snap container.
//
// Note that the snap area data should only be used when snap-type is not 'none'
// There is not guarantee that this information is up-to-date otherwise. In
// fact, we skip updating these info as an optiomization.
class CC_EXPORT SnapContainerData {
 public:
  SnapContainerData();
  explicit SnapContainerData(ScrollSnapType type);
  SnapContainerData(ScrollSnapType type,
                    const gfx::RectF& rect,
                    const gfx::PointF& max);
  SnapContainerData(const SnapContainerData& other);
  SnapContainerData(SnapContainerData&& other);
  ~SnapContainerData();

  SnapContainerData& operator=(const SnapContainerData& other);
  SnapContainerData& operator=(SnapContainerData&& other);

  bool operator==(const SnapContainerData& other) const {
    return (other.scroll_snap_type_ == scroll_snap_type_) &&
           (other.rect_ == rect_) && (other.max_position_ == max_position_) &&
           (other.proximity_range_ == proximity_range_) &&
           (other.snap_area_list_ == snap_area_list_) &&
           (other.target_snap_area_element_ids_ ==
            target_snap_area_element_ids_) &&
           (other.targeted_area_id_ == targeted_area_id_) &&
           (other.has_horizontal_writing_mode_ == has_horizontal_writing_mode_);
  }

  bool operator!=(const SnapContainerData& other) const {
    return !(*this == other);
  }

  SnapPositionData FindSnapPositionWithViewportAdjustment(
      const SnapSelectionStrategy& strategy,
      double snapport_height_adjustment);

  SnapPositionData FindSnapPosition(
      const SnapSelectionStrategy& strategy) const;

  const TargetSnapAreaElementIds& GetTargetSnapAreaElementIds() const;
  // Returns true if the target snap area element ids were changed.
  bool SetTargetSnapAreaElementIds(TargetSnapAreaElementIds ids);

  void AddSnapAreaData(SnapAreaData snap_area_data);
  void UpdateSnapAreaFocus(size_t index, bool has_focus_within) {
    DCHECK(index < snap_area_list_.size());
    snap_area_list_[index].has_focus_within = has_focus_within;
  }
  size_t size() const { return snap_area_list_.size(); }
  const SnapAreaData& at(size_t index) const { return snap_area_list_[index]; }

  void set_scroll_snap_type(ScrollSnapType type) { scroll_snap_type_ = type; }
  ScrollSnapType scroll_snap_type() const { return scroll_snap_type_; }

  void set_rect(const gfx::RectF& rect) { rect_ = rect; }
  gfx::RectF rect() const { return rect_; }

  void set_max_position(gfx::PointF position) { max_position_ = position; }
  gfx::PointF max_position() const { return max_position_; }

  void set_proximity_range(const gfx::PointF& range) {
    proximity_range_ = range;
  }
  gfx::PointF proximity_range() const { return proximity_range_; }

  void set_targeted_area_id(const std::optional<ElementId>& id) {
    targeted_area_id_ = id;
  }

  void set_has_horizontal_writing_mode(bool has_horizontal_writing_mode) {
    has_horizontal_writing_mode_ = has_horizontal_writing_mode;
  }

 private:
  // Finds the best SnapArea candidate that's optimal for the given selection
  // strategy, while satisfying two invariants:
  // - |candidate.snap_offset| is within |cross_axis_snap_result|'s visible
  // range on |axis|.
  // - |cross_axis_snap_result.snap_offset| is within |candidate|'s visible
  // range on the cross axis.
  // |cross_axis_snap_result| is what we've found to snap on the cross axis,
  // or the original scroll offset if this is the first iteration of search.
  // Returns the candidate as SnapSearchResult that includes the area's
  // |snap_offset| and its visible range on the cross axis.
  // When |should_consider_covering| is true, the current offset can be valid if
  // it makes a snap area cover the snapport.
  // When |active_element_range| is provided, only snap areas that overlap
  // the active element are considered.
  std::optional<SnapSearchResult> FindClosestValidAreaInternal(
      SearchAxis axis,
      const SnapSelectionStrategy& strategy,
      const SnapSearchResult& cross_axis_snap_result,
      bool should_consider_covering = true,
      std::optional<gfx::RangeF> active_element_range = std::nullopt) const;

  // A wrapper of FindClosestValidAreaInternal(). If
  // FindClosestValidAreaInternal() doesn't return a valid result when the snap
  // type is mandatory and the strategy has an intended direction, we relax the
  // strategy to ignore the direction and find again.
  std::optional<SnapSearchResult> FindClosestValidArea(
      SearchAxis axis,
      const SnapSelectionStrategy& strategy,
      const SnapSearchResult& cross_axis_snap_result) const;

  bool FindSnapPositionForMutualSnap(const SnapSelectionStrategy& strategy,
                                     gfx::PointF* snap_position) const;

  // Finds the snap area associated with the target snap area element id for the
  // given axis.
  std::optional<SnapSearchResult> GetTargetSnapAreaSearchResult(
      const SnapSelectionStrategy& strategy,
      SearchAxis axis,
      SnapSearchResult cross_axis_snap_result) const;

  // Returns all the info needed to snap at this area on the given axis,
  // including:
  // - The offset at which the snap area and the snap container meet the
  //   requested alignment.
  // - The visible range within which the snap area is visible on the cross
  //   axis.
  SnapSearchResult GetSnapSearchResult(SearchAxis axis,
                                       const SnapAreaData& data) const;

  bool IsSnapportCoveredOnAxis(SearchAxis axis,
                               float current_offset,
                               const gfx::RectF& area_rect) const;

  void UpdateSnapAreaForTesting(ElementId element_id,
                                SnapAreaData snap_area_data);

  std::optional<SnapSearchResult> FindCoveringCandidate(
      const SnapAreaData& area,
      SearchAxis axis,
      const SnapSearchResult& aligned_candidate,
      float intended_position) const;

  bool IsSnappedToArea(const SnapAreaData& area,
                       const gfx::PointF& scroll_offset) const;

  gfx::RectF snapport() const;

  // Updates the alternative of a SnapSearchResult, |current_result|, so that,
  // if |candidate_area| is a closer snap point (in the cross axis) than the
  // area currently represented by |current_result|'s alternative,
  // |current_result|'s alternative is made to represent |candidate_area|
  // instead.
  void UpdateSearchAlternative(SnapSearchResult& current_result,
                               const SnapSearchResult& candidate_result,
                               const SnapAreaData& candidate_area,
                               const SnapSelectionStrategy& strategy) const;

  // This evaluates whether the snap area represented by the alternative of a
  // SnapSearchResult, |selection|, is aligned in both axes and is therefore a
  // better selection than the snap area of |selection|.
  void SelectAlternativeIdForSearchResult(
      SnapSearchResult& selection,
      const std::optional<SnapSearchResult>& cross_selection,
      float cross_current_position,
      float cross_max_position) const;

  SnapAxis SelectAxisToFollowForMutualVisibility(
      const SnapSelectionStrategy&,
      const SnapSearchResult& x_result,
      const SnapSearchResult& y_result) const;

  // Specifies whether a scroll container is a scroll snap container, how
  // strictly it snaps, and which axes are considered.
  // See https://www.w3.org/TR/css-scroll-snap-1/#scroll-snap-type for details.
  ScrollSnapType scroll_snap_type_;

  // The rect of the snap_container relative to its boundary.  This is the
  // snapport supplied by Blink; it is subject to browser controls adjustment
  // through snapport_height_adjustment_.
  gfx::RectF rect_;

  // The maximal scroll position of the SnapContainer, in the same coordinate
  // with blink's scroll position.
  gfx::PointF max_position_;

  // A valid snap position should be within the |proximity_range_| of the
  // current offset on the snapping axis.
  gfx::PointF proximity_range_;

  // The SnapAreaData for the snap areas in this snap container. When a scroll
  // happens, we iterate through the snap_area_list to find the best snap
  // position.
  std::vector<SnapAreaData> snap_area_list_;

  // Represents the ElementId(s) of the latest targeted snap areas.
  // ElementId(s) will be invalid (ElementId::kInvalidElementId) if the snap
  // container is not snapped to a position.
  TargetSnapAreaElementIds target_snap_area_element_ids_;

  // Transient adjustment to the height of the snapport (rect_) to account for
  // showing or hiding browser controls during a scroll gesture.  This is only
  // set while a call to FindSnapPosition is executing.
  double snapport_height_adjustment_ = 0;

  // Whether or not the writing mode of this snap container is a horizontal
  // writing mode.
  bool has_horizontal_writing_mode_ = true;

  // This is the ElementId of the snap area (snapped to by this snap container)
  // that is targeted[1] or contains a targeted[1] element. It is std::nullopt
  // if no such snap area exists.
  // [1]https://drafts.csswg.org/selectors/#the-target-pseudo
  std::optional<ElementId> targeted_area_id_;

  FRIEND_TEST_ALL_PREFIXES(ScrollSnapDataTest, SnapToFocusedElementHorizontal);
  FRIEND_TEST_ALL_PREFIXES(ScrollSnapDataTest, SnapToFocusedElementVertical);
  FRIEND_TEST_ALL_PREFIXES(ScrollSnapDataTest, SnapToFocusedElementBoth);
};

CC_EXPORT std::ostream& operator<<(std::ostream&, const SnapAreaData&);
CC_EXPORT std::ostream& operator<<(std::ostream&, const SnapContainerData&);

}  // namespace cc

#endif  // CC_INPUT_SCROLL_SNAP_DATA_H_
