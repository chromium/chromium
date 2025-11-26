// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_ACTOR_UI_TAB_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_ACTOR_UI_TAB_CONTROLLER_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

namespace actor::ui {

class MockHandoffButtonController;

// A mock factory for creating MockActorUiTabControllers for testing.
//
// This class implements the ActorUiTabControllerFactoryInterface to create
// and provide mock versions of controllers used by the ActorUiTabController.
class MockActorUiTabControllerFactory
    : public ActorUiTabControllerFactoryInterface {
 public:
  MockActorUiTabControllerFactory();
  ~MockActorUiTabControllerFactory() override;

  std::unique_ptr<HandoffButtonController> CreateHandoffButtonController(
      tabs::TabInterface& tab) override;

  MockHandoffButtonController* handoff_button_controller();

 private:
  raw_ptr<MockHandoffButtonController> mock_handoff_button_controller_ =
      nullptr;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_ACTOR_UI_TAB_CONTROLLER_FACTORY_H_
