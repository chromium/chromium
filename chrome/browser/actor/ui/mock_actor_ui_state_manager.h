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
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/common/actor.mojom.h"
#endif

namespace tabs {
class TabInterface;
}

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
  MOCK_METHOD(ActorUiTabControllerInterface*,
              GetUiTabController,
              (tabs::TabInterface * tab),
              (override));

#if BUILDFLAG(ENABLE_GLIC)
  MOCK_METHOD(void,
              OnGlicUpdateFloatyState,
              (glic::GlicWindowController::State floaty_state,
               BrowserWindowInterface* interface),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterFloatyTaskStateChange,
              (FloatyTaskStateChangeCallback callback),
              (override));
#endif
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_MOCK_ACTOR_UI_STATE_MANAGER_H_
