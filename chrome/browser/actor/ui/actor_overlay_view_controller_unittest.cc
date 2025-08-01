// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_view_controller.h"

#include "chrome/browser/actor/ui/mock_actor_ui_tab_controller.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {

class FakeActorOverlayViewController : public ActorOverlayViewController {
 public:
  explicit FakeActorOverlayViewController(tabs::TabInterface& tab_interface)
      : ActorOverlayViewController(tab_interface) {
    mock_tab_controller_ = std::make_unique<MockActorUiTabController>();
  }

  MockActorUiTabController* GetTabController() override {
    return mock_tab_controller_.get();
  }

 private:
  std::unique_ptr<MockActorUiTabController> mock_tab_controller_;
};

// This test suite focuses solely on verifying the ActorOverlayViewController's
// implementation of the mojom::ActorOverlayPageHandler interface.
class ActorOverlayViewControllerTest : public testing::Test {
 public:
  ActorOverlayViewControllerTest() {
    overlay_view_controller =
        std::make_unique<FakeActorOverlayViewController>(mock_tab);
  }

 protected:
  tabs::MockTabInterface mock_tab;
  std::unique_ptr<FakeActorOverlayViewController> overlay_view_controller;
};

TEST_F(ActorOverlayViewControllerTest, OnHoverStatusChanged) {
  EXPECT_CALL(*overlay_view_controller->GetTabController(),
              SetOverlayHoverStatus(true))
      .Times(1);
  EXPECT_CALL(*overlay_view_controller->GetTabController(),
              SetOverlayHoverStatus(false))
      .Times(1);
  overlay_view_controller->OnHoverStatusChanged(true);
  overlay_view_controller->OnHoverStatusChanged(false);
}

}  // namespace
}  // namespace actor::ui
