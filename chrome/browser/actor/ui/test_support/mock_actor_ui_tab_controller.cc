// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/test_support/mock_actor_ui_tab_controller.h"

#include <utility>

#include "base/functional/callback_helpers.h"

namespace actor::ui {

MockActorUiTabController::MockActorUiTabController(tabs::TabInterface& tab)
    : ActorUiTabControllerInterface(tab) {
  ON_CALL(*this, GetWeakPtr())
      .WillByDefault(testing::Return(weak_factory_.GetWeakPtr()));
  ON_CALL(*this, OnUiTabStateChange)
      .WillByDefault([](const UiTabState&, UiResultCallback callback) {
        if (callback) {
          std::move(callback).Run(true);
        }
      });
  ON_CALL(*this, GetCurrentUiTabState())
      .WillByDefault(testing::Return(UiTabState{}));
}

MockActorUiTabController::~MockActorUiTabController() = default;

void MockActorUiTabController::SetupDefaultBrowserWindow(
    tabs::MockTabInterface& mock_tab,
    MockBrowserWindowInterface& mock_browser_window_interface,
    ::ui::UnownedUserDataHost& user_data_host) {
  ON_CALL(mock_tab, GetBrowserWindowInterface())
      .WillByDefault(testing::Return(&mock_browser_window_interface));
  ON_CALL(mock_tab, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));
  ON_CALL(mock_browser_window_interface, GetUnownedUserDataHost)
      .WillByDefault(testing::ReturnRef(user_data_host));
}

}  // namespace actor::ui
