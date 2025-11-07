// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/ui_event.h"

namespace actor::ui {

StartTask::StartTask(actor::TaskId id) : task_id(id) {}
StartTask::~StartTask() = default;
StartTask::StartTask(const StartTask&) = default;

TaskStateChanged::TaskStateChanged(actor::TaskId id,
                                   ActorTask::State state,
                                   const std::string& title)
    : task_id(id), state(state), title(title) {}
TaskStateChanged::TaskStateChanged(const TaskStateChanged&) = default;
TaskStateChanged::~TaskStateChanged() = default;

StartingToActOnTab::StartingToActOnTab(tabs::TabInterface::Handle th,
                                       actor::TaskId id)
    : tab_handle(th), task_id(id) {}
StartingToActOnTab::~StartingToActOnTab() = default;
StartingToActOnTab::StartingToActOnTab(const StartingToActOnTab&) = default;

StoppedActingOnTab::StoppedActingOnTab(tabs::TabInterface::Handle th)
    : tab_handle(th) {}
StoppedActingOnTab::~StoppedActingOnTab() = default;
StoppedActingOnTab::StoppedActingOnTab(const StoppedActingOnTab&) = default;

MouseClick::MouseClick(tabs::TabInterface::Handle th,
                       MouseClickType ct,
                       MouseClickCount cc)
    : tab_handle(th), click_type(ct), click_count(cc) {}
MouseClick::~MouseClick() = default;
MouseClick::MouseClick(const MouseClick&) = default;

MouseMove::MouseMove(tabs::TabInterface::Handle th,
                     std::optional<gfx::Point> t,
                     TargetSource s)
    : tab_handle(th), target(t), target_source(s) {}
MouseMove::~MouseMove() = default;
MouseMove::MouseMove(const MouseMove&) = default;

}  // namespace actor::ui
