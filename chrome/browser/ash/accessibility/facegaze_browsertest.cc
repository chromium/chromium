// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/facegaze_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

class FaceGazeIntegrationTest : public AccessibilityFeatureBrowserTest {
 public:
  FaceGazeIntegrationTest() = default;
  ~FaceGazeIntegrationTest() override = default;
  FaceGazeIntegrationTest(const FaceGazeIntegrationTest&) = delete;
  FaceGazeIntegrationTest& operator=(const FaceGazeIntegrationTest&) = delete;

 protected:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    utils_ = std::make_unique<FaceGazeTestUtils>();
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityFaceGaze);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        Shell::Get()->GetPrimaryRootWindow());
    display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
        .UpdateDisplay("1200x800");

    // Initialize FaceGaze.
    utils_->EnableFaceGaze();
    utils_->CreateFaceLandmarker();
    utils_->InitializeBufferSizeAndAcceleration(1);
    utils_->InitializeGesturesToMacros(
        base::Value::Dict().Set("jawOpen", /*RESET_CURSOR*/ 37));
    SetMouseSourceDeviceId(1);
    // By default the mouse is placed at the center of the screen. To initialize
    // FaceGaze at the center of the screen, move the mouse somewhere, then
    // move it back to the center.
    SendMouseMoveTo(gfx::Point(0, 0));
    utils_->WaitForMousePosition(gfx::Point(0, 0));
    SendMouseMoveTo(gfx::Point(600, 400));
    utils_->WaitForMousePosition(gfx::Point(600, 400));

    // No matter the starting location, the cursor position won't change
    // initially, and upcoming forehead locations will be computed relative to
    // this.
    utils_->ProcessFaceLandmarkerResult(
        FaceGazeTestUtils::MockFaceLandmarkerResult()
            .WithNormalizedForeheadLocation(0.1, 0.2));
    utils_->TriggerMouseControllerInterval();
    ASSERT_EQ(gfx::Point(600, 400),
              display::Screen::GetScreen()->GetCursorScreenPoint());
  }

  void SendMouseMoveTo(const gfx::Point& location) {
    event_generator_->MoveMouseTo(location.x(), location.y());
  }

  void SetMouseSourceDeviceId(int id) {
    event_generator_->set_mouse_source_device_id(id);
  }

  FaceGazeTestUtils* utils() { return utils_.get(); }

 private:
  std::unique_ptr<FaceGazeTestUtils> utils_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, UpdateMouseLocation) {
  utils()->ProcessFaceLandmarkerResult(
      FaceGazeTestUtils::MockFaceLandmarkerResult()
          .WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();

  // Verify mouse location.
  ASSERT_EQ(gfx::Point(360, 560),
            display::Screen::GetScreen()->GetCursorScreenPoint());
}

IN_PROC_BROWSER_TEST_F(FaceGazeIntegrationTest, ResetCursor) {
  // Move mouse to location.
  utils()->ProcessFaceLandmarkerResult(
      FaceGazeTestUtils::MockFaceLandmarkerResult()
          .WithNormalizedForeheadLocation(0.11, 0.21));
  utils()->TriggerMouseControllerInterval();
  ASSERT_EQ(gfx::Point(360, 560),
            display::Screen::GetScreen()->GetCursorScreenPoint());

  // Reset the mouse to the center of the screen using a gesture.
  utils()->ProcessFaceLandmarkerResult(
      FaceGazeTestUtils::MockFaceLandmarkerResult().WithGesture("jawOpen",
                                                                0.9));
  ASSERT_EQ(gfx::Point(600, 400),
            display::Screen::GetScreen()->GetCursorScreenPoint());
}

}  // namespace ash
