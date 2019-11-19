// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/shell.h"
#include "ash/system/palette/mock_palette_tool_delegate.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/palette/palette_tool.h"
#include "ash/system/palette/tools/capture_region_mode.h"
#include "ash/system/palette/tools/capture_screen_action.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_screenshot_delegate.h"
#include "ash/utility/screenshot_controller.h"
#include "base/macros.h"
#include "ui/events/test/event_generator.h"

namespace ash {

// Base class for all screenshot pallette tools tests.
class ScreenshotToolTest : public AshTestBase {
 public:
  ScreenshotToolTest() = default;
  ~ScreenshotToolTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    palette_tool_delegate_ = std::make_unique<MockPaletteToolDelegate>();
  }

 protected:
  std::unique_ptr<MockPaletteToolDelegate> palette_tool_delegate_;

  bool IsPartialScreenshotActive() {
    return static_cast<bool>(
        Shell::Get()->screenshot_controller()->on_screenshot_session_done_);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenshotToolTest);
};

// Verifies that capturing a region triggers the partial screenshot delegate
// method. Invoking the callback passed to the delegate disables the tool.
TEST_F(ScreenshotToolTest, EnablingCaptureRegionCallsDelegateAndDisablesTool) {
  std::unique_ptr<PaletteTool> tool =
      std::make_unique<CaptureRegionMode>(palette_tool_delegate_.get());

  // Starting a partial screenshot calls the calls the palette delegate to start
  // a screenshot session and hides the palette.
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePalette());
  tool->OnEnable();
  EXPECT_TRUE(IsPartialScreenshotActive());
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());

  // Simulate region selection.
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::CAPTURE_REGION));

  const gfx::Rect selection(100, 200, 300, 399);
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->EnterPenPointerMode();
  event_generator->MoveTouch(selection.origin());
  event_generator->PressTouch();
  event_generator->MoveTouch(gfx::Point(selection.right(), selection.bottom()));
  event_generator->ReleaseTouch();

  EXPECT_FALSE(IsPartialScreenshotActive());
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_partial_screenshot_count());
  EXPECT_EQ(selection.ToString(),
            GetScreenshotDelegate()->last_rect().ToString());
  testing::Mock::VerifyAndClearExpectations(palette_tool_delegate_.get());

  // Enable the tool again
  tool->OnEnable();
  EXPECT_TRUE(IsPartialScreenshotActive());
  // Calling the associated callback (partial screenshot finished) will disable
  // the tool.
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::CAPTURE_REGION));
  Shell::Get()->screenshot_controller()->CancelScreenshotSession();
  EXPECT_FALSE(IsPartialScreenshotActive());
}

// Verifies that capturing the screen triggers the screenshot delegate method,
// disables the tool, and hides the palette.
TEST_F(ScreenshotToolTest, EnablingCaptureScreenCallsDelegateAndDisablesTool) {
  std::unique_ptr<PaletteTool> tool =
      std::make_unique<CaptureScreenAction>(palette_tool_delegate_.get());
  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::CAPTURE_SCREEN));
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePaletteImmediately());
  tool->OnEnable();
  EXPECT_EQ(1, GetScreenshotDelegate()->handle_take_screenshot_count());
}

}  // namespace ash
