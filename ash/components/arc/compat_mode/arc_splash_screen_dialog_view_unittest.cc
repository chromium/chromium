// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/arc_splash_screen_dialog_view.h"

#include <memory>

#include "ash/components/arc/compat_mode/test/compat_mode_test_base.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/widget/widget.h"

namespace arc {

class ArcSplashScreenDialogViewTest : public CompatModeTestBase {
 public:
  ArcSplashScreenDialogViewTest() = default;
  ArcSplashScreenDialogViewTest(const ArcSplashScreenDialogViewTest& other) =
      delete;
  ArcSplashScreenDialogViewTest& operator=(
      const ArcSplashScreenDialogViewTest& other) = delete;
  ~ArcSplashScreenDialogViewTest() override = default;

  // CompatModeTestBase:
  void SetUp() override {
    CompatModeTestBase::SetUp();
    parent_widget_ = CreateWidget();
    EnsureAnchor();
  }

  void TearDown() override {
    parent_widget_->CloseNow();
    parent_widget_.reset();
    CompatModeTestBase::TearDown();
  }

 protected:
  views::Widget* ShowAsBubble(
      std::unique_ptr<ArcSplashScreenDialogView> dialog_view) {
    auto* const widget =
        views::BubbleDialogDelegateView::CreateBubble(std::move(dialog_view));
    widget->Show();
    return widget;
  }

  void EnsureAnchor() {
    if (anchor_)
      return;
    anchor_ = parent_widget_->GetRootView()->AddChildView(
        std::make_unique<views::View>());
  }
  void RemoveAnchor() {
    parent_widget_->GetRootView()->RemoveChildViewT(anchor_.get());
    anchor_ = nullptr;
  }

  views::View* anchor() { return anchor_; }
  aura::Window* parent_window() { return parent_widget_->GetNativeView(); }
  views::Widget* parent_widget() { return parent_widget_.get(); }

 private:
  ash::AshColorProvider ash_color_provider_;
  std::unique_ptr<views::Widget> parent_widget_;
  raw_ptr<views::View, DanglingUntriaged> anchor_{nullptr};
};

TEST_F(ArcSplashScreenDialogViewTest, TestCloseButton) {
  for (const bool is_for_unresizable : {true, false}) {
    bool on_close_callback_called = false;
    auto dialog_view = std::make_unique<ArcSplashScreenDialogView>(
        base::BindLambdaForTesting([&]() { on_close_callback_called = true; }),
        parent_window(), anchor(), is_for_unresizable);
    ArcSplashScreenDialogView::TestApi dialog_view_test(dialog_view.get());
    ShowAsBubble(std::move(dialog_view));
    EXPECT_TRUE(dialog_view_test.close_button()->GetVisible());
    EXPECT_FALSE(on_close_callback_called);
    LeftClickOnView(parent_widget(), dialog_view_test.close_button());
    EXPECT_TRUE(on_close_callback_called);
  }
}

TEST_F(ArcSplashScreenDialogViewTest, TestEscKey) {
  for (const bool is_for_unresizable : {true, false}) {
    bool on_close_callback_called = false;
    auto dialog_view = std::make_unique<ArcSplashScreenDialogView>(
        base::BindLambdaForTesting([&]() { on_close_callback_called = true; }),
        parent_window(), anchor(), is_for_unresizable);
    ArcSplashScreenDialogView::TestApi dialog_view_test(dialog_view.get());
    auto* const bubble = ShowAsBubble(std::move(dialog_view));
    EXPECT_FALSE(on_close_callback_called);
    EXPECT_TRUE(
        anchor()->GetIndexOf(dialog_view_test.highlight_border()).has_value());

    // Simulates esc key event to close the dialog.
    ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                       ui::EF_NONE);
    bubble->OnKeyEvent(&event);

    EXPECT_TRUE(on_close_callback_called);
    EXPECT_FALSE(
        anchor()->GetIndexOf(dialog_view_test.highlight_border()).has_value());
  }
}

TEST_F(ArcSplashScreenDialogViewTest, TestAnchorHighlight) {
  for (const bool is_for_unresizable : {true, false}) {
    auto dialog_view = std::make_unique<ArcSplashScreenDialogView>(
        base::DoNothing(), parent_window(), anchor(), is_for_unresizable);
    ArcSplashScreenDialogView::TestApi dialog_view_test(dialog_view.get());
    ShowAsBubble(std::move(dialog_view));
    EXPECT_TRUE(
        anchor()->GetIndexOf(dialog_view_test.highlight_border()).has_value());
    LeftClickOnView(parent_widget(), dialog_view_test.close_button());
    EXPECT_FALSE(
        anchor()->GetIndexOf(dialog_view_test.highlight_border()).has_value());
  }
}

TEST_F(ArcSplashScreenDialogViewTest, TestAnchorDestroy) {
  for (const bool is_for_unresizable : {true, false}) {
    EnsureAnchor();
    auto dialog_view = std::make_unique<ArcSplashScreenDialogView>(
        base::DoNothing(), parent_window(), anchor(), is_for_unresizable);
    ArcSplashScreenDialogView::TestApi dialog_view_test(dialog_view.get());
    ShowAsBubble(std::move(dialog_view));

    // Removing the anchor from view hierarchy makes the anchor destroyed.
    RemoveAnchor();

    // Verify that clicking the button won't cause any crash even after
    // destroying the anchor.
    LeftClickOnView(parent_widget(), dialog_view_test.close_button());
  }
}

TEST_F(ArcSplashScreenDialogViewTest,
       TestSplashScreenInFullscreenOrMaximinzedWindow) {
  for (const bool is_for_unresizable : {true, false}) {
    for (const auto state : {ui::mojom::WindowShowState::kFullscreen,
                             ui::mojom::WindowShowState::kMaximized}) {
      bool on_close_callback_called = false;
      auto dialog_view = std::make_unique<ArcSplashScreenDialogView>(
          base::BindLambdaForTesting(
              [&]() { on_close_callback_called = true; }),
          parent_window(), anchor(), is_for_unresizable);
      ShowAsBubble(std::move(dialog_view));
      EXPECT_FALSE(on_close_callback_called);
      parent_window()->SetProperty(aura::client::kShowStateKey, state);
      EXPECT_TRUE(on_close_callback_called);
    }
  }
}

// Test that the activation is forwarded to the bubble when the parent window is
// activated.
TEST_F(ArcSplashScreenDialogViewTest, TestForwardActivation) {
  auto* const bubble = ShowAsBubble(std::make_unique<ArcSplashScreenDialogView>(
      base::DoNothing(), parent_window(), anchor(),
      /*is_for_unresizable=*/false));

  EXPECT_TRUE(bubble->IsActive());
  EXPECT_FALSE(parent_widget()->IsActive());

  parent_widget()->Activate();
  EXPECT_FALSE(bubble->IsActive());
  EXPECT_TRUE(parent_widget()->IsActive());

  RunPendingMessages();
  EXPECT_TRUE(bubble->IsActive());
  EXPECT_FALSE(parent_widget()->IsActive());
}

}  // namespace arc
