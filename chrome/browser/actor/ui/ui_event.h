// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_UI_EVENT_H_
#define CHROME_BROWSER_ACTOR_UI_UI_EVENT_H_

#include <optional>
#include <variant>

#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/task_id.h"
#include "components/tabs/public/tab_interface.h"

namespace actor::ui {
struct StartTask {
  explicit StartTask(actor::TaskId);
  StartTask(const StartTask&);
  ~StartTask();

  actor::TaskId task_id;
};

struct StartingToActOnTab {
  StartingToActOnTab(tabs::TabInterface::Handle, actor::TaskId);
  StartingToActOnTab(const StartingToActOnTab&);
  ~StartingToActOnTab();

  tabs::TabInterface::Handle tab_handle;
  actor::TaskId task_id;
};

struct MouseMove {
  MouseMove(tabs::TabInterface::Handle, PageTarget);
  MouseMove(const MouseMove&);
  ~MouseMove();

  tabs::TabInterface::Handle tab_handle;
  PageTarget target;
};

struct MouseClick {
  MouseClick(tabs::TabInterface::Handle, MouseClickType, MouseClickCount);
  MouseClick(const MouseClick&);
  ~MouseClick();

  tabs::TabInterface::Handle tab_handle;
  MouseClickType click_type;
  MouseClickCount click_count;
};

using UiEvent =
    std::variant<StartTask, StartingToActOnTab, MouseClick, MouseMove>;

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_UI_EVENT_H_
