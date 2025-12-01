// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_STATES_ACTOR_TASK_NUDGE_STATE_H_
#define CHROME_BROWSER_ACTOR_UI_STATES_ACTOR_TASK_NUDGE_STATE_H_

#include <string_view>

namespace actor::ui {
// LINT.IfChange(ActorTaskNudgeState)
struct ActorTaskNudgeState {
  enum class Text {
    // Default/no text.
    kDefault = 0,
    // `Needs attention` text.
    kNeedsAttention = 1,
    // `Multiple tasks need attention` text.
    kMultipleTasksNeedAttention = 2,
    // `Complete Tasks` text.
    kCompleteTasks = 3,
    kMaxValue = kCompleteTasks,
  };
  Text text = Text::kDefault;

  bool operator==(const ActorTaskNudgeState& other) const {
    return text == other.text;
  }
};
// LINT.ThenChange(//chrome/tools/metrics/histograms/metadata/actor/enums.xml:TaskNudgeState)

inline std::string_view ToString(const ActorTaskNudgeState& state) {
  switch (state.text) {
    case ActorTaskNudgeState::Text::kDefault:
      return "Default";
    case ActorTaskNudgeState::Text::kNeedsAttention:
      return "NeedsAttention";
    case ActorTaskNudgeState::Text::kMultipleTasksNeedAttention:
      return "MultipleTasksNeedAttention";
    case ActorTaskNudgeState::Text::kCompleteTasks:
      return "CompleteTasks";
  }
}

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_STATES_ACTOR_TASK_NUDGE_STATE_H_
