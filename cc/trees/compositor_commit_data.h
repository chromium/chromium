// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_COMPOSITOR_COMMIT_DATA_H_
#define CC_TREES_COMPOSITOR_COMMIT_DATA_H_

#include <memory>
#include <optional>
#include <vector>

#include "cc/cc_export.h"
#include "cc/input/browser_controls_state.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/input/snap_selection_strategy.h"
#include "cc/paint/element_id.h"
#include "cc/trees/layer_tree_host_client.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d.h"

namespace cc {

class SwapPromise;

struct CC_EXPORT CompositorCommitData {
  CompositorCommitData();
  CompositorCommitData(const CompositorCommitData&) = delete;
  ~CompositorCommitData();

  CompositorCommitData& operator=(const CompositorCommitData&) = delete;

  struct CC_EXPORT ScrollUpdateInfo {
    ScrollUpdateInfo();
    ScrollUpdateInfo(ElementId id,
                     gfx::Vector2dF delta,
                     std::optional<TargetSnapAreaElementIds> snap_target_ids);
    ScrollUpdateInfo(const ScrollUpdateInfo& other);
    ScrollUpdateInfo& operator=(const ScrollUpdateInfo&);
    ElementId element_id;
    gfx::Vector2dF scroll_delta;

    // The target snap area element ids of the scrolling element.
    // This will have a value if the scrolled element's scroll node has snap
    // container data and the scroll delta is non-zero.
    std::optional<TargetSnapAreaElementIds> snap_target_element_ids;

    bool operator==(const ScrollUpdateInfo& other) const {
      return element_id == other.element_id &&
             scroll_delta == other.scroll_delta &&
             snap_target_element_ids == other.snap_target_element_ids;
    }
  };

  // The inner viewport scroll delta is kept separate since it's special.
  // Because the inner (visual) viewport's maximum offset depends on the
  // current page scale, the two must be committed at the same time to prevent
  // clamping.
  ScrollUpdateInfo inner_viewport_scroll;

  std::vector<ScrollUpdateInfo> scrolls;
  float page_scale_delta = 1.f;
  bool is_pinch_gesture_active = false;
  bool is_scroll_active = false;

  // Elastic overscroll effect offset delta. This is used only on Mac and shows
  // the pixels that the page is rubber-banned/stretched by.
  gfx::Vector2dF elastic_overscroll_delta;

  // Unconsumed scroll delta used to send overscroll events to the latched
  // element on the main thread;
  gfx::Vector2dF overscroll_delta;

  // The element id of the node to which scrolling is latched. This is used to
  // send overscroll/scrollend DOM events to proper targets whenever needed.
  ElementId scroll_latched_element_id;

  float top_controls_delta = 0.f;
  float bottom_controls_delta = 0.f;

  // Used to communicate scrollbar visibility from Impl thread to Blink.
  // Scrollbar input is handled by Blink but the compositor thread animates
  // opacity on scrollbars to fade them out when they're overlay. Blink needs
  // to be told when they're faded out so it can stop handling input for
  // invisible scrollbars.
  struct CC_EXPORT ScrollbarsUpdateInfo {
    ElementId element_id;
    bool hidden = true;

    bool operator==(const ScrollbarsUpdateInfo& other) const {
      return element_id == other.element_id && hidden == other.hidden;
    }
  };
  std::vector<ScrollbarsUpdateInfo> scrollbars;

  std::vector<std::unique_ptr<SwapPromise>> swap_promises;
  BrowserControlsState browser_controls_constraint =
      BrowserControlsState::kBoth;
  bool browser_controls_constraint_changed = false;

  struct ScrollEndInfo {
    // Set to true when a scroll gesture being handled on the compositor has
    // ended.
    bool scroll_gesture_did_end = false;

    bool gesture_affects_outer_viewport_scroll = false;
    bool gesture_affects_inner_viewport_scroll = false;
  };
  ScrollEndInfo scroll_end_data;

  // Tracks whether there is an ongoing compositor-driven animation for a
  // scroll, excluding autoscrolls (i.e., a continuous scroll animation
  // initiated by pressing on a scrollbar button).
  bool ongoing_scroll_animation = false;

  // Tracks whether there is an ongoing compositor-driven scroll animation for
  // a pressed scrollbar part.
  bool is_auto_scrolling = false;

  // Tracks different methods of scrolling (e.g. wheel, touch, precision
  // touchpad, etc.).
  ManipulationInfo manipulation_info = kManipulationInfoNone;

  // This tracks the strategy cc will use to snap at the end of the current
  // scroll based on the scroll updates so far. The main thread will use this to
  // determine whether to fire scrollsnapchanging or not.
  std::unique_ptr<SnapSelectionStrategy> snap_strategy;
};

}  // namespace cc

#endif  // CC_TREES_COMPOSITOR_COMMIT_DATA_H_
