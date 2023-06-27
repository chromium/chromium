// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"

#include "content/public/browser/picture_in_picture_window_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"

namespace {

class MockPictureInPictureWindowController
    : public content::PictureInPictureWindowController {
 public:
  MockPictureInPictureWindowController() = default;
  MockPictureInPictureWindowController(
      const MockPictureInPictureWindowController&) = delete;
  MockPictureInPictureWindowController& operator=(
      const MockPictureInPictureWindowController&) = delete;
  ~MockPictureInPictureWindowController() override = default;

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, FocusInitiator, (), (override));
  MOCK_METHOD(void, Close, (bool), (override));
  MOCK_METHOD(void, CloseAndFocusInitiator, (), (override));
  MOCK_METHOD(void, OnWindowDestroyed, (bool), (override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (override));
  MOCK_METHOD(absl::optional<gfx::Rect>, GetWindowBounds, (), (override));
  MOCK_METHOD(content::WebContents*, GetChildWebContents, (), (override));
};

using PictureInPictureWindowManagerTest = testing::Test;

}  // namespace

TEST_F(PictureInPictureWindowManagerTest, RespectsMinAndMaxSize) {
  // The max window size should be 80% of the screen.
  display::Display display(/*id=*/1, gfx::Rect(0, 0, 1000, 1000));
  EXPECT_EQ(gfx::Size(800, 800),
            PictureInPictureWindowManager::GetMaximumWindowSize(display));

  // The initial bounds of the PiP window should respect that.
  blink::mojom::PictureInPictureWindowOptions pip_options;
  pip_options.width = 900;
  pip_options.height = 900;
  EXPECT_EQ(
      gfx::Size(800, 800),
      PictureInPictureWindowManager::
          CalculateInitialPictureInPictureWindowBounds(pip_options, display)
              .size());

  // The minimum size should also be respected.
  pip_options.width = 100;
  pip_options.height = 500;
  EXPECT_EQ(
      gfx::Size(300, 500),
      PictureInPictureWindowManager::
          CalculateInitialPictureInPictureWindowBounds(pip_options, display)
              .size());

  // An extremely small aspect ratio should still respect minimum width and
  // maximum height.
  pip_options.width = 0;
  pip_options.height = 0;
  pip_options.initial_aspect_ratio = 0.00000001;
  EXPECT_EQ(
      gfx::Size(300, 800),
      PictureInPictureWindowManager::
          CalculateInitialPictureInPictureWindowBounds(pip_options, display)
              .size());

  // An extremely large aspect ratio should still respect maximum width and
  // minimum height.
  pip_options.initial_aspect_ratio = 100000;
  EXPECT_EQ(
      gfx::Size(800, 300),
      PictureInPictureWindowManager::
          CalculateInitialPictureInPictureWindowBounds(pip_options, display)
              .size());
}

TEST_F(PictureInPictureWindowManagerTest,
       ExitPictureInPictureReturnsFalseWhenThereIsNoWindow) {
  EXPECT_FALSE(
      PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture());
}

TEST_F(PictureInPictureWindowManagerTest,
       ExitPictureInPictureReturnsTrueAndClosesWindow) {
  MockPictureInPictureWindowController controller;
  PictureInPictureWindowManager::GetInstance()
      ->EnterPictureInPictureWithController(&controller);
  EXPECT_CALL(controller, Close(/*should_pause_video=*/false));
  EXPECT_TRUE(
      PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture());
}
