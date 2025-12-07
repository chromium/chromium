// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_ACTOR_UI_STATE_MANAGER_H_
#define CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_ACTOR_UI_STATE_MANAGER_H_

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
  MOCK_METHOD(void, MaybeShowToast, (BrowserWindowInterface * bwi), (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterActorTaskStateChange,
              (ActorTaskStateChangeCallback callback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterActorTaskStopped,
              (ActorTaskStoppedCallback callback),
              (override));
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_ACTOR_UI_STATE_MANAGER_H_
