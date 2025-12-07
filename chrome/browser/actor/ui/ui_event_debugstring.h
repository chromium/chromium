// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_UI_EVENT_DEBUGSTRING_H_
#define CHROME_BROWSER_ACTOR_UI_UI_EVENT_DEBUGSTRING_H_

#include "chrome/browser/actor/ui/ui_event.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace actor::ui {

std::string DebugString(const UiEvent&);
std::string DebugString(const AsyncUiEvent&);
std::string DebugString(const SyncUiEvent&);

std::string DebugString(TargetSource);

// LINT.IfChange(GetUiEventName)
inline constexpr absl::Overload UiEventNameFn{
    [](const StartTask&) -> std::string_view { return "StartTask"; },
    [](const TaskStateChanged&) -> std::string_view {
      return "TaskStateChanged";
    },
    [](const StartingToActOnTab&) -> std::string_view {
      return "StartingToActOnTab";
    },
    [](const StoppedActingOnTab&) -> std::string_view {
      return "StoppedActingOnTab";
    },
    [](const MouseClick&) -> std::string_view { return "MouseClick"; },
    [](const MouseMove&) -> std::string_view { return "MouseMove"; },
};

template <typename T>
const std::string_view GetUiEventName(const T& ui_event) {
  return std::visit(UiEventNameFn, ui_event);
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/histograms.xml:UiEvent)

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_UI_EVENT_DEBUGSTRING_H_
