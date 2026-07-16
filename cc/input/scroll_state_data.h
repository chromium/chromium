// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_STATE_DATA_H_
#define CC_INPUT_SCROLL_STATE_DATA_H_

#include <stdint.h>

#include "cc/cc_export.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/trees/property_tree.h"
#include "ui/events/types/scroll_types.h"

namespace cc {

class CC_EXPORT ScrollStateData {
 public:
  ScrollStateData();
  ScrollStateData(const ScrollStateData& other);
  ScrollStateData& operator=(const ScrollStateData& other);

  // Scroll delta in viewport coordinates (DIP).
  double delta_x;
  double delta_y;
  // Scroll delta hint in viewport coordinates (DIP).
  // Delta hints are equal to deltas of the first gesture scroll update event in
  // a scroll sequence and are used for hittesting.
  double delta_x_hint;
  double delta_y_hint;
  // Pointer (i.e. cursor/touch point) position in viewport coordinates (DIP).
  int position_x;
  int position_y;

  bool is_beginning;
  bool is_in_inertial_phase;
  bool is_ending;

  bool from_user_input;

  // Whether the scroll sequence has had any delta consumed, in the
  // current frame, or any child frames.
  bool delta_consumed_for_scroll_sequence;
  // True if the user interacts directly with the display, e.g., via
  // touch.
  bool is_direct_manipulation;
  // True if the scroll is the result of a scrollbar interaction.
  bool is_scrollbar_interaction;

  // Granularity units for the scroll delta.
  ui::ScrollGranularity delta_granularity;

  // TODO(tdresser): ScrollState shouldn't need to keep track of whether or not
  // this ScrollState object has caused a scroll. Ideally, any native scroller
  // consuming delta has caused a scroll. Currently, there are some cases where
  // we consume delta without scrolling, such as in
  // |Viewport::AdjustOverscroll|. Once these cases are fixed, we should get rid
  // of |caused_scroll_*_|. See crbug.com/510045 for details.
  bool caused_scroll_x;
  bool caused_scroll_y;

  // Track if the scroll_chain has been cut by overscroll_behavior, in
  // order to properly handle overscroll-effects.
  // TODO(sunyunjia): overscroll should be handled at the top of scroll_chain,
  // as implemented at blink side. This field should be removed after it's
  // resolved. crbug.com/755164.
  bool is_scroll_chain_cut;

  ElementId current_native_scrolling_element() const;
  void set_current_native_scrolling_element(ElementId element_id);

  // Used in scroll unification to specify that a scroll state has been hit
  // tested on the main thread. If this is nonzero, the hit test result will be
  // placed in the current_native_scrolling_element_.
  uint32_t main_thread_hit_tested_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;

 private:
  // The id of the last native element to respond to a scroll, or 0 if none
  // exists.
  // TODO(bokan): In the compositor, this is now only used as an override to
  // scroller targeting. I.e. we'll latch scrolling to the specified
  // element_id. It will be renamed to a better name (target_element_id?) when
  // the main thread is also converted.
  ElementId current_native_scrolling_element_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_STATE_DATA_H_
