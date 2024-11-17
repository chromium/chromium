// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"

#include <utility>

#include "ash/metrics/pip_uma.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_metrics.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

using chromeos::WindowStateType;

namespace ash {
namespace {

class AlwaysMaximizeTestState : public WindowState::State {
 public:
  explicit AlwaysMaximizeTestState(WindowStateType initial_state_type)
      : state_type_(initial_state_type) {}

  AlwaysMaximizeTestState(const AlwaysMaximizeTestState&) = delete;
  AlwaysMaximizeTestState& operator=(const AlwaysMaximizeTestState&) = delete;

  ~AlwaysMaximizeTestState() override = default;

  // WindowState::State overrides:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override {
    // We don't do anything here.
  }
  WindowStateType GetType() const override { return state_type_; }
  void AttachState(WindowState* window_state,
                   WindowState::State* previous_state) override {
    // We always maximize.
    if (state_type_ != WindowStateType::kMaximized) {
      window_state->Maximize();
      state_type_ = WindowStateType::kMaximized;
    }
  }
  void DetachState(WindowState* window_state) override {}

 private:
  WindowStateType state_type_;
};

using WindowStateTest = AshTestBase;

using Sample = base::HistogramBase::Sample;

// Test that a window gets properly snapped to the display's edges in a
// multi monitor environment.
TEST_F(WindowStateTest, SnapWindowBasic) {
  UpdateDisplay("0+0-500x400, 0+500-600x400");
  const gfx::Rect kPrimaryDisplayWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Rect kSecondaryDisplayWorkAreaBounds =
      GetSecondaryDisplay().work_area();

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  WindowState* window_state = WindowState::Get(window.get());
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);
  gfx::Rect expected = gfx::Rect(kPrimaryDisplayWorkAreaBounds.x(),
                                 kPrimaryDisplayWorkAreaBounds.y(),
                                 kPrimaryDisplayWorkAreaBounds.width() / 2,
                                 kPrimaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_secondary);
  expected.set_x(kPrimaryDisplayWorkAreaBounds.right() - expected.width());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  // Move the window to the secondary display.
  window->SetBoundsInScreen(gfx::Rect(600, 0, 100, 100), GetSecondaryDisplay());

  window_state->OnWMEvent(&snap_secondary);
  expected = gfx::Rect(kSecondaryDisplayWorkAreaBounds.x() +
                           kSecondaryDisplayWorkAreaBounds.width() / 2,
                       kSecondaryDisplayWorkAreaBounds.y(),
                       kSecondaryDisplayWorkAreaBounds.width() / 2,
                       kSecondaryDisplayWorkAreaBounds.height());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());

  window_state->OnWMEvent(&snap_primary);
  expected.set_x(kSecondaryDisplayWorkAreaBounds.x());
  EXPECT_EQ(expected.ToString(), window->GetBoundsInScreen().ToString());
}

// Test snapped window bounds when the work area length is odd. For multiresize
// functionality to work, it is important that the snapped windows exactly
// touch. An odd work area length makes this requirement tricky because the
// window widths must be unequal to add up to an odd number.
TEST_F(WindowStateTest, SnapWindowOddWorkAreaLength) {
  UpdateDisplay("1517x805");
  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  ASSERT_EQ(0, work_area.x());
  ASSERT_EQ(1517, work_area.width());

  std::unique_ptr<aura::Window> left_window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  std::unique_ptr<aura::Window> right_window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
  WindowState::Get(left_window.get())->OnWMEvent(&snap_primary);
  WindowState::Get(right_window.get())->OnWMEvent(&snap_secondary);
  EXPECT_EQ(gfx::Rect(0, work_area.y(), 758, work_area.bottom()),
            left_window->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(758, work_area.y(), 759, work_area.bottom()),
            right_window->GetBoundsInScreen());
}

// Test how the minimum width and maximize behavior specified by the
// aura::WindowDelegate affect snapping in landscape display layout.
TEST_F(WindowStateTest, SnapWindowMinimumSizeLandscape) {
  UpdateDisplay("900x600");
  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(0, 100, kWorkAreaBounds.width() - 1, 100)));

  // It should be possible to snap a window with a minimum size.
  const int kMinimumWidth = 750;
  delegate.set_minimum_size(gfx::Size(kMinimumWidth, 0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->CanSnap());
  const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_secondary);
  // Expect right snap with the minimum width.
  const gfx::Rect expected_right_snap(kWorkAreaBounds.width() - kMinimumWidth,
                                      kWorkAreaBounds.y(), kMinimumWidth,
                                      kWorkAreaBounds.height());
  EXPECT_EQ(expected_right_snap, window->GetBoundsInScreen());

  // It should not be possible to snap a window if not maximizable.
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      window->GetProperty(aura::client::kResizeBehaviorKey) ^
                          aura::client::kResizeBehaviorCanMaximize);
  EXPECT_FALSE(window_state->CanSnap());
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      window->GetProperty(aura::client::kResizeBehaviorKey) |
                          aura::client::kResizeBehaviorCanMaximize);
  // It should be possible to snap a window if it can be maximized.
  EXPECT_TRUE(window_state->CanSnap());
}

// Test that a unresizable snappable property allows the window to be snapped.
TEST_F(WindowStateTest, UnresizableWindowSnap) {
  const std::array<bool, 2> orientation_params{false, true};

  for (const auto is_landscape : orientation_params) {
    UpdateDisplay(is_landscape ? "900x600,200x100" : "600x900,100x200");
    auto* const screen = display::Screen::GetScreen();
    ASSERT_EQ(2, screen->GetNumDisplays());

    const display::Display primary_display = screen->GetAllDisplays()[0];
    const display::Display secondary_small_display =
        screen->GetAllDisplays()[1];
    ASSERT_EQ(is_landscape, primary_display.is_landscape());
    ASSERT_EQ(is_landscape, secondary_small_display.is_landscape());

    std::unique_ptr<aura::Window> window(
        CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

    // Make the window unresizable.
    window->SetProperty(aura::client::kResizeBehaviorKey,
                        aura::client::kResizeBehaviorNone);

    auto* const window_state = WindowState::Get(window.get());
    EXPECT_FALSE(window_state->CanSnap());
    EXPECT_FALSE(window_state->CanSnapOnDisplay(primary_display));
    EXPECT_FALSE(window_state->CanSnapOnDisplay(secondary_small_display));

    auto* const opposite_orientation_size =
        is_landscape ? new gfx::Size(0, 300) : new gfx::Size(300, 0);
    window->SetProperty(kUnresizableSnappedSizeKey, opposite_orientation_size);
    EXPECT_FALSE(window_state->CanSnap());
    EXPECT_FALSE(window_state->CanSnapOnDisplay(primary_display));
    EXPECT_FALSE(window_state->CanSnapOnDisplay(secondary_small_display));

    auto* const correct_orientation_size =
        is_landscape ? new gfx::Size(300, 0) : new gfx::Size(0, 300);
    window->SetProperty(kUnresizableSnappedSizeKey, correct_orientation_size);
    EXPECT_TRUE(window_state->CanSnap());
    EXPECT_TRUE(window_state->CanSnapOnDisplay(primary_display));
    EXPECT_FALSE(window_state->CanSnapOnDisplay(secondary_small_display));

    window_util::MoveWindowToDisplay(window.get(),
                                     secondary_small_display.id());
    EXPECT_FALSE(window_state->CanSnap());
    EXPECT_TRUE(window_state->CanSnapOnDisplay(primary_display));
    EXPECT_FALSE(window_state->CanSnapOnDisplay(secondary_small_display));
  }
}

// Test that a unresizable snappable property doesn't have any effect in tablet
// mode.
TEST_F(WindowStateTest, UnresizableWindowSnapInTablet) {
  UpdateDisplay("900x600");

  // Enter tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);
  window->SetProperty(kUnresizableSnappedSizeKey, new gfx::Size(300, 0));

  auto* const window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->CanSnap());
}

// Test that a window's state type can be changed to PIP via a WM transition
// event.
TEST_F(WindowStateTest, CanTransitionToPipWindow) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsPip());

  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());
}

// Test that the PIP window is set to the `PipController` before the
// widget is deactivated. Regression test for http://b/309362942.
TEST_F(WindowStateTest, PipWindowIsSetBeforeWidgetDeactivate) {
  // Make `background_widget` to trigger shelf visibility change after
  // entering PIP.
  auto background_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto* window_state = WindowState::Get(background_widget->GetNativeWindow());
  const WMEvent enter_fullscreen(WM_EVENT_FULLSCREEN);
  window_state->OnWMEvent(&enter_fullscreen);

  auto pip_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto* pip_window_state = WindowState::Get(pip_widget->GetNativeWindow());
  const WMEvent enter_pip(WM_EVENT_PIP);

  // Entering PIP results in shelf visibility change, but it shouldn't
  // cause any crash.
  pip_window_state->OnWMEvent(&enter_pip);
}

// Test that a PIP window cannot be snapped.
TEST_F(WindowStateTest, PipWindowCannotSnap) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->CanSnap());

  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  EXPECT_FALSE(window_state->CanSnap());
}

TEST_F(WindowStateTest, ChromePipWindowUmaMetrics) {
  base::HistogramTester histograms;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_START)));
  EXPECT_EQ(1,
            histograms.GetBucketCount(kAshPipEventsHistogramName,
                                      Sample(AshPipEvents::CHROME_PIP_START)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 2);

  const WMEvent enter_normal(WM_EVENT_NORMAL);
  window_state->OnWMEvent(&enter_normal);

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_END)));
  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::CHROME_PIP_END)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 4);
}

TEST_F(WindowStateTest, AndroidPipWindowUmaMetrics) {
  base::HistogramTester histograms;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_START)));
  EXPECT_EQ(1,
            histograms.GetBucketCount(kAshPipEventsHistogramName,
                                      Sample(AshPipEvents::ANDROID_PIP_START)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 2);

  const WMEvent enter_normal(WM_EVENT_NORMAL);
  window_state->OnWMEvent(&enter_normal);

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_END)));
  EXPECT_EQ(1,
            histograms.GetBucketCount(kAshPipEventsHistogramName,
                                      Sample(AshPipEvents::ANDROID_PIP_END)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 4);

  // Check time count:
  histograms.ExpectTotalCount(kAshPipAndroidPipUseTimeHistogramName, 1);
}

TEST_F(WindowStateTest, ChromePipWindowUmaMetricsCountsExitOnDestroy) {
  base::HistogramTester histograms;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  // Destroy the window.
  window.reset();

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_END)));
  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::CHROME_PIP_END)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 4);
}

TEST_F(WindowStateTest, AndroidPipWindowUmaMetricsCountsExitOnDestroy) {
  base::HistogramTester histograms;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);

  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  // Destroy the window.
  window.reset();

  EXPECT_EQ(1, histograms.GetBucketCount(kAshPipEventsHistogramName,
                                         Sample(AshPipEvents::PIP_END)));
  EXPECT_EQ(1,
            histograms.GetBucketCount(kAshPipEventsHistogramName,
                                      Sample(AshPipEvents::ANDROID_PIP_END)));
  histograms.ExpectTotalCount(kAshPipEventsHistogramName, 4);
}

// Test that modal window dialogs can be snapped.
TEST_F(WindowStateTest, SnapModalWindow) {
  UpdateDisplay("0+0-600x900");
  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  aura::test::TestWindowDelegate parent_delegate;
  std::unique_ptr<aura::Window> parent_window(
      CreateTestWindowInShellWithDelegate(
          &parent_delegate, -1,
          gfx::Rect(kWorkAreaBounds.width(), 0, kWorkAreaBounds.width() / 2,
                    kWorkAreaBounds.height() - 1)));

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(100, 100, 400, 500)));

  delegate.set_minimum_size(gfx::Size(200, 300));

  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->CanSnap());

  ::wm::AddTransientChild(parent_window.get(), window.get());
  EXPECT_TRUE(window_state->CanSnap());

  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanResize);
  EXPECT_FALSE(window_state->CanSnap());

  ::wm::RemoveTransientChild(parent_window.get(), window.get());
}

// Test that the minimum size specified by aura::WindowDelegate gets respected.
TEST_F(WindowStateTest, TestRespectMinimumSize) {
  UpdateDisplay("0+0-1024x768");

  aura::test::TestWindowDelegate delegate;
  const gfx::Size minimum_size(gfx::Size(500, 300));
  delegate.set_minimum_size(minimum_size);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(0, 100, 100, 100)));

  // Check that the window has the correct minimum size.
  EXPECT_EQ(minimum_size.ToString(), window->bounds().size().ToString());

  // Set the size to something bigger - that should work.
  gfx::Rect bigger_bounds(700, 500, 700, 500);
  window->SetBounds(bigger_bounds);
  EXPECT_EQ(bigger_bounds.ToString(), window->bounds().ToString());

  // Set the size to something smaller - that should only resize to the smallest
  // possible size.
  gfx::Rect smaller_bounds(700, 500, 100, 100);
  window->SetBounds(smaller_bounds);
  EXPECT_EQ(minimum_size.ToString(), window->bounds().size().ToString());
}

// Test that the minimum window size specified by aura::WindowDelegate does not
// exceed the screen size.
TEST_F(WindowStateTest, TestIgnoreTooBigMinimumSize) {
  UpdateDisplay("0+0-1024x768");
  const gfx::Size work_area_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().size();
  const gfx::Size illegal_size(1280, 960);
  const gfx::Rect illegal_bounds(gfx::Point(0, 0), illegal_size);

  aura::test::TestWindowDelegate delegate;
  const gfx::Size minimum_size(illegal_size);
  delegate.set_minimum_size(minimum_size);

  // The creation should force the window to respect the screen size.
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithDelegate(&delegate, -1, illegal_bounds));
  EXPECT_EQ(work_area_size.ToString(), window->bounds().size().ToString());

  // Trying to set the size to something bigger then the screen size should be
  // ignored.
  window->SetBounds(illegal_bounds);
  EXPECT_EQ(work_area_size.ToString(), window->bounds().size().ToString());

  // Maximizing the window should not allow it to go bigger than that either.
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Maximize();
  EXPECT_EQ(work_area_size.ToString(), window->bounds().size().ToString());
}

// Test that the maximum size specified by aura::WindowDelegate gets respected.
TEST_F(WindowStateTest, TestRespectMaximumSize) {
  aura::test::TestWindowDelegate delegate;
  constexpr gfx::Size max_size(300, 250);
  constexpr gfx::Size smaller_size(100, 100);
  constexpr gfx::Size larger_size(500, 400);

  delegate.set_maximum_size(max_size);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(larger_size)));

  // Check that the window has the correct maximum size.
  EXPECT_EQ(max_size, window->bounds().size());

  window->SetBounds(gfx::Rect(smaller_size));
  EXPECT_EQ(smaller_size, window->bounds().size());

  window->SetBounds(gfx::Rect(larger_size));
  EXPECT_EQ(max_size, window->bounds().size());
}

// Tests UpdateSnapRatio. (1) It should have ratio reset when window
// enters snapped state; (2) it should update ratio on bounds event when
// snapped.
TEST_F(WindowStateTest, UpdateSnapWidthRatioTest) {
  UpdateDisplay("0+0-900x600");
  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(100, 100, 100, 100)));
  delegate.set_window_component(HTRIGHT);
  WindowState* window_state = WindowState::Get(window.get());
  const WindowSnapWMEvent cycle_snap_primary(WM_EVENT_CYCLE_SNAP_PRIMARY);
  window_state->OnWMEvent(&cycle_snap_primary);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  gfx::Rect expected =
      gfx::Rect(kWorkAreaBounds.x(), kWorkAreaBounds.y(),
                kWorkAreaBounds.width() / 2, kWorkAreaBounds.height());
  EXPECT_EQ(expected, window->GetBoundsInScreen());
  EXPECT_EQ(chromeos::kDefaultSnapRatio, *window_state->snap_ratio());

  // Drag to change snapped window width.
  const int kIncreasedWidth = 225;
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(window->bounds().right(), window->bounds().y());
  generator->PressLeftButton();
  generator->MoveMouseTo(window->bounds().right() + kIncreasedWidth,
                         window->bounds().y());
  generator->ReleaseLeftButton();
  expected.set_width(expected.width() + kIncreasedWidth);
  EXPECT_EQ(expected, window->GetBoundsInScreen());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  EXPECT_EQ(0.75f, *window_state->snap_ratio());

  // Another cycle snap left event will restore window state to normal.
  window_state->OnWMEvent(&cycle_snap_primary);
  EXPECT_EQ(WindowStateType::kNormal, window_state->GetStateType());
  EXPECT_TRUE(window_state->snap_ratio());

  // Another cycle snap left event will snap window and reset snapped width
  // ratio.
  window_state->OnWMEvent(&cycle_snap_primary);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  EXPECT_EQ(chromeos::kDefaultSnapRatio, *window_state->snap_ratio());
}

// Tests that dragging and snapping the snapped window update the width ratio
// correctly (crbug.com/1208969).
TEST_F(WindowStateTest, SnapSnappedWindow) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  UpdateDisplay("800x600");
  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  aura::test::TestWindowDelegate delegate;
  gfx::Size window_normal_size = gfx::Size(800, 100);
  std::unique_ptr<aura::Window> window =
      TestWindowBuilder()
          .SetBounds(gfx::Rect(window_normal_size))
          .SetDelegate(&delegate)
          .AllowAllWindowStates()
          .Build();
  delegate.set_window_component(HTCAPTION);
  WindowState* window_state = WindowState::Get(window.get());
  const WindowSnapWMEvent cycle_snap_primary(WM_EVENT_CYCLE_SNAP_PRIMARY);
  window_state->OnWMEvent(&cycle_snap_primary);

  // Snap window to primary position (left).
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  const gfx::Rect expected_snapped_bounds =
      gfx::Rect(kWorkAreaBounds.x(), kWorkAreaBounds.y(),
                kWorkAreaBounds.width() / 2, kWorkAreaBounds.height());
  // Wait for the snapped animation to complete and test that the window bound
  // is primary-snapped and the snap width ratio is updated.
  window->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                       base::Seconds(1));
  EXPECT_EQ(expected_snapped_bounds, window->GetBoundsInScreen());
  EXPECT_EQ(chromeos::kDefaultSnapRatio, *window_state->snap_ratio());

  // Drag the window to unsnap but do not release.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(window->bounds().CenterPoint());
  generator->PressLeftButton();
  generator->MoveMouseBy(5, 0);
  // While dragged, the window size should restore to its normal bound.
  EXPECT_EQ(window_normal_size, window->bounds().size());
  // Note at this point the window will still have snapped state but appear
  // visually unsnapped so it will still have the previous snap ratio.
  EXPECT_NE(expected_snapped_bounds.size(), window_normal_size);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  EXPECT_EQ(0.5f, *window_state->snap_ratio());

  // Continue dragging the window and snap it back to the same position.
  generator->MoveMouseBy(-405, 0);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  generator->ReleaseLeftButton();

  // The snapped ratio should be correct regardless of whether the animation
  // is finished or not.
  EXPECT_EQ(chromeos::kDefaultSnapRatio, *window_state->snap_ratio());
}

// Test that snapping left/right preserves the restore bounds.
TEST_F(WindowStateTest, RestoreBounds) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  WindowState* window_state = WindowState::Get(window.get());

  EXPECT_TRUE(window_state->IsNormalStateType());

  // 1) Start with restored window with restore bounds set.
  gfx::Rect restore_bounds = window->GetBoundsInScreen();
  restore_bounds.set_width(restore_bounds.width() + 1);
  window_state->SetRestoreBoundsInScreen(restore_bounds);
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);
  const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_secondary);
  EXPECT_NE(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());
  window_state->Restore();
  EXPECT_EQ(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());

  // 2) Start with restored bounds set as a result of maximizing the window.
  window_state->Maximize();
  gfx::Rect maximized_bounds = window->GetBoundsInScreen();
  EXPECT_NE(maximized_bounds.ToString(), restore_bounds.ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());

  window_state->OnWMEvent(&snap_primary);
  EXPECT_NE(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
  EXPECT_NE(maximized_bounds.ToString(),
            window->GetBoundsInScreen().ToString());
  EXPECT_EQ(restore_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(restore_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

// Test that maximizing an auto managed window, then snapping it puts the window
// at the snapped bounds and not at the auto-managed (centered) bounds.
TEST_F(WindowStateTest, AutoManaged) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->SetWindowPositionManaged(true);
  window->Hide();
  window->SetBounds(gfx::Rect(100, 100, 100, 100));
  window->Show();

  window_state->Maximize();
  const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_secondary);

  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect expected_snapped_bounds(
      kWorkAreaBounds.x() + kWorkAreaBounds.width() / 2, kWorkAreaBounds.y(),
      kWorkAreaBounds.width() / 2, kWorkAreaBounds.height());
  EXPECT_EQ(expected_snapped_bounds.ToString(),
            window->GetBoundsInScreen().ToString());

  // The window should still be auto managed despite being right maximized.
  EXPECT_TRUE(window_state->GetWindowPositionManaged());
}

// Test that the replacement of a State object works as expected.
TEST_F(WindowStateTest, SimpleStateSwap) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  window_state->SetStateObject(std::unique_ptr<WindowState::State>(
      new AlwaysMaximizeTestState(window_state->GetStateType())));
  EXPECT_TRUE(window_state->IsMaximized());
}

// Test that the replacement of a state object, following a restore with the
// original one restores the window to its original state.
TEST_F(WindowStateTest, StateSwapRestore) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  std::unique_ptr<WindowState::State> old(
      window_state->SetStateObject(std::unique_ptr<WindowState::State>(
          new AlwaysMaximizeTestState(window_state->GetStateType()))));
  EXPECT_TRUE(window_state->IsMaximized());
  window_state->SetStateObject(std::move(old));
  EXPECT_FALSE(window_state->IsMaximized());
}

// Tests that a window that had same bounds as the work area shrinks after the
// window is maximized and then restored.
TEST_F(WindowStateTest, RestoredWindowBoundsShrink) {
  UpdateDisplay("0+0-600x900");
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  window->SetBounds(work_area);
  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(work_area.ToString(), window->bounds().ToString());

  window_state->Restore();
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_NE(work_area.ToString(), window->bounds().ToString());
  EXPECT_TRUE(work_area.Contains(window->bounds()));
}

TEST_F(WindowStateTest, DoNotResizeMaximizedWindowInFullscreen) {
  const int shelf_inset_first = 600 - ShelfConfig::Get()->shelf_size();
  const int shelf_inset_second = 700 - ShelfConfig::Get()->shelf_size();
  std::unique_ptr<aura::Window> maximized(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> fullscreen(CreateTestWindowInShellWithId(1));
  WindowState* maximized_state = WindowState::Get(maximized.get());
  maximized_state->Maximize();
  ASSERT_TRUE(maximized_state->IsMaximized());
  EXPECT_EQ(gfx::Rect(0, 0, 800, shelf_inset_first).ToString(),
            maximized->GetBoundsInScreen().ToString());

  // Entering fullscreen mode will not update the maximized window's size
  // under fullscreen.
  WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  WindowState* fullscreen_state = WindowState::Get(fullscreen.get());
  fullscreen_state->OnWMEvent(&fullscreen_event);
  ASSERT_TRUE(fullscreen_state->IsFullscreen());
  ASSERT_TRUE(maximized_state->IsMaximized());
  EXPECT_EQ(gfx::Rect(0, 0, 800, shelf_inset_first).ToString(),
            maximized->GetBoundsInScreen().ToString());

  // Updating display size will update the maximum window size.
  UpdateDisplay("900x700");
  EXPECT_EQ("0,0 900x700", maximized->GetBoundsInScreen().ToString());
  fullscreen.reset();

  // Exiting fullscreen will update the maximized window to the work area.
  EXPECT_EQ(gfx::Rect(0, 0, 900, shelf_inset_second).ToString(),
            maximized->GetBoundsInScreen().ToString());
}

TEST_F(WindowStateTest, TrustedPinned) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsTrustedPinned());
  window_util::PinWindow(window.get(), true /* trusted */);
  EXPECT_TRUE(window_state->IsTrustedPinned());

  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  EXPECT_EQ(work_area.ToString(), window->bounds().ToString());

  // Sending non-unpin/non-workspace related event should be ignored.
  {
    const WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
    window_state->OnWMEvent(&fullscreen_event);
  }
  EXPECT_TRUE(window_state->IsTrustedPinned());

  // Update display triggers workspace event.
  UpdateDisplay("300x200");
  EXPECT_EQ("0,0 300x200", window->GetBoundsInScreen().ToString());

  // Unpin should work.
  window_state->Restore();
  EXPECT_FALSE(window_state->IsTrustedPinned());
}

TEST_F(WindowStateTest, AllowSetBoundsDirect) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsMaximized());
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Rect original_bounds(50, 50, 200, 200);
  window->SetBounds(original_bounds);
  ASSERT_EQ(original_bounds, window->bounds());

  window_state->set_allow_set_bounds_direct(true);
  window_state->Maximize();

  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(work_area, window->bounds());

  gfx::Rect new_bounds(10, 10, 300, 300);
  window->SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, window->bounds());

  window_state->Restore();
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(original_bounds, window->bounds());

  window_state->set_allow_set_bounds_direct(false);
  window_state->Maximize();

  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(work_area, window->bounds());
  window->SetBounds(new_bounds);
  EXPECT_EQ(work_area, window->bounds());
}

TEST_F(WindowStateTest, FullscreenMinimizedSwitching) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());

  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());

  // Toggling the fullscreen window should restore to normal.
  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsNormalStateType());

  window_state->Maximize();
  ASSERT_TRUE(window_state->IsMaximized());

  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());

  // Toggling the fullscreen window should restore to maximized.
  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsMaximized());

  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());

  // Minimize from fullscreen.
  window_state->Minimize();
  ASSERT_TRUE(window_state->IsMinimized());

  // Unminimize should restore to fullscreen.
  window_state->Unminimize();
  ASSERT_TRUE(window_state->IsFullscreen());

  // Toggling the fullscreen window should restore to maximized.
  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsMaximized());

  // Minimize from fullscreen.
  window_state->Minimize();
  ASSERT_TRUE(window_state->IsMinimized());

  // Fullscreen a minimized window.
  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());

  // Toggling the fullscreen window should not return to minimized. It should
  // return to the state before minimizing and fullscreen.
  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsMaximized());
}

TEST_F(WindowStateTest, FullscreenToCurrentDisplayExplicitly) {
  UpdateDisplay("800x600,1024x768");
  const auto& displays = display_manager()->active_display_list();
  ASSERT_EQ(displays.size(), 2u);
  EXPECT_EQ(displays[0].size(), gfx::Size(800, 600));
  EXPECT_EQ(displays[1].size(), gfx::Size(1024, 768));

  display::Screen* screen = display::Screen::GetScreen();

  // Start from the 1st display.
  const gfx::Rect initial_bounds(100, 10, 200, 100);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(initial_bounds));
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[0].id());
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsFullscreen());

  // Fullscreen onto current display explicitly.
  ::wm::SetWindowFullscreen(window.get(), true, displays[0].id());
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[0].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), displays[0].bounds());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);

  // Restore back to current display.
  ToggleFullScreen(window_state, nullptr);
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[0].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), initial_bounds);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());
}

TEST_F(WindowStateTest, FullscreenToAnotherDisplayFromNormal) {
  UpdateDisplay("800x600,1024x768,1280x720");
  const auto& displays = display_manager()->active_display_list();
  ASSERT_EQ(displays.size(), 3u);
  EXPECT_EQ(displays[0].size(), gfx::Size(800, 600));
  EXPECT_EQ(displays[1].size(), gfx::Size(1024, 768));
  EXPECT_EQ(displays[2].size(), gfx::Size(1280, 720));

  display::Screen* screen = display::Screen::GetScreen();

  // Start from the 2nd display.
  const gfx::Rect initial_bounds(900, 10, 200, 100);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(initial_bounds));
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsFullscreen());

  // Fullscreen onto 3rd display.
  ::wm::SetWindowFullscreen(window.get(), true, displays[2].id());
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[2].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), displays[2].bounds());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);

  // Restore back to 2nd display.
  ToggleFullScreen(window_state, nullptr);
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), initial_bounds);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());
}

TEST_F(WindowStateTest, FullscreenToAnotherDisplayFromOtherStates) {
  UpdateDisplay("800x600,1024x768,1280x720");
  const auto& displays = display_manager()->active_display_list();
  ASSERT_EQ(displays.size(), 3u);
  EXPECT_EQ(displays[0].size(), gfx::Size(800, 600));
  EXPECT_EQ(displays[1].size(), gfx::Size(1024, 768));
  EXPECT_EQ(displays[2].size(), gfx::Size(1280, 720));

  display::Screen* screen = display::Screen::GetScreen();

  // Start from the 2nd display.
  const gfx::Rect initial_bounds(900, 10, 200, 100);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(initial_bounds));
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsFullscreen());

  const WindowSnapWMEvent snap_right_event(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_right_event);
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);
  const gfx::Rect snapped_bounds = window_state->GetCurrentBoundsInScreen();

  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);
  const gfx::Rect maximized_bounds = window_state->GetCurrentBoundsInScreen();
  EXPECT_EQ(maximized_bounds, displays[1].work_area());

  // Fullscreen onto 3rd display.
  ::wm::SetWindowFullscreen(window.get(), true, displays[2].id());
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[2].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), displays[2].bounds());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);

  // Restore back to 2nd display maximized.
  ToggleFullScreen(window_state, nullptr);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), maximized_bounds);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);

  // Restore again back to snapped.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), snapped_bounds);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);

  // Restore again back to normal state.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), initial_bounds);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());
}

TEST_F(WindowStateTest, FullscreenToAnotherDisplayFromFullscreen) {
  UpdateDisplay("800x600,1024x768,1280x720");
  const auto& displays = display_manager()->active_display_list();
  ASSERT_EQ(displays.size(), 3u);
  EXPECT_EQ(displays[0].size(), gfx::Size(800, 600));
  EXPECT_EQ(displays[1].size(), gfx::Size(1024, 768));
  EXPECT_EQ(displays[2].size(), gfx::Size(1280, 720));

  display::Screen* screen = display::Screen::GetScreen();

  // Start from the 2nd display.
  const gfx::Rect initial_bounds(900, 10, 200, 100);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(initial_bounds));
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsFullscreen());

  // Fullscreen onto 2nd display.
  ToggleFullScreen(window_state, nullptr);
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), displays[1].bounds());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);

  // Fullscreen onto 3rd display.
  ::wm::SetWindowFullscreen(window.get(), true, displays[2].id());
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[2].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), displays[2].bounds());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);

  // Restore back to normal state.
  ToggleFullScreen(window_state, nullptr);
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), initial_bounds);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());
}

TEST_F(WindowStateTest, FullscreenToAnotherDisplayWithMinimize) {
  UpdateDisplay("800x600,1024x768,1280x720");
  const auto& displays = display_manager()->active_display_list();
  ASSERT_EQ(displays.size(), 3u);
  EXPECT_EQ(displays[0].size(), gfx::Size(800, 600));
  EXPECT_EQ(displays[1].size(), gfx::Size(1024, 768));
  EXPECT_EQ(displays[2].size(), gfx::Size(1280, 720));

  display::Screen* screen = display::Screen::GetScreen();

  // Start from the 2nd display.
  const gfx::Rect initial_bounds(900, 10, 200, 100);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(initial_bounds));
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_FALSE(window_state->IsFullscreen());

  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);
  const gfx::Rect maximized_bounds = window_state->GetCurrentBoundsInScreen();
  EXPECT_EQ(maximized_bounds, displays[1].work_area());

  // Fullscreen onto 3rd display.
  ::wm::SetWindowFullscreen(window.get(), true, displays[2].id());
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[2].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), displays[2].bounds());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);

  // Minimize and restore.
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);

  window_state->Restore();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[2].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), displays[2].bounds());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);

  // Restore back to 2nd display snapped.
  ToggleFullScreen(window_state, nullptr);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), maximized_bounds);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), initial_bounds);

  // Restore again back to normal state.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(screen->GetDisplayNearestWindow(window.get()).id(),
            displays[1].id());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), initial_bounds);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());
}

// Tests the window state changes in multi displays while dragging the window to
// a different display through mouse.
TEST_F(WindowStateTest, MouseDragWindowInMultiDisplays) {
  UpdateDisplay("0+0-800x600,801+0-1024x768");
  const auto& displays = display_manager()->active_display_list();

  // Starts with kDefault window state in the 1st display.
  display::Screen* screen = display::Screen::GetScreen();
  const gfx::Rect initial_bounds(10, 20, 200, 100);

  aura::test::TestWindowDelegate test_window_delegate;
  test_window_delegate.set_window_component(HTCAPTION);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithDelegateAndType(
          &test_window_delegate, aura::client::WINDOW_TYPE_NORMAL, 0,
          initial_bounds));
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_EQ(displays[0].id(),
            screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  // Transition to kPrimarySnapped window state.
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(displays[0].id(),
            screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_EQ(initial_bounds, window_state->GetRestoreBoundsInScreen());
  ASSERT_EQ(1u, restore_stack.size());
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);

  // Then transition to kMaximized window state.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  EXPECT_EQ(displays[0].id(),
            screen->GetDisplayNearestWindow(window.get()).id());
  ASSERT_EQ(2u, restore_stack.size());
  EXPECT_EQ(initial_bounds, window_state->GetRestoreBoundsInScreen());
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);

  // Mouse drag the window to snap on the 2nd display. Both the restore bounds
  // property and resotore bounds inside the history stack should be updated to
  // bounds inside the 2nd display. Note since `display2_bounds` are in screen,
  // the event generator coordinates should also be in screen.
  auto* event_generator = GetEventGenerator();
  const gfx::Rect display2_bounds = displays[1].bounds();
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(display2_bounds.left_center());
  ASSERT_TRUE(window_state->is_dragged());
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_TRUE(
      display2_bounds.Contains(window_state->GetRestoreBoundsInScreen()));
  EXPECT_EQ(1u, restore_stack.size());

  // Maximize the window, it should stay inside the 2nd display.
  window_state->OnWMEvent(&maximize_event);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(window.get()).id());

  // Restore the window, it should go back to snapped state and stay inside the
  // 2nd display.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(window.get()).id());

  // Restore the window, it should further go back to normal state and stay
  // inside the 2nd display.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());
  EXPECT_TRUE(restore_stack.empty());
}

// Tests the window state changes in multi displays while moving the window to a
// different display through shortcut.
TEST_F(WindowStateTest, ShortcutMovingWindowInMultiDisplays) {
  UpdateDisplay("0+0-800x600,801+0-1024x768");
  const auto& displays = display_manager()->active_display_list();

  // Starts with kDefault window state in the 1st display.
  display::Screen* screen = display::Screen::GetScreen();
  const gfx::Rect initial_bounds(10, 20, 200, 100);
  std::unique_ptr<aura::Window> window = CreateAppWindow(initial_bounds);
  WindowState* window_state = WindowState::Get(window.get());

  // Snap and then maximize the window in the 1st display to get the restore
  // bounds property and history stack.
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  EXPECT_TRUE(window_state->IsMaximized());

  // Using the shortcut ALT+SEARCH+M to move the window to the 2nd display.
  PressAndReleaseKey(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN);
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(window.get()).id());
  EXPECT_TRUE(window_state->IsMaximized());
  // The restore bounds should be updated to bounds inside the new display.
  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history();
  const gfx::Rect display2_bounds = displays[1].bounds();
  EXPECT_TRUE(
      display2_bounds.Contains(window_state->GetRestoreBoundsInScreen()));
  EXPECT_EQ(2u, restore_stack.size());

  // Restore the window, it should go back to snapped state and stay inside the
  // 2nd display.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(window.get()).id());

  // Maximize the window, it should stay inside the 2nd display.
  window_state->OnWMEvent(&maximize_event);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(window.get()).id());

  // Restore the maximized window, it should go back to snapped state and stay
  // inside the 2nd display.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(window.get()).id());

  // Restore the window, it should further go back to normal state and stay
  // inside the 2nd display.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(window.get()).id());
}

// Tests that the window should not be almost offscreen while being moved to
// another display with multiple historical window states.
TEST_F(WindowStateTest, WindowNoOffscreenInMultiDisplays) {
  UpdateDisplay("0+0-800x600,801+0-1024x768");
  const auto& displays = display_manager()->active_display_list();

  // Starts with kDefault window state in the 2nd display.
  display::Screen* screen = display::Screen::GetScreen();
  const gfx::Rect initial_bounds(900, 10, 200, 100);
  std::unique_ptr<aura::Window> window = CreateAppWindow(initial_bounds);
  WindowState* window_state = WindowState::Get(window.get());

  // Maximize and then fullscreen the window inside the 2nd display to get the
  // restore bounds property and restore history stack.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(window.get()).id());

  // Detach the display. The window should be moved to the 1st display and stay
  // in fullscreen state.
  UpdateDisplay("800x600");
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(displays[0].id(),
            screen->GetDisplayNearestWindow(window.get()).id());

  // Un-fullscreen the window. It should be restored to maximize state.
  window_state->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_TRUE(displays[0].bounds().Contains(window->bounds()));

  // Restore the window, it should go back to normal state and fully inside the
  // display. No offscreen should happen.
  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(displays[0].bounds().Contains(window->bounds()));
}

TEST_F(WindowStateTest, CanFullscreen) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  WindowState* window_state = WindowState::Get(window.get());

  // Allow everything to test for cross interactions with other flags.
  int behavior = ~aura::client::kResizeBehaviorCanFullscreen;

  window->SetProperty(aura::client::kResizeBehaviorKey,
                      behavior | aura::client::kResizeBehaviorCanFullscreen);
  EXPECT_TRUE(window_state->CanFullscreen());
  ToggleFullScreen(window_state, nullptr);
  EXPECT_TRUE(window_state->IsFullscreen());
  ToggleFullScreen(window_state, nullptr);
  EXPECT_FALSE(window_state->IsFullscreen());

  window->SetProperty(aura::client::kResizeBehaviorKey,
                      behavior & ~aura::client::kResizeBehaviorCanFullscreen);
  EXPECT_FALSE(window_state->CanFullscreen());
  ToggleFullScreen(window_state, nullptr);
  EXPECT_FALSE(window_state->IsFullscreen());
}

TEST_F(WindowStateTest, CanConsumeSystemKeys) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  WindowState* window_state = WindowState::Get(window.get());

  EXPECT_FALSE(window_state->CanConsumeSystemKeys());

  window->SetProperty(kCanConsumeSystemKeysKey, true);
  EXPECT_TRUE(window_state->CanConsumeSystemKeys());
}

TEST_F(WindowStateTest,
       RestoreStateAfterEnteringPipViaOcculusionAndDismissingPip) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  window->Show();
  EXPECT_TRUE(window->layer()->visible());

  // Ensure a maximized window gets maximized again after it enters PIP via
  // occlusion, gets minimized, and unminimized.
  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());

  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsMaximized());

  // Ensure a freeform window gets freeform again after it enters PIP via
  // occulusion, gets minimized, and unminimized.
  ::wm::SetWindowState(window.get(), ui::mojom::WindowShowState::kNormal);

  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  window_state->Unminimize();
  EXPECT_TRUE(window_state->GetStateType() == WindowStateType::kNormal);
}

TEST_F(WindowStateTest, RestoreStateAfterEnterPipViaMinimizeAndDismissingPip) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  window->Show();
  EXPECT_TRUE(window->layer()->visible());

  // Ensure a maximized window gets maximized again after it enters PIP via
  // minimize, gets minimized, and unminimized.
  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsMaximized());

  // Ensure a freeform window gets freeform again after it enters PIP via
  // minimize, gets minimized, and unminimized.
  ::wm::SetWindowState(window.get(), ui::mojom::WindowShowState::kNormal);

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  window_state->OnWMEvent(&enter_pip);
  EXPECT_TRUE(window_state->IsPip());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());

  window_state->Unminimize();
  EXPECT_TRUE(window_state->GetStateType() == WindowStateType::kNormal);
}

TEST_F(WindowStateTest, SetBoundsUpdatesSizeOfPipRestoreBounds) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState* window_state = WindowState::Get(window.get());
  window->Show();
  window->SetBounds(gfx::Rect(0, 0, 50, 50));

  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  EXPECT_TRUE(window_state->IsPip());
  EXPECT_TRUE(window_state->HasRestoreBounds());
  EXPECT_EQ(gfx::Rect(8, 8, 50, 50), window_state->GetRestoreBoundsInScreen());
  window_state->window()->SetBounds(gfx::Rect(100, 100, 100, 100));
  // SetBounds only updates the size of the restore bounds.
  EXPECT_EQ(gfx::Rect(8, 8, 100, 100),
            window_state->GetRestoreBoundsInScreen());
}

TEST_F(WindowStateTest, SetBoundsSnapsPipBoundsToScreenEdge) {
  UpdateDisplay("600x900");
  // Create a new PiP window using TestWindowBuilder().
  // Set SetShow to false upon creation to simulate the window being created
  // as a PiP rather than being changed to PiP.
  aura::test::TestWindowDelegate delegate;
  delegate.set_minimum_size(gfx::Size(51, 51));
  std::unique_ptr<aura::Window> window(
      TestWindowBuilder()
          .AllowAllWindowStates()
          .SetBounds(gfx::Rect(541, 50, 50, 50))
          .SetDelegate(&delegate)
          .SetShow(false)
          .Build()
          .release());
  WindowState* window_state = WindowState::Get(window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  window->SetProperty(aura::client::kZOrderingKey,
                      ui::ZOrderLevel::kFloatingWindow);
  EXPECT_TRUE(window_state->IsPip());
  window->Show();

  // Ensure that SnapFraction is set when entering PiP.
  EXPECT_TRUE(PipPositioner::HasSnapFraction(window_state));

  // Ensure that the PIP window is along the right edge of the screen even when
  // the new bounds is adjusted by the minimum size.
  // 541 (left origin) + 51 (PIP width) + 8 (PIP insets) == 600.
  EXPECT_EQ(gfx::Rect(541, 50, 51, 51),
            window_state->window()->GetBoundsInScreen());

  PipPositioner::SaveSnapFraction(window_state,
                                  window_state->window()->GetBoundsInScreen());

  // Ensure that SnapFraction is set.
  EXPECT_TRUE(PipPositioner::HasSnapFraction(window_state));

  // Ensure PiP is set to correct position.
  EXPECT_EQ(gfx::Rect(541, 50, 51, 51),
            PipPositioner::GetPositionAfterMovementAreaChange(window_state));
}

// Make sure the window is transparent only when it is in normal state.
TEST_F(WindowStateTest, OpacityChange) {
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(window->GetTransparent());

  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_FALSE(window->GetTransparent());

  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(window->GetTransparent());

  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_FALSE(window->GetTransparent());

  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(window->GetTransparent());

  ToggleFullScreen(window_state, nullptr);
  ASSERT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(window->GetTransparent());

  window_state->Restore();
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(window->GetTransparent());

  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);
  EXPECT_FALSE(window->GetTransparent());

  window_state->Restore();
  EXPECT_TRUE(window->GetTransparent());

  const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_primary);
  EXPECT_FALSE(window->GetTransparent());

  window_state->OnWMEvent(&snap_primary);
  EXPECT_FALSE(window->GetTransparent());
}

// Tests the basic functionalties related to window state restore history stack.
TEST_F(WindowStateTest, WindowStateRestoreHistoryBasicFunctionalites) {
  UpdateDisplay("800x600");
  const gfx::Rect fullscreen_bounds = GetPrimaryDisplay().bounds();
  const gfx::Rect work_area_bounds = GetPrimaryDisplay().work_area();
  const gfx::Size snap_window_size(work_area_bounds.width() / 2,
                                   work_area_bounds.height());

  // Start with kDefault window state.
  const gfx::Rect default_bounds(20, 10, 200, 150);
  std::unique_ptr<aura::Window> window = CreateAppWindow(default_bounds);
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_EQ(window->GetBoundsInScreen(), default_bounds);

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  // Transition to kPrimarySnapped window state.
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);
  EXPECT_EQ(window->GetBoundsInScreen(), gfx::Rect(snap_window_size));
  ASSERT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Then transition to kMaximized window state.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  EXPECT_EQ(window->GetBoundsInScreen(), work_area_bounds);
  ASSERT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kPrimarySnapped);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Then transition to kFullscreen window state.
  const WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen_event);
  EXPECT_EQ(window->GetBoundsInScreen(), fullscreen_bounds);
  ASSERT_EQ(restore_stack.size(), 3u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(restore_stack[2], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Then transition to kMinimized window state.
  const WMEvent minimized_event(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimized_event);
  ASSERT_EQ(restore_stack.size(), 4u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(restore_stack[2], WindowStateType::kMaximized);
  EXPECT_EQ(restore_stack[3], WindowStateType::kFullscreen);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kFullscreen);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Then start restore from here. It should restore back to kFullscreen window
  // state.
  window_state->Restore();
  EXPECT_EQ(window->GetBoundsInScreen(), fullscreen_bounds);
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kFullscreen);
  ASSERT_EQ(restore_stack.size(), 3u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(restore_stack[2], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Then restore back to kMaximized window state.
  window_state->Restore();
  EXPECT_EQ(window->GetBoundsInScreen(), work_area_bounds);
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kMaximized);
  ASSERT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kPrimarySnapped);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Then restore back to kPrimarySnapped window state.
  window_state->Restore();
  EXPECT_EQ(window->GetBoundsInScreen(), gfx::Rect(snap_window_size));
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kPrimarySnapped);
  ASSERT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Then restore back to kNormal window state.
  window_state->Restore();
  EXPECT_EQ(window->GetBoundsInScreen(), default_bounds);
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  // Restore a kNormal window state window will keep the window's kNormal window
  // state.
  window_state->Restore();
  EXPECT_EQ(window->GetBoundsInScreen(), default_bounds);
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());
}

// Tests that window state transitioning from higher to lower layer will erase
// the window state restore history in between.
TEST_F(WindowStateTest, TransitionFromHighToLowerLayerEraseRestoreHistory) {
  UpdateDisplay("800x600");

  // Start with kDefault window state.
  const gfx::Rect default_bounds(20, 10, 200, 150);
  std::unique_ptr<aura::Window> window = CreateAppWindow(default_bounds);
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  // Transition to kPrimarySnapped window state.
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);

  // Then transition to kMaximized window state.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);

  // Then transition to kFullscreen window state.
  const WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  window_state->OnWMEvent(&fullscreen_event);
  ASSERT_EQ(restore_stack.size(), 3u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(restore_stack[2], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Now transition back to kPrimarySnapped window state. It should have erased
  // any restore history after kPrimarySnapped.
  window_state->OnWMEvent(&snap_primary);
  ASSERT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);
}

// Tests the restore behaviors when window state transitions in the same layer.
// There are 3 cases: {kNormal & kDefault}, {kPrimarySnapped &
// kSecondarySnapped}, and {kMinimized & kPip}.
TEST_F(WindowStateTest, TransitionInTheSameLayerKeepSameRestoreHistory) {
  UpdateDisplay("800x600");

  // First we test kNormal & kDefault.
  // Start with kDefault window state.
  const gfx::Rect default_bounds(20, 10, 200, 150);
  std::unique_ptr<aura::Window> window = CreateAppWindow(default_bounds);
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  // Transition to kNormal window state. Since it's on the same layer as
  // kDefault, kDefault won't be pushed into the restore history stack.
  const WMEvent normal_event(WM_EVENT_NORMAL);
  window_state->OnWMEvent(&normal_event);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  // Test kPrimarySnapped & kSecondarySnapped.
  // Transition to kPrimarySnapped window state.
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);
  ASSERT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Transition to kSecondarySnapped window state. Since it's on the same layer
  // as kPrimarySnapped, kPrimarySnapped won't be pushed into the restore
  // history stack.
  const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_secondary);
  ASSERT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Test kMinimized & kPip.
  // Transition to kMinimized window state.
  const WMEvent minimized_event(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimized_event);
  ASSERT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kNormal);
  EXPECT_EQ(restore_stack[1], WindowStateType::kSecondarySnapped);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kSecondarySnapped);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Transition to kPip Window state. Since it's on the same layer as
  // kMinimized, kMinimized won't be pushed into the restore history stack.
  const WMEvent pip_event(WM_EVENT_PIP);
  window_state->OnWMEvent(&pip_event);
  ASSERT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kNormal);
  EXPECT_EQ(restore_stack[1], WindowStateType::kSecondarySnapped);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kSecondarySnapped);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);
}

// TODO(minch): Check the expected behavior of restoring from window state like
// kPinned, kTrustedPinned that do not support the window state restore history.
// Test the restore behaviors of kPinned and kTrustedPinned window state. They
// are different with kFullscreen restore behaviors.
TEST_F(WindowStateTest, PinnedRestoreTest) {
  UpdateDisplay("800x600");
  const gfx::Rect fullscreen_bounds = GetPrimaryDisplay().bounds();
  const gfx::Rect work_area_bounds = GetPrimaryDisplay().work_area();

  // Start with kDefault window state.
  const gfx::Rect default_bounds(20, 10, 200, 150);
  std::unique_ptr<aura::Window> window = CreateAppWindow(default_bounds);
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  // Transition to kPrimarySnapped window state.
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);

  // Then transition to kMaximized window state.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  ASSERT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kPrimarySnapped);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kPrimarySnapped);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Then transition to kPinned window state. Since kPinned window state is not
  // supported in the window state restore history layer, the restore history
  // stack will be cleared. It can only restore back to kNormal window state.
  const WMEvent pinned_event(WM_EVENT_PIN);
  window_state->OnWMEvent(&pinned_event);
  EXPECT_EQ(window->GetBoundsInScreen(), fullscreen_bounds);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  window_state->Restore();
  EXPECT_EQ(window->GetBoundsInScreen(), work_area_bounds);
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  // Same should happen for kTrustedPinned as well.
  window_state->OnWMEvent(&snap_primary);
  window_state->OnWMEvent(&maximize_event);
  ASSERT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(window_state->GetRestoreWindowState(),
            WindowStateType::kPrimarySnapped);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), work_area_bounds);

  const WMEvent trusted_pinned_event(WM_EVENT_TRUSTED_PIN);
  window_state->OnWMEvent(&trusted_pinned_event);
  EXPECT_EQ(window->GetBoundsInScreen(), fullscreen_bounds);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  window_state->Restore();
  EXPECT_EQ(window->GetBoundsInScreen(), work_area_bounds);
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());
}

// Test the restore behaviors of kMinimized and kPip window state. They are both
// viewed as the final state in the restore layer.
TEST_F(WindowStateTest, MinimizedAndPipRestoreTest) {
  UpdateDisplay("800x600");

  // Start with kDefault window state.
  const gfx::Rect default_bounds(20, 10, 200, 150);
  std::unique_ptr<aura::Window> window = CreateAppWindow(default_bounds);
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  // Maximize the window.
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  ASSERT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // kPip window can be minimized to kMinimized window state, but restoring from
  // kMinimized window state can't restore back to kPip window state.
  const WMEvent pip_event(WM_EVENT_PIP);
  window_state->OnWMEvent(&pip_event);
  ASSERT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  const WMEvent minimized_event(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimized_event);
  ASSERT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Restore the minimized window. It should go back to pre-pip window state.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kMaximized);
  ASSERT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Similarly, if the pre-pip window state is kMinimized, restoring from kPip
  // should go back to the pre-minimized window state.
  window_state->OnWMEvent(&minimized_event);
  ASSERT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  window_state->OnWMEvent(&pip_event);
  ASSERT_EQ(restore_stack.size(), 2u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(restore_stack[1], WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kMaximized);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Restore the Pip window. It should go back to pre-minimized window state.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kMaximized);
  ASSERT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);
}

TEST_F(WindowStateTest, HorizontalMaximizeThenMinimizeAndRestore) {
  UpdateDisplay("800x600");
  const gfx::Rect work_area_bounds = GetPrimaryDisplay().work_area();

  // Start with kDefault window state.
  const gfx::Rect default_bounds(20, 10, 200, 150);
  std::unique_ptr<aura::Window> window = CreateAppWindow(default_bounds);
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  const gfx::Rect horizontal_maximize_bounds(
      0, default_bounds.y(), work_area_bounds.width(), default_bounds.height());
  const WMEvent horizontal_maximize_event(WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE);
  window_state->OnWMEvent(&horizontal_maximize_event);
  EXPECT_EQ(window->GetBoundsInScreen(), horizontal_maximize_bounds);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  const WMEvent minimize_event(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimize_event);
  ASSERT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Unminimize should restore back to horizontally maximized bounds while
  // maintaining restore bounds.
  window_state->Restore();
  EXPECT_EQ(window->GetBoundsInScreen(), horizontal_maximize_bounds);
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);
}

TEST_F(WindowStateTest, HorizontalMaximizeThenMaximizeAndRestore) {
  UpdateDisplay("800x600");
  const gfx::Rect work_area_bounds = GetPrimaryDisplay().work_area();

  // Start with kDefault window state.
  const gfx::Rect default_bounds(20, 10, 200, 150);
  std::unique_ptr<aura::Window> window = CreateAppWindow(default_bounds);
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  const gfx::Rect horizontal_maximize_bounds(
      0, default_bounds.y(), work_area_bounds.width(), default_bounds.height());
  const WMEvent horizontal_maximize_event(WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE);
  window_state->OnWMEvent(&horizontal_maximize_event);
  EXPECT_EQ(window->GetBoundsInScreen(), horizontal_maximize_bounds);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  window_state->OnWMEvent(&maximize_event);
  ASSERT_EQ(restore_stack.size(), 1u);
  EXPECT_EQ(restore_stack[0], WindowStateType::kDefault);
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  // Restore from maximized should go back to default bounds, not the
  // horizontally maximized bounds.
  window_state->Restore();
  EXPECT_EQ(window->GetBoundsInScreen(), default_bounds);
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
}

TEST_F(WindowStateTest, SnapThenResize) {
  UpdateDisplay("800x600");
  const gfx::Rect work_area_bounds = GetPrimaryDisplay().work_area();

  // Start with kDefault window state.
  constexpr gfx::Rect default_bounds(20, 10, 200, 150);
  std::unique_ptr<aura::Window> window = CreateAppWindow(default_bounds);
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->IsNormalStateType());

  const std::vector<chromeos::WindowStateType>& restore_stack =
      window_state->window_state_restore_history();
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetRestoreWindowState(), WindowStateType::kNormal);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());

  // Important! Change the restore bounds, so they are not the same. This will
  // create a conflict between the window's current bounds and the restore
  // bounds when restoring.
  constexpr gfx::Rect moved_bounds(10, 10, 200, 150);
  window->SetBounds(moved_bounds);
  window_state->SetRestoreBoundsInScreen(default_bounds);

  const WindowSnapWMEvent snap_left_event(WM_EVENT_SNAP_PRIMARY);
  const gfx::Rect snapped_left_bounds(0, 0, work_area_bounds.width() / 2,
                                      work_area_bounds.height());
  window_state->OnWMEvent(&snap_left_event);
  EXPECT_EQ(window->GetBoundsInScreen(), snapped_left_bounds);
  EXPECT_EQ(restore_stack.size(), 1U);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), default_bounds);

  const int resize = 100;
  const gfx::Rect resized_bounds(0, resize, work_area_bounds.width() / 2,
                                 work_area_bounds.height() - resize);
  {
    // Drag the top of the window to unsnap and resize.
    ui::test::EventGenerator* generator = GetEventGenerator();
    generator->MoveMouseTo(snapped_left_bounds.top_center().x(),
                           snapped_left_bounds.top_center().y());
    generator->PressLeftButton();
    generator->MoveMouseTo(snapped_left_bounds.top_center().x(), resize);
    generator->ReleaseLeftButton();
  }

  EXPECT_TRUE(window_state->IsNormalStateType());
  EXPECT_TRUE(restore_stack.empty());
  EXPECT_EQ(window_state->GetCurrentBoundsInScreen(), resized_bounds);
  EXPECT_EQ(window_state->GetRestoreBoundsInScreen(), gfx::Rect());
}

// Tests the restore behavior for default or normal window.
TEST_F(WindowStateTest, NormalOrDefaultRestore) {
  // Start with kDefault window state.
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kDefault);

  // Restoring a kDefault window will change its window state to kNormal.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);

  // Restoring kNormal window will do nothing.
  window_state->Restore();
  EXPECT_EQ(window_state->GetStateType(), WindowStateType::kNormal);
}

TEST_F(WindowStateTest, WindowSnapActionSourceUmaMetrics) {
  UpdateDisplay("800x600");
  base::HistogramTester histograms;
  std::unique_ptr<aura::Window> window(CreateAppWindow());
  WindowState* window_state = WindowState::Get(window.get());

  // Use WMEvent to directly snap the window.
  WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_primary);
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kNotSpecified, 1);
  window_state->Maximize();

  // Drag the window to the screen edge to snap.
  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      window.get(), gfx::PointF(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_TOUCH));
  resizer->Drag(gfx::PointF(0, 400), 0);
  resizer->CompleteDrag();
  resizer.reset();
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kDragWindowToEdgeToSnap,
                               1);
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kNotSpecified, 1);
  window_state->Maximize();

  // Use keyboard to snap a window.
  AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kWindowCycleSnapLeft, {});
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kKeyboardShortcutToSnap,
                               1);
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kNotSpecified, 1);
  window_state->Maximize();

  // Restore the maximized window to snap window state.
  window_state->Restore();
  histograms.ExpectBucketCount(
      kWindowSnapActionSourceHistogram,
      WindowSnapActionSource::kSnapByWindowStateRestore, 1);
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kNotSpecified, 1);
  window_state->Maximize();

  // Drag or select overview window to snap window.
  ui::test::EventGenerator* generator = GetEventGenerator();
  EnterOverview();
  ASSERT_TRUE(GetOverviewSession());
  const gfx::Point center_point =
      gfx::ToRoundedPoint(GetOverviewSession()
                              ->GetOverviewItemForWindow(window.get())
                              ->target_bounds()
                              .CenterPoint());
  generator->MoveMouseTo(center_point);
  generator->DragMouseTo(gfx::Point(0, 400));
  histograms.ExpectBucketCount(
      kWindowSnapActionSourceHistogram,
      WindowSnapActionSource::kDragOrSelectOverviewWindowToSnap, 1);
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kNotSpecified, 1);
  window_state->Maximize();

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Use keyboard to snap the window in tablet mode.
  AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kWindowCycleSnapLeft, {});
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kKeyboardShortcutToSnap,
                               2);
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kNotSpecified, 1);

  // Auto-snap in splitview.
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kAutoSnapInSplitView, 1);
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kNotSpecified, 1);

  // Resize in splitview.
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  auto* split_view_divider = split_view_controller->split_view_divider();
  gfx::Rect divider_bounds =
      split_view_divider->GetDividerBoundsInScreen(false);
  split_view_divider->StartResizeWithDivider(divider_bounds.CenterPoint());
  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window.get());
  gfx::Point resize_point(display_bounds.width() * 0.33f, 0);
  split_view_divider->ResizeWithDivider(resize_point);
  // This should not cause any metrics change.
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kNotSpecified, 1);
  split_view_divider->EndResizeWithDivider(resize_point);
  histograms.ExpectBucketCount(kWindowSnapActionSourceHistogram,
                               WindowSnapActionSource::kNotSpecified, 1);
}

// Test how the minimum height specified by the aura::WindowDelegate affects
// snapping in portrait display layout.
TEST_F(WindowStateTest, SnapWindowMinimumSizePortrait) {
  UpdateDisplay("600x900");
  const gfx::Rect kWorkAreaBounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(0, 100, kWorkAreaBounds.width() - 1, 100)));

  // It should be possible to snap a window with a minimum width that is larger
  // a half screen width in horizontal snap layout and snap a window with a
  // minimum height that is longer than a half screen height in vertical snap
  // layout.
  const gfx::Size kMinimumSize = gfx::Size(0, 500);
  delegate.set_minimum_size(kMinimumSize);
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_TRUE(window_state->CanSnap());
  const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
  window_state->OnWMEvent(&snap_secondary);
  // Expect right snap for horizontal snap layout with the minimum width and
  // bottom snap for vertical snap layout with the minimum height.
  const gfx::Rect expected_snap = gfx::Rect(
      kWorkAreaBounds.x(), kWorkAreaBounds.height() - kMinimumSize.height(),
      kWorkAreaBounds.width(), kMinimumSize.height());
  EXPECT_EQ(expected_snap, window->GetBoundsInScreen());
}

// Tests the snapped window states in the external display while removing the
// internal display.
TEST_F(WindowStateTest, SnappedWindowsInExternalDisplay) {
  UpdateDisplay("800x600,1920x1200");

  const auto& displays = display_manager()->active_display_list();
  const int64_t primary_id = displays[0].id();
  const int64_t secondary_id = displays[1].id();
  display::Screen* screen = display::Screen::GetScreen();

  // Create two windows inside the external display.
  std::unique_ptr<aura::Window> w1 =
      CreateTestWindow(gfx::Rect(801, 0, 200, 100));
  std::unique_ptr<aura::Window> w2 =
      CreateTestWindow(gfx::Rect(1000, 0, 200, 100));
  ASSERT_EQ(secondary_id, screen->GetDisplayNearestWindow(w1.get()).id());
  ASSERT_EQ(secondary_id, screen->GetDisplayNearestWindow(w2.get()).id());

  // Put the shelf of the internal display at the bottom while the external
  // display shelf at the left side.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  SetShelfAlignmentPref(prefs, primary_id, ShelfAlignment::kBottom);
  SetShelfAlignmentPref(prefs, secondary_id, ShelfAlignment::kLeft);
  EXPECT_EQ(ShelfAlignment::kBottom,
            Shell::GetRootWindowControllerWithDisplayId(primary_id)
                ->shelf()
                ->alignment());
  EXPECT_EQ(ShelfAlignment::kLeft,
            Shell::GetRootWindowControllerWithDisplayId(secondary_id)
                ->shelf()
                ->alignment());

  // Make `w1` to be left snapped.
  WindowState* window_state1 = WindowState::Get(w1.get());
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state1->OnWMEvent(&snap_left);
  EXPECT_TRUE(window_state1->IsSnapped());
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(w1.get()).id());
  EXPECT_EQ(chromeos::kDefaultSnapRatio, *window_state1->snap_ratio());

  // Make `w2` to be right snapped.
  WindowState* window_state2 = WindowState::Get(w2.get());
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window_state2->OnWMEvent(&snap_right);
  EXPECT_TRUE(window_state2->IsSnapped());
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(w2.get()).id());
  EXPECT_EQ(chromeos::kDefaultSnapRatio, *window_state2->snap_ratio());

  // Store the two snapped window bounds with a left aligned shelf.
  const gfx::Rect w1_local_bounds = w1->bounds();
  const gfx::Rect w2_local_bounds = w2->bounds();

  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);
  // Remove the primary display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Verify that both `w1` and `w2` are still snapped in the external display
  // with unchanged snap ratio, unchanged bounds. There should have no gap
  // between the two snapped windows.
  EXPECT_TRUE(window_state1->IsSnapped());
  EXPECT_TRUE(window_state2->IsSnapped());
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(w1.get()).id());
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(w2.get()).id());
  EXPECT_EQ(chromeos::kDefaultSnapRatio, *window_state1->snap_ratio());
  EXPECT_EQ(chromeos::kDefaultSnapRatio, *window_state2->snap_ratio());
  EXPECT_EQ(w1_local_bounds, w1->bounds());
  EXPECT_EQ(w2_local_bounds, w2->bounds());
  EXPECT_EQ(ShelfAlignment::kLeft,
            Shell::GetRootWindowControllerWithDisplayId(secondary_id)
                ->shelf()
                ->alignment());
}

class WindowStateMetricsTest : public AshTestBase {
 public:
  WindowStateMetricsTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void AdvanceClock(base::TimeDelta delta) {
    task_environment()->AdvanceClock(delta);
    task_environment()->RunUntilIdle();
  }
};

TEST_F(WindowStateMetricsTest, PartialSplitDuration) {
  base::HistogramTester histogram_tester;
  const std::string kHistogramName =
      chromeos::kPartialSplitDurationHistogramName;
  std::unique_ptr<aura::Window> window(CreateAppWindow());
  WindowState* window_state = WindowState::Get(window.get());

  auto* desks_controller = DesksController::Get();
  NewDesk();
  ASSERT_EQ(2u, desks_controller->desks().size());

  // Partial split for 30 seconds, then maximize. Test that it records 0 since
  // it has been less than 1 minute.
  WindowSnapWMEvent partial_event(WM_EVENT_SNAP_PRIMARY,
                                  chromeos::kTwoThirdSnapRatio);
  window_state->OnWMEvent(&partial_event);
  AdvanceClock(base::Seconds(30));
  window_state->Maximize();
  histogram_tester.ExpectBucketCount(kHistogramName, 0, 1);

  // Partial split for 3 minutes, then minimize. Test that it records.
  window_state->OnWMEvent(&partial_event);
  AdvanceClock(base::Minutes(3));
  window_state->Minimize();
  histogram_tester.ExpectBucketCount(kHistogramName, 3, 1);

  // Partial split for 3 hours, then default split. Test that it records
  // in the 180 minute bucket.
  window_state->OnWMEvent(&partial_event);
  AdvanceClock(base::Hours(3));
  WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_event);
  histogram_tester.ExpectBucketCount(kHistogramName, 180, 1);

  // Partial split for 3 minutes, then change display work area, then wait 3
  // minutes, then drag to resize. Test that it continues recording through the
  // work area change but stops when the snap ratio is adjusted.
  window_state->OnWMEvent(&partial_event);
  AdvanceClock(base::Minutes(3));
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  AdvanceClock(base::Minutes(3));
  const int kIncreasedWidth = 225;
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(window->bounds().right(), window->bounds().y());
  generator->PressLeftButton();
  generator->MoveMouseTo(window->bounds().right() + kIncreasedWidth,
                         window->bounds().y());
  generator->ReleaseLeftButton();
  histogram_tester.ExpectBucketCount(kHistogramName, 6, 1);

  // Partial split for 3 minutes, then activate desk 2. Test that it
  // records as the partial window is no longer active and visible.
  window_state->OnWMEvent(&partial_event);
  AdvanceClock(base::Minutes(3));
  ActivateDesk(desks_controller->desks()[1].get());
  histogram_tester.ExpectBucketCount(kHistogramName, 3, 2);

  // Activate desk 1. The partial window will be visible again and start the
  // recording. Test that sending the window to desk 2 records the duration.
  ActivateDesk(desks_controller->desks()[0].get());
  AdvanceClock(base::Minutes(3));
  desks_controller->SendToDeskAtIndex(window.get(), 1);
  histogram_tester.ExpectBucketCount(kHistogramName, 3, 3);

  // Activate desk 2 with the partial window, wait 1 minute, create another
  // partial window, wait another minute, then close both windows. Test that
  // window 1 records in the 2 minute bucket, and window 2 in the 1 minute
  // bucket.
  ActivateDesk(desks_controller->desks()[1].get());
  AdvanceClock(base::Minutes(1));
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  WindowSnapWMEvent partial_secondary(WM_EVENT_SNAP_SECONDARY,
                                      chromeos::kOneThirdSnapRatio);
  WindowState::Get(window2.get())->OnWMEvent(&partial_secondary);
  AdvanceClock(base::Minutes(1));
  window.reset();
  window2.reset();
  histogram_tester.ExpectBucketCount(kHistogramName, 2, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, 1, 1);

  // TODO(sophiewen): Determine whether to stop recording if a partial split
  // window swaps sides, e.g. from one third to two thirds.
}

// TODO(skuhne): Add more unit test to verify the correctness for the restore
// operation.

}  // namespace
}  // namespace ash
