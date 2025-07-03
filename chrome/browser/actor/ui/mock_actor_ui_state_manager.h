// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_MOCK_ACTOR_UI_STATE_MANAGER_H_
#define CHROME_BROWSER_ACTOR_UI_MOCK_ACTOR_UI_STATE_MANAGER_H_

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace tabs {
class TabInterface;
}

namespace actor::ui {

class MockActorUiStateManager : public ActorUiStateManagerInterface {
 public:
  MockActorUiStateManager();
  ~MockActorUiStateManager() override;

  MOCK_METHOD(void,
              OnActorTaskStateChange,
              (TaskId task_id, ActorTask::State task_state),
              (override));
  MOCK_METHOD(void,
              OnUiEvent,
              (UiEvent event, UiCompleteCallback callback),
              (override));
  MOCK_METHOD(void,
              NotifyUiTabController,
              (tabs::TabInterface & tab, const UiTabState& ui_tab_state),
              (override));
  MOCK_METHOD(void, MaybeShowToast, (), (override));
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_MOCK_ACTOR_UI_STATE_MANAGER_H_
