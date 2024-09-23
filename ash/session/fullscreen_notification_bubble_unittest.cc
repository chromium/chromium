// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/fullscreen_notification_bubble.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr int kExpectedAutoHideDelayInSeconds = 4;

class FullscreenNotificationBubbleTest : public AshTestBase {
 public:
  FullscreenNotificationBubbleTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  FullscreenNotificationBubbleTest(const FullscreenNotificationBubbleTest&) =
      delete;
  FullscreenNotificationBubbleTest& operator=(
      const FullscreenNotificationBubbleTest&) = delete;

  ~FullscreenNotificationBubbleTest() override {}

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Create a test window in full screen mode.
    window_ = CreateTestWindow();
    window_->SetProperty(aura::client::kShowStateKey,
                         ui::mojom::WindowShowState::kFullscreen);
    window_state_ = WindowState::Get(window_.get());

    bubble_ = std::make_unique<FullscreenNotificationBubble>();
  }

  void TearDown() override {
    window_.reset();
    bubble_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<aura::Window> window_;
  std::unique_ptr<FullscreenNotificationBubble> bubble_;

  raw_ptr<WindowState, DanglingUntriaged> window_state_ = nullptr;
};

TEST_F(FullscreenNotificationBubbleTest, AutoHideBubbleAfterDelay) {
  views::Widget* widget = bubble_->widget_for_test();
  EXPECT_FALSE(widget->IsVisible());

  bubble_->ShowForWindowState(window_state_);
  EXPECT_TRUE(widget->IsVisible());

  // The bubble is still visible if the timer has not yet elapsed.
  task_environment()->FastForwardBy(
      base::Seconds(kExpectedAutoHideDelayInSeconds - 1));
  EXPECT_TRUE(widget->IsVisible());

  // The bubble is automatically hidden after the timer has elapsed.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(widget->IsVisible());
}

TEST_F(FullscreenNotificationBubbleTest, HideBubbleOnExitFullscreen) {
  views::Widget* widget = bubble_->widget_for_test();
  EXPECT_FALSE(widget->IsVisible());

  bubble_->ShowForWindowState(window_state_);
  EXPECT_TRUE(widget->IsVisible());

  // The bubble is hidden early if the user exits full screen mode via full
  // screen key.
  PressAndReleaseKey(ui::VKEY_ZOOM);
  EXPECT_FALSE(widget->IsVisible());
}

TEST_F(FullscreenNotificationBubbleTest, HandleWindowDestruction) {
  views::Widget* widget = bubble_->widget_for_test();
  EXPECT_FALSE(widget->IsVisible());

  bubble_->ShowForWindowState(window_state_);
  EXPECT_TRUE(widget->IsVisible());

  // Destroy the window before the timer is elapsed.
  window_.reset();
  EXPECT_FALSE(widget->IsVisible());

  task_environment()->FastForwardBy(
      base::Seconds(kExpectedAutoHideDelayInSeconds));
  EXPECT_FALSE(widget->IsVisible());
}

}  // namespace
}  // namespace ash
