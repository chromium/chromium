// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TEST_SUPPORT_MOCK_ACTOR_UI_STATE_MANAGER_H_
#define CHROME_BROWSER_ACTOR_UI_TEST_SUPPORT_MOCK_ACTOR_UI_STATE_MANAGER_H_

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/common/actor/task_id.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace actor::ui {

class MockActorUiStateManager : public ActorUiStateManagerInterface {
 public:
  MockActorUiStateManager();
  ~MockActorUiStateManager() override;

  MOCK_METHOD(void,
              OnUiEvent,
              (AsyncUiEvent event, UiCompleteCallback callback),
              (override));
  MOCK_METHOD(void, OnUiEvent, (SyncUiEvent event), (override));
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  MOCK_METHOD(void, MaybeShowToast, (BrowserWindowInterface * bwi), (override));
#endif  // BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterActorTaskStateChange,
              (ActorTaskStateChangeCallback callback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterActorTaskStopped,
              (ActorTaskStoppedCallback callback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterActorTaskRemoved,
              (ActorTaskRemovedCallback callback),
              (override));
  MOCK_METHOD(std::optional<std::string>,
              GetActorTaskTitle,
              (TaskId task_id),
              (override));
  MOCK_METHOD(std::optional<raw_ptr<tabs::TabInterface>>,
              GetLastActedOnTab,
              (TaskId task_id),
              (override));
  MOCK_METHOD(std::optional<actor::ActorTask::State>,
              GetActorTaskState,
              (TaskId task_id),
              (override));
  MOCK_METHOD(size_t, GetInactiveTaskCount, (), (override));
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_TEST_SUPPORT_MOCK_ACTOR_UI_STATE_MANAGER_H_
