// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/state_transitions.h"

#include <ostream>
#include <string>

#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

enum class State { kState1 = 0, kState2, kState3, kState4 };

std::ostream& operator<<(std::ostream& o, const State& s) {
  return o << static_cast<int>(s);
}

TEST(StateTransitionsTest, Constructor) {
  // No expectations, just make sure the constructor works.
  const StateTransitions<State> transitions({
      {State::kState1, {State::kState2, State::kState3}},
      {State::kState2, {State::kState3, State::kState4}},
  });
}

TEST(StateTransitionsTest, GetValidTransitions) {
  const StateTransitions<State> transitions({
      {State::kState1, {State::kState2, State::kState3}},
      {State::kState2, {State::kState3, State::kState4}},
  });
  EXPECT_THAT(transitions.GetValidTransitions(State::kState1),
              testing::ElementsAre(State::kState2, State::kState3));
  EXPECT_THAT(transitions.GetValidTransitions(State::kState2),
              testing::ElementsAre(State::kState3, State::kState4));
  EXPECT_THAT(transitions.GetValidTransitions(State::kState3),
              testing::ElementsAre());
  EXPECT_THAT(transitions.GetValidTransitions(State::kState4),
              testing::ElementsAre());
}

TEST(StateTransitionsTest, IsTransitionValid) {
  const StateTransitions<State> transitions({
      {State::kState1, {State::kState2, State::kState3}},
      {State::kState2, {State::kState3, State::kState4}},
  });
  ASSERT_TRUE(transitions.IsTransitionValid(State::kState1, State::kState2));
  ASSERT_TRUE(transitions.IsTransitionValid(State::kState2, State::kState3));
  ASSERT_FALSE(transitions.IsTransitionValid(State::kState1, State::kState4));
  // kState3 was omitted from the definition.
  ASSERT_FALSE(transitions.IsTransitionValid(State::kState3, State::kState4));
}

TEST(StateTransitionsTest, DCHECK_STATE_TRANSITION) {
  const StateTransitions<State> transitions({
      {State::kState1, {State::kState2, State::kState3}},
      {State::kState2, {State::kState3, State::kState4}},
  });
  DCHECK_STATE_TRANSITION(&transitions, State::kState1, State::kState2);
  DCHECK_STATE_TRANSITION(&transitions, State::kState2, State::kState3);

#if DCHECK_IS_ON()
  // EXPECT_DEATH is not defined on IOS.
#if !BUILDFLAG(IS_IOS)
  EXPECT_DEATH(
      DCHECK_STATE_TRANSITION(&transitions, State::kState1, State::kState4),
      "Check failed.*Invalid transition: 0 -> 3");
  // kState3 was omitted from the definition.
  EXPECT_DEATH(
      DCHECK_STATE_TRANSITION(&transitions, State::kState3, State::kState4),
      "Check failed.*Invalid transition: 2 -> 3");
#endif  // !BUILDFLAG(IS_IOS)
#endif  // DCHECK_IS_ON()
}

// Test that everything works OK with some other data type.
TEST(StateTransitionsTest, NonEnum) {
  const StateTransitions<std::string> transitions({
      {"state1", {"state2", "state3"}},
      {"state2", {"state3", "state4"}},
  });
  ASSERT_TRUE(transitions.IsTransitionValid("state1", "state2"));
  ASSERT_TRUE(transitions.IsTransitionValid("state2", "state3"));
  ASSERT_FALSE(transitions.IsTransitionValid("state1", "state4"));
  // kState3 was omitted from the definition.
  ASSERT_FALSE(transitions.IsTransitionValid("state3", "state4"));
  DCHECK_STATE_TRANSITION(&transitions, "state1", "state2");
  DCHECK_STATE_TRANSITION(&transitions, "state2", "state3");

  // Try some states that are not in the specification at all.
  ASSERT_FALSE(transitions.IsTransitionValid("foo", "state2"));
  ASSERT_FALSE(transitions.IsTransitionValid("state1", "foo"));
  ASSERT_FALSE(transitions.IsTransitionValid("foo", "bar"));
}

}  // namespace base
