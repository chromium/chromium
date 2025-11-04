// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_UI_EVENT_H_
#define CHROME_BROWSER_ACTOR_UI_UI_EVENT_H_

#include <optional>
#include <variant>

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/shared_types.h"
#include "chrome/common/actor/task_id.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/geometry/point.h"

namespace actor::ui {

// The source of a target on a page.
enum class TargetSource {
  kUnresolvableInApc = 0,  // The ToolRequest DomTarget couldn't be resolved
                           // from the AnnotatedPageContent.
  kToolRequest = 1,        // The target came directly from the ToolRequest.
  kDerivedFromApc = 2,     // The target was derived from AnnotatedPageContent.
  kMaxValue = kDerivedFromApc,
};

// STATUS: Dispatched when ActorTask state changes from Created to Acting.
struct StartTask {
  actor::TaskId task_id;

  explicit StartTask(actor::TaskId);
  StartTask(const StartTask&);
  ~StartTask();
};

// STATUS: Dispatched when ActorTask state changes.
struct TaskStateChanged {
  actor::TaskId task_id;
  ActorTask::State state;
  std::string title;

  TaskStateChanged(actor::TaskId, ActorTask::State, const std::string& title);
  TaskStateChanged(const TaskStateChanged&);
  ~TaskStateChanged();
};

// STATUS: Dispatched when a tab is added to ActorTask.
struct StartingToActOnTab {
  tabs::TabInterface::Handle tab_handle;
  actor::TaskId task_id;

  StartingToActOnTab(tabs::TabInterface::Handle, actor::TaskId);
  StartingToActOnTab(const StartingToActOnTab&);
  ~StartingToActOnTab();
};

// STATUS: Not yet dispatched anywhere.
struct StoppedActingOnTab {
  tabs::TabInterface::Handle tab_handle;

  explicit StoppedActingOnTab(tabs::TabInterface::Handle);
  StoppedActingOnTab(const StoppedActingOnTab&);
  ~StoppedActingOnTab();
};

// STATUS: Dispatched pre-tool invocation.
struct MouseMove {
  tabs::TabInterface::Handle tab_handle;
  std::optional<gfx::Point> target;
  TargetSource target_source;

  MouseMove(tabs::TabInterface::Handle,
            std::optional<gfx::Point>,
            TargetSource);
  MouseMove(const MouseMove&);
  ~MouseMove();
};

// STATUS: Dispatched pre-tool invocation.
struct MouseClick {
  tabs::TabInterface::Handle tab_handle;
  MouseClickType click_type;
  MouseClickCount click_count;

  MouseClick(tabs::TabInterface::Handle, MouseClickType, MouseClickCount);
  MouseClick(const MouseClick&);
  ~MouseClick();
};

// AsyncUiEvents may be sent to ActorUiStateManager's asynchronous handler.
// ActorUiStateManager must complete the async callback with a result.  Callers
// may wait for the result callback to allow ActorUiStateManager to finish async
// work before proceeding.
using AsyncUiEvent = std::variant<StartingToActOnTab, MouseClick, MouseMove>;

// SyncUiEvents may be sent to ActorUiStateManager's synchronous handler.
// There's no affordance for ActorUiStateManager to report errors processing
// these events or for callers to wait for ActorUiStateManager to finish async
// work before proceeding.
using SyncUiEvent =
    std::variant<StartTask, TaskStateChanged, StoppedActingOnTab>;

using UiEvent = std::variant<StartTask,
                             StartingToActOnTab,
                             StoppedActingOnTab,
                             TaskStateChanged,
                             MouseClick,
                             MouseMove>;

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_UI_EVENT_H_
