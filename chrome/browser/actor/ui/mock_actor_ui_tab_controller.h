// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_MOCK_ACTOR_UI_TAB_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_MOCK_ACTOR_UI_TAB_CONTROLLER_H_

#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace actor::ui {

class MockActorUiTabController : public ActorUiTabControllerInterface {
 public:
  MockActorUiTabController();
  ~MockActorUiTabController() override;

  MOCK_METHOD(void,
              OnUiTabStateChange,
              (const UiTabState& ui_tab_state, UiResultCallback callback),
              (override));

  MOCK_METHOD(void,
              OnTabActiveStatusChanged,
              (bool tab_active_status, tabs::TabInterface* tab),
              (override));

  MOCK_METHOD(void, SetActiveTaskId, (TaskId task_id), (override));

  MOCK_METHOD(void, ClearActiveTaskId, (), (override));

  MOCK_METHOD(base::WeakPtr<ActorUiTabControllerInterface>,
              GetWeakPtr,
              (),
              (override));

  MOCK_METHOD(void, SetActorTaskPaused, (), (override));

  MOCK_METHOD(void, SetActorTaskResume, (), (override));

  MOCK_METHOD(void, SetOverlayHoverStatus, (bool is_hovering), (override));

  MOCK_METHOD(void,
              SetHandoffButtonHoverStatus,
              (bool is_hovering),
              (override));

  MOCK_METHOD(void,
              BindActorOverlay,
              (mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver),
              (override));

  MOCK_METHOD(void,
              SetCallbackForTesting,
              (base::OnceClosure callback),
              (override));
  MOCK_METHOD(bool, ShouldShowActorTabIndicator, (), (override));

 private:
  base::WeakPtrFactory<MockActorUiTabController> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_MOCK_ACTOR_UI_TAB_CONTROLLER_H_
