// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_STATE_H_
#define CC_INPUT_SCROLL_STATE_H_

#include <list>
#include <memory>

#include "cc/cc_export.h"
#include "cc/input/scroll_state_data.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace cc {

class LayerTreeImpl;

// ScrollState is based on the proposal for scroll customization in blink, found
// here: https://goo.gl/1ipTpP.
class CC_EXPORT ScrollState {
 public:
  explicit ScrollState(ScrollStateData data);
  ScrollState(const ScrollState& other);
  ~ScrollState();

  // Reduce deltas by x, y.
  void ConsumeDelta(double x, double y);
  // Pops the first layer off of |scroll_chain_| and calls
  // |DistributeScroll| on it.
  void DistributeToScrollChainDescendant();
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

  double velocity_x() const { return data_.velocity_x; }
  double velocity_y() const { return data_.velocity_y; }

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

  void set_scroll_chain_and_layer_tree(
      const std::list<ScrollNode*>& scroll_chain,
      LayerTreeImpl* layer_tree_impl) {
    layer_tree_impl_ = layer_tree_impl;
    scroll_chain_ = scroll_chain;
  }

  void set_current_native_scrolling_node(ScrollNode* scroll_node) {
    data_.set_current_native_scrolling_node(scroll_node);
  }

  ScrollNode* current_native_scrolling_node() const {
    return data_.current_native_scrolling_node();
  }

  bool delta_consumed_for_scroll_sequence() const {
    return data_.delta_consumed_for_scroll_sequence;
  }
  void set_delta_consumed_for_scroll_sequence(bool delta_consumed) {
    data_.delta_consumed_for_scroll_sequence = delta_consumed;
  }

  bool FullyConsumed() const { return !data_.delta_x && !data_.delta_y; }

  void set_caused_scroll(bool x, bool y) {
    data_.caused_scroll_x |= x;
    data_.caused_scroll_y |= y;
  }

  bool caused_scroll_x() const { return data_.caused_scroll_x; }
  bool caused_scroll_y() const { return data_.caused_scroll_y; }

  void set_is_scroll_chain_cut(bool cut) { data_.is_scroll_chain_cut = cut; }

  bool is_scroll_chain_cut() const { return data_.is_scroll_chain_cut; }

  double delta_granularity() const { return data_.delta_granularity; }

  LayerTreeImpl* layer_tree_impl() { return layer_tree_impl_; }
  ScrollStateData* data() { return &data_; }

 private:
  ScrollStateData data_;
  LayerTreeImpl* layer_tree_impl_;
  std::list<ScrollNode*> scroll_chain_;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_STATE_H_
