// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_highlight_controller.h"

#include <stdint.h>

#include <cmath>
#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/ui/accessibility_cursor_ring_layer.h"
#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class MockTextInputClient : public ui::FakeTextInputClient {
 public:
  MockTextInputClient() : ui::FakeTextInputClient(ui::TEXT_INPUT_TYPE_TEXT) {}

  MockTextInputClient(const MockTextInputClient&) = delete;
  MockTextInputClient& operator=(const MockTextInputClient&) = delete;

  ~MockTextInputClient() override = default;

  void SetCaretBounds(const gfx::Rect& bounds) { caret_bounds_ = bounds; }

 private:
  gfx::Rect GetCaretBounds() const override { return caret_bounds_; }

  gfx::Rect caret_bounds_;
};

}  // namespace

class AccessibilityHighlightControllerTest : public AshTestBase {
 public:
  AccessibilityHighlightControllerTest(
      const AccessibilityHighlightControllerTest&) = delete;
  AccessibilityHighlightControllerTest& operator=(
      const AccessibilityHighlightControllerTest&) = delete;

 protected:
  AccessibilityHighlightControllerTest() = default;
  ~AccessibilityHighlightControllerTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnablePixelOutputInTests);
    AshTestBase::SetUp();
    Shell::Get()->accessibility_focus_ring_controller()->SetNoFadeForTesting();
  }

  void CaptureBeforeImage(const gfx::Rect& bounds) {
    Capture(bounds, &before_);
  }

  void CaptureAfterImage(const gfx::Rect& bounds) { Capture(bounds, &after_); }

  void ComputeImageStats() {
    double accum[4] = {0, 0, 0, 0};
    SkBitmap before = before_.AsBitmap();
    SkBitmap after = after_.AsBitmap();
    for (int x = 0; x < before.width(); ++x) {
      for (int y = 0; y < before.height(); ++y) {
        SkColor before_color = before.getColor(x, y);
        SkColor after_color = after.getColor(x, y);
        if (before_color != after_color) {
          ++diff_count_;
          accum[0] += SkColorGetB(after_color);
          accum[1] += SkColorGetG(after_color);
          accum[2] += SkColorGetR(after_color);
          accum[3] += SkColorGetA(after_color);
        }
      }
    }
    if (diff_count_ > 0) {
      average_diff_color_ =
          SkColorSetARGB(static_cast<uint8_t>(accum[3] / diff_count_),
                         static_cast<uint8_t>(accum[2] / diff_count_),
                         static_cast<uint8_t>(accum[1] / diff_count_),
                         static_cast<uint8_t>(accum[0] / diff_count_));
    }
  }

  int diff_count() const { return diff_count_; }
  SkColor average_diff_color() const { return average_diff_color_; }

  void Capture(const gfx::Rect& bounds, gfx::Image* image) {
    // Occasionally we don't get any pixels the first try.
    while (true) {
      aura::Window* window = Shell::GetPrimaryRootWindow();
      const auto on_got_snapshot = [](base::RunLoop* run_loop,
                                      gfx::Image* image, gfx::Image got_image) {
        *image = got_image;
        run_loop->Quit();
      };
      base::RunLoop run_loop;
      ui::GrabWindowSnapshot(window, bounds,
                             base::BindOnce(on_got_snapshot, &run_loop, image));
      run_loop.Run();
      if (image->Size() != bounds.size()) {
        LOG(INFO) << "Bitmap not correct size, trying to capture again";
        continue;
      }
      if (SkColorGetA(image->AsBitmap().getColor(0, 0)) != SK_AlphaOPAQUE) {
        LOG(INFO) << "Bitmap not opaque, trying to capture again";
        continue;
      }
      break;
    }
  }

 private:
  gfx::Image before_;
  gfx::Image after_;
  int diff_count_ = 0;
  SkColor average_diff_color_ = SK_ColorTRANSPARENT;
};

TEST_F(AccessibilityHighlightControllerTest, TestCaretRingDrawsBluePixels) {
  // Create a white background window for captured image color smoke test.
  std::unique_ptr<aura::Window> window =
      TestWindowBuilder()
          .SetColorWindowDelegate(SK_ColorWHITE)
          .SetBounds(Shell::GetPrimaryRootWindow()->bounds())
          .Build();

  gfx::Rect capture_bounds(200, 300, 100, 100);
  gfx::Rect caret_bounds(230, 330, 1, 25);

  CaptureBeforeImage(capture_bounds);

  AccessibilityHighlightController controller;
  controller.HighlightCaret(true);
  MockTextInputClient text_input_client;
  text_input_client.SetCaretBounds(caret_bounds);
  controller.OnCaretBoundsChanged(&text_input_client);

  CaptureAfterImage(capture_bounds);
  ComputeImageStats();

  // This is a smoke test to assert that something is drawn in the right part of
  // the screen of approximately the right size and color.
  // There's deliberately some tolerance for tiny errors.
  EXPECT_NEAR(1487, diff_count(), 50);
  EXPECT_NEAR(175, SkColorGetR(average_diff_color()), 5);
  EXPECT_NEAR(175, SkColorGetG(average_diff_color()), 5);
  EXPECT_NEAR(255, SkColorGetB(average_diff_color()), 5);
}

TEST_F(AccessibilityHighlightControllerTest, TestFocusRingDrawsPixels) {
  // Create a white background window for captured image color smoke test.
  std::unique_ptr<aura::Window> window =
      TestWindowBuilder()
          .SetColorWindowDelegate(SK_ColorWHITE)
          .SetBounds(Shell::GetPrimaryRootWindow()->bounds())
          .Build();

  gfx::Rect capture_bounds(200, 300, 100, 100);
  gfx::Rect focus_bounds(230, 330, 40, 40);

  CaptureBeforeImage(capture_bounds);

  AccessibilityHighlightController controller;
  controller.HighlightFocus(true);
  controller.SetFocusHighlightRect(focus_bounds);

  CaptureAfterImage(capture_bounds);
  ComputeImageStats();

  // This is a smoke test to assert that something is drawn in the right part of
  // the screen of approximately the right size and color.
  // There's deliberately some tolerance for tiny errors.
  EXPECT_NEAR(1608, diff_count(), 50);
  EXPECT_NEAR(255, SkColorGetR(average_diff_color()), 5);
  EXPECT_NEAR(201, SkColorGetG(average_diff_color()), 5);
  EXPECT_NEAR(152, SkColorGetB(average_diff_color()), 5);
}

// Integration test of cursor handling between AccessibilityHighlightController
// and AccessibilityFocusRingController.
TEST_F(AccessibilityHighlightControllerTest, CursorWorksOnMultipleDisplays) {
  UpdateDisplay("500x400,500x400");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  aura::Window* window0_container = Shell::GetContainer(
      root_windows[0], kShellWindowId_AccessibilityBubbleContainer);
  aura::Window* window1_container = Shell::GetContainer(
      root_windows[1], kShellWindowId_AccessibilityBubbleContainer);

  AccessibilityHighlightController highlight_controller;
  highlight_controller.HighlightCursor(true);
  gfx::Point location(90, 90);
  ui::MouseEvent event0(ui::EventType::kMouseMoved, location, location,
                        ui::EventTimeForNow(), 0, 0);
  ui::Event::DispatcherApi event_mod(&event0);
  event_mod.set_target(root_windows[0]);
  highlight_controller.OnMouseEvent(&event0);

  AccessibilityFocusRingControllerImpl* focus_ring_controller =
      Shell::Get()->accessibility_focus_ring_controller();
  auto* cursor_layer = focus_ring_controller->cursor_layer_for_testing();
  EXPECT_EQ(window0_container, cursor_layer->root_window());
  EXPECT_LT(
      std::abs(cursor_layer->layer()->GetTargetBounds().x() - location.x()),
      50);
  EXPECT_LT(
      std::abs(cursor_layer->layer()->GetTargetBounds().y() - location.y()),
      50);

  ui::MouseEvent event1(ui::EventType::kMouseMoved, location, location,
                        ui::EventTimeForNow(), 0, 0);
  ui::Event::DispatcherApi event_mod1(&event1);
  event_mod1.set_target(root_windows[1]);
  highlight_controller.OnMouseEvent(&event1);

  cursor_layer = focus_ring_controller->cursor_layer_for_testing();
  EXPECT_EQ(window1_container, cursor_layer->root_window());
  EXPECT_LT(
      std::abs(cursor_layer->layer()->GetTargetBounds().x() - location.x()),
      50);
  EXPECT_LT(
      std::abs(cursor_layer->layer()->GetTargetBounds().y() - location.y()),
      50);
}

// Integration test of caret handling between AccessibilityHighlightController
// and AccessibilityFocusRingController.
TEST_F(AccessibilityHighlightControllerTest, CaretRingDrawnOnlyWithinBounds) {
  // Given caret bounds that are not within the active window, expect that the
  // caret ring highlight is not drawn.
  std::unique_ptr<views::Widget> window =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  window->SetBounds(gfx::Rect(5, 5, 300, 300));

  AccessibilityHighlightController highlight_controller;
  MockTextInputClient text_input_client;
  highlight_controller.HighlightCaret(true);
  gfx::Rect caret_bounds(10, 10, 40, 40);
  text_input_client.SetCaretBounds(caret_bounds);
  highlight_controller.OnCaretBoundsChanged(&text_input_client);

  AccessibilityFocusRingControllerImpl* focus_ring_controller =
      Shell::Get()->accessibility_focus_ring_controller();
  auto* caret_layer = focus_ring_controller->caret_layer_for_testing();
  EXPECT_EQ(
      std::abs(caret_layer->layer()->GetTargetBounds().x() - caret_bounds.x()),
      20);
  EXPECT_EQ(
      std::abs(caret_layer->layer()->GetTargetBounds().y() - caret_bounds.y()),
      20);

  gfx::Rect not_visible_bounds(301, 301, 10, 10);
  text_input_client.SetCaretBounds(not_visible_bounds);
  highlight_controller.OnCaretBoundsChanged(&text_input_client);

  EXPECT_FALSE(focus_ring_controller->caret_layer_for_testing());
}

// Tests that a zero-width text caret still results in a visible highlight.
// https://crbug.com/882762
TEST_F(AccessibilityHighlightControllerTest, ZeroWidthCaretRingVisible) {
  AccessibilityHighlightController highlight_controller;
  MockTextInputClient text_input_client;
  highlight_controller.HighlightCaret(true);

  // Simulate a zero-width text caret.
  gfx::Rect zero_width(0, 16);
  text_input_client.SetCaretBounds(zero_width);
  highlight_controller.OnCaretBoundsChanged(&text_input_client);

  // Caret ring is created.
  EXPECT_TRUE(Shell::Get()
                  ->accessibility_focus_ring_controller()
                  ->caret_layer_for_testing());

  // Simulate an empty text caret.
  gfx::Rect empty;
  text_input_client.SetCaretBounds(empty);
  highlight_controller.OnCaretBoundsChanged(&text_input_client);

  // Caret ring is gone.
  EXPECT_FALSE(Shell::Get()
                   ->accessibility_focus_ring_controller()
                   ->caret_layer_for_testing());
}

// Tests setting the caret bounds explicitly via AccessibilityController, rather
// than via the input method observer. This path is used in production in mash.
TEST_F(AccessibilityHighlightControllerTest, SetCaretBounds) {
  std::unique_ptr<views::Widget> window =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  window->SetBounds(gfx::Rect(5, 5, 300, 300));

  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  accessibility_controller->caret_highlight().SetEnabled(true);

  // Bounds inside the active window create a highlight.
  accessibility_controller->SetCaretBounds(gfx::Rect(10, 10, 1, 16));
  AccessibilityFocusRingControllerImpl* focus_ring_controller =
      Shell::Get()->accessibility_focus_ring_controller();
  EXPECT_TRUE(focus_ring_controller->caret_layer_for_testing());

  // Empty bounds remove the highlight.
  accessibility_controller->SetCaretBounds(gfx::Rect());
  EXPECT_FALSE(focus_ring_controller->caret_layer_for_testing());
}

}  // namespace ash
