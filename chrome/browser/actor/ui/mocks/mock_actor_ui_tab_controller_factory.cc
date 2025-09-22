// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/mocks/mock_actor_ui_tab_controller_factory.h"

#include "chrome/browser/actor/ui/mocks/mock_handoff_button_controller.h"

namespace actor::ui {

MockActorUiTabControllerFactory::MockActorUiTabControllerFactory() = default;

MockActorUiTabControllerFactory::~MockActorUiTabControllerFactory() {
  mock_handoff_button_controller_ = nullptr;
}

std::unique_ptr<HandoffButtonController>
MockActorUiTabControllerFactory::CreateHandoffButtonController(
    tabs::TabInterface& tab) {
  auto controller = std::make_unique<MockHandoffButtonController>(tab);
  mock_handoff_button_controller_ = controller.get();
  return controller;
}

MockHandoffButtonController*
MockActorUiTabControllerFactory::handoff_button_controller() {
  return mock_handoff_button_controller_;
}

}  // namespace actor::ui
