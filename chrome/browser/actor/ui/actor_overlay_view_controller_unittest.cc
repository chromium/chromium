// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_view_controller.h"

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/mocks/mock_actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/mocks/mock_actor_ui_tab_controller_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {

// Fake implementation for the ActorOverlayPage interface.
class FakeActorOverlayPage : public mojom::ActorOverlayPage {
 public:
  FakeActorOverlayPage() = default;
  ~FakeActorOverlayPage() override = default;

  mojo::PendingRemote<mojom::ActorOverlayPage> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  // mojom::ActorOverlayPage
  void SetScrimBackground(bool is_visible) override {
    is_scrim_background_visible_ = is_visible;
    set_scrim_background_call_count_++;
  }

  // Test accessors
  bool is_scrim_background_visible() { return is_scrim_background_visible_; }

  int scrim_background_call_count() { return set_scrim_background_call_count_; }

 private:
  mojo::Receiver<mojom::ActorOverlayPage> receiver_{this};
  bool is_scrim_background_visible_ = false;
  int set_scrim_background_call_count_ = 0;
};

// This test suite focuses solely on verifying the ActorOverlayViewController's
// implementation of the mojom::ActorOverlayPageHandler and
// mojom::ActorOverlayPage interfaces.
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
  content::BrowserTaskEnvironment task_environment_;
  ::ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  std::unique_ptr<ActorOverlayViewController> overlay_view_controller_;
  std::optional<MockActorUiTabController> mock_actor_ui_tab_controller_;
};

TEST_F(ActorOverlayViewControllerTest, OnHoverStatusChanged) {
  EXPECT_CALL(*mock_actor_ui_tab_controller(), OnOverlayHoverStatusChanged())
      .Times(2);
  overlay_view_controller_->OnHoverStatusChanged(true);
  overlay_view_controller_->OnHoverStatusChanged(false);
  // Verify that if the same hover status is sent, we return early and don't
  // call the tab controller's OnOverlayHoverStatusChanged function.
  overlay_view_controller_->OnHoverStatusChanged(false);
}

TEST_F(ActorOverlayViewControllerTest, SetScrimBackground) {
  FakeActorOverlayPage fake_page;
  mojo::Remote<mojom::ActorOverlayPageHandler> handler_remote;
  overlay_view_controller_->BindOverlay(
      fake_page.BindAndGetRemote(),
      handler_remote.BindNewPipeAndPassReceiver());

  overlay_view_controller_->SetScrimBackground(true);
  fake_page.FlushForTesting();

  EXPECT_TRUE(fake_page.is_scrim_background_visible());
  EXPECT_EQ(fake_page.scrim_background_call_count(), 1);

  overlay_view_controller_->SetScrimBackground(false);
  fake_page.FlushForTesting();

  EXPECT_FALSE(fake_page.is_scrim_background_visible());
  EXPECT_EQ(fake_page.scrim_background_call_count(), 2);
}

}  // namespace
}  // namespace actor::ui
