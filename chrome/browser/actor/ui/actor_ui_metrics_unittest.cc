// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/action_variants_reader.h"
#include "chrome/browser/actor/ui/states/actor_task_nudge_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {

TEST(ActorUiMetricsTest, CheckActorTaskNudgeVariantNames) {
  const auto variants =
      base::test::ReadActionVariantsForAction("Actor.Ui.TaskNudge.");
  ASSERT_EQ(1U, variants.size());
  ASSERT_EQ(variants[0].size(),
            static_cast<size_t>(ActorTaskNudgeState::Text::kMaxValue));

  for (int i = 0; i <= static_cast<int>(ActorTaskNudgeState::Text::kMaxValue);
       i++) {
    if (i == static_cast<int>(ActorTaskNudgeState::Text::kDefault)) {
      continue;  // Default state is not recorded or included in variant.
    }
    std::string state_string(ToString(ActorTaskNudgeState{
        .text = static_cast<ActorTaskNudgeState::Text>(i)}));
    EXPECT_TRUE(variants[0].contains(state_string));
  }
}

}  // namespace
}  // namespace actor::ui
