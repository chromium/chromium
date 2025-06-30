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
              (const UiTabState& ui_tab_state),
              (override));
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_MOCK_ACTOR_UI_TAB_CONTROLLER_H_
