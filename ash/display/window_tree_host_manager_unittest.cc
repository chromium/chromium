// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/window_tree_host_manager.h"

#include <memory>
#include <string>

#include "ash/display/display_util.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/rounded_display/rounded_display_provider.h"
#include "ash/rounded_display/rounded_display_provider_test_api.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_widget_builder.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/cursor_manager_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/display_features.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/display_observer.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/event_monitor.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

const char kWallpaperView[] = "WallpaperViewWidget";
constexpr auto kTestRoundedPanelRadii = gfx::RoundedCornersF(10, 10, 15, 15);

template <typename T>
class Resetter {
 public:
  explicit Resetter(T* value) : value_(*value) { *value = 0; }

  Resetter(const Resetter&) = delete;
  Resetter& operator=(const Resetter&) = delete;

  ~Resetter() = default;
  T value() { return value_; }

 private:
  T value_;
};

display::ManagedDisplayInfo CreateDisplayInfo(int64_t id,
                                              const gfx::Rect& bounds) {
  display::ManagedDisplayInfo info = display::CreateDisplayInfo(id, bounds);
  // Each display should have at least one native mode.
  display::ManagedDisplayMode mode(bounds.size(), /*refresh_rate=*/60.f,
                                   /*is_interlaced=*/true,
                                   /*native=*/true);
  info.SetManagedDisplayModes({mode});
  return info;
}

class TestObserver : public display::DisplayManagerObserver,
                     public display::DisplayObserver,
                     public aura::client::FocusChangeObserver,
                     public ::wm::ActivationChangeObserver {
 public:
  TestObserver() {
    Shell::Get()->display_manager()->AddDisplayManagerObserver(this);
    aura::client::GetFocusClient(Shell::GetPrimaryRootWindow())
        ->AddObserver(this);
    ::wm::GetActivationClient(Shell::GetPrimaryRootWindow())->AddObserver(this);
  }

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override {
    Shell::Get()->display_manager()->RemoveDisplayManagerObserver(this);
    aura::client::GetFocusClient(Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
    ::wm::GetActivationClient(Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
  }

  // Overridden from WindowTreeHostManager::Observer
  void OnWillApplyDisplayChanges() override { ++changing_count_; }
  void OnDidApplyDisplayChanges() override { ++changed_count_; }

  // Overrideen from display::DisplayObserver
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override {
    changed_display_id_ = display.id();
    if (metrics & DISPLAY_METRIC_BOUNDS)
      ++bounds_changed_count_;
    if (metrics & DISPLAY_METRIC_ROTATION)
      ++rotation_changed_count_;
    if (metrics & DISPLAY_METRIC_WORK_AREA)
      ++workarea_changed_count_;
    if (metrics & DISPLAY_METRIC_PRIMARY)
      ++primary_changed_count_;
  }

  // Overridden from aura::client::FocusChangeObserver
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override {
    focus_changed_count_++;
  }

  // Overridden from wm::ActivationChangeObserver
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override {
    activation_changed_count_++;
  }
  void OnAttemptToReactivateWindow(aura::Window* request_active,
                                   aura::Window* actual_active) override {}

  int CountAndReset() {
    EXPECT_EQ(changing_count_, changed_count_);
    changed_count_ = 0;
    return Resetter<int>(&changing_count_).value();
  }

  int64_t GetBoundsChangedCountAndReset() {
    return Resetter<int>(&bounds_changed_count_).value();
  }

  int64_t GetRotationChangedCountAndReset() {
    return Resetter<int>(&rotation_changed_count_).value();
  }

  int64_t GetWorkareaChangedCountAndReset() {
    return Resetter<int>(&workarea_changed_count_).value();
  }

  int64_t GetPrimaryChangedCountAndReset() {
    return Resetter<int>(&primary_changed_count_).value();
  }

  int64_t GetChangedDisplayIdAndReset() {
    return Resetter<int64_t>(&changed_display_id_).value();
  }

  int GetFocusChangedCountAndReset() {
    return Resetter<int>(&focus_changed_count_).value();
  }

  int GetActivationChangedCountAndReset() {
    return Resetter<int>(&activation_changed_count_).value();
  }

 private:
  int changing_count_ = 0;
  int changed_count_ = 0;

  int bounds_changed_count_ = 0;
  int rotation_changed_count_ = 0;
  int workarea_changed_count_ = 0;
  int primary_changed_count_ = 0;
  int64_t changed_display_id_ = 0;

  int focus_changed_count_ = 0;
  int activation_changed_count_ = 0;

  display::ScopedDisplayObserver display_observer_{this};
};

class TestHelper {
 public:
  explicit TestHelper(AshTestBase* delegate);

  TestHelper(const TestHelper&) = delete;
  TestHelper& operator=(const TestHelper&) = delete;

  ~TestHelper();

  void SetSecondaryDisplayLayoutAndOffset(
      display::DisplayPlacement::Position position,
      int offset);

  void SetSecondaryDisplayLayout(display::DisplayPlacement::Position position);

  void SetDefaultDisplayLayout(display::DisplayPlacement::Position position);

  float GetStoredZoomScale(int64_t id);

 private:
  raw_ptr<AshTestBase> delegate_;  // Not owned
};

TestHelper::TestHelper(AshTestBase* delegate) : delegate_(delegate) {}
TestHelper::~TestHelper() = default;

void TestHelper::SetSecondaryDisplayLayoutAndOffset(
    display::DisplayPlacement::Position position,
    int offset) {
  std::unique_ptr<display::DisplayLayout> layout(
      display::test::CreateDisplayLayout(delegate_->display_manager(), position,
                                         offset));
  ASSERT_GT(display::Screen::GetScreen()->GetNumDisplays(), 1);
  delegate_->display_manager()->SetLayoutForCurrentDisplays(std::move(layout));
}

void TestHelper::SetSecondaryDisplayLayout(
    display::DisplayPlacement::Position position) {
  SetSecondaryDisplayLayoutAndOffset(position, 0);
}

void TestHelper::SetDefaultDisplayLayout(
    display::DisplayPlacement::Position position) {
  display::DisplayPlacement default_placement(position, 0);
  delegate_->display_manager()->layout_store()->SetDefaultDisplayPlacement(
      default_placement);
}

float TestHelper::GetStoredZoomScale(int64_t id) {
  return delegate_->display_manager()->GetDisplayInfo(id).zoom_factor();
}

class WindowTreeHostManagerShutdownTest : public AshTestBase,
                                          public TestHelper {
 public:
  WindowTreeHostManagerShutdownTest() : TestHelper(this) {}

  WindowTreeHostManagerShutdownTest(const WindowTreeHostManagerShutdownTest&) =
      delete;
  WindowTreeHostManagerShutdownTest& operator=(
      const WindowTreeHostManagerShutdownTest&) = delete;

  ~WindowTreeHostManagerShutdownTest() override = default;

  void TearDown() override {
    AshTestBase::TearDown();

    // Make sure that primary display is accessible after shutdown.
    display::Display primary =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    EXPECT_EQ(gfx::Rect(0, 0, 444, 333), primary.bounds());
    EXPECT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  }
};

class WindowTreeHostManagerStartupTest : public AshTestBase, public TestHelper {
 public:
  WindowTreeHostManagerStartupTest() : TestHelper(this) {}

  WindowTreeHostManagerStartupTest(const WindowTreeHostManagerStartupTest&) =
      delete;
  WindowTreeHostManagerStartupTest& operator=(
      const WindowTreeHostManagerStartupTest&) = delete;

  ~WindowTreeHostManagerStartupTest() override = default;
};

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
    if (event->flags() & ui::EF_IS_SYNTHESIZED &&
        event->type() != ui::EventType::kMouseExited &&
        event->type() != ui::EventType::kMouseEntered) {
      return;
    }
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

  float touch_radius_x() { return touch_radius_x_; }
  float touch_radius_y() { return touch_radius_y_; }
  float scroll_x_offset() { return scroll_x_offset_; }
  float scroll_y_offset() { return scroll_y_offset_; }
  float scroll_x_offset_ordinal() { return scroll_x_offset_ordinal_; }
  float scroll_y_offset_ordinal() { return scroll_y_offset_ordinal_; }

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

class TestMouseWatcherListener : public views::MouseWatcherListener {
 public:
  TestMouseWatcherListener() = default;

  TestMouseWatcherListener(const TestMouseWatcherListener&) = delete;
  TestMouseWatcherListener& operator=(const TestMouseWatcherListener&) = delete;

 private:
  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override {}
};

// This test fixture adds rounded-corners to the default display on SetUp.
class WindowTreeHostManagerRoundedDisplayTest : public AshTestBase {
 public:
  std::string ToDisplaySpecRadiiString(const gfx::RoundedCornersF& radii) {
    return base::StringPrintf("%1.f|%1.f|%1.f|%1.f", radii.upper_left(),
                              radii.upper_right(), radii.lower_right(),
                              radii.lower_left());
  }

  WindowTreeHostManagerRoundedDisplayTest() = default;

  WindowTreeHostManagerRoundedDisplayTest(
      const WindowTreeHostManagerRoundedDisplayTest&) = delete;
  WindowTreeHostManagerRoundedDisplayTest& operator=(
      const WindowTreeHostManagerRoundedDisplayTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kHostWindowBounds,
        "1920x1080~" + ToDisplaySpecRadiiString(kTestRoundedPanelRadii));
    scoped_features_.InitAndEnableFeature(display::features::kRoundedDisplay);
    AshTestBase::SetUp();

    display::Display primary_display =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    first_display_info_ =
        display_manager()->GetDisplayInfo(primary_display.id());
  }

 protected:
  // Currently `display::features::kRoundedDisplay` feature is used during the
  // `ash::Shell` shutdown as we call `AshTestBase::TearDown()`, therefore
  // `scoped_features_` needs to outlive the call.
  base::test::ScopedFeatureList scoped_features_;

  // ManagedDisplayInfo of the display initialized on the
  // `AshTestBase::SetUp()`.
  display::ManagedDisplayInfo first_display_info_;
};

class WindowTreeHostManagerHistogramTest : public AshTestBase,
                                           public TestHelper {
 public:
  WindowTreeHostManagerHistogramTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        TestHelper(this) {}

  WindowTreeHostManagerHistogramTest(
      const WindowTreeHostManagerHistogramTest&) = delete;
  WindowTreeHostManagerHistogramTest& operator=(
      const WindowTreeHostManagerHistogramTest&) = delete;

  ~WindowTreeHostManagerHistogramTest() override = default;

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  void VerifyActiveEffectiveDPIEmitted(const base::HistogramTester& tester,
                                       bool is_internal_display,
                                       int bucket,
                                       int count) {
    const std::string umaName =
        is_internal_display ? "Ash.Display.InternalDisplay.ActiveEffectiveDPI"
                            : "Ash.Display.ExternalDisplay.ActiveEffectiveDPI";
    tester.ExpectBucketCount(umaName, bucket, count);
  }
};

}  // namespace

class WindowTreeHostManagerTest : public AshTestBase, public TestHelper {
 public:
  WindowTreeHostManagerTest() : TestHelper(this) {}

  WindowTreeHostManagerTest(const WindowTreeHostManagerTest&) = delete;
  WindowTreeHostManagerTest& operator=(const WindowTreeHostManagerTest&) =
      delete;

  ~WindowTreeHostManagerTest() override = default;
};

TEST_F(WindowTreeHostManagerShutdownTest, Shutdown) {
  UpdateDisplay("444x333, 300x200");
}

TEST_F(WindowTreeHostManagerStartupTest, Startup) {
  // Ensure that WindowTreeHostManager was initialized and created at least one
  // root window.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_FALSE(root_windows.empty());
}

TEST_F(WindowTreeHostManagerHistogramTest,
       EmitInternalDisplayEffectiveDPIHistogram) {
  const float kDefaultDeviceDPI = 100.f;
  const float kZoomFactor1 = 1.2f;
  const float kZoomFactor2 = 1.5f;
  const int kRepeatingDelay = 30;
  base::HistogramTester tester;

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 800, 600));
  display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(456, gfx::Rect(100, 200, 1024, 768));
  internal_display_info.set_device_dpi(kDefaultDeviceDPI);
  external_display_info.set_device_dpi(kDefaultDeviceDPI);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  // The expected external display effective dpi bucket is calculated by
  // applying the recommended default scaling factor to the initial device DPI.
  const int expected_external_display_effective_dpi_bucket = 94;

  // Do not emit right after initialization.
  VerifyActiveEffectiveDPIEmitted(tester, /*is_internal_display=*/true,
                                  /*bucket=*/kDefaultDeviceDPI, /*count=*/0);
  VerifyActiveEffectiveDPIEmitted(
      tester, /*is_internal_display=*/false,
      /*bucket=*/expected_external_display_effective_dpi_bucket, /*count=*/0);

  // Firstly emitted after half of delayed time.
  FastForwardBy(base::Minutes(kRepeatingDelay / 2 + 1));
  VerifyActiveEffectiveDPIEmitted(tester, /*is_internal_display=*/true,
                                  /*bucket=*/kDefaultDeviceDPI, /*count=*/1);
  VerifyActiveEffectiveDPIEmitted(
      tester, /*is_internal_display=*/false,
      /*bucket=*/expected_external_display_effective_dpi_bucket, /*count=*/1);

  // Emitted repeatedly after delayed time.
  FastForwardBy(base::Minutes(kRepeatingDelay - 2));
  VerifyActiveEffectiveDPIEmitted(tester, /*is_internal_display=*/true,
                                  /*bucket=*/kDefaultDeviceDPI, /*count=*/1);
  VerifyActiveEffectiveDPIEmitted(
      tester, /*is_internal_display=*/false,
      /*bucket=*/expected_external_display_effective_dpi_bucket, /*count=*/1);
  FastForwardBy(base::Minutes(2));
  VerifyActiveEffectiveDPIEmitted(tester, /*is_internal_display=*/true,
                                  /*bucket=*/kDefaultDeviceDPI, /*count=*/2);
  VerifyActiveEffectiveDPIEmitted(
      tester, /*is_internal_display=*/false,
      /*bucket=*/expected_external_display_effective_dpi_bucket, /*count=*/2);

  // Changing zoom factor will emit to a different bucket.
  internal_display_info.set_zoom_factor(kZoomFactor1);
  external_display_info.set_zoom_factor(kZoomFactor2);
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  FastForwardBy(base::Minutes(kRepeatingDelay));
  VerifyActiveEffectiveDPIEmitted(tester, /*is_internal_display=*/true,
                                  /*bucket=*/kDefaultDeviceDPI / kZoomFactor1,
                                  /*count=*/1);
  VerifyActiveEffectiveDPIEmitted(tester, /*is_internal_display=*/false,
                                  /*bucket=*/kDefaultDeviceDPI / kZoomFactor2,
                                  /*count=*/1);
}

TEST_F(WindowTreeHostManagerTest, SecondaryDisplayLayout) {
  // Creates windows to catch activation change event.
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithId(1));
  w1->Focus();

  TestObserver observer;
  UpdateDisplay("600x500,500x400");
  // only 1st display gets resized.
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  gfx::Insets insets(5);
  int64_t secondary_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  display_manager()->UpdateWorkAreaOfDisplay(secondary_display_id, insets);

  // Default layout is RIGHT.
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(600, 0, 500, 400), GetSecondaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(605, 5, 490, 390), GetSecondaryDisplay().work_area());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());

  // Layout the secondary display to the bottom of the primary.
  SetSecondaryDisplayLayout(display::DisplayPlacement::BOTTOM);
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  // TODO (oshima): work area changes twice because ShelfLayoutManager updates
  // to its own insets.
  EXPECT_EQ(2, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(0, 500, 500, 400), GetSecondaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(5, 505, 490, 390), GetSecondaryDisplay().work_area());

  // Layout the secondary display to the left of the primary.
  SetSecondaryDisplayLayout(display::DisplayPlacement::LEFT);
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(-500, 0, 500, 400), GetSecondaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(-495, 5, 490, 390), GetSecondaryDisplay().work_area());

  // Layout the secondary display to the top of the primary.
  SetSecondaryDisplayLayout(display::DisplayPlacement::TOP);
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(0, -400, 500, 400), GetSecondaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(5, -395, 490, 390), GetSecondaryDisplay().work_area());

  // Layout to the right with an offset.
  SetSecondaryDisplayLayoutAndOffset(display::DisplayPlacement::RIGHT, 300);
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(600, 300, 500, 400), GetSecondaryDisplay().bounds());

  // Keep the minimum 100.
  SetSecondaryDisplayLayoutAndOffset(display::DisplayPlacement::RIGHT, 490);
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(600, 400, 500, 400), GetSecondaryDisplay().bounds());

  SetSecondaryDisplayLayoutAndOffset(display::DisplayPlacement::RIGHT, -400);
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(600, -300, 500, 400), GetSecondaryDisplay().bounds());

  //  Layout to the bottom with an offset.
  SetSecondaryDisplayLayoutAndOffset(display::DisplayPlacement::BOTTOM, -200);
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(-200, 500, 500, 400), GetSecondaryDisplay().bounds());

  // Keep the minimum 100.
  SetSecondaryDisplayLayoutAndOffset(display::DisplayPlacement::BOTTOM, 590);
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(500, 500, 500, 400), GetSecondaryDisplay().bounds());

  SetSecondaryDisplayLayoutAndOffset(display::DisplayPlacement::BOTTOM, -500);
  EXPECT_EQ(secondary_display_id, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());  // resize and add
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(-400, 500, 500, 400), GetSecondaryDisplay().bounds());

  // Setting the same layout shouldn't invoke observers.
  SetSecondaryDisplayLayoutAndOffset(display::DisplayPlacement::BOTTOM, -500);
  EXPECT_EQ(0, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(0, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(0, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(0, observer.CountAndReset());  // resize and add
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(-400, 500, 500, 400), GetSecondaryDisplay().bounds());

  UpdateDisplay("600x500");
  EXPECT_LE(1, observer.GetFocusChangedCountAndReset());
  EXPECT_LE(1, observer.GetActivationChangedCountAndReset());
}

namespace {

display::ManagedDisplayInfo CreateDisplayInfoWithRotation(
    int64_t id,
    int y,
    display::Display::Rotation rotation) {
  display::ManagedDisplayInfo info =
      CreateDisplayInfo(id, gfx::Rect(0, y, 600, 500));
  info.SetRotation(rotation, display::Display::RotationSource::ACTIVE);
  return info;
}

display::ManagedDisplayInfo CreateMirroredDisplayInfo(
    int64_t id,
    float device_scale_factor) {
  display::ManagedDisplayInfo info =
      CreateDisplayInfoWithRotation(id, 0, display::Display::ROTATE_0);
  info.set_device_scale_factor(device_scale_factor);
  return info;
}

}  // namespace

TEST_F(WindowTreeHostManagerTest, MirrorToDockedWithFullscreen) {
  // Creates windows to catch activation change event.
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithId(1));
  w1->Focus();

  // Docked mode.

  const display::ManagedDisplayInfo internal_display_info =
      CreateMirroredDisplayInfo(1, 2.0f);
  const display::ManagedDisplayInfo external_display_info =
      CreateMirroredDisplayInfo(2, 1.0f);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  // Mirror.
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  EXPECT_EQ(1, internal_display_id);
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  WindowState* window_state = WindowState::Get(w1.get());
  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(gfx::Rect(0, 0, 300, 250), w1->bounds());
  // Dock mode.
  TestObserver observer;
  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  // Observers are called due to primary change.
  EXPECT_EQ(2, observer.GetChangedDisplayIdAndReset());
  EXPECT_EQ(1, observer.GetBoundsChangedCountAndReset());
  EXPECT_EQ(1, observer.GetWorkareaChangedCountAndReset());
  EXPECT_EQ(1, observer.GetPrimaryChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());

  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(gfx::Rect(0, 0, 600, 500), w1->bounds());
}

TEST_F(WindowTreeHostManagerTest, BoundsUpdated) {
  // Creates windows to catch activation change event.
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithId(1));
  w1->Focus();

  TestObserver observer;
  SetDefaultDisplayLayout(display::DisplayPlacement::BOTTOM);
  UpdateDisplay("300x200,400x300");  // layout, resize and add.
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());

  gfx::Insets insets(5);
  display_manager()->UpdateWorkAreaOfDisplay(GetSecondaryDisplay().id(),
                                             insets);

  EXPECT_EQ(gfx::Rect(0, 0, 300, 200), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(0, 200, 400, 300), GetSecondaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(5, 205, 390, 290), GetSecondaryDisplay().work_area());

  UpdateDisplay("500x400,300x200");
  EXPECT_EQ(1, observer.CountAndReset());  // two resizes
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(0, 400, 300, 200), GetSecondaryDisplay().bounds());

  UpdateDisplay("500x400,400x300");
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(0, 400, 400, 300), GetSecondaryDisplay().bounds());

  UpdateDisplay("500x400");
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_LE(1, observer.GetFocusChangedCountAndReset());
  EXPECT_LE(1, observer.GetActivationChangedCountAndReset());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400), GetPrimaryDisplay().bounds());
  EXPECT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  UpdateDisplay("400x500*2,400x300");
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 250), GetPrimaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(0, 250, 400, 300), GetSecondaryDisplay().bounds());

  // No change
  UpdateDisplay("400x500*2,400x300");
  // We still call into Pre/PostDisplayConfigurationChange().
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());

  // Rotation
  observer.GetRotationChangedCountAndReset();  // we only want to reset.
  int64_t primary_id = GetPrimaryDisplay().id();
  display_manager()->SetDisplayRotation(
      primary_id, display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(1, observer.GetRotationChangedCountAndReset());
  EXPECT_EQ(1, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
  display_manager()->SetDisplayRotation(
      primary_id, display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(0, observer.GetRotationChangedCountAndReset());
  EXPECT_EQ(0, observer.CountAndReset());
  EXPECT_EQ(0, observer.GetFocusChangedCountAndReset());
  EXPECT_EQ(0, observer.GetActivationChangedCountAndReset());
}

TEST_F(WindowTreeHostManagerTest, FindNearestDisplay) {
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  UpdateDisplay("200x201,300x301");
  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::RIGHT, 50));

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  display::Display secondary_display =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay();
  EXPECT_NE(primary_display.id(), secondary_display.id());
  aura::Window* primary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(primary_display.id());
  aura::Window* secondary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(
          secondary_display.id());
  EXPECT_NE(primary_root, secondary_root);

  // Test that points outside of any display return the nearest display.
  EXPECT_EQ(primary_display.id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point(-100, 0))
                .id());
  EXPECT_EQ(primary_display.id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point(0, -100))
                .id());
  EXPECT_EQ(primary_display.id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point(100, 100))
                .id());
  EXPECT_EQ(primary_display.id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point(224, 25))
                .id());
  EXPECT_EQ(secondary_display.id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point(226, 25))
                .id());
  EXPECT_EQ(secondary_display.id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point(600, 100))
                .id());
  EXPECT_EQ(primary_display.id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point(176, 225))
                .id());
  EXPECT_EQ(secondary_display.id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point(178, 225))
                .id());
  EXPECT_EQ(secondary_display.id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point(300, 400))
                .id());
}

// When shell is initialized, WindowTreeHost for primary display is created
// through WindowTreeHostManager::CreatePrimaryHost which does not call in
// OnDisplayAdded, thus making sure RoundedDisplayProvider is created for the
// primary host during shell initialization.
TEST_F(WindowTreeHostManagerRoundedDisplayTest,
       RoundedDisplayProviderCreatedForPrimaryDisplay) {
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  RoundedDisplayProvider* primary_display_rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(primary_display.id());

  EXPECT_TRUE(primary_display_rounded_display_provider);

  ash::RoundedDisplayProviderTestApi primary_display_provider_test(
      primary_display_rounded_display_provider);
  EXPECT_EQ(kTestRoundedPanelRadii,
            primary_display_provider_test.GetCurrentPanelRadii());
}

TEST_F(WindowTreeHostManagerRoundedDisplayTest,
       SettingAndUpdatingRoundedCornerPropertyOnDisplay) {
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  display::test::DisplayManagerTestApi display_manager_test(display_manager());

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  RoundedDisplayProvider* primary_display_rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(primary_display.id());

  ash::RoundedDisplayProviderTestApi primary_display_provider_test(
      primary_display_rounded_display_provider);

  // Primary display has panel radii set in
  // `WindowTreeHostManagerRoundedDisplayTest::SetUp()`.
  EXPECT_EQ(kTestRoundedPanelRadii,
            primary_display_provider_test.GetCurrentPanelRadii());

  // Adding a secondary display should not propagate the radii of display's
  // panel.
  display_manager()->OnNativeDisplaysChanged(
      {first_display_info_,
       display::ManagedDisplayInfo::CreateFromSpec("1+1-300x200")});

  display::Display secondary_display =
      display_manager_test.GetSecondaryDisplay();

  RoundedDisplayProvider* secondary_display_rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(
          secondary_display.id());

  EXPECT_EQ(kTestRoundedPanelRadii,
            primary_display_provider_test.GetCurrentPanelRadii());
  EXPECT_FALSE(secondary_display_rounded_display_provider);

  // Removing the secondary display should not effect the radii of display's
  // panel.
  display_manager()->OnNativeDisplaysChanged({first_display_info_});
  primary_display_rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(primary_display.id());

  EXPECT_EQ(kTestRoundedPanelRadii,
            primary_display_provider_test.GetCurrentPanelRadii());

  // Changing the the metrics should not effect radii of display's panel.
  display::ManagedDisplayInfo update_first_display_info = first_display_info_;
  update_first_display_info.set_device_scale_factor(2.0);
  update_first_display_info.SetBounds(gfx::Rect(400, 200));

  display_manager()->OnNativeDisplaysChanged({update_first_display_info});
  primary_display_rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(primary_display.id());

  EXPECT_EQ(kTestRoundedPanelRadii,
            primary_display_provider_test.GetCurrentPanelRadii());
}

TEST_F(WindowTreeHostManagerRoundedDisplayTest,
       SwappingPrimaryDisplayShouldUpdateTheHostParent) {
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  display::test::DisplayManagerTestApi display_manager_test(display_manager());

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  RoundedDisplayProvider* primary_display_rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(primary_display.id());

  ash::RoundedDisplayProviderTestApi primary_display_provider_test(
      primary_display_rounded_display_provider);

  // Primary display has panel radii set in
  // `WindowTreeHostManagerRoundedDisplayTest::SetUp()`.
  EXPECT_EQ(kTestRoundedPanelRadii,
            primary_display_provider_test.GetCurrentPanelRadii());

  // Adding a secondary display without rounded-display.
  display_manager()->OnNativeDisplaysChanged(
      {first_display_info_,
       display::ManagedDisplayInfo::CreateFromSpec("1+1-300x200")});

  display::Display secondary_display =
      display_manager_test.GetSecondaryDisplay();

  RoundedDisplayProvider* secondary_display_rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(
          secondary_display.id());

  EXPECT_TRUE(primary_display_rounded_display_provider);
  EXPECT_FALSE(secondary_display_rounded_display_provider);

  RoundedDisplayProviderTestApi primary_provider_test(
      primary_display_rounded_display_provider);

  // Host Widow of RoundedDisplayProvider of primary display is attached to the
  // root window of the display.
  EXPECT_EQ(primary_provider_test.GetHostWindow()->parent(),
            window_tree_host_manager->GetRootWindowForDisplayId(
                primary_display.id()));

  // Switch primary and secondary by display ID.
  window_tree_host_manager->SetPrimaryDisplayId(secondary_display.id());

  // Getting the primary and secondary displays after the swap.
  primary_display = display::Screen::GetScreen()->GetPrimaryDisplay();
  secondary_display = display_manager_test.GetSecondaryDisplay();

  RoundedDisplayProvider* new_primary_display_rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(primary_display.id());

  // New primary display(previous secondary display) should not have rounded
  // display provider.
  EXPECT_FALSE(new_primary_display_rounded_display_provider);

  RoundedDisplayProvider* new_secondary_display_rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(
          secondary_display.id());
  EXPECT_TRUE(new_secondary_display_rounded_display_provider);

  RoundedDisplayProviderTestApi secondary_provider_test(
      new_secondary_display_rounded_display_provider);

  // When a display is swapped, we should update the parent of the host window
  // of the provider instead of creating a new provider.
  EXPECT_EQ(new_secondary_display_rounded_display_provider,
            primary_display_rounded_display_provider);

  // We should have updated the parent of the host window after the swap.
  EXPECT_EQ(secondary_provider_test.GetHostWindow()->parent(),
            window_tree_host_manager->GetRootWindowForDisplayId(
                secondary_display.id()));
}

class HostWindowObserver : aura::WindowObserver {
 public:
  explicit HostWindowObserver(const aura::Window* host_window)
      : host_window_(host_window) {
    // Host window need to be attached to a window tree host i.e part of window
    // tree hierarchy.
    DCHECK(host_window_->parent());
  }

  HostWindowObserver(const HostWindowObserver&) = delete;
  HostWindowObserver& operator=(const HostWindowObserver&) = delete;

  ~HostWindowObserver() override = default;

  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override {
    if (window == host_window_ && !parent) {
      removed_from_host_ = true;
    }
  }

  bool removed_from_host() const { return removed_from_host_; }

 private:
  bool removed_from_host_ = false;
  raw_ptr<const aura::Window, DanglingUntriaged> host_window_ = nullptr;
};

// Tests that RoundedDisplayProvider and its host window are correctly deleted
// when we have only one display that display is being replaced i.e the primary
// window tree host is temporarily stored and then attached to the new display.
TEST_F(WindowTreeHostManagerRoundedDisplayTest,
       RoundedDisplayProviderRemovedFromPrimaryWindowTreeHost) {
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();
  display::test::DisplayManagerTestApi display_manager_test(display_manager());

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  RoundedDisplayProvider* rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(primary_display.id());
  RoundedDisplayProviderTestApi provider_test(rounded_display_provider);

  HostWindowObserver observer(provider_test.GetHostWindow());

  // First display has rounded corners.
  display_manager()->OnNativeDisplaysChanged({first_display_info_});

  // Since the primary display was removed and it was the only display,
  // the primary window tree was temporarily stored and then attached to the new
  // display.
  display_manager()->OnNativeDisplaysChanged(
      {display::ManagedDisplayInfo::CreateFromSpec("1+1-300x200")});

  primary_display = display::Screen::GetScreen()->GetPrimaryDisplay();

  // Confirms that RoundedDisplayProvider was deleted and the host_window was
  // removed from the root_window of the primary window tree host.
  EXPECT_FALSE(window_tree_host_manager->GetRoundedDisplayProvider(
      primary_display.id()));
  EXPECT_FALSE(observer.removed_from_host());
}

TEST_F(WindowTreeHostManagerTest, SwapPrimaryById) {
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  UpdateDisplay("300x200,400x300/h");
  const int shelf_inset_first = 200 - ShelfConfig::Get()->shelf_size();
  const int shelf_inset_second = 300 - ShelfConfig::Get()->shelf_size();
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  display::Display secondary_display =
      display_manager_test.GetSecondaryDisplay();

  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::RIGHT, 50));

  EXPECT_NE(primary_display.id(), secondary_display.id());
  aura::Window* primary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(primary_display.id());
  aura::Window* secondary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(
          secondary_display.id());
  aura::Window* shelf_window =
      GetPrimaryShelf()->shelf_widget()->GetNativeView();
  EXPECT_TRUE(primary_root->Contains(shelf_window));
  EXPECT_FALSE(secondary_root->Contains(shelf_window));
  EXPECT_NE(primary_root, secondary_root);
  EXPECT_EQ(primary_display.id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point(-100, -100))
                .id());
  EXPECT_EQ(
      primary_display.id(),
      display::Screen::GetScreen()->GetDisplayNearestWindow(nullptr).id());

  EXPECT_EQ(gfx::Rect(0, 0, 300, 200), primary_display.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 300, shelf_inset_first),
            primary_display.work_area());
  EXPECT_EQ(gfx::Rect(300, 0, 400, 300), secondary_display.bounds());
  EXPECT_EQ(gfx::Rect(300, 0, 400, shelf_inset_second),
            secondary_display.work_area());
  EXPECT_EQ("id=2200000257, parent=2200000000, right, 50",
            display_manager()
                ->GetCurrentDisplayLayout()
                .placement_list[0]
                .ToString());

  // Switch primary and secondary by display ID.
  TestObserver observer;
  window_tree_host_manager->SetPrimaryDisplayId(secondary_display.id());
  EXPECT_EQ(secondary_display.id(),
            display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(),
            display_manager_test.GetSecondaryDisplay().id());
  EXPECT_LT(0, observer.CountAndReset());

  EXPECT_EQ(primary_root, window_tree_host_manager->GetRootWindowForDisplayId(
                              secondary_display.id()));
  EXPECT_EQ(secondary_root, window_tree_host_manager->GetRootWindowForDisplayId(
                                primary_display.id()));
  EXPECT_TRUE(primary_root->Contains(shelf_window));
  EXPECT_FALSE(secondary_root->Contains(shelf_window));

  const display::DisplayLayout& inverted_layout =
      display_manager()->GetCurrentDisplayLayout();

  EXPECT_EQ("id=2200000000, parent=2200000257, left, -50",
            inverted_layout.placement_list[0].ToString());
  // Test if the bounds are correctly swapped.
  display::Display swapped_primary =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  display::Display swapped_secondary =
      display_manager_test.GetSecondaryDisplay();
  EXPECT_EQ(gfx::Rect(0, 0, 400, 300), swapped_primary.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 400, shelf_inset_second),
            swapped_primary.work_area());
  EXPECT_EQ(gfx::Rect(-300, -50, 300, 200), swapped_secondary.bounds());
  EXPECT_EQ(gfx::Rect(-300, -50, 300, shelf_inset_first),
            swapped_secondary.work_area());

  // Test that the color spaces are correctly swapped.
  auto* swapped_primary_compositor =
      window_tree_host_manager->GetRootWindowForDisplayId(swapped_primary.id())
          ->GetHost()
          ->compositor();
  auto* swapped_secondary_compositor =
      window_tree_host_manager
          ->GetRootWindowForDisplayId(swapped_secondary.id())
          ->GetHost()
          ->compositor();

  EXPECT_EQ(swapped_primary.GetColorSpaces(),
            swapped_primary_compositor->display_color_spaces());
  EXPECT_EQ(swapped_secondary.GetColorSpaces(),
            swapped_secondary_compositor->display_color_spaces());

  // Calling the same ID don't do anything.
  window_tree_host_manager->SetPrimaryDisplayId(secondary_display.id());
  EXPECT_EQ(0, observer.CountAndReset());

  aura::WindowTracker tracker;
  tracker.Add(primary_root);
  tracker.Add(secondary_root);

  // Deleting 2nd display should move the primary to original primary display.
  UpdateDisplay("300x200");
  base::RunLoop().RunUntilIdle();  // RootWindow is deleted in a posted task.
  EXPECT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ(primary_display.id(),
            display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point(-100, -100))
                .id());
  EXPECT_EQ(
      primary_display.id(),
      display::Screen::GetScreen()->GetDisplayNearestWindow(nullptr).id());
  EXPECT_TRUE(tracker.Contains(primary_root));
  EXPECT_FALSE(tracker.Contains(secondary_root));
  EXPECT_TRUE(primary_root->Contains(shelf_window));

  // Adding 2nd display with the same ID.  The 2nd display should become primary
  // since secondary id is still stored as desirable_primary_id.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(
      display_manager()->GetDisplayInfo(primary_display.id()));
  display_info_list.push_back(
      display_manager()->GetDisplayInfo(secondary_display.id()));

  display_manager()->OnNativeDisplaysChanged(display_info_list);

  EXPECT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ(secondary_display.id(),
            display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(primary_display.id(),
            display_manager_test.GetSecondaryDisplay().id());
  EXPECT_EQ(primary_root, window_tree_host_manager->GetRootWindowForDisplayId(
                              secondary_display.id()));
  EXPECT_NE(primary_root, window_tree_host_manager->GetRootWindowForDisplayId(
                              primary_display.id()));
  EXPECT_TRUE(primary_root->Contains(shelf_window));

  // Deleting 2nd display and adding 2nd display with a different ID.  The 2nd
  // display shouldn't become primary.
  UpdateDisplay("300x200");
  display::ManagedDisplayInfo third_display_info =
      CreateDisplayInfo(secondary_display.id() + 1, secondary_display.bounds());
  ASSERT_NE(primary_display.id(), third_display_info.id());

  const display::ManagedDisplayInfo& primary_display_info =
      display_manager()->GetDisplayInfo(primary_display.id());
  std::vector<display::ManagedDisplayInfo> display_info_list2;
  display_info_list2.push_back(primary_display_info);
  display_info_list2.push_back(third_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list2);
  EXPECT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ(primary_display.id(),
            display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(third_display_info.id(),
            display_manager_test.GetSecondaryDisplay().id());
  EXPECT_EQ(primary_root, window_tree_host_manager->GetRootWindowForDisplayId(
                              primary_display.id()));
  EXPECT_NE(primary_root, window_tree_host_manager->GetRootWindowForDisplayId(
                              third_display_info.id()));
  EXPECT_TRUE(primary_root->Contains(shelf_window));
}

TEST_F(WindowTreeHostManagerTest, SetPrimaryWithThreeDisplays) {
  UpdateDisplay("500x400,400x300,300x200");
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList non_primary_ids =
      display_manager()->GetConnectedDisplayIdList();
  auto itr =
      std::remove(non_primary_ids.begin(), non_primary_ids.end(), primary_id);
  ASSERT_TRUE(itr != non_primary_ids.end());
  non_primary_ids.erase(itr, non_primary_ids.end());
  ASSERT_EQ(2u, non_primary_ids.size());

  // Build the following layout:
  //
  // +----------------+       +--------------------+
  // | primary_id (P) | <---- | non_primary_ids[0] |
  // +----------------+       +--------------------+
  //      ^
  //      |
  // +--------------------+
  // | non_primary_ids[1] |
  // +--------------------+
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(non_primary_ids[0], primary_id,
                              display::DisplayPlacement::RIGHT, 0);
  builder.AddDisplayPlacement(non_primary_ids[1], primary_id,
                              display::DisplayPlacement::BOTTOM, 0);
  display_manager()->SetLayoutForCurrentDisplays(builder.Build());

  EXPECT_EQ(primary_id, display::Screen::GetScreen()->GetPrimaryDisplay().id());
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  aura::Window* primary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(primary_id);
  aura::Window* non_primary_root_0 =
      window_tree_host_manager->GetRootWindowForDisplayId(non_primary_ids[0]);
  aura::Window* non_primary_root_1 =
      window_tree_host_manager->GetRootWindowForDisplayId(non_primary_ids[1]);

  // Make non_primary_ids[0] primary.
  window_tree_host_manager->SetPrimaryDisplayId(non_primary_ids[0]);
  EXPECT_EQ(non_primary_ids[0],
            display::Screen::GetScreen()->GetPrimaryDisplay().id());

  // Expect the root windows to be swapped.
  EXPECT_EQ(primary_root, window_tree_host_manager->GetRootWindowForDisplayId(
                              non_primary_ids[0]));
  EXPECT_EQ(non_primary_root_0,
            window_tree_host_manager->GetRootWindowForDisplayId(primary_id));
  EXPECT_EQ(
      non_primary_root_1,
      window_tree_host_manager->GetRootWindowForDisplayId(non_primary_ids[1]));

  // Expect that the layout will be changed to:
  //
  // +----------------+       +-----------------------+
  // | primary_id     | ----> | non_primary_ids[0] (P)|
  // +----------------+       +-----------------------+
  //      ^
  //      |
  // +--------------------+
  // | non_primary_ids[1] |
  // +--------------------+
  {
    const display::DisplayLayout& current_layout =
        display_manager()->GetCurrentDisplayLayout();
    EXPECT_EQ(non_primary_ids[0], current_layout.primary_id);
    ASSERT_EQ(2u, current_layout.placement_list.size());
    EXPECT_EQ(primary_id, current_layout.placement_list[0].display_id);
    EXPECT_EQ(non_primary_ids[0],
              current_layout.placement_list[0].parent_display_id);
    EXPECT_EQ(display::DisplayPlacement::LEFT,
              current_layout.placement_list[0].position);
    EXPECT_EQ(non_primary_ids[1], current_layout.placement_list[1].display_id);
    EXPECT_EQ(primary_id, current_layout.placement_list[1].parent_display_id);
    EXPECT_EQ(display::DisplayPlacement::BOTTOM,
              current_layout.placement_list[1].position);
  }

  // Make non_primary_ids[1] primary.
  window_tree_host_manager->SetPrimaryDisplayId(non_primary_ids[1]);
  EXPECT_EQ(non_primary_ids[1],
            display::Screen::GetScreen()->GetPrimaryDisplay().id());

  // Expect the root windows to be swapped.
  EXPECT_EQ(primary_root, window_tree_host_manager->GetRootWindowForDisplayId(
                              non_primary_ids[1]));
  EXPECT_EQ(
      non_primary_root_1,
      window_tree_host_manager->GetRootWindowForDisplayId(non_primary_ids[0]));
  EXPECT_EQ(non_primary_root_0,
            window_tree_host_manager->GetRootWindowForDisplayId(primary_id));

  // Expect that the layout will be changed to:
  //
  // +----------------+       +--------------------+
  // | primary_id     | <---- | non_primary_ids[0] |
  // +----------------+       +--------------------+
  //      |
  //      V
  // +------------------------+
  // | non_primary_ids[1] (P) |
  // +------------------------+
  {
    const display::DisplayLayout& current_layout =
        display_manager()->GetCurrentDisplayLayout();
    EXPECT_EQ(non_primary_ids[1], current_layout.primary_id);
    ASSERT_EQ(2u, current_layout.placement_list.size());
    EXPECT_EQ(primary_id, current_layout.placement_list[0].display_id);
    EXPECT_EQ(non_primary_ids[1],
              current_layout.placement_list[0].parent_display_id);
    EXPECT_EQ(display::DisplayPlacement::TOP,
              current_layout.placement_list[0].position);
    EXPECT_EQ(non_primary_ids[0], current_layout.placement_list[1].display_id);
    EXPECT_EQ(primary_id, current_layout.placement_list[1].parent_display_id);
    EXPECT_EQ(display::DisplayPlacement::RIGHT,
              current_layout.placement_list[1].position);
  }
}

TEST_F(WindowTreeHostManagerTest, SetPrimaryWithFourDisplays) {
  UpdateDisplay("600x500,500x400,400x300,300x200");
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList non_primary_ids =
      display_manager()->GetConnectedDisplayIdList();
  auto itr =
      std::remove(non_primary_ids.begin(), non_primary_ids.end(), primary_id);
  ASSERT_TRUE(itr != non_primary_ids.end());
  non_primary_ids.erase(itr, non_primary_ids.end());
  ASSERT_EQ(3u, non_primary_ids.size());

  // Build the following layout:
  //
  // +--------------------+   +--------------------+   +--------------------+
  // |                    |   | primary_id (P)     |   |                    |
  // |                    |   +--------------------+   |                    |
  // |                    |             ^              |                    |
  // | non_primary_ids[1] |             |              | non_primary_ids[2] |
  // |                    |   +--------------------+   |                    |
  // |                    |-->| non_primary_ids[0] |<--|                    |
  // +--------------------+   +--------------------+   +--------------------+
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(non_primary_ids[0], primary_id,
                              display::DisplayPlacement::BOTTOM, 0);
  builder.AddDisplayPlacement(non_primary_ids[1], non_primary_ids[0],
                              display::DisplayPlacement::LEFT, 0);
  builder.AddDisplayPlacement(non_primary_ids[2], non_primary_ids[0],
                              display::DisplayPlacement::RIGHT, 0);
  display_manager()->SetLayoutForCurrentDisplays(builder.Build());

  EXPECT_EQ(primary_id, display::Screen::GetScreen()->GetPrimaryDisplay().id());
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  // Make non_primary_ids[2] primary.
  window_tree_host_manager->SetPrimaryDisplayId(non_primary_ids[2]);
  EXPECT_EQ(non_primary_ids[2],
            display::Screen::GetScreen()->GetPrimaryDisplay().id());

  // Expect that the layout will be changed to:
  //
  // +--------------------+   +--------------------+   +--------------------+
  // |                    |   | primary_id         |   |                    |
  // |                    |   +--------------------+   |                    |
  // |                    |             |              |                    |
  // | non_primary_ids[1] |             V              | non_primary_ids[2] |
  // |                    |   +--------------------+   |        (P)         |
  // |                    |-->| non_primary_ids[0] |-->|                    |
  // +--------------------+   +--------------------+   +--------------------+
  {
    const display::DisplayLayout& current_layout =
        display_manager()->GetCurrentDisplayLayout();
    EXPECT_EQ(non_primary_ids[2], current_layout.primary_id);
    ASSERT_EQ(3u, current_layout.placement_list.size());
    EXPECT_EQ(primary_id, current_layout.placement_list[0].display_id);
    EXPECT_EQ(non_primary_ids[0],
              current_layout.placement_list[0].parent_display_id);
    EXPECT_EQ(display::DisplayPlacement::TOP,
              current_layout.placement_list[0].position);
    EXPECT_EQ(non_primary_ids[0], current_layout.placement_list[1].display_id);
    EXPECT_EQ(non_primary_ids[2],
              current_layout.placement_list[1].parent_display_id);
    EXPECT_EQ(display::DisplayPlacement::LEFT,
              current_layout.placement_list[1].position);
    EXPECT_EQ(non_primary_ids[1], current_layout.placement_list[2].display_id);
    EXPECT_EQ(non_primary_ids[0],
              current_layout.placement_list[2].parent_display_id);
    EXPECT_EQ(display::DisplayPlacement::LEFT,
              current_layout.placement_list[2].position);
  }

  // Make non_primary_ids[1] primary.
  window_tree_host_manager->SetPrimaryDisplayId(non_primary_ids[1]);
  EXPECT_EQ(non_primary_ids[1],
            display::Screen::GetScreen()->GetPrimaryDisplay().id());

  // Expect that the layout will be changed to:
  //
  // +--------------------+   +--------------------+   +--------------------+
  // |                    |   | primary_id         |   |                    |
  // |                    |   +--------------------+   |                    |
  // |                    |             |              |                    |
  // | non_primary_ids[1] |             V              | non_primary_ids[2] |
  // |       (P)          |   +--------------------+   |                    |
  // |                    |<--| non_primary_ids[0] |<--|                    |
  // +--------------------+   +--------------------+   +--------------------+
  {
    const display::DisplayLayout& current_layout =
        display_manager()->GetCurrentDisplayLayout();
    EXPECT_EQ(non_primary_ids[1], current_layout.primary_id);
    ASSERT_EQ(3u, current_layout.placement_list.size());
    EXPECT_EQ(primary_id, current_layout.placement_list[0].display_id);
    EXPECT_EQ(non_primary_ids[0],
              current_layout.placement_list[0].parent_display_id);
    EXPECT_EQ(display::DisplayPlacement::TOP,
              current_layout.placement_list[0].position);
    EXPECT_EQ(non_primary_ids[0], current_layout.placement_list[1].display_id);
    EXPECT_EQ(non_primary_ids[1],
              current_layout.placement_list[1].parent_display_id);
    EXPECT_EQ(display::DisplayPlacement::RIGHT,
              current_layout.placement_list[1].position);
    EXPECT_EQ(non_primary_ids[2], current_layout.placement_list[2].display_id);
    EXPECT_EQ(non_primary_ids[0],
              current_layout.placement_list[2].parent_display_id);
    EXPECT_EQ(display::DisplayPlacement::RIGHT,
              current_layout.placement_list[2].position);
  }
}

TEST_F(WindowTreeHostManagerTest, OverscanInsets) {
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();
  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("120x200,300x400*2");
  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  window_tree_host_manager->SetOverscanInsets(
      display1.id(), gfx::Insets::TLBR(10, 15, 20, 25));
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  EXPECT_EQ(gfx::Rect(0, 0, 80, 170), root_windows[0]->bounds());
  EXPECT_EQ(gfx::Size(150, 200), root_windows[1]->bounds().size());
  EXPECT_EQ(gfx::Rect(80, 0, 150, 200),
            display_manager_test.GetSecondaryDisplay().bounds());

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(20, 25);
  EXPECT_EQ(gfx::Point(5, 15), event_handler.GetLocationAndReset());

  window_tree_host_manager->SetOverscanInsets(display1.id(), gfx::Insets());
  EXPECT_EQ(gfx::Rect(0, 0, 120, 200), root_windows[0]->bounds());
  EXPECT_EQ(gfx::Rect(120, 0, 150, 200),
            display_manager_test.GetSecondaryDisplay().bounds());

  generator.MoveMouseToInHost(30, 20);
  EXPECT_EQ(gfx::Point(30, 20), event_handler.GetLocationAndReset());

  // Make sure the root window transformer uses correct scale
  // factor when swapping display. Test crbug.com/253690.
  UpdateDisplay("400x300*2,600x400/o");
  root_windows = Shell::GetAllRootWindows();
  gfx::Point point =
      Shell::GetAllRootWindows()[1]->GetHost()->GetRootTransform().MapPoint(
          gfx::Point());
  EXPECT_EQ(gfx::Point(15, 10), point);

  SwapPrimaryDisplay();
  point = Shell::GetAllRootWindows()[1]->GetHost()->GetRootTransform().MapPoint(
      gfx::Point());
  EXPECT_EQ(gfx::Point(15, 10), point);

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

TEST_F(WindowTreeHostManagerTest, Rotate) {
  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("120x200,300x400*2");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  int64_t display2_id = display_manager_test.GetSecondaryDisplay().id();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ui::test::EventGenerator generator1(root_windows[0]);

  TestObserver observer;
  EXPECT_EQ(gfx::Size(120, 200), root_windows[0]->bounds().size());
  EXPECT_EQ(gfx::Size(150, 200), root_windows[1]->bounds().size());
  EXPECT_EQ(gfx::Rect(120, 0, 150, 200),
            display_manager_test.GetSecondaryDisplay().bounds());
  generator1.MoveMouseToInHost(50, 40);
  EXPECT_EQ(gfx::Point(50, 40), event_handler.GetLocationAndReset());
  EXPECT_EQ(display::Display::ROTATE_0,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_0, GetActiveDisplayRotation(display2_id));
  EXPECT_EQ(0, observer.GetRotationChangedCountAndReset());

  display_manager()->SetDisplayRotation(
      display1.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(gfx::Size(200, 120), root_windows[0]->bounds().size());
  EXPECT_EQ(gfx::Size(150, 200), root_windows[1]->bounds().size());
  EXPECT_EQ(gfx::Rect(200, 0, 150, 200),
            display_manager_test.GetSecondaryDisplay().bounds());
  generator1.MoveMouseToInHost(50, 40);
  EXPECT_EQ(gfx::Point(40, 70), event_handler.GetLocationAndReset());
  EXPECT_EQ(display::Display::ROTATE_90,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_0, GetActiveDisplayRotation(display2_id));
  EXPECT_EQ(1, observer.GetRotationChangedCountAndReset());

  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(
          display_manager(), display::DisplayPlacement::BOTTOM, 50));
  EXPECT_EQ(gfx::Rect(50, 120, 150, 200),
            display_manager_test.GetSecondaryDisplay().bounds());

  display_manager()->SetDisplayRotation(
      display2_id, display::Display::ROTATE_270,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(gfx::Size(200, 120), root_windows[0]->bounds().size());
  EXPECT_EQ(gfx::Size(200, 150), root_windows[1]->bounds().size());
  EXPECT_EQ(gfx::Rect(50, 120, 200, 150),
            display_manager_test.GetSecondaryDisplay().bounds());
  EXPECT_EQ(display::Display::ROTATE_90,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_270,
            GetActiveDisplayRotation(display2_id));
  EXPECT_EQ(1, observer.GetRotationChangedCountAndReset());

  ui::test::EventGenerator generator2(root_windows[1]);
  generator2.MoveMouseToInHost(50, 40);
  EXPECT_EQ(gfx::Point(180, 25), event_handler.GetLocationAndReset());
  display_manager()->SetDisplayRotation(
      display1.id(), display::Display::ROTATE_180,
      display::Display::RotationSource::ACTIVE);

  EXPECT_EQ(gfx::Size(120, 200), root_windows[0]->bounds().size());
  EXPECT_EQ(gfx::Size(200, 150), root_windows[1]->bounds().size());
  // Display must share at least 100, so the x's offset becomes 20.
  EXPECT_EQ(gfx::Rect(20, 200, 200, 150),
            display_manager_test.GetSecondaryDisplay().bounds());
  EXPECT_EQ(display::Display::ROTATE_180,
            GetActiveDisplayRotation(display1.id()));
  EXPECT_EQ(display::Display::ROTATE_270,
            GetActiveDisplayRotation(display2_id));
  EXPECT_EQ(1, observer.GetRotationChangedCountAndReset());

  generator1.MoveMouseToInHost(50, 40);
  EXPECT_EQ(gfx::Point(70, 160), event_handler.GetLocationAndReset());

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

TEST_F(WindowTreeHostManagerTest, ScaleRootWindow) {
  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("600x400*1.6,500x300");

  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display1.id());

  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  display::Display display2 = display_manager_test.GetSecondaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(gfx::Rect(0, 0, 375, 250), display1.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 375, 250), root_windows[0]->bounds());
  EXPECT_EQ(gfx::Rect(375, 0, 500, 300), display2.bounds());
  EXPECT_FLOAT_EQ(1.0f, GetStoredZoomScale(display1.id()));
  EXPECT_FLOAT_EQ(1.0f, GetStoredZoomScale(display2.id()));

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(599, 200);
  EXPECT_EQ(gfx::Point(374, 125), event_handler.GetLocationAndReset());

  display_manager()->UpdateZoomFactor(display1.id(), 1.f / 1.2f);
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  display2 = display_manager_test.GetSecondaryDisplay();
  EXPECT_EQ(gfx::Rect(0, 0, 450, 300), display1.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 450, 300), root_windows[0]->bounds());
  EXPECT_EQ(gfx::Rect(450, 0, 500, 300), display2.bounds());
  EXPECT_FLOAT_EQ(1.f / 1.2f, GetStoredZoomScale(display1.id()));
  EXPECT_FLOAT_EQ(1.0f, GetStoredZoomScale(display2.id()));

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

TEST_F(WindowTreeHostManagerTest, TouchScale) {
  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("300x200*2");
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Window* root_window = root_windows[0];
  ui::test::EventGenerator generator(root_window);

  generator.PressMoveAndReleaseTouchTo(50, 50);
  // Default test touches have radius_x/y = 1.0, with device scale
  // factor = 2, the scaled radius_x/y should be 0.5.
  EXPECT_EQ(0.5, event_handler.touch_radius_x());
  EXPECT_EQ(0.5, event_handler.touch_radius_y());

  generator.ScrollSequence(gfx::Point(0, 0), base::Milliseconds(100), 10.0, 1.0,
                           5, 1);

  // ordinal_offset is invariant to the device scale factor.
  EXPECT_EQ(event_handler.scroll_x_offset(),
            event_handler.scroll_x_offset_ordinal());
  EXPECT_EQ(event_handler.scroll_y_offset(),
            event_handler.scroll_y_offset_ordinal());

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

TEST_F(WindowTreeHostManagerTest, ConvertHostToRootCoords) {
  TestEventHandler event_handler;
  Shell::Get()->AddPreTargetHandler(&event_handler);

  UpdateDisplay("600x400*2/r@0.8");

  display::Display display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(gfx::Rect(0, 0, 250, 375), display1.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 250, 375), root_windows[0]->bounds());
  EXPECT_EQ(0.8f, GetStoredZoomScale(display1.id()));

  ui::test::EventGenerator generator(root_windows[0]);
  generator.MoveMouseToInHost(0, 0);
  // The mouse location must be inside the root bounds in dp.
  EXPECT_EQ(gfx::Point(0, 374), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 0);
  EXPECT_EQ(gfx::Point(0, 0), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 399);
  EXPECT_EQ(gfx::Point(249, 0), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 399);
  EXPECT_EQ(gfx::Point(249, 374), event_handler.GetLocationAndReset());

  UpdateDisplay("600x400*2/u@0.8");
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(gfx::Rect(0, 0, 375, 250), display1.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 375, 250), root_windows[0]->bounds());
  EXPECT_EQ(0.8f, GetStoredZoomScale(display1.id()));

  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ(gfx::Point(374, 249), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 0);
  EXPECT_EQ(gfx::Point(0, 249), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 399);
  EXPECT_EQ(gfx::Point(0, 0), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 399);
  EXPECT_EQ(gfx::Point(374, 0), event_handler.GetLocationAndReset());

  UpdateDisplay("600x400*2/l@0.8");
  display1 = display::Screen::GetScreen()->GetPrimaryDisplay();
  root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(gfx::Rect(0, 0, 250, 375), display1.bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 250, 375), root_windows[0]->bounds());
  EXPECT_EQ(0.8f, GetStoredZoomScale(display1.id()));

  generator.MoveMouseToInHost(0, 0);
  EXPECT_EQ(gfx::Point(249, 0), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 0);
  EXPECT_EQ(gfx::Point(249, 374), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(599, 399);
  EXPECT_EQ(gfx::Point(0, 374), event_handler.GetLocationAndReset());
  generator.MoveMouseToInHost(0, 399);
  EXPECT_EQ(gfx::Point(0, 0), event_handler.GetLocationAndReset());

  Shell::Get()->RemovePreTargetHandler(&event_handler);
}

// Make sure that the compositor based mirroring can switch
// from/to dock mode.
TEST_F(WindowTreeHostManagerTest, DockToSingle) {
  const int64_t internal_id = 1;

  const display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfoWithRotation(internal_id, 0, display::Display::ROTATE_0);
  const display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfoWithRotation(2, 1, display::Display::ROTATE_90);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  // Extended
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  EXPECT_EQ(internal_id, internal_display_id);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  // Dock mode.
  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_FALSE(Shell::GetPrimaryRootWindow()
                   ->GetHost()
                   ->GetRootTransform()
                   .IsIdentityOrIntegerTranslation());

  // Switch to single mode and make sure the transform is the one
  // for the internal display.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(Shell::GetPrimaryRootWindow()
                  ->GetHost()
                  ->GetRootTransform()
                  .IsIdentityOrIntegerTranslation());
}

// Tests if switching two displays at the same time while the primary display
// is swapped should not cause a crash. (crbug.com/426292)
TEST_F(WindowTreeHostManagerTest, ReplaceSwappedPrimary) {
  const display::ManagedDisplayInfo first_display_info =
      CreateDisplayInfoWithRotation(111, 0, display::Display::ROTATE_0);
  const display::ManagedDisplayInfo second_display_info =
      CreateDisplayInfoWithRotation(222, 1, display::Display::ROTATE_0);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  // Extended
  display_info_list.push_back(first_display_info);
  display_info_list.push_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  SwapPrimaryDisplay();

  EXPECT_EQ(222, display::Screen::GetScreen()->GetPrimaryDisplay().id());

  display_info_list.clear();
  const display::ManagedDisplayInfo new_first_display_info =
      CreateDisplayInfoWithRotation(333, 0, display::Display::ROTATE_0);
  const display::ManagedDisplayInfo new_second_display_info =
      CreateDisplayInfoWithRotation(444, 1, display::Display::ROTATE_0);
  display_info_list.push_back(new_first_display_info);
  display_info_list.push_back(new_second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  EXPECT_EQ(333, display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

namespace {

class RootWindowTestObserver : public aura::WindowObserver {
 public:
  RootWindowTestObserver() = default;

  RootWindowTestObserver(const RootWindowTestObserver&) = delete;
  RootWindowTestObserver& operator=(const RootWindowTestObserver&) = delete;

  ~RootWindowTestObserver() override = default;

  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    shelf_display_bounds_ = screen_util::GetDisplayBoundsWithShelf(window);
  }

  // Returns the shelf display bounds, in screen coordinates.
  const gfx::Rect& shelf_display_bounds() const {
    return shelf_display_bounds_;
  }

 private:
  gfx::Rect shelf_display_bounds_;
};

}  // namespace

// Make sure that GetDisplayBoundsWithShelf returns the correct bounds
// when the primary display gets replaced in one of the following scenarios:
// 1) Two displays connected: a) b)
// 2) both are disconnected and new one with the same size as b) is connected
// in one configuration event.
// See crbug.com/547280.
TEST_F(WindowTreeHostManagerTest, ReplacePrimary) {
  display::ManagedDisplayInfo first_display_info =
      CreateDisplayInfoWithRotation(10, 0, display::Display::ROTATE_0);
  first_display_info.SetBounds(gfx::Rect(0, 0, 400, 300));
  const display::ManagedDisplayInfo second_display_info =
      CreateDisplayInfoWithRotation(11, 500, display::Display::ROTATE_0);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  // Extended
  display_info_list.push_back(first_display_info);
  display_info_list.push_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  aura::Window* primary_root = Shell::GetAllRootWindows()[0];

  int64_t new_display_id = 20;
  RootWindowTestObserver test_observer;
  primary_root->AddObserver(&test_observer);

  display_info_list.clear();
  const display::ManagedDisplayInfo new_first_display_info =
      CreateDisplayInfoWithRotation(new_display_id, 0,
                                    display::Display::ROTATE_0);

  display_info_list.push_back(new_first_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  // The shelf is now on the second display.
  EXPECT_EQ(gfx::Rect(400, 0, 600, 500), test_observer.shelf_display_bounds());
  primary_root->RemoveObserver(&test_observer);
}

TEST_F(WindowTreeHostManagerTest, UpdateMouseLocationAfterDisplayChange_Noop) {
  UpdateDisplay("1600x1000*.9");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator generator(root_windows[0]);

  // Set the initial position.
  generator.MoveMouseToInHost(627, 446);
  EXPECT_EQ(gfx::Point(696, 495), env->last_mouse_location());

  // A mouse pointer will stay at the same position.
  UpdateDisplay("1600x1000*.9");
  EXPECT_EQ(gfx::Point(696, 495), env->last_mouse_location());
}

TEST_F(WindowTreeHostManagerTest, UpdateMouseLocationAfterDisplayChange) {
  UpdateDisplay("300x200,400x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator generator_on_2nd(root_windows[1]);

  // Set the initial position.
  generator_on_2nd.MoveMouseToInHost(150, 150);
  EXPECT_EQ(gfx::Point(450, 150), env->last_mouse_location());

  // A mouse pointer will stay in the 2nd display.
  UpdateDisplay("400x300,300x200");
  EXPECT_EQ(gfx::Point(550, 50), env->last_mouse_location());

  // A mouse pointer will be outside of displays and move to the
  // center of 2nd display.
  UpdateDisplay("400x300,110x100");
  EXPECT_EQ(gfx::Point(455, 50), env->last_mouse_location());

  // 2nd display was disconnected, and the cursor is
  // now in the 1st display.
  UpdateDisplay("500x400");
  EXPECT_EQ(gfx::Point(55, 350), env->last_mouse_location());

  // 1st display's resolution has changed, and the mouse pointer is
  // now outside. Move the mouse pointer to the center of 1st display.
  UpdateDisplay("400x300");
  EXPECT_EQ(gfx::Point(200, 150), env->last_mouse_location());

  // Move the mouse pointer to the bottom of 1st display.
  ui::test::EventGenerator generator_on_1st(root_windows[0]);
  generator_on_1st.MoveMouseToInHost(150, 290);
  EXPECT_EQ(gfx::Point(150, 290), env->last_mouse_location());

  // The mouse pointer is now on 2nd display.
  UpdateDisplay("300x280,300x200");
  EXPECT_EQ(gfx::Point(450, 10), env->last_mouse_location());
}

TEST_F(WindowTreeHostManagerTest,
       DontUpdateInvisibleCursorLocationAfterDisplayChange) {
  UpdateDisplay("500x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator generator(root_windows[0]);

  // Logical cursor location is updated to keep the same physical location.
  generator.MoveMouseToInHost(350, 150);
  EXPECT_EQ(gfx::Point(350, 150), env->last_mouse_location());

  UpdateDisplay("300x500/r");
  EXPECT_EQ(gfx::Point(250, 150), env->last_mouse_location());

  // Logical cursor location change shouldn't change when the cursor isn't
  // visible.
  UpdateDisplay("500x300");
  generator.MoveMouseToInHost(350, 150);
  EXPECT_EQ(gfx::Point(350, 150), env->last_mouse_location());

  Shell::Get()->cursor_manager()->HideCursor();
  UpdateDisplay("300x500/r");
  EXPECT_EQ(gfx::Point(350, 150), env->last_mouse_location());
}

TEST_F(WindowTreeHostManagerTest,
       UpdateMouseLocationAfterDisplayChange_2ndOnLeft) {
  // Set the 2nd display on the left.
  display::DisplayLayoutStore* layout_store = display_manager()->layout_store();
  display::DisplayPlacement new_default(display::DisplayPlacement::LEFT, 0);
  layout_store->SetDefaultDisplayPlacement(new_default);

  UpdateDisplay("300x200,400x300");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  EXPECT_EQ(gfx::Rect(-400, 0, 400, 300),
            display::test::DisplayManagerTestApi(display_manager())
                .GetSecondaryDisplay()
                .bounds());

  aura::Env* env = aura::Env::GetInstance();

  // Set the initial position.
  root_windows[0]->MoveCursorTo(gfx::Point(-150, 250));
  EXPECT_EQ(gfx::Point(-150, 250), env->last_mouse_location());

  // A mouse pointer will stay in 2nd display.
  UpdateDisplay("400x300,200x300");
  EXPECT_EQ(gfx::Point(-100, 150), env->last_mouse_location());

  // A mouse pointer will be outside of displays and move to the
  // center of 2nd display.
  UpdateDisplay("400x300,200x100");
  EXPECT_EQ(gfx::Point(-100, 50), env->last_mouse_location());

  // 2nd display was disconnected. Mouse pointer should move to
  // 1st display.
  UpdateDisplay("400x300");
  EXPECT_EQ(gfx::Point(200, 150), env->last_mouse_location());
}

// Test that the cursor swaps displays and that its scale factor and rotation
// are updated when the primary display is swapped.
TEST_F(WindowTreeHostManagerTest,
       UpdateMouseLocationAfterDisplayChange_SwapPrimary) {
  UpdateDisplay("300x200,300x200*2/r");

  aura::Env* env = aura::Env::GetInstance();
  Shell* shell = Shell::Get();
  WindowTreeHostManager* window_tree_host_manager =
      shell->window_tree_host_manager();
  auto* cursor_manager = shell->cursor_manager();
  const auto& cursor_shape_client = aura::client::GetCursorShapeClient();
  CursorManagerTestApi test_api;

  window_tree_host_manager->GetPrimaryRootWindow()->MoveCursorTo(
      gfx::Point(20, 50));

  EXPECT_EQ(gfx::Point(20, 50), env->last_mouse_location());
  EXPECT_EQ(1.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  EXPECT_EQ(display::Display::ROTATE_0, test_api.GetCurrentCursorRotation());

  SwapPrimaryDisplay();

  EXPECT_EQ(gfx::Point(20, 50), env->last_mouse_location());
  EXPECT_EQ(2.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  EXPECT_EQ(display::Display::ROTATE_90, test_api.GetCurrentCursorRotation());
}

// Test that the cursor moves to the other display and that its scale factor
// and rotation are updated when the primary display is disconnected.
TEST_F(WindowTreeHostManagerTest,
       UpdateMouseLocationAfterDisplayChange_PrimaryDisconnected) {
  aura::Env* env = aura::Env::GetInstance();
  Shell* shell = Shell::Get();
  WindowTreeHostManager* window_tree_host_manager =
      shell->window_tree_host_manager();
  auto* cursor_manager = shell->cursor_manager();
  const auto& cursor_shape_client = aura::client::GetCursorShapeClient();
  CursorManagerTestApi test_api;

  UpdateDisplay("400x300*2/r,300x200");
  // Swap the primary display to make it possible to remove the primary display
  // via UpdateDisplay().
  SwapPrimaryDisplay();
  int primary_display_id = window_tree_host_manager->GetPrimaryDisplayId();

  window_tree_host_manager->GetPrimaryRootWindow()->MoveCursorTo(
      gfx::Point(20, 50));

  EXPECT_EQ(gfx::Point(20, 50), env->last_mouse_location());
  EXPECT_EQ(1.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  EXPECT_EQ(display::Display::ROTATE_0, test_api.GetCurrentCursorRotation());

  UpdateDisplay("400x300*2/r");
  ASSERT_NE(primary_display_id,
            window_tree_host_manager->GetPrimaryDisplayId());

  // Cursor should be centered on the remaining display.
  EXPECT_EQ(gfx::Point(75, 100), env->last_mouse_location());

  EXPECT_EQ(2.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  EXPECT_EQ(display::Display::ROTATE_90, test_api.GetCurrentCursorRotation());
}

TEST_F(WindowTreeHostManagerTest,
       UpdateNonVisibleMouseLocationAfterDisplayChange_PrimaryDisconnected) {
  aura::Env* env = aura::Env::GetInstance();
  Shell* shell = Shell::Get();
  WindowTreeHostManager* window_tree_host_manager =
      shell->window_tree_host_manager();
  auto* cursor_manager = shell->cursor_manager();
  const auto& cursor_shape_client = aura::client::GetCursorShapeClient();
  CursorManagerTestApi test_api;

  UpdateDisplay("400x300*2/r,300x200");
  // Swap the primary display to make it possible to remove the primary display
  // via UpdateDisplay().
  SwapPrimaryDisplay();
  int primary_display_id = window_tree_host_manager->GetPrimaryDisplayId();
  gfx::Point cursor_location(20, 50);

  window_tree_host_manager->GetPrimaryRootWindow()->MoveCursorTo(
      cursor_location);

  // Hide cursor before disconnecting the display.
  cursor_manager->HideCursor();

  EXPECT_EQ(cursor_location, env->last_mouse_location());
  EXPECT_EQ(1.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  EXPECT_EQ(display::Display::ROTATE_0, test_api.GetCurrentCursorRotation());

  UpdateDisplay("400x300*2/r");
  ASSERT_NE(primary_display_id,
            window_tree_host_manager->GetPrimaryDisplayId());

  // Show the mouse cursor before checking properties which should be as if the
  // mouse cursor was never hidden.
  cursor_manager->ShowCursor();

  // The cursor will not be centered since it was hidden when the display list
  // was updated.
  EXPECT_EQ(gfx::Point(20, 50), env->last_mouse_location());

  // The cursor scale and rotation should be updated.
  EXPECT_EQ(2.0f,
            cursor_shape_client.GetCursorData(cursor_manager->GetCursor())
                ->scale_factor);
  EXPECT_EQ(display::Display::ROTATE_90, test_api.GetCurrentCursorRotation());
}

// GetRootWindowForDisplayId() for removed display::Display during
// OnDisplayRemoved() should not cause crash. See http://crbug.com/415222
TEST_F(WindowTreeHostManagerTest,
       GetRootWindowForDisplayIdDuringDisplayDisconnection) {
  UpdateDisplay("400x300,300x200");
  aura::Window* root2 =
      Shell::Get()->window_tree_host_manager()->GetRootWindowForDisplayId(
          display::test::DisplayManagerTestApi(display_manager())
              .GetSecondaryDisplay()
              .id());
  views::Widget* widget = views::Widget::CreateWindowWithContext(
      nullptr, root2, gfx::Rect(350, 0, 100, 100));
  views::View* view = new views::View();
  widget->GetContentsView()->AddChildView(view);
  view->SetBounds(0, 0, 100, 100);
  widget->Show();

  TestMouseWatcherListener listener;
  auto watcher = std::make_unique<views::MouseWatcher>(
      std::make_unique<views::MouseWatcherViewHost>(view, gfx::Insets()),
      &listener);
  watcher->Start(root2);

  ui::test::EventGenerator event_generator(
      widget->GetNativeWindow()->GetRootWindow());
  event_generator.MoveMouseToCenterOf(widget->GetNativeWindow());

  UpdateDisplay("400x300");
  watcher.reset();

  widget->CloseNow();
}

// Replicates the behavior of MouseWatcher MouseEvent handling that led to crash
// in https://crbug.com/1278429.
class RootWindowTestEventHandler : public ui::EventHandler {
 public:
  explicit RootWindowTestEventHandler() = default;

  RootWindowTestEventHandler(const RootWindowTestEventHandler&) = delete;
  RootWindowTestEventHandler& operator=(const RootWindowTestEventHandler&) =
      delete;

  ~RootWindowTestEventHandler() override = default;

 private:
  // ui::EventHandler overrides:
  void OnMouseEvent(ui::MouseEvent* event) override {
    // Crash happened when finding the display in
    // |screen::GetDisplayNearestPoint()|, we got the display that was being
    // removed and in turn got null root window from
    // |window_util::GetRootWindowAt| since the root window was moved to the new
    // primary display.
    display::Screen::GetScreen()->GetWindowAtScreenPoint(
        display::Screen::GetScreen()->GetCursorScreenPoint());
  }
};

// Tests for the crash during the replacement of the primary display.
// See https://crbug.com/1278429.
TEST_F(WindowTreeHostManagerTest, GetActiveDisplayWhenReplacingPrimaryDisplay) {
  // We observed a crash during handling of a MouseEvent.
  UpdateDisplay("800x600");
  aura::Window* root_window =
      Shell::Get()->window_tree_host_manager()->GetRootWindowForDisplayId(
          GetPrimaryDisplay().id());

  RootWindowTestEventHandler handler;
  root_window->AddPreTargetHandler(&handler);

  ui::test::EventGenerator generator(root_window);

  // Move the cursor to a coordinate that is in the logical bounds of the older
  // display[0,0 800x600] but not in the logical bounds[0,0 350x250] of the new
  // display. The cursor coordinates also needs to be in the root window's
  // bounds[0,0 700x500] attached to the new primary display.
  generator.MoveMouseTo(400, 300);

  // Replace the primary display with a newer display with a different device
  // scale factor compared to original display.
  display::ManagedDisplayInfo first_display_info =
      CreateDisplayInfoWithRotation(100, 0, display::Display::ROTATE_0);
  first_display_info.SetBounds(gfx::Rect(0, 0, 700, 500));
  first_display_info.set_device_scale_factor(2.0);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(first_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  root_window->RemovePreTargetHandler(&handler);
}

TEST_F(WindowTreeHostManagerTest, KeyEventFromSecondaryDisplay) {
  UpdateDisplay("400x300,300x200");
  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_RETURN, 0);
  ui::Event::DispatcherApi dispatcher_api(&key_event);
  // Set the target to the second display. WindowTreeHostManager will end up
  // targeting the primary display.
  dispatcher_api.set_target(
      Shell::Get()->window_tree_host_manager()->GetRootWindowForDisplayId(
          GetSecondaryDisplay().id()));
  Shell::Get()->window_tree_host_manager()->DispatchKeyEventPostIME(&key_event);
  // As long as nothing crashes, we're good.
}

}  // namespace ash
