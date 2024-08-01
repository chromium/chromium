// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_STATE_H_
#define CC_INPUT_SCROLL_STATE_H_

#include "cc/cc_export.h"
#include "cc/input/scroll_state_data.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

// ScrollState is based on the proposal for scroll customization in blink, found
// here: https://goo.gl/1ipTpP.
class CC_EXPORT ScrollState {
 public:
  explicit ScrollState(ScrollStateData data);
  ScrollState(const ScrollState& other);
  ~ScrollState();

  // Reduce deltas by x, y.
  void ConsumeDelta(double x, double y);
  // Positive when scrolling right.
  double delta_x() const { return data_.delta_x; }
  // Positive when scrolling down.
  double delta_y() const { return data_.delta_y; }
  // Positive when scrolling right.
  double delta_x_hint() const { return data_.delta_x_hint; }
  // Positive when scrolling down.
  double delta_y_hint() const { return data_.delta_y_hint; }
  // The location associated with this scroll update. For touch, this is the
  // position of the finger. For mouse, the location of the cursor.
  int position_x() const { return data_.position_x; }
  int position_y() const { return data_.position_y; }

  bool is_beginning() const { return data_.is_beginning; }
  void set_is_beginning(bool is_beginning) {
    data_.is_beginning = is_beginning;
  }
  bool is_in_inertial_phase() const { return data_.is_in_inertial_phase; }
  void set_is_in_inertial_phase(bool is_in_inertial_phase) {
    data_.is_in_inertial_phase = is_in_inertial_phase;
  }
  bool is_ending() const { return data_.is_ending; }
  void set_is_ending(bool is_ending) { data_.is_ending = is_ending; }

  // True if the user interacts directly with the screen, e.g., via touch.
  bool is_direct_manipulation() const { return data_.is_direct_manipulation; }
  void set_is_direct_manipulation(bool is_direct_manipulation) {
    data_.is_direct_manipulation = is_direct_manipulation;
  }

  // True if the user interacts with the scrollbar.
  bool is_scrollbar_interaction() const {
    return data_.is_scrollbar_interaction;
  }
  void set_is_scrollbar_interaction(bool is_scrollbar_interaction) {
    data_.is_scrollbar_interaction = is_scrollbar_interaction;
  }

  bool delta_consumed_for_scroll_sequence() const {
    return data_.delta_consumed_for_scroll_sequence;
  }
  void set_delta_consumed_for_scroll_sequence(bool delta_consumed) {
    data_.delta_consumed_for_scroll_sequence = delta_consumed;
  }

  void set_caused_scroll(bool x, bool y) {
    data_.caused_scroll_x |= x;
    data_.caused_scroll_y |= y;
  }

  bool caused_scroll_x() const { return data_.caused_scroll_x; }
  bool caused_scroll_y() const { return data_.caused_scroll_y; }

  void set_is_scroll_chain_cut(bool cut) { data_.is_scroll_chain_cut = cut; }

  bool is_scroll_chain_cut() const { return data_.is_scroll_chain_cut; }

  ui::ScrollGranularity delta_granularity() const {
    return data_.delta_granularity;
  }

  // Returns a the delta hints if this is a scroll begin or the real delta if
  // it's a scroll update
  gfx::Vector2dF DeltaOrHint() const;

  ElementId target_element_id() const {
    return data_.current_native_scrolling_element();
  }

  uint32_t main_thread_hit_tested_reasons() const {
    return data_.main_thread_hit_tested_reasons;
  }

  ScrollStateData* data() { return &data_; }

 private:
  ScrollStateData data_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_STATE_H_
