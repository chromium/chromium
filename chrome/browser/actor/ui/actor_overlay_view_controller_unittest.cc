// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_view_controller.h"

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/mocks/mock_actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/mocks/mock_actor_ui_tab_controller_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {

// This test suite focuses solely on verifying the ActorOverlayViewController's
// implementation of the mojom::ActorOverlayPageHandler interface.
class ActorOverlayViewControllerTest : public testing::Test {
 public:
  ActorOverlayViewControllerTest() {
    overlay_view_controller_ =
        std::make_unique<ActorOverlayViewController>(mock_tab_);
    MockActorUiTabController::SetupDefaultBrowserWindow(
        mock_tab_, mock_browser_window_interface_, user_data_host_);

    mock_actor_ui_tab_controller_.emplace(mock_tab_);
  }

  MockActorUiTabController* mock_actor_ui_tab_controller() {
    return &mock_actor_ui_tab_controller_.value();
  }

 protected:
  ::ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  std::unique_ptr<ActorOverlayViewController> overlay_view_controller_;
  std::optional<MockActorUiTabController> mock_actor_ui_tab_controller_;
};

TEST_F(ActorOverlayViewControllerTest, OnHoverStatusChanged) {
  EXPECT_CALL(*mock_actor_ui_tab_controller(), SetOverlayHoverStatus(true))
      .Times(1);
  EXPECT_CALL(*mock_actor_ui_tab_controller(), SetOverlayHoverStatus(false))
      .Times(1);
  overlay_view_controller_->OnHoverStatusChanged(true);
  overlay_view_controller_->OnHoverStatusChanged(false);
}

}  // namespace
}  // namespace actor::ui
