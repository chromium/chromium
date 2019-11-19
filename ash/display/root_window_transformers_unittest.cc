// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/root_window_transformers.h"

#include <memory>

#include "ash/display/display_util.h"
#include "ash/display/mirror_window_test_api.h"
#include "ash/host/root_window_transformer.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/cursor_manager_test_api.h"
#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_switches.h"
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

    if (event->type() == ui::ET_SCROLL) {
      scroll_x_offset_ = event->x_offset();
      scroll_y_offset_ = event->y_offset();
      scroll_x_offset_ordinal_ = event->x_offset_ordinal();
      scroll_y_offset_ordinal_ = event->y_offset_ordinal();
    }
    event->StopPropagation();
  }

  std::string GetLocationAndReset() {
    std::string result = mouse_location_.ToString();
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
  aura::Window* target_root_;

  float touch_radius_x_;
  float touch_radius_y_;
  float scroll_x_offset_;
  float scroll_y_offset_;
  float scroll_x_offset_ordinal_;
  float scroll_y_offset_ordinal_;

  DISALLOW_COPY_AND_ASSIGN(TestEventHandler);
};

class RootWindowTransformersTest : public AshTestBase {
 public:
  RootWindowTransformersTest() = default;
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
    return std::unique_ptr<RootWindowTransformer>(
        CreateRootWindowTransformerForMirroredDisplay(source_display_info,
                                                      mirror_display_info));
  }

  DISALLOW_COPY_AND_ASSIGN(RootWindowTransformersTest);
};

class UnfiedRootWindowTransformersTest : public RootWindowTransformersTest {
 public:
  UnfiedRootWindowTransformersTest() = default;
  ~UnfiedRootWindowTransformersTest() override = default;

  // RootWindowTransformersTest:
  void SetUp() override {
    // kEnableUnifiedDesktop switch needs to be added before DisplayManager
    // creation. Hence before calling SetUp.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableUnifiedDesktop);

    RootWindowTransformersTest::SetUp();
  }

  DISALLOW_COPY_AND_ASSIGN(UnfiedRootWindowTransformersTest);
};

}  // namespace

TEST_F(RootWindowTransformersTest, RotateAndMagnify) {
  MagnificationController* magnifier = Shell::Get()->magnification_controller();

  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("120x200,300x400*2");
  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  int64_t display2_id = display_manager()->GetSecondaryDisplay().id();

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ui::test::EventGenerator generator1(root_windows[0]);
  ui::test::EventGenerator generator2(root_windows[1]);

  magnifier->SetEnabled(true);
  EXPECT_EQ(2.0f, magnifier->GetScale());
  EXPECT_EQ("120x200", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("150x200", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("120,0 150x200",
            display_manager()->GetSecondaryDisplay().bounds().ToString());
  generator1.MoveMouseToInHost(40, 80);
  EXPECT_EQ("50,90", event_handler.GetLocationAndReset());
  EXPECT_EQ("50,90",
            aura::Env::GetInstance()->last_mouse_location().ToString());
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
  EXPECT_EQ("200x120", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("150x200", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("200,0 150x200",
            display_manager()->GetSecondaryDisplay().bounds().ToString());
  generator1.MoveMouseToInHost(39, 120);
  EXPECT_EQ("110,70", event_handler.GetLocationAndReset());
  EXPECT_EQ("110,70",
            aura::Env::GetInstance()->last_mouse_location().ToString());
  EXPECT_EQ(display::Display::ROTATE_90,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_0, GetActiveDisplayRotation(display2_id));
  magnifier->SetEnabled(false);

  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(
          display_manager(), display::DisplayPlacement::BOTTOM, 50));
  EXPECT_EQ("50,120 150x200",
            display_manager()->GetSecondaryDisplay().bounds().ToString());

  display_manager()->SetDisplayRotation(
      display2_id, display::Display::ROTATE_270,
      display::Display::RotationSource::ACTIVE);
  // Move the cursor to the center of the second root window.
  generator2.MoveMouseToInHost(151, 199);

  magnifier->SetEnabled(true);
  EXPECT_EQ("200x120", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("200x150", root_windows[1]->bounds().size().ToString());
  EXPECT_EQ("50,120 200x150",
            display_manager()->GetSecondaryDisplay().bounds().ToString());
  generator2.MoveMouseToInHost(172, 219);
  EXPECT_EQ("95,80", event_handler.GetLocationAndReset());
  EXPECT_EQ("145,200",
            aura::Env::GetInstance()->last_mouse_location().ToString());
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
  EXPECT_EQ("120x200", root_windows[0]->bounds().size().ToString());
  EXPECT_EQ("200x150", root_windows[1]->bounds().size().ToString());
  // Dislay must share at least 100, so the x's offset becomes 20.
  EXPECT_EQ("20,200 200x150",
            display_manager()->GetSecondaryDisplay().bounds().ToString());
  generator1.MoveMouseToInHost(39, 59);
  EXPECT_EQ("70,120", event_handler.GetLocationAndReset());
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
  display::Display display2 = display_manager()->GetSecondaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  MagnificationController* magnifier = Shell::Get()->magnification_controller();

  magnifier->SetEnabled(true);
  EXPECT_EQ(2.0f, magnifier->GetScale());
  EXPECT_EQ(1.6f, display1.device_scale_factor());
  EXPECT_EQ("0,0 375x250", display1.bounds().ToString());
  EXPECT_EQ("0,0 375x250", root_windows[0]->bounds().ToString());
  EXPECT_EQ("375,0 500x300", display2.bounds().ToString());
  EXPECT_EQ(1.0f, GetStoredZoomScale(display1.id()));
  EXPECT_EQ(1.0f, GetStoredZoomScale(display2.id()));

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(500, 200);
  EXPECT_EQ("249,124", event_handler.GetLocationAndReset());
  magnifier->SetEnabled(false);

  display_manager()->UpdateZoomFactor(display1.id(), 1.f / 1.2f);
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  display2 = display_manager()->GetSecondaryDisplay();
  magnifier->SetEnabled(true);
  EXPECT_EQ(2.0f, magnifier->GetScale());
  EXPECT_EQ("0,0 450x300", display1.bounds().ToString());
  EXPECT_EQ("0,0 450x300", root_windows[0]->bounds().ToString());
  EXPECT_EQ("450,0 500x300", display2.bounds().ToString());
  EXPECT_FLOAT_EQ(1.f / 1.2f, GetStoredZoomScale(display1.id()));
  EXPECT_EQ(1.0f, GetStoredZoomScale(display2.id()));
  magnifier->SetEnabled(false);

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

// Make sure the origin of rotated root layer is aligned with pixels
// on 2.25 scale factor device so that HW overlay kicks in.
// https://crbug.com/869090.
TEST_F(RootWindowTransformersTest, OriginAlignmentWithFractionalScale) {
  auto* host = Shell::GetPrimaryRootWindow()->GetHost();
  auto* host_window = host->window();
  EXPECT_EQ(Shell::GetPrimaryRootWindow(), host_window);

  float device_scale_factor = 2.25f;
  gfx::Transform scale_transform;
  scale_transform.matrix().set3x3(device_scale_factor, 0, 0, 0,
                                  device_scale_factor, 0, 0, 0, 1);
  gfx::Transform invert_transform;
  invert_transform.matrix().set3x3(1.0f / device_scale_factor, 0, 0, 0,
                                   1.0f / device_scale_factor, 0, 0, 0, 1);

  {
    // Rotate 90 degree to right.
    UpdateDisplay("3000x2000*2.25/r");

    // The size of the scaled layer.
    gfx::RectF tmp(1998, 2999);
    // Creates a transform that can be applied to already scaled layer.
    gfx::Transform transform(invert_transform);
    transform.ConcatTransform(host->GetRootTransform() * invert_transform);
    transform.ConcatTransform(scale_transform);
    transform.TransformRect(&tmp);
    EXPECT_EQ(gfx::SizeF(2999, 1998), tmp.size());
    EXPECT_TRUE(gfx::IsNearestRectWithinDistance(tmp, 0.01f));
  }

  {
    // Upside Down.
    UpdateDisplay("3000x2000*2.25/u");

    gfx::RectF tmp(2999, 1998);
    gfx::Transform transform(invert_transform);
    transform.ConcatTransform(host->GetRootTransform() * invert_transform);
    transform.ConcatTransform(scale_transform);
    transform.TransformRect(&tmp);
    EXPECT_EQ(gfx::SizeF(2999, 1998), tmp.size());
    EXPECT_TRUE(gfx::IsNearestRectWithinDistance(tmp, 0.01f));
  }

  {
    // Rotate 90 degree to left.
    UpdateDisplay("3000x2000*2.25/l");

    gfx::RectF tmp(1998, 2999);
    gfx::Transform transform(invert_transform);
    transform.ConcatTransform(host->GetRootTransform() * invert_transform);
    transform.ConcatTransform(scale_transform);
    transform.TransformRect(&tmp);
    EXPECT_EQ(gfx::SizeF(2999, 1998), tmp.size());
    EXPECT_TRUE(gfx::IsNearestRectWithinDistance(tmp, 0.01f));
  }
}

TEST_F(RootWindowTransformersTest, TouchScaleAndMagnify) {
  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("200x200*2");
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Window* root_window = root_windows[0];
  ui::test::EventGenerator generator(root_window);
  MagnificationController* magnifier = Shell::Get()->magnification_controller();

  magnifier->SetEnabled(true);
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());
  magnifier->SetScale(2.5f, false);
  EXPECT_FLOAT_EQ(2.5f, magnifier->GetScale());
  generator.PressMoveAndReleaseTouchTo(50, 50);
  // Default test touches have radius_x/y = 1.0, with device scale
  // factor = 2, the scaled radius_x/y should be 0.5.
  EXPECT_FLOAT_EQ(0.2f, event_handler.touch_radius_x());
  EXPECT_FLOAT_EQ(0.2f, event_handler.touch_radius_y());

  generator.ScrollSequence(gfx::Point(0, 0),
                           base::TimeDelta::FromMilliseconds(100), 10.0, 1.0, 5,
                           1);

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
  MagnificationController* magnifier = Shell::Get()->magnification_controller();

  // Test 1
  UpdateDisplay("600x400*2/r@0.8");

  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ("0,0 250x375", display1.bounds().ToString());
  EXPECT_EQ("0,0 250x375", root_windows[0]->bounds().ToString());
  EXPECT_EQ(0.8f, GetStoredZoomScale(display1.id()));

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(300, 200);
  magnifier->SetEnabled(true);
  EXPECT_EQ("125,187", event_handler.GetLocationAndReset());
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());

  generator.MoveMouseToInHost(300, 200);
  EXPECT_EQ("124,186", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(200, 300);
  EXPECT_EQ("155,218", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(100, 400);
  EXPECT_EQ("204,249", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ("125,298", event_handler.GetLocationAndReset());

  magnifier->SetEnabled(false);
  EXPECT_FLOAT_EQ(1.0f, magnifier->GetScale());

  // Test 2
  UpdateDisplay("600x400*2/u@0.8");
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ("0,0 375x250", display1.bounds().ToString());
  EXPECT_EQ("0,0 375x250", root_windows[0]->bounds().ToString());
  EXPECT_EQ(0.8f, GetStoredZoomScale(display1.id()));

  generator.MoveMouseToInHost(300, 200);
  magnifier->SetEnabled(true);
  EXPECT_EQ("187,125", event_handler.GetLocationAndReset());
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());

  generator.MoveMouseToInHost(300, 200);
  EXPECT_EQ("186,124", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(200, 300);
  EXPECT_EQ("218,93", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(100, 400);
  EXPECT_EQ("249,43", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ("298,125", event_handler.GetLocationAndReset());

  magnifier->SetEnabled(false);
  EXPECT_FLOAT_EQ(1.0f, magnifier->GetScale());

  // Test 3
  UpdateDisplay("600x400*2/l@0.8");
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ("0,0 250x375", display1.bounds().ToString());
  EXPECT_EQ("0,0 250x375", root_windows[0]->bounds().ToString());
  EXPECT_EQ(0.8f, GetStoredZoomScale(display1.id()));

  generator.MoveMouseToInHost(300, 200);
  magnifier->SetEnabled(true);
  EXPECT_EQ("125,187", event_handler.GetLocationAndReset());
  EXPECT_FLOAT_EQ(2.0f, magnifier->GetScale());

  generator.MoveMouseToInHost(300, 200);
  EXPECT_EQ("124,186", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(200, 300);
  EXPECT_EQ("93,155", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(100, 400);
  EXPECT_EQ("43,124", event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ("125,74", event_handler.GetLocationAndReset());

  magnifier->SetEnabled(false);
  EXPECT_FLOAT_EQ(1.0f, magnifier->GetScale());

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

TEST_F(RootWindowTransformersTest, LetterBoxPillarBox) {
  MirrorWindowTestApi test_api;
  UpdateDisplay("400x200,500x500");
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
  std::unique_ptr<RootWindowTransformer> transformer(
      CreateCurrentRootWindowTransformerForMirroring());
  // Y margin must be margin is (500 - 500/400 * 200) / 2 = 125.
  EXPECT_EQ("0,125,0,125", transformer->GetHostInsets().ToString());

  UpdateDisplay("200x400,500x500");
  // The aspect ratio is flipped, so X margin is now 125.
  transformer = CreateCurrentRootWindowTransformerForMirroring();
  EXPECT_EQ("125,0,125,0", transformer->GetHostInsets().ToString());
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
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(100));
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
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(100));
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
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(100));
    layer->SetOpacity(0.1f);
  }

  // Rotate screen 90 degrees to "right".
  // Will triger window_tree_host->SetRootWindowTransformer().
  // The window size will be updated because there is no ongoing transform
  // animation, even there is an opacity animation.
  UpdateDisplay("800x600/r");
  EXPECT_EQ(root_window->GetTargetBounds(), gfx::Rect(0, 0, 600, 800));
}

TEST_F(UnfiedRootWindowTransformersTest, HostBoundsAndTransform) {
  UpdateDisplay("800x600,800x600");
  // Has only one logical root window.
  EXPECT_EQ(1u, Shell::GetAllRootWindows().size());

  MirrorWindowTestApi test_api;
  std::vector<aura::WindowTreeHost*> hosts = test_api.GetHosts();
  // Have 2 WindowTreeHosts, one per display.
  ASSERT_EQ(2u, hosts.size());

  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), hosts[0]->window()->GetBoundsInScreen());
  gfx::Point viewport_0_origin(0, 0);
  hosts[0]->window()->transform().TransformPointReverse(&viewport_0_origin);
  EXPECT_EQ(gfx::Point(0, 0), viewport_0_origin);

  EXPECT_EQ(gfx::Rect(800, 0, 800, 600),
            hosts[1]->window()->GetBoundsInScreen());
  gfx::Point viewport_1_origin(0, 0);
  hosts[1]->window()->transform().TransformPointReverse(&viewport_1_origin);
  EXPECT_EQ(gfx::Point(800, 0), viewport_1_origin);
}

}  // namespace ash
