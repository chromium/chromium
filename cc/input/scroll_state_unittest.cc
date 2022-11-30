// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/scroll_state.h"

#include "cc/layers/layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_node.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class ScrollStateTest : public testing::Test {};

TEST_F(ScrollStateTest, ConsumeDeltaNative) {
  const float delta_x = 12.3f;
  const float delta_y = 3.9f;

  const float delta_x_to_consume = 1.2f;
  const float delta_y_to_consume = 2.3f;

  ScrollStateData scroll_state_data;
  scroll_state_data.delta_x = delta_x;
  scroll_state_data.delta_y = delta_y;
  ScrollState scroll_state(scroll_state_data);

  EXPECT_FLOAT_EQ(delta_x, scroll_state.delta_x());
  EXPECT_FLOAT_EQ(delta_y, scroll_state.delta_y());
  EXPECT_FALSE(scroll_state.delta_consumed_for_scroll_sequence());

  scroll_state.ConsumeDelta(0, 0);
  EXPECT_FLOAT_EQ(delta_x, scroll_state.delta_x());
  EXPECT_FLOAT_EQ(delta_y, scroll_state.delta_y());
  EXPECT_FALSE(scroll_state.delta_consumed_for_scroll_sequence());

  scroll_state.ConsumeDelta(delta_x_to_consume, 0);
  EXPECT_FLOAT_EQ(delta_x - delta_x_to_consume, scroll_state.delta_x());
  EXPECT_FLOAT_EQ(delta_y, scroll_state.delta_y());
  EXPECT_TRUE(scroll_state.delta_consumed_for_scroll_sequence());

  scroll_state.ConsumeDelta(0, delta_y_to_consume);
  EXPECT_FLOAT_EQ(delta_x - delta_x_to_consume, scroll_state.delta_x());
  EXPECT_FLOAT_EQ(delta_y - delta_y_to_consume, scroll_state.delta_y());
  EXPECT_TRUE(scroll_state.delta_consumed_for_scroll_sequence());

  scroll_state.ConsumeDelta(scroll_state.delta_x(), scroll_state.delta_y());
  EXPECT_TRUE(scroll_state.delta_consumed_for_scroll_sequence());
}
}  // namespace cc
