// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_overlay_handler.h"

#include "base/test/test_future.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/mocks/mock_actor_ui_tab_controller.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_util_desktop.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor::ui {
namespace {

using ::testing::_;
using ::testing::Return;

// Fake implementation for the ActorOverlayPage interface.
class FakeActorOverlayPage : public mojom::ActorOverlayPage {
 public:
  FakeActorOverlayPage() = default;
  ~FakeActorOverlayPage() override = default;

  mojo::PendingRemote<mojom::ActorOverlayPage> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  void ResetCounters() {
    set_scrim_background_call_count_ = 0;
    set_border_glow_call_count_ = 0;
    theme_call_count_ = 0;
  }

  // mojom::ActorOverlayPage
  void SetScrimBackground(bool is_visible) override {
    is_scrim_background_visible_ = is_visible;
    set_scrim_background_call_count_++;
  }

  // mojom::ActorOverlayPage
  void SetBorderGlowVisibility(bool is_visible) override {
    is_border_glow_visible_ = is_visible;
    set_border_glow_call_count_++;
  }

  // mojom::ActorOverlayPage
  void SetTheme(mojom::ThemePtr theme) override { theme_call_count_++; }

  // Test accessors
  bool is_scrim_background_visible() { return is_scrim_background_visible_; }
  int scrim_background_call_count() { return set_scrim_background_call_count_; }
  bool is_border_glow_visible() { return is_border_glow_visible_; }
  int border_glow_call_count() { return set_border_glow_call_count_; }
  int theme_call_count() { return theme_call_count_; }

 private:
  mojo::Receiver<mojom::ActorOverlayPage> receiver_{this};
  bool is_scrim_background_visible_ = false;
  int set_scrim_background_call_count_ = 0;
  bool is_border_glow_visible_ = false;
  int set_border_glow_call_count_ = 0;
  int theme_call_count_ = 0;
};

class ActorOverlayHandlerTest : public testing::Test {
 public:
  ActorOverlayHandlerTest() {
    profile_ = TestingProfile::Builder().Build();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));
    handler_ = std::make_unique<ActorOverlayHandler>(
        fake_page_.BindAndGetRemote(),
        handler_remote_.BindNewPipeAndPassReceiver(), web_contents_.get());
    MockActorUiTabController::SetupDefaultBrowserWindow(
        mock_tab_, mock_browser_window_interface_, user_data_host_);

    mock_actor_ui_tab_controller_.emplace(mock_tab_);
    webui::SetTabInterface(web_contents_.get(), &mock_tab_);
  }

  MockActorUiTabController* mock_actor_ui_tab_controller() {
    return &mock_actor_ui_tab_controller_.value();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  FakeActorOverlayPage fake_page_;
  mojo::Remote<mojom::ActorOverlayPageHandler> handler_remote_;
  std::unique_ptr<TestingProfile> profile_;
  ::ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ActorOverlayHandler> handler_;
  std::optional<MockActorUiTabController> mock_actor_ui_tab_controller_;
};

TEST_F(ActorOverlayHandlerTest, OnHoverStatusChanged) {
  EXPECT_CALL(*mock_actor_ui_tab_controller(), OnOverlayHoverStatusChanged)
      .Times(2);
  handler_->OnHoverStatusChanged(true);
  handler_->OnHoverStatusChanged(false);
  // Verify that if the same hover status is sent, we return early and don't
  // call the tab controller's OnOverlayHoverStatusChanged function.
  handler_->OnHoverStatusChanged(false);
}

TEST_F(ActorOverlayHandlerTest, GetCurrentBorderGlowVisibility) {
  base::test::TestFuture<bool> future;
  UiTabState ui_tab_state;

  ui_tab_state.actor_overlay.border_glow_visible = true;
  EXPECT_CALL(*mock_actor_ui_tab_controller(), GetCurrentUiTabState())
      .WillOnce(Return(ui_tab_state));
  handler_->GetCurrentBorderGlowVisibility(future.GetCallback());
  EXPECT_TRUE(future.Take());

  ui_tab_state.actor_overlay.border_glow_visible = false;
  EXPECT_CALL(*mock_actor_ui_tab_controller(), GetCurrentUiTabState())
      .WillOnce(Return(ui_tab_state));
  handler_->GetCurrentBorderGlowVisibility(future.GetCallback());
  EXPECT_FALSE(future.Take());
}

TEST_F(ActorOverlayHandlerTest, SetScrimBackground) {
  handler_->SetOverlayBackground(true);
  fake_page_.FlushForTesting();

  EXPECT_TRUE(fake_page_.is_scrim_background_visible());
  EXPECT_EQ(fake_page_.scrim_background_call_count(), 1);

  handler_->SetOverlayBackground(false);
  fake_page_.FlushForTesting();

  EXPECT_FALSE(fake_page_.is_scrim_background_visible());
  EXPECT_EQ(fake_page_.scrim_background_call_count(), 2);
}

TEST_F(ActorOverlayHandlerTest, SetBorderGlowVisibility) {
  handler_->SetBorderGlowVisibility(true);
  fake_page_.FlushForTesting();

  EXPECT_TRUE(fake_page_.is_border_glow_visible());
  EXPECT_EQ(fake_page_.border_glow_call_count(), 1);

  handler_->SetBorderGlowVisibility(false);
  fake_page_.FlushForTesting();

  EXPECT_FALSE(fake_page_.is_border_glow_visible());
  EXPECT_EQ(fake_page_.border_glow_call_count(), 2);
}

TEST_F(ActorOverlayHandlerTest, OnThemeChanged) {
  // Setting up the first time calls set theme once so we reset the counters.
  fake_page_.FlushForTesting();
  fake_page_.ResetCounters();
  // Flag off
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitAndDisableFeature(features::kActorUiThemed);
    webui::GetNativeThemeDeprecated(web_contents_.get())
        ->NotifyOnNativeThemeUpdated();
    fake_page_.FlushForTesting();

    EXPECT_EQ(fake_page_.theme_call_count(), 0);
  }
  // Flag on
  {
    base::test::ScopedFeatureList scoped_features;
    scoped_features.InitAndEnableFeatureWithParameters(features::kActorUiThemed,
                                                       {});
    webui::GetNativeThemeDeprecated(web_contents_.get())
        ->NotifyOnNativeThemeUpdated();
    fake_page_.FlushForTesting();

    EXPECT_EQ(fake_page_.theme_call_count(), 1);
  }
}

TEST_F(ActorOverlayHandlerTest, HandlesNullTab) {
  // Verify that when the tab controller is null, we don't send the hover status
  // change.
  webui::SetTabInterface(web_contents_.get(), nullptr);
  EXPECT_CALL(*mock_actor_ui_tab_controller(), OnOverlayHoverStatusChanged)
      .Times(0);
  handler_->OnHoverStatusChanged(true);

  // Verify that when the tab controller is null, we don't try to receive the
  // current border glow visibility state.
  base::test::TestFuture<bool> future;
  EXPECT_CALL(*mock_actor_ui_tab_controller(), GetCurrentUiTabState()).Times(0);
  handler_->GetCurrentBorderGlowVisibility(future.GetCallback());
  EXPECT_FALSE(future.Take());
}

}  // namespace
}  // namespace actor::ui
