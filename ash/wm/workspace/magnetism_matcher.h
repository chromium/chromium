// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_MAGNETISM_MATCHER_H_
#define ASH_WM_WORKSPACE_MAGNETISM_MATCHER_H_

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

enum MagnetismEdge {
  MAGNETISM_EDGE_TOP = 1 << 0,
  MAGNETISM_EDGE_LEFT = 1 << 1,
  MAGNETISM_EDGE_BOTTOM = 1 << 2,
  MAGNETISM_EDGE_RIGHT = 1 << 3,
};

const uint32_t kAllMagnetismEdges = MAGNETISM_EDGE_TOP | MAGNETISM_EDGE_LEFT |
                                    MAGNETISM_EDGE_BOTTOM |
                                    MAGNETISM_EDGE_RIGHT;

// MagnetismEdgeMatcher is used for matching a particular edge of a window. You
// shouldn't need to use this directly, instead use MagnetismMatcher which takes
// care of all edges.
// MagnetismEdgeMatcher maintains a range of the visible portions of the
// edge. As ShouldAttach() is invoked the visible range is updated.
class MagnetismEdgeMatcher {
 public:
  MagnetismEdgeMatcher(const gfx::Rect& bounds, MagnetismEdge edge);

  MagnetismEdgeMatcher(const MagnetismEdgeMatcher&) = delete;
  MagnetismEdgeMatcher& operator=(const MagnetismEdgeMatcher&) = delete;

  ~MagnetismEdgeMatcher();

  MagnetismEdge edge() const { return edge_; }
  const gfx::Rect& bounds() const { return bounds_; }

  // Returns true if the edge is completely obscured. If true ShouldAttach()
  // will return false.
  bool is_edge_obscured() const { return ranges_.empty(); }

  // Returns true if should attach to the specified bounds.
  bool ShouldAttach(const gfx::Rect& bounds);

 private:
  typedef std::pair<int, int> Range;
  typedef std::vector<Range> Ranges;

  // Removes |range| from |ranges_|.
  void UpdateRanges(const Range& range);

  static int GetPrimaryCoordinate(const gfx::Rect& bounds, MagnetismEdge edge) {
    switch (edge) {
      case MAGNETISM_EDGE_TOP:
        return bounds.y();
      case MAGNETISM_EDGE_LEFT:
        return bounds.x();
      case MAGNETISM_EDGE_BOTTOM:
        return bounds.bottom();
      case MAGNETISM_EDGE_RIGHT:
        return bounds.right();
    }
    NOTREACHED();
  }

  static MagnetismEdge FlipEdge(MagnetismEdge edge) {
    switch (edge) {
      case MAGNETISM_EDGE_TOP:
        return MAGNETISM_EDGE_BOTTOM;
      case MAGNETISM_EDGE_BOTTOM:
        return MAGNETISM_EDGE_TOP;
      case MAGNETISM_EDGE_LEFT:
        return MAGNETISM_EDGE_RIGHT;
      case MAGNETISM_EDGE_RIGHT:
        return MAGNETISM_EDGE_LEFT;
    }
    NOTREACHED();
  }

  Range GetPrimaryRange(const gfx::Rect& bounds) const {
    switch (edge_) {
      case MAGNETISM_EDGE_TOP:
      case MAGNETISM_EDGE_BOTTOM:
        return Range(bounds.y(), bounds.bottom());
      case MAGNETISM_EDGE_LEFT:
      case MAGNETISM_EDGE_RIGHT:
        return Range(bounds.x(), bounds.right());
    }
    NOTREACHED();
  }

  Range GetSecondaryRange(const gfx::Rect& bounds) const {
    switch (edge_) {
      case MAGNETISM_EDGE_TOP:
      case MAGNETISM_EDGE_BOTTOM:
        return Range(bounds.x(), bounds.right());
      case MAGNETISM_EDGE_LEFT:
      case MAGNETISM_EDGE_RIGHT:
        return Range(bounds.y(), bounds.bottom());
    }
    NOTREACHED();
  }

  static bool RangesIntersect(const Range& r1, const Range& r2) {
    return r2.first < r1.second && r2.second > r1.first;
  }

  // The bounds of window.
  const gfx::Rect bounds_;

  // The edge this matcher checks.
  const MagnetismEdge edge_;

  // Visible ranges of the edge. Initialized with GetSecondaryRange() and
  // updated as ShouldAttach() is invoked. When empty the edge is completely
  // obscured by other bounds.
  Ranges ranges_;
};

enum SecondaryMagnetismEdge {
  SECONDARY_MAGNETISM_EDGE_LEADING,
  SECONDARY_MAGNETISM_EDGE_TRAILING,
  SECONDARY_MAGNETISM_EDGE_NONE,
};

// Used to identify a matched edge. |primary_edge| is relative to the source and
// indicates the edge the two are to share. For example, if |primary_edge| is
// MAGNETISM_EDGE_RIGHT then the right edge of the source should snap to to the
// left edge of the target. |secondary_edge| indicates one of the edges along
// the opposite axis should should also be aligned. For example, if
// |primary_edge| is MAGNETISM_EDGE_RIGHT and |secondary_edge| is
// SECONDARY_MAGNETISM_EDGE_LEADING then the source should snap to the left top
// corner of the target.
struct MatchedEdge {
  MagnetismEdge primary_edge;
  SecondaryMagnetismEdge secondary_edge;
};

// MagnetismMatcher is used to test if a window should snap to another window.
// To use MagnetismMatcher do the following:
// . Create it with the bounds of the window being dragged.
// . Iterate over the child windows checking if the window being dragged should
//   attach to it using ShouldAttach().
// . Use AreEdgesObscured() to test if no other windows can match (because all
//   edges are completely obscured).
class ASH_EXPORT MagnetismMatcher {
 public:
  static const int kMagneticDistance;

  // |edges| is a bitmask of MagnetismEdges to match against.
  MagnetismMatcher(const gfx::Rect& bounds, uint32_t edges);

  MagnetismMatcher(const MagnetismMatcher&) = delete;
  MagnetismMatcher& operator=(const MagnetismMatcher&) = delete;

  ~MagnetismMatcher();

  // Returns true if |bounds| is close enough to the initial bounds that the two
  // should be attached. If true is returned |edge| is set to indicates how the
  // two should snap together. See description of MatchedEdge for details.
  bool ShouldAttach(const gfx::Rect& bounds, MatchedEdge* edge);

  // Returns true if no other matches are possible.
  bool AreEdgesObscured() const;

 private:
  // Sets |secondary_edge| based on whether the secondary edges should snap.
  void AttachToSecondaryEdge(const gfx::Rect& bounds,
                             MagnetismEdge edge,
                             SecondaryMagnetismEdge* secondary_edge) const;

  // The edges to match against.
  const int32_t edges_;

  std::vector<std::unique_ptr<MagnetismEdgeMatcher>> matchers_;
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_MAGNETISM_MATCHER_H_
