// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/root_window_transformers.h"

#include <memory>
#include <optional>

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/display/display_util.h"
#include "ash/display/mirror_window_test_api.h"
#include "ash/host/root_window_transformer.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/cursor_manager_test_api.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const char kWallpaperView[] = "WallpaperViewWidget";

class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler()
      : target_root_(nullptr),
        touch_radius_x_(0.0),
        touch_radius_y_(0.0),
        scroll_x_offset_(0.0),
        scroll_y_offset_(0.0),
        scroll_x_offset_ordinal_(0.0),
        scroll_y_offset_ordinal_(0.0) {}

  TestEventHandler(const TestEventHandler&) = delete;
  TestEventHandler& operator=(const TestEventHandler&) = delete;

  ~TestEventHandler() override = default;

  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->flags() & ui::EF_IS_SYNTHESIZED)
      return;
    aura::Window* target = static_cast<aura::Window*>(event->target());
    mouse_location_ = event->root_location();
    target_root_ = target->GetRootWindow();
    event->StopPropagation();
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    // Only record when the target is the wallpaper, which covers the entire
    // root window.
    if (target->GetName() != kWallpaperView)
      return;
    touch_radius_x_ = event->pointer_details().radius_x;
    touch_radius_y_ = event->pointer_details().radius_y;
    event->StopPropagation();
  }

  void OnScrollEvent(ui::ScrollEvent* event) override {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    // Only record when the target is the wallpaper, which covers the entire
    // root window.
    if (target->GetName() != kWallpaperView)
      return;

    if (event->type() == ui::EventType::kScroll) {
      scroll_x_offset_ = event->x_offset();
      scroll_y_offset_ = event->y_offset();
      scroll_x_offset_ordinal_ = event->x_offset_ordinal();
      scroll_y_offset_ordinal_ = event->y_offset_ordinal();
    }
    event->StopPropagation();
  }

  gfx::Point GetLocationAndReset() {
    gfx::Point result = mouse_location_;
    mouse_location_.SetPoint(0, 0);
    target_root_ = nullptr;
    return result;
  }

  float touch_radius_x() const { return touch_radius_x_; }
  float touch_radius_y() const { return touch_radius_y_; }
  float scroll_x_offset() const { return scroll_x_offset_; }
  float scroll_y_offset() const { return scroll_y_offset_; }
  float scroll_x_offset_ordinal() const { return scroll_x_offset_ordinal_; }
  float scroll_y_offset_ordinal() const { return scroll_y_offset_ordinal_; }

 private:
  gfx::Point mouse_location_;
  raw_ptr<aura::Window> target_root_;

  float touch_radius_x_;
  float touch_radius_y_;
  float scroll_x_offset_;
  float scroll_y_offset_;
  float scroll_x_offset_ordinal_;
  float scroll_y_offset_ordinal_;
};

class RootWindowTransformersTest : public AshTestBase {
 public:
  RootWindowTransformersTest() = default;

  RootWindowTransformersTest(const RootWindowTransformersTest&) = delete;
  RootWindowTransformersTest& operator=(const RootWindowTransformersTest&) =
      delete;

  ~RootWindowTransformersTest() override = default;

  float GetStoredZoomScale(int64_t id) {
    return display_manager()->GetDisplayInfo(id).zoom_factor();
  }

  std::unique_ptr<RootWindowTransformer>
  CreateCurrentRootWindowTransformerForMirroring() {
    DCHECK(display_manager()->IsInMirrorMode());
    const display::ManagedDisplayInfo& mirror_display_info =
        display_manager()->GetDisplayInfo(
            display_manager()->GetMirroringDestinationDisplayIdList()[0]);
    const display::ManagedDisplayInfo& source_display_info =
        display_manager()->GetDisplayInfo(
            display::Screen::GetScreen()->GetPrimaryDisplay().id());
    return CreateRootWindowTransformerForMirroredDisplay(source_display_info,
                                                         mirror_display_info);
  }
};

class UnifiedRootWindowTransformersTest : public RootWindowTransformersTest {
 public:
  void SetUp() override {
    RootWindowTransformersTest::SetUp();
    display_manager()->SetUnifiedDesktopEnabled(true);
  }
};

}  // namespace

TEST_F(RootWindowTransformersTest, RotateAndMagnify) {
  FullscreenMagnifierController* magnifier =
      Shell::Get()->fullscreen_magnifier_controller();

  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("120x200,300x400*2");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  int64_t display2_id = display_manager_test.GetSecondaryDisplay().id();

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ui::test::EventGenerator generator1(root_windows[0]);
  ui::test::EventGenerator generator2(root_windows[1]);

  magnifier->SetEnabled(true);
  EXPECT_EQ(2.0f, magnifier->GetScale());
  EXPECT_EQ(gfx::Size(120, 200), root_windows[0]->bounds().size());
  EXPECT_EQ(gfx::Size(150, 200), root_windows[1]->bounds().size());
  EXPECT_EQ(gfx::Rect(120, 0, 150, 200),
            display_manager_test.GetSecondaryDisplay().bounds());
  generator1.MoveMouseToInHost(40, 80);
  EXPECT_EQ(gfx::Point(50, 90), event_handler.GetLocationAndReset());
  EXPECT_EQ(gfx::Point(50, 90),
            aura::Env::GetInstance()->last_mouse_location());
  EXPECT_EQ(display::Display::ROTATE_0,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_0, GetActiveDisplayRotation(display2_id));
  magnifier->SetEnabled(false);

  display_manager()->SetDisplayRotation(
      display1.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  // Move the cursor to the center of the first root window.
  generator1.MoveMouseToInHost(59, 100);

  magnifier->SetEnabled(true);
  EXPECT_EQ(2.0f, magnifier->GetScale());
  EXPECT_EQ(gfx::Size(200, 120), root_windows[0]->bounds().size());
  EXPECT_EQ(gfx::Size(150, 200), root_windows[1]->bounds().size());
  EXPECT_EQ(gfx::Rect(200, 0, 150, 200),
            display_manager_test.GetSecondaryDisplay().bounds());
  generator1.MoveMouseToInHost(39, 120);
  // The EventHandler truncates floating-point locations to integer locations,
  // while Magnifier will round them up.
  EXPECT_EQ(gfx::Point(110, 70), event_handler.GetLocationAndReset());
  EXPECT_EQ(gfx::Point(110, 71),
            aura::Env::GetInstance()->last_mouse_location());
  EXPECT_EQ(display::Display::ROTATE_90,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_0, GetActiveDisplayRotation(display2_id));
  magnifier->SetEnabled(false);

  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(
          display_manager(), display::DisplayPlacement::BOTTOM, 50));
  EXPECT_EQ(gfx::Rect(50, 120, 150, 200),
            display_manager_test.GetSecondaryDisplay().bounds());

  display_manager()->SetDisplayRotation(
      display2_id, display::Display::ROTATE_270,
      display::Display::RotationSource::ACTIVE);
  // Move the cursor to the center of the second root window.
  generator2.MoveMouseToInHost(151, 199);

  magnifier->SetEnabled(true);
  EXPECT_EQ(gfx::Size(200, 120), root_windows[0]->bounds().size());
  EXPECT_EQ(gfx::Size(200, 150), root_windows[1]->bounds().size());
  EXPECT_EQ(gfx::Rect(50, 120, 200, 150),
            display_manager_test.GetSecondaryDisplay().bounds());
  generator2.MoveMouseToInHost(172, 219);
  EXPECT_EQ(gfx::Point(95, 80), event_handler.GetLocationAndReset());
  EXPECT_EQ(gfx::Point(145, 200),
            aura::Env::GetInstance()->last_mouse_location());
  EXPECT_EQ(display::Display::ROTATE_90,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_270,
            GetActiveDisplayRotation(display2_id));
  magnifier->SetEnabled(false);

  display_manager()->SetDisplayRotation(
      display1.id(), display::Display::ROTATE_180,
      display::Display::RotationSource::ACTIVE);
  // Move the cursor to the center of the first root window.
  generator1.MoveMouseToInHost(59, 99);

  magnifier->SetEnabled(true);
  EXPECT_EQ(gfx::Size(120, 200), root_windows[0]->bounds().size());
  EXPECT_EQ(gfx::Size(200, 150), root_windows[1]->bounds().size());
  // Display must share at least 100, so the x's offset becomes 20.
  EXPECT_EQ(gfx::Rect(20, 200, 200, 150),
            display_manager_test.GetSecondaryDisplay().bounds());
  generator1.MoveMouseToInHost(39, 59);
  EXPECT_EQ(gfx::Point(70, 120), event_handler.GetLocationAndReset());
  EXPECT_EQ(display::Display::ROTATE_180,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_270,
            GetActiveDisplayRotation(display2_id));
  magnifier->SetEnabled(false);

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

TEST_F(RootWindowTransformersTest, ScaleAndMagnify) {
  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("600x400*1.6,500x300");

  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display1.id());
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  display::Display display2 = display_manager_test.GetSecondaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  FullscreenMagnifierController* magnifier =
      Shell::Get()->fullscreen_magnifier_controller();

  magnifier->SetEnabled(true);
  EXPECT_EQ(2.0f, magnifier->GetScale());
  EXPECT_EQ(1.6f, display1.device_scale_factor());
  EXPECT_EQ(gfx::Rect(0, 0, 375, 250), display1.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 375, 250), root_windows[0]->bounds());
  EXPECT_EQ(gfx::Rect(375, 0, 500, 300), display2.bounds());
  EXPECT_EQ(1.0f, GetStoredZoomScale(display1.id()));
  EXPECT_EQ(1.0f, GetStoredZoomScale(display2.id()));

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(500, 200);
  EXPECT_EQ(gfx::Point(249, 124), event_handler.GetLocationAndReset());
  magnifier->SetEnabled(false);

  display_manager()->UpdateZoomFactor(display1.id(), 1.f / 1.2f);
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  display2 = display_manager_test.GetSecondaryDisplay();
  magnifier->SetEnabled(true);
  EXPECT_EQ(2.0f, magnifier->GetScale());
  EXPECT_EQ(gfx::Rect(0, 0, 450, 300), display1.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 450, 300), root_windows[0]->bounds());
  EXPECT_EQ(gfx::Rect(450, 0, 500, 300), display2.bounds());
  EXPECT_FLOAT_EQ(1.f / 1.2f, GetStoredZoomScale(display1.id()));
  EXPECT_EQ(1.0f, GetStoredZoomScale(display2.id()));
  magnifier->SetEnabled(false);

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

TEST_F(RootWindowTransformersTest, TouchScaleAndMagnify) {
  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("300x200*2");
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Window* root_window = root_windows[0];
  ui::test::EventGenerator generator(root_window);
  FullscreenMagnifierController* magnifier =
      Shell::Get()->fullscreen_magnifier_controller();

  magnifier->SetEnabled(true);
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());
  magnifier->SetScale(2.5f, false);
  EXPECT_FLOAT_EQ(2.5f, magnifier->GetScale());
  generator.PressMoveAndReleaseTouchTo(50, 50);
  // Default test touches have radius_x/y = 1.0, with device scale
  // factor = 2, the scaled radius_x/y should be 0.5.
  EXPECT_FLOAT_EQ(0.2f, event_handler.touch_radius_x());
  EXPECT_FLOAT_EQ(0.2f, event_handler.touch_radius_y());

  generator.ScrollSequence(gfx::Point(0, 0), base::Milliseconds(100), 10.0, 1.0,
                           5, 1);

  // ordinal_offset is invariant to the device scale factor.
  EXPECT_FLOAT_EQ(event_handler.scroll_x_offset(),
                  event_handler.scroll_x_offset_ordinal());
  EXPECT_FLOAT_EQ(event_handler.scroll_y_offset(),
                  event_handler.scroll_y_offset_ordinal());
  magnifier->SetEnabled(false);

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

TEST_F(RootWindowTransformersTest, ConvertHostToRootCoords) {
  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);
  FullscreenMagnifierController* magnifier =
      Shell::Get()->fullscreen_magnifier_controller();

  // Test 1
  UpdateDisplay("600x400*2/r@0.8");

  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(gfx::Rect(0, 0, 250, 375), display1.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 250, 375), root_windows[0]->bounds());
  EXPECT_EQ(0.8f, GetStoredZoomScale(display1.id()));

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(300, 200);
  magnifier->SetEnabled(true);
  EXPECT_EQ(gfx::Point(125, 187), event_handler.GetLocationAndReset());
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());

  generator.MoveMouseToInHost(300, 200);
  EXPECT_EQ(gfx::Point(124, 186), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(200, 300);
  EXPECT_EQ(gfx::Point(155, 218), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(100, 400);
  EXPECT_EQ(gfx::Point(205, 249), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ(gfx::Point(125, 298), event_handler.GetLocationAndReset());

  magnifier->SetEnabled(false);
  EXPECT_FLOAT_EQ(1.0f, magnifier->GetScale());

  // Test 2
  UpdateDisplay("600x400*2/u@0.8");
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(gfx::Rect(0, 0, 375, 250), display1.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 375, 250), root_windows[0]->bounds());
  EXPECT_EQ(0.8f, GetStoredZoomScale(display1.id()));

  generator.MoveMouseToInHost(300, 200);
  magnifier->SetEnabled(true);
  EXPECT_EQ(gfx::Point(187, 125), event_handler.GetLocationAndReset());
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());

  generator.MoveMouseToInHost(300, 200);
  EXPECT_EQ(gfx::Point(186, 124), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(200, 300);
  EXPECT_EQ(gfx::Point(218, 93), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(100, 400);
  EXPECT_EQ(gfx::Point(249, 43), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ(gfx::Point(298, 125), event_handler.GetLocationAndReset());

  magnifier->SetEnabled(false);
  EXPECT_FLOAT_EQ(1.0f, magnifier->GetScale());

  // Test 3
  UpdateDisplay("600x400*2/l@0.8");
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(gfx::Rect(0, 0, 250, 375), display1.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 250, 375), root_windows[0]->bounds());
  EXPECT_EQ(0.8f, GetStoredZoomScale(display1.id()));

  generator.MoveMouseToInHost(300, 200);
  magnifier->SetEnabled(true);
  EXPECT_EQ(gfx::Point(125, 187), event_handler.GetLocationAndReset());
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());

  generator.MoveMouseToInHost(300, 200);
  EXPECT_EQ(gfx::Point(124, 186), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(200, 300);
  EXPECT_EQ(gfx::Point(93, 155), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(100, 400);
  EXPECT_EQ(gfx::Point(43, 124), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ(gfx::Point(125, 74), event_handler.GetLocationAndReset());

  magnifier->SetEnabled(false);
  EXPECT_FLOAT_EQ(1.0f, magnifier->GetScale());

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

TEST_F(RootWindowTransformersTest, LetterBoxPillarBox) {
  MirrorWindowTestApi test_api;
  // Letter boxed
  UpdateDisplay("400x200,500x400");
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  std::unique_ptr<RootWindowTransformer> transformer(
      CreateCurrentRootWindowTransformerForMirroring());
  // Y margin must be margin is (400 - 500/400 * 200) / 2 = 75
  EXPECT_EQ(gfx::Insets::TLBR(0, 75, 0, 75), transformer->GetHostInsets());

  // Pillar boxed
  UpdateDisplay("200x400,500x400");
  // X margin must be margin is (500 - 200) / 2 = 150
  transformer = CreateCurrentRootWindowTransformerForMirroring();
  EXPECT_EQ(gfx::Insets::TLBR(150, 0, 150, 0), transformer->GetHostInsets());
}

TEST_F(RootWindowTransformersTest, MirrorWithRotation) {
  MirrorWindowTestApi test_api;
  UpdateDisplay("400x200,500x400");
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);

  for (auto rotation :
       {display::Display::ROTATE_0, display::Display::ROTATE_90,
        display::Display::ROTATE_180, display::Display::ROTATE_270}) {
    SCOPED_TRACE(::testing::Message() << "Rotation: " << rotation);
    display_manager()->SetDisplayRotation(
        display::Screen::GetScreen()->GetPrimaryDisplay().id(), rotation,
        display::Display::RotationSource::ACTIVE);
    std::unique_ptr<RootWindowTransformer> transformer(
        CreateCurrentRootWindowTransformerForMirroring());

    const bool need_transpose = rotation == display::Display::ROTATE_90 ||
                                rotation == display::Display::ROTATE_270;
    // Y margin is (400 - 500/400 * 200) / 2 = 75 for no rotation. Transposed
    // on 90/270 degree.
    gfx::Insets expected_insets =
        need_transpose ? gfx::Insets::VH(75, 0) : gfx::Insets::VH(0, 75);
    EXPECT_EQ(expected_insets, transformer->GetHostInsets());

    // Expected rect in mirror of the source root, with y margin applied for no
    // rotation. Transposed on 90/270 degree.
    gfx::RectF expected_rect(0, 75, 500, 250);
    if (need_transpose)
      expected_rect.Transpose();

    gfx::RectF rect = transformer->GetTransform().MapRect(
        gfx::RectF(transformer->GetRootWindowBounds(gfx::Size())));
    EXPECT_EQ(expected_rect, rect);
  }
}

TEST_F(RootWindowTransformersTest, ShouldSetWindowSize) {
  UpdateDisplay("800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Window* root_window = root_windows[0];

  // Rotate screen 90 degrees to "right".
  // Will triger window_tree_host->SetRootWindowTransformer().
  // The window size will be updated because there is no ongoing transform
  // animation.
  UpdateDisplay("800x600/r");
  EXPECT_EQ(root_window->GetTargetBounds(), gfx::Rect(0, 0, 600, 800));
}

TEST_F(RootWindowTransformersTest,
       ShouldNotSetWindowSizeWithEnqueuedTransformAnimation) {
  UpdateDisplay("800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Window* root_window = root_windows[0];

  ui::ScopedAnimationDurationScaleMode test_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  ui::Layer* layer = root_window->layer();
  {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(100));
    gfx::Transform transform;
    transform.Translate(100, 200);
    layer->SetTransform(transform);
  }
  layer->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);

  // Rotate screen 90 degrees to "right".
  // Will triger window_tree_host->SetRootWindowTransformer().
  // The window size will not be updated because there is ongoing transform
  // animation.
  UpdateDisplay("800x600/r");
  EXPECT_NE(root_window->GetTargetBounds(), gfx::Rect(0, 0, 600, 800));
}

TEST_F(RootWindowTransformersTest,
       ShouldSetWindowSizeWithStoppedTransformAnimation) {
  UpdateDisplay("800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Window* root_window = root_windows[0];

  ui::ScopedAnimationDurationScaleMode test_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  ui::Layer* layer = root_window->layer();
  {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(100));
    gfx::Transform transform;
    transform.Translate(100, 200);
    layer->SetTransform(transform);
  }
  layer->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);

  // Rotate screen 90 degrees to "right".
  // Will triger window_tree_host->SetRootWindowTransformer().
  // The window size will be updated because there is no ongoing transform
  // animation.
  UpdateDisplay("800x600/r");
  EXPECT_EQ(root_window->GetTargetBounds(), gfx::Rect(0, 0, 600, 800));
}

TEST_F(RootWindowTransformersTest, ShouldSetWindowSizeDuringOpacityAnimation) {
  UpdateDisplay("800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Window* root_window = root_windows[0];

  ui::ScopedAnimationDurationScaleMode test_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  {
    ui::Layer* layer = root_window->layer();
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(100));
    layer->SetOpacity(0.1f);
  }

  // Rotate screen 90 degrees to "right".
  // Will triger window_tree_host->SetRootWindowTransformer().
  // The window size will be updated because there is no ongoing transform
  // animation, even there is an opacity animation.
  UpdateDisplay("800x600/r");
  EXPECT_EQ(root_window->GetTargetBounds(), gfx::Rect(0, 0, 600, 800));
}

TEST_F(UnifiedRootWindowTransformersTest, HostBoundsAndTransform) {
  UpdateDisplay("800x600,800x600");
  // Has only one logical root window.
  EXPECT_EQ(1u, Shell::GetAllRootWindows().size());

  MirrorWindowTestApi test_api;
  std::vector<aura::WindowTreeHost*> hosts = test_api.GetHosts();
  // Have 2 WindowTreeHosts, one per display.
  ASSERT_EQ(2u, hosts.size());

  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), hosts[0]->window()->GetBoundsInScreen());
  EXPECT_EQ(gfx::Point(),
            hosts[0]->window()->transform().InverseMapPoint(gfx::Point()));

  EXPECT_EQ(gfx::Rect(800, 0, 800, 600),
            hosts[1]->window()->GetBoundsInScreen());
  EXPECT_EQ(gfx::Point(800, 0),
            hosts[1]->window()->transform().InverseMapPoint(gfx::Point()));
}

TEST_F(UnifiedRootWindowTransformersTest,
       PrimaryDisplayRotationAndInputEvents) {
  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  // Use different sized displays with primary display rotated to the right.
  UpdateDisplay("1920x1080*2/r,800x600");
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  // Has only one logical root window.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(1u, root_windows.size());

  MirrorWindowTestApi test_api;
  std::vector<aura::WindowTreeHost*> hosts = test_api.GetHosts();
  // Have 2 WindowTreeHosts, one per display.
  ASSERT_EQ(2u, hosts.size());

  EXPECT_EQ(display::Display::ROTATE_90,
            GetActiveDisplayRotation(hosts[0]->GetDisplayId()));
  EXPECT_EQ(display::Display::ROTATE_0,
            GetActiveDisplayRotation(hosts[1]->GetDisplayId()));

  EXPECT_EQ(gfx::Rect(0, 0, 1080, 1920),
            hosts[0]->window()->GetBoundsInScreen());
  EXPECT_EQ(gfx::Point(),
            hosts[0]->window()->transform().InverseMapPoint(gfx::Point()));
  EXPECT_EQ(
      gfx::Point(1000, 1900),
      hosts[0]->window()->transform().InverseMapPoint(gfx::Point(1000, 1900)));

  EXPECT_EQ(gfx::Rect(1080, 0, 800, 600),
            hosts[1]->window()->GetBoundsInScreen());
  EXPECT_EQ(gfx::Point(1080, 0),
            hosts[1]->window()->transform().InverseMapPoint(gfx::Point()));
  // Since the first display is rotated, the unified height is 1920.
  // The 2nd display's height of 600 is scaled to this: 1920/600=3.2.
  // So the bottom right corner of the 2nd display has
  // x=1080+(800*3.2)=3640 and y=0+(600*3.2)=1920.
  EXPECT_EQ(
      gfx::Point(3640, 1920),
      hosts[1]->window()->transform().InverseMapPoint(gfx::Point(800, 600)));

  // Mouse input on the 1st display.
  ui::test::EventGenerator generator0(hosts[0]->window());
  generator0.MoveMouseToInHost(0, 0);
  // x=0/2=0 y=(1920-0)/2=960
  // But y=959 because it's at the bottom edge.
  EXPECT_EQ(gfx::Point(0, 959), event_handler.GetLocationAndReset());
  generator0.MoveMouseToInHost(300, 200);
  // x=200/2=100 y=(1920-300)/2=810
  EXPECT_EQ(gfx::Point(100, 810), event_handler.GetLocationAndReset());
  generator0.MoveMouseToInHost(1900, 1050);
  // x=1050/2=525 y=(1920-1900)/2=10
  EXPECT_EQ(gfx::Point(525, 10), event_handler.GetLocationAndReset());

  // Mouse input on the 2nd display.
  ui::test::EventGenerator generator1(hosts[1]->window());
  generator1.MoveMouseToInHost(0, 0);
  // x=(1080+(0*3.2))/2=540 y=0*3.2/2=0
  EXPECT_EQ(gfx::Point(540, 0), event_handler.GetLocationAndReset());
  generator1.MoveMouseToInHost(100, 200);
  // x=(1080+(100*3.2))/2=700 y=200*3.2/2=320
  EXPECT_EQ(gfx::Point(700, 320), event_handler.GetLocationAndReset());
  generator1.MoveMouseToInHost(600, 500);
  // x=(1080+(600*3.2))/2=1500 y=500*3.2/2=800
  EXPECT_EQ(gfx::Point(1500, 800), event_handler.GetLocationAndReset());

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

TEST_F(UnifiedRootWindowTransformersTest,
       SecondaryDisplayRotationAndInputEvents) {
  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  // Use different sized displays with secondary display rotated to the right.
  UpdateDisplay("1920x1080*2,800x600/r");
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  // Has only one logical root window.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(1u, root_windows.size());

  MirrorWindowTestApi test_api;
  std::vector<aura::WindowTreeHost*> hosts = test_api.GetHosts();
  // Have 2 WindowTreeHosts, one per display.
  ASSERT_EQ(2u, hosts.size());

  EXPECT_EQ(display::Display::ROTATE_0,
            GetActiveDisplayRotation(hosts[0]->GetDisplayId()));
  EXPECT_EQ(display::Display::ROTATE_90,
            GetActiveDisplayRotation(hosts[1]->GetDisplayId()));

  EXPECT_EQ(gfx::Rect(0, 0, 1920, 1080),
            hosts[0]->window()->GetBoundsInScreen());
  EXPECT_EQ(gfx::Point(),
            hosts[0]->window()->transform().InverseMapPoint(gfx::Point()));
  EXPECT_EQ(
      gfx::Point(1900, 1000),
      hosts[0]->window()->transform().InverseMapPoint(gfx::Point(1900, 1000)));

  EXPECT_EQ(gfx::Rect(1920, 0, 600, 800),
            hosts[1]->window()->GetBoundsInScreen());
  EXPECT_EQ(gfx::Point(1920, 0),
            hosts[1]->window()->transform().InverseMapPoint(gfx::Point()));
  // Since the 2nd display is rotated, its height is 800. This is scaled to the
  // height of the 1st display, which is 1080. So the 2nd display's scaling is
  // 1080/800=1.35. So the bottom right corner of the 2nd display has
  // x=1920+(600*1.35)=2730 and y=0+(800*1.35)=1080.
  EXPECT_EQ(
      gfx::Point(2730, 1080),
      hosts[1]->window()->transform().InverseMapPoint(gfx::Point(600, 800)));

  // Mouse input on the 1st display.
  ui::test::EventGenerator generator0(hosts[0]->window());
  generator0.MoveMouseToInHost(0, 0);
  EXPECT_EQ(gfx::Point(0, 0), event_handler.GetLocationAndReset());
  generator0.MoveMouseToInHost(300, 200);
  EXPECT_EQ(gfx::Point(150, 100), event_handler.GetLocationAndReset());
  generator0.MoveMouseToInHost(1900, 1000);
  EXPECT_EQ(gfx::Point(950, 500), event_handler.GetLocationAndReset());

  // Mouse input on the 2nd display.
  ui::test::EventGenerator generator1(hosts[1]->window());
  generator1.MoveMouseToInHost(0, 0);
  // x=(1920+(0*1.35))/2=960 y=(800-0)*1.35/2=540
  // But y=539 because it's at the bottom edge.
  EXPECT_EQ(gfx::Point(960, 539), event_handler.GetLocationAndReset());
  generator1.MoveMouseToInHost(100, 200);
  // x=(1920+(200*1.35))/2=1095 y=(800-100)*1.35/2=472.5
  EXPECT_EQ(gfx::Point(1095, 472), event_handler.GetLocationAndReset());
  generator1.MoveMouseToInHost(700, 500);
  // x=(1920+(500*1.35))/2=1297.5 y=(800-700)*1.35/2=67.5
  EXPECT_EQ(gfx::Point(1297, 67), event_handler.GetLocationAndReset());

  // Now rotate the 2nd display to the left.
  UpdateDisplay("1920x1080*2,800x600/l");
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  hosts = test_api.GetHosts();
  // Have 2 WindowTreeHosts, one per display.
  ASSERT_EQ(2u, hosts.size());

  EXPECT_EQ(display::Display::ROTATE_270,
            GetActiveDisplayRotation(hosts[1]->GetDisplayId()));

  EXPECT_EQ(gfx::Rect(1920, 0, 600, 800),
            hosts[1]->window()->GetBoundsInScreen());

  // Mouse input on the 2nd display.
  ui::test::EventGenerator generator2(hosts[1]->window());
  generator2.MoveMouseToInHost(0, 0);
  // x=(1920+((600-0)*1.35))/2=1365 y=0*1.35/2=0
  // But x=1364 because it's at the right most edge.
  EXPECT_EQ(gfx::Point(1364, 0), event_handler.GetLocationAndReset());
  generator2.MoveMouseToInHost(100, 200);
  // x=(1920+((600-200)*1.35))/2=1230 y=100*1.35/2=67.5
  EXPECT_EQ(gfx::Point(1230, 67), event_handler.GetLocationAndReset());
  generator2.MoveMouseToInHost(110, 210);
  // x=(1920+((600-210)*1.35))/2=1223.25 y=110*1.35/2=74.25
  EXPECT_EQ(gfx::Point(1223, 74), event_handler.GetLocationAndReset());

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

}  // namespace ash
