// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_manager.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "ash/components/arc/test/fake_compatibility_mode_instance.h"
#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/arc_test_window.h"
#include "chrome/browser/ash/arc/input_overlay/test/event_capturer.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/views/widget/widget.h"

namespace arc::input_overlay {
namespace {
// Package names for testing.
constexpr char kRandomPackageName[] =
    "org.chromium.arc.testapp.inputoverlay_no_data";
constexpr char kRandomGamePackageName[] =
    "org.chromium.arc.testapp.inputoverlay_game";
constexpr char kGameControlsOptOutPackageName[] =
    "org.chromium.arc.testapp.inputoverlay_opt_out";

constexpr const float kTolerance = 0.999f;

constexpr const float kAction0PositionRatio = 0.5f;
constexpr const float kAction1PositionRatio = 0.9f;

constexpr const gfx::Rect window_bounds = gfx::Rect(10, 10, 100, 100);

// Simulates the feature (if `is_feature` is true) or hint (if `is_feature` is
// false) toggle on `window`. When toggling the feature, it also
// toggles the hint.
void ToggleGameControls(aura::Window* window, bool is_feature) {
  const bool toggle_on =
      !IsFlagSet(window->GetProperty(ash::kArcGameControlsFlagsKey),
                 is_feature ? ash::ArcGameControlsFlag::kEnabled
                            : ash::ArcGameControlsFlag::kHint);
  window->SetProperty(
      ash::kArcGameControlsFlagsKey,
      UpdateFlag(window->GetProperty(ash::kArcGameControlsFlagsKey),
                 is_feature
                     ? static_cast<ash::ArcGameControlsFlag>(
                           /*enable_flag=*/ash::ArcGameControlsFlag::kEnabled |
                           ash::ArcGameControlsFlag::kHint)
                     : ash::ArcGameControlsFlag::kHint,
                 toggle_on));
}

// Verifies UKM event entry size of ToggleWithMappingSource is
// `expected_entry_size` and the entry of `index` matches
// `expected_event_values`.
void VerifyToggleWithMappingSourceUkmEvent(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    size_t expected_entry_size,
    size_t index,
    std::map<std::string, int64_t> expected_event_values) {
  DCHECK_LT(index, expected_entry_size);
  const auto ukm_entries = ukm_recorder.GetEntriesByName(
      BuildGameControlsUkmEventName(kToggleWithMappingSourceHistogram));
  EXPECT_EQ(expected_entry_size, ukm_entries.size());
  for (const auto& value_item : expected_event_values) {
    ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
        ukm_entries[index], value_item.first, value_item.second);
  }
}

}  // namespace

class TestArcInputOverlayManager : public ArcInputOverlayManager {
 public:
  TestArcInputOverlayManager()
      : ArcInputOverlayManager(
            /*BrowserContext=*/TestingProfile::Builder().Build().get(),
            /*ArcBridgeService=*/nullptr) {}
  ~TestArcInputOverlayManager() override = default;

 private:
  // ArcInputOverlayManager:
  void AddDisplayOverlayController(TouchInjector* touch_injector) override {
    DCHECK(registered_top_level_window_);
    DCHECK(touch_injector);
    if (!registered_top_level_window_ || !touch_injector) {
      return;
    }
    DCHECK(!display_overlay_controller_);
    display_overlay_controller_ = std::make_unique<DisplayOverlayController>(
        touch_injector, /*first_launch=*/false);
  }
};

// ArcInputOverlayManagerTest needs to run on MainThread when involving with
// profile and mojo connection.
class ArcInputOverlayManagerTest : public ash::AshTestBase {
 public:
  ArcInputOverlayManagerTest()
      : ash::AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))) {}

  bool IsInputOverlayEnabled(const aura::Window* window) const {
    return arc_test_input_overlay_manager_->input_overlay_enabled_windows_
        .contains(window);
  }

  ui::InputMethod* GetInputMethod() {
    return arc_test_input_overlay_manager_->input_method_;
  }

  bool IsTextInputActive() {
    return arc_test_input_overlay_manager_->is_text_input_active_;
  }

  int EnabledWindows() {
    return arc_test_input_overlay_manager_->input_overlay_enabled_windows_
        .size();
  }

  TouchInjector* GetTouchInjector(aura::Window* window) {
    auto it =
        arc_test_input_overlay_manager_->input_overlay_enabled_windows_.find(
            window);
    if (it !=
        arc_test_input_overlay_manager_->input_overlay_enabled_windows_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  aura::Window* GetRegisteredWindow() {
    return arc_test_input_overlay_manager_->registered_top_level_window_;
  }

  KeyEventSourceRewriter* GetKeyEventSourceRewriter() {
    return arc_test_input_overlay_manager_->key_event_source_rewriter_.get();
  }

  DisplayOverlayController* GetDisplayOverlayController() {
    return arc_test_input_overlay_manager_->display_overlay_controller_.get();
  }

  bool IsObserving(aura::Window* window) const {
    return arc_test_input_overlay_manager_->window_observations_
        .IsObservingSource(window);
  }

 protected:
  // ash::AshTestBase:
  void SetUp() override {
    ash::AshTestBase::SetUp();
    arc_test_input_overlay_manager_ =
        base::WrapUnique(new TestArcInputOverlayManager());
  }

  void TearDown() override {
    arc_test_input_overlay_manager_->Shutdown();
    arc_test_input_overlay_manager_.reset();
    ash::AshTestBase::TearDown();
  }

  std::unique_ptr<ArcInputOverlayManager> arc_test_input_overlay_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// -----------------------------------------------------------------------------
// VersionArcInputOverlayManagerTest:
// Test fixture to test both pre-beta and beta version depending on the test
// param (true for beta version, false for pre-beta version).
class VersionArcInputOverlayManagerTest
    : public ArcInputOverlayManagerTest,
      public testing::WithParamInterface<bool> {
 public:
  VersionArcInputOverlayManagerTest() = default;
  ~VersionArcInputOverlayManagerTest() override = default;

  // ArcInputOverlayManagerTest:
  void SetUp() override {
    ArcInputOverlayManagerTest::SetUp();
    if (IsBetaVersion()) {
      scoped_feature_list_.InitAndEnableFeature(ash::features::kGameDashboard);
    } else {
      scoped_feature_list_.InitAndDisableFeature(ash::features::kGameDashboard);
    }

    profile_ = std::make_unique<TestingProfile>();
    arc_app_test_.set_wait_compatibility_mode(true);
    arc_app_test_.SetUp(profile_.get());

    SimulatedAppInstalled(task_environment(), arc_app_test_,
                          kEnabledPackageName,
                          /*is_gc_opt_out=*/false,
                          /*is_game=*/true);
    SimulatedAppInstalled(task_environment(), arc_app_test_,
                          kRandomGamePackageName,
                          /*is_gc_opt_out=*/false,
                          /*is_game=*/true);
  }

  void TearDown() override {
    arc_app_test_.TearDown();
    profile_.reset();
    ArcInputOverlayManagerTest::TearDown();
  }

 protected:
  bool IsBetaVersion() const { return GetParam(); }

  ArcAppTest arc_app_test_;

 private:
  std::unique_ptr<TestingProfile> profile_;
};

TEST_P(VersionArcInputOverlayManagerTest, TestPropertyChangeAndWindowDestroy) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  // Test app with input overlay data.
  auto arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kEnabledPackageName);
  auto* arc_window_ptr = arc_window->GetNativeWindow();
  EXPECT_TRUE(IsObserving(arc_window_ptr));
  EXPECT_TRUE(IsInputOverlayEnabled(arc_window_ptr));
  // Input overlay registers the window after reading the data when the window
  // is still focused. In the test, the arc_window is considered as focused now.
  EXPECT_TRUE(GetRegisteredWindow());
  focus_client->FocusWindow(arc_window_ptr);
  EXPECT_TRUE(GetRegisteredWindow());

  // Test app with input overlay data when window is destroyed.
  arc_window.reset();
  EXPECT_FALSE(IsObserving(arc_window_ptr));
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window_ptr));

  // Test app without input overlay data.
  auto arc_window_no_data = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kRandomPackageName);
  EXPECT_FALSE(IsObserving(arc_window_ptr));
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window_no_data->GetNativeWindow()));
}

TEST_P(VersionArcInputOverlayManagerTest, TestWindowDestroyNoWait) {
  // This test is to check UAF issue reported in crbug.com/1363030.
  task_environment()->RunUntilIdle();
  auto arc_window = CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                                    window_bounds, kEnabledPackageName);
  const auto* arc_window_ptr = arc_window->GetNativeWindow();

  // Destroy window before finishing I/O reading. The window can't be destroyed
  // during ReadDefaultData(), but it can be destroyed before
  // ReadCustomizedData() and TouchInjector.RecordMenuStateOnLaunch() would
  // catch it.
  arc_window.reset();
  task_environment()->FastForwardBy(kIORead);
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window_ptr));
}

TEST_P(VersionArcInputOverlayManagerTest, TestInputMethodObsever) {
  ASSERT_FALSE(GetInputMethod());
  ASSERT_FALSE(IsTextInputActive());
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  auto arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kEnabledPackageName);
  focus_client->FocusWindow(arc_window->GetNativeWindow());
  ui::InputMethod* input_method = GetInputMethod();
  EXPECT_TRUE(GetInputMethod());
  input_method->SetFocusedTextInputClient(nullptr);
  EXPECT_FALSE(IsTextInputActive());
  ui::DummyTextInputClient dummy_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&dummy_text_input_client);
  EXPECT_TRUE(IsTextInputActive());
  ui::DummyTextInputClient dummy_text_none_input_client(
      ui::TEXT_INPUT_TYPE_NONE);
  input_method->SetFocusedTextInputClient(&dummy_text_none_input_client);
  EXPECT_FALSE(IsTextInputActive());
  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_P(VersionArcInputOverlayManagerTest, TestWindowFocusChange) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  auto arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kEnabledPackageName);
  auto arc_window_no_data = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kRandomPackageName);
  EXPECT_EQ(1, EnabledWindows());

  auto* injector = GetTouchInjector(arc_window->GetNativeWindow());
  EXPECT_TRUE(injector);
  // The action number should be adjusted with the data in the
  // org.chromium.arc.testapp.inputoverlay.json.
  EXPECT_EQ(3u, injector->actions().size());

  EXPECT_TRUE(!GetRegisteredWindow() && !GetDisplayOverlayController());
  focus_client->FocusWindow(arc_window->GetNativeWindow());
  EXPECT_EQ(arc_window->GetNativeWindow(), GetRegisteredWindow());
  EXPECT_TRUE(GetDisplayOverlayController());
  focus_client->FocusWindow(arc_window_no_data->GetNativeWindow());
  EXPECT_TRUE(!GetRegisteredWindow() && !GetDisplayOverlayController());
}

// This simulates the test for crash in b/344665489 to test focus change with
// a window without a widget.
TEST_P(VersionArcInputOverlayManagerTest, TestWindowFocusChangeWithNullWidget) {
  if (!IsBetaVersion()) {
    return;
  }

  auto arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kEnabledPackageName);
  std::unique_ptr<aura::test::TestWindowDelegate> test_window_delegate =
      std::make_unique<aura::test::TestWindowDelegate>();
  test_window_delegate->set_window_component(HTCAPTION);
  std::unique_ptr<aura::Window> window_no_widget(
      CreateTestWindowInShellWithDelegateAndType(
          test_window_delegate.get(), aura::client::WINDOW_TYPE_NORMAL, 0,
          gfx::Rect(100, 100)));
  EXPECT_FALSE(views::Widget::GetWidgetForNativeWindow(window_no_widget.get()));

  // Focus on the window without widget.
  aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
      ->FocusWindow(window_no_widget.get());
  // Close the focused window.
  window_no_widget.reset();
  // Focus is updated to `arc_window`.
  EXPECT_EQ(arc_window->GetNativeWindow(), GetRegisteredWindow());
}

TEST_P(VersionArcInputOverlayManagerTest, TestTabletMode) {
  // Launch app in tablet mode and switch to desktop mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  auto arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kEnabledPackageName);
  EXPECT_TRUE(IsInputOverlayEnabled(arc_window->GetNativeWindow()));
  EXPECT_FALSE(GetRegisteredWindow());
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_TRUE(GetRegisteredWindow());
  arc_window.reset();

  // Launch app in desktop mode and switch to tablet mode.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  arc_window = CreateArcWindowSyncAndWait(task_environment(),
                                          ash::Shell::GetPrimaryRootWindow(),
                                          window_bounds, kEnabledPackageName);
  EXPECT_TRUE(IsInputOverlayEnabled(arc_window->GetNativeWindow()));
  EXPECT_TRUE(GetRegisteredWindow());
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(GetRegisteredWindow());
}

TEST_P(VersionArcInputOverlayManagerTest,
       TestKeyEventSourceRewriterForMultiDisplay) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  UpdateDisplay("1000x900,1000x900");
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  display::Display display0 = display::Screen::GetScreen()->GetDisplayMatching(
      root_windows[0]->GetBoundsInScreen());
  display::Display display1 = display::Screen::GetScreen()->GetDisplayMatching(
      root_windows[1]->GetBoundsInScreen());

  // Test when launching input overlay window on the secondary display, there
  // should be `key_event_source_rewriter_` registered on the primary root
  // window.
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  task_environment()->RunUntilIdle();
  auto screen_bounds = window_bounds;
  const auto& display_bounds = display1.bounds();
  screen_bounds.Offset(display_bounds.x(), display_bounds.y());
  auto arc_window =
      CreateArcWindow(root_windows[1], screen_bounds, kEnabledPackageName);
  arc_window->GetNativeWindow()->SetBoundsInScreen(screen_bounds, display1);
  // I/O takes time here.
  task_environment()->FastForwardBy(kIORead);
  // Make sure to dismiss the educational dialog in beforehand.
  auto* injector = GetTouchInjector(arc_window->GetNativeWindow());
  EXPECT_TRUE(injector);
  focus_client->FocusWindow(arc_window->GetNativeWindow());
  EXPECT_TRUE(GetKeyEventSourceRewriter());
  // Simulate the fact that key events are only sent to primary root window
  // when there is no text input focus. Make sure the input overlay window can
  // receive simulated touch events on the secondary window.
  auto event_generator =
      std::make_unique<ui::test::EventGenerator>(root_windows[0]);
  test::EventCapturer event_capturer;
  root_windows[1]->AddPostTargetHandler(&event_capturer);
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE, /*source_device_id=*/1);
  EXPECT_TRUE(event_capturer.key_events().empty());
  EXPECT_EQ(1u, event_capturer.touch_events().size());
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1);
  EXPECT_TRUE(event_capturer.key_events().empty());
  EXPECT_EQ(2u, event_capturer.touch_events().size());
  event_capturer.Clear();
  root_windows[1]->RemovePostTargetHandler(&event_capturer);
  // Move to the primary display.
  arc_window->GetNativeWindow()->SetBoundsInScreen(window_bounds, display0);
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  // Move back to the secondary display.
  arc_window->GetNativeWindow()->SetBoundsInScreen(screen_bounds, display1);
  EXPECT_TRUE(GetKeyEventSourceRewriter());
  arc_window.reset();

  // Test when launching input overlay window on the primary display, there
  // shouldn't be `key_event_source_rewriter_`.
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  arc_window = CreateArcWindowSyncAndWait(task_environment(),
                                          ash::Shell::GetPrimaryRootWindow(),
                                          window_bounds, kEnabledPackageName);
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  // Move to the secondary display.
  arc_window->GetNativeWindow()->SetBoundsInScreen(window_bounds, display1);
  EXPECT_TRUE(GetKeyEventSourceRewriter());
  // When losing focus, `key_event_source_rewriter_` should be destroyed too.
  focus_client->FocusWindow(nullptr);
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  arc_window.reset();

  // Test when this is non input overlay window launched on the secondry
  // display, there shouldn't be `key_event_source_rewriter_`.
  auto arc_window_no_data = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kRandomPackageName);
  focus_client->FocusWindow(arc_window_no_data->GetNativeWindow());
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  arc_window_no_data.reset();

  // Test with no text input focused, when input overlay window on the secondary
  // root window is registered/focused, primary window shouldn't receive any key
  // events. When input overlay window on the secondary root window is not
  // registered/not focused, primary window should receive key events.
  root_windows[0]->AddPostTargetHandler(&event_capturer);
  arc_window = CreateArcWindowSyncAndWait(task_environment(), root_windows[1],
                                          screen_bounds, kEnabledPackageName);
  arc_window->GetNativeWindow()->SetBoundsInScreen(screen_bounds, display1);
  arc_window_no_data = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kRandomPackageName);
  // Focus on window without input overlay.
  focus_client->FocusWindow(arc_window_no_data->GetNativeWindow());
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  EXPECT_EQ(2u, event_capturer.key_events().size());
  event_capturer.Clear();
  // Focus input overlay window.
  focus_client->FocusWindow(arc_window->GetNativeWindow());
  EXPECT_TRUE(GetKeyEventSourceRewriter());
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  EXPECT_TRUE(event_capturer.key_events().empty());
  event_capturer.Clear();
  root_windows[0]->RemovePostTargetHandler(&event_capturer);
}

TEST_P(VersionArcInputOverlayManagerTest, TestWindowBoundsChanged) {
  auto* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  auto arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kEnabledPackageName);
  // Make sure to dismiss the educational dialog in beforehand.
  auto* injector = GetTouchInjector(arc_window->GetNativeWindow());
  DCHECK(injector);
  focus_client->FocusWindow(arc_window->GetNativeWindow());
  int caption_height = -arc_window->non_client_view()
                            ->frame_view()
                            ->GetWindowBoundsForClientBounds(gfx::Rect())
                            .y();
  auto expected_content_bounds = gfx::RectF(
      window_bounds.x(), window_bounds.y() + caption_height,
      window_bounds.width(), window_bounds.height() - caption_height);
  EXPECT_EQ(injector->content_bounds_f(), expected_content_bounds);
  EXPECT_POINTF_NEAR(
      injector->actions()[0]->touch_down_positions()[0],
      gfx::PointF(expected_content_bounds.x() +
                      expected_content_bounds.width() * kAction0PositionRatio,
                  expected_content_bounds.y() +
                      expected_content_bounds.height() * kAction0PositionRatio),
      kTolerance);
  EXPECT_POINTF_NEAR(
      injector->actions()[1]->touch_down_positions()[0],
      gfx::PointF(expected_content_bounds.x() +
                      expected_content_bounds.width() * kAction1PositionRatio,
                  expected_content_bounds.y() +
                      expected_content_bounds.height() * kAction1PositionRatio),
      kTolerance);

  // Confirm the content bounds and touch down positions are updated after
  // window bounds changed.
  auto display = display::Screen::GetScreen()->GetDisplayMatching(
      ash::Shell::GetPrimaryRootWindow()->GetBoundsInScreen());
  auto new_window_bounds = gfx::Rect(10, 10, 150, 150);
  arc_window->GetNativeWindow()->SetBoundsInScreen(new_window_bounds, display);
  expected_content_bounds = gfx::RectF(
      new_window_bounds.x(), new_window_bounds.y() + caption_height,
      new_window_bounds.width(), new_window_bounds.height() - caption_height);
  EXPECT_EQ(injector->content_bounds_f(), expected_content_bounds);
  EXPECT_POINTF_NEAR(
      injector->actions()[0]->touch_down_positions()[0],
      gfx::PointF(expected_content_bounds.x() +
                      expected_content_bounds.width() * kAction0PositionRatio,
                  expected_content_bounds.y() +
                      expected_content_bounds.height() * kAction0PositionRatio),
      kTolerance);
  EXPECT_POINTF_NEAR(
      injector->actions()[1]->touch_down_positions()[0],
      gfx::PointF(expected_content_bounds.x() +
                      expected_content_bounds.width() * kAction1PositionRatio,
                  expected_content_bounds.y() +
                      expected_content_bounds.height() * kAction1PositionRatio),
      kTolerance);
}

TEST_P(VersionArcInputOverlayManagerTest, TestDisplayRotationChanged) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  auto arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kEnabledPackageName);
  // Make sure to dismiss the educational dialog in beforehand.
  auto* injector = GetTouchInjector(arc_window->GetNativeWindow());
  DCHECK(injector);
  focus_client->FocusWindow(arc_window->GetNativeWindow());
  EXPECT_FALSE(injector->rotation_transform());
  int caption_height = -arc_window->non_client_view()
                            ->frame_view()
                            ->GetWindowBoundsForClientBounds(gfx::Rect())
                            .y();
  auto expected_content_bounds = gfx::RectF(
      window_bounds.x(), window_bounds.y() + caption_height,
      window_bounds.width(), window_bounds.height() - caption_height);
  EXPECT_EQ(injector->content_bounds_f(), expected_content_bounds);
  auto expect_touch_a =
      gfx::PointF(expected_content_bounds.x() +
                      expected_content_bounds.width() * kAction0PositionRatio,
                  expected_content_bounds.y() +
                      expected_content_bounds.height() * kAction0PositionRatio);
  EXPECT_POINTF_NEAR(injector->actions()[0]->touch_down_positions()[0],
                     expect_touch_a, kTolerance);
  auto expect_touch_b =
      gfx::PointF(expected_content_bounds.x() +
                      expected_content_bounds.width() * kAction1PositionRatio,
                  expected_content_bounds.y() +
                      expected_content_bounds.height() * kAction1PositionRatio);
  EXPECT_POINTF_NEAR(injector->actions()[1]->touch_down_positions()[0],
                     expect_touch_b, kTolerance);

  // Confirm the touch down positions are updated after display rotated.
  UpdateDisplay("800x600/r");
  EXPECT_TRUE(injector->rotation_transform());
  EXPECT_EQ(injector->content_bounds_f(), expected_content_bounds);
  expect_touch_a = injector->rotation_transform()->MapPoint(expect_touch_a);
  EXPECT_POINTF_NEAR(injector->actions()[0]->touch_down_positions()[0],
                     expect_touch_a, kTolerance);
  expect_touch_b = injector->rotation_transform()->MapPoint(expect_touch_b);
  EXPECT_POINTF_NEAR(injector->actions()[1]->touch_down_positions()[0],
                     expect_touch_b, kTolerance);
}

TEST_P(VersionArcInputOverlayManagerTest, TestNonGameApp) {
  // Test a random non-game app.
  auto window = CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                                window_bounds, kRandomPackageName);
  task_environment()->FastForwardBy(kIORead);
  auto* injector = GetTouchInjector(window->GetNativeWindow());
  EXPECT_FALSE(injector);
  window.reset();
}

TEST_P(VersionArcInputOverlayManagerTest, TestGameWithDefaultMapping) {
  // Test with a game with default mapping.
  auto window = CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                                window_bounds, kEnabledPackageName);
  task_environment()->FastForwardBy(kIORead);
  auto* injector = GetTouchInjector(window->GetNativeWindow());
  EXPECT_TRUE(injector);
  size_t actions_size = injector->actions().size();

  const auto center =
      gfx::Point(window_bounds.width() / 2, window_bounds.height() / 2);
  if (IsBetaVersion()) {
    // Add two new actions.
    injector->AddNewAction(ActionType::TAP, center);
    injector->AddNewAction(ActionType::TAP, center);
    EXPECT_EQ(actions_size + 2, injector->actions().size());
  }

  // Relaunch the game to check whether previous customized data is loaded.
  window.reset();
  window = CreateArcWindow(ash::Shell::GetPrimaryRootWindow(), window_bounds,
                           kEnabledPackageName);
  task_environment()->FastForwardBy(kIORead);
  injector = GetTouchInjector(window->GetNativeWindow());
  EXPECT_TRUE(injector);
  if (IsBetaVersion()) {
    EXPECT_EQ(actions_size + 2, injector->actions().size());
  } else {
    EXPECT_EQ(actions_size, injector->actions().size());
  }

  window.reset();
}

TEST_P(VersionArcInputOverlayManagerTest, TestGameWithoutDefaultMapping) {
  // Test with a random non-O4C game.
  auto game_window = CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                                     window_bounds, kRandomGamePackageName);
  task_environment()->FastForwardBy(kIORead);
  auto* injector = GetTouchInjector(game_window->GetNativeWindow());
  if (IsBetaVersion()) {
    EXPECT_TRUE(injector);
    EXPECT_EQ(0u, injector->actions().size());
    // Add two new actions.
    const auto center =
        gfx::Point(window_bounds.width() / 2, window_bounds.height() / 2);
    injector->AddNewAction(ActionType::TAP, center);
    injector->AddNewAction(ActionType::TAP, center);
    injector->OnBindingSave();
  } else {
    EXPECT_FALSE(injector);
  }

  // Relaunch the game to check whether previous customized_data is loaded.
  game_window.reset();
  task_environment()->RunUntilIdle();
  game_window = CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                                window_bounds, kRandomGamePackageName);
  task_environment()->FastForwardBy(kIORead);
  injector = GetTouchInjector(game_window->GetNativeWindow());
  if (IsBetaVersion()) {
    EXPECT_TRUE(injector);
    EXPECT_EQ(2u, injector->actions().size());
  } else {
    EXPECT_FALSE(injector);
  }

  game_window.reset();
}

TEST_P(VersionArcInputOverlayManagerTest, TestGameControlsOptOut) {
  SimulatedAppInstalled(task_environment(), arc_app_test_,
                        kGameControlsOptOutPackageName,
                        /*is_gc_opt_out=*/true,
                        /*is_game=*/true);
  auto game_window =
      CreateArcWindow(ash::Shell::GetPrimaryRootWindow(), window_bounds,
                      kGameControlsOptOutPackageName);
  EXPECT_FALSE(GetTouchInjector(game_window->GetNativeWindow()));
}

TEST_P(VersionArcInputOverlayManagerTest, TestO4CGame) {
  // Test an O4C game without any input mapping.
  arc_app_test_.compatibility_mode_instance()->set_o4c_pkg(
      kRandomGamePackageName);
  auto game_window = CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                                     window_bounds, kRandomGamePackageName);
  task_environment()->FastForwardBy(kIORead);
  auto* injector = GetTouchInjector(game_window->GetNativeWindow());
  if (IsBetaVersion()) {
    EXPECT_TRUE(injector);
  } else {
    EXPECT_FALSE(injector);
  }
  game_window.reset();
  task_environment()->RunUntilIdle();

  // Test an O4C game with input mappings.
  arc_app_test_.compatibility_mode_instance()->set_o4c_pkg(kEnabledPackageName);
  game_window = CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                                window_bounds, kEnabledPackageName);
  task_environment()->FastForwardBy(kIORead);
  injector = GetTouchInjector(game_window->GetNativeWindow());
  EXPECT_TRUE(injector);
  EXPECT_EQ(3u, injector->actions().size());
  game_window.reset();
}

TEST_P(VersionArcInputOverlayManagerTest, TestOverviewMode) {
  auto arc_window_widget = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kEnabledPackageName);
  auto* arc_window = arc_window_widget->GetNativeWindow();
  EXPECT_EQ(arc_window, GetRegisteredWindow());
  EnterOverview();
  EXPECT_EQ(nullptr, GetRegisteredWindow());
  ExitOverview();
  EXPECT_EQ(arc_window, GetRegisteredWindow());

  // Test beta in edit mode.
  if (IsBetaVersion()) {
    UpdateFlagAndProperty(arc_window, ash::ArcGameControlsFlag::kEdit,
                          /*turn_on=*/true);
    EnterOverview();
    EXPECT_EQ(nullptr, GetRegisteredWindow());
    ExitOverview();
    EXPECT_EQ(arc_window, GetRegisteredWindow());
  }
}

TEST_P(VersionArcInputOverlayManagerTest, TestFullscreen) {
  auto arc_window_widget = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kEnabledPackageName);
  auto* arc_window = arc_window_widget->GetNativeWindow();
  EXPECT_EQ(arc_window, GetRegisteredWindow());

  // Set it to fullscreen.
  arc_window_widget->SetFullscreen(true);
  EXPECT_TRUE(arc_window_widget->IsFullscreen());
  EXPECT_EQ(arc_window, GetRegisteredWindow());
  EXPECT_TRUE(arc_window_widget->IsFullscreen());

  // Focus on another random window.
  auto* primary_root_window = ash::Shell::GetPrimaryRootWindow();
  auto random_window = CreateArcWindowSyncAndWait(
      task_environment(), primary_root_window, gfx::Rect(310, 300, 300, 280),
      kRandomPackageName);
  auto* focus_client = aura::client::GetFocusClient(primary_root_window);
  focus_client->FocusWindow(random_window->GetNativeWindow());
  EXPECT_EQ(nullptr, GetRegisteredWindow());

  // Focus back on the game window.
  focus_client->FocusWindow(arc_window);
  EXPECT_EQ(arc_window, GetRegisteredWindow());

  // Test beta in edit mode.
  if (IsBetaVersion()) {
    UpdateFlagAndProperty(arc_window, ash::ArcGameControlsFlag::kEdit,
                          /*turn_on=*/true);
    focus_client->FocusWindow(random_window->GetNativeWindow());
    EXPECT_EQ(nullptr, GetRegisteredWindow());
  }
}

TEST_P(VersionArcInputOverlayManagerTest, TestFullscreenToFloating) {
  auto arc_window_widget = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kEnabledPackageName);
  auto* arc_window = arc_window_widget->GetNativeWindow();
  EXPECT_EQ(arc_window, GetRegisteredWindow());

  // Set it to fullscreen.
  arc_window_widget->SetFullscreen(true);
  EXPECT_TRUE(arc_window_widget->IsFullscreen());
  EXPECT_EQ(arc_window, GetRegisteredWindow());

  // Set it to floating.
  arc_window_widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  EXPECT_EQ(arc_window_widget->GetZOrderLevel(),
            ui::ZOrderLevel::kFloatingWindow);
  EXPECT_EQ(arc_window, GetRegisteredWindow());

  // Set it back to fullscreen.
  arc_window_widget->SetFullscreen(true);
  EXPECT_TRUE(arc_window_widget->IsFullscreen());
  EXPECT_EQ(arc_window, GetRegisteredWindow());
}

TEST_P(VersionArcInputOverlayManagerTest, TestHistograms) {
  if (!IsBetaVersion()) {
    return;
  }

  base::HistogramTester histograms;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  std::map<MappingSource, int> expected_histogram_values_for_hint_on;
  std::map<MappingSource, int> expected_histogram_values_for_hint_off;
  std::map<MappingSource, int> expected_histogram_values_for_feature_on;
  std::map<MappingSource, int> expected_histogram_values_for_feature_off;

  const std::string feature_on_histogram_name =
      BuildGameControlsHistogramName(
          base::JoinString(
              std::vector<std::string>{kFeatureHistogramName,
                                       kToggleWithMappingSourceHistogram},
              ""))
          .append(kGameControlsHistogramSeparator)
          .append(kToggleOnHistogramName);

  const std::string feature_off_histogram_name =
      BuildGameControlsHistogramName(
          base::JoinString(
              std::vector<std::string>{kFeatureHistogramName,
                                       kToggleWithMappingSourceHistogram},
              ""))
          .append(kGameControlsHistogramSeparator)
          .append(kToggleOffHistogramName);

  const std::string hint_on_histogram_name =
      BuildGameControlsHistogramName(
          base::JoinString(
              std::vector<std::string>{kHintHistogramName,
                                       kToggleWithMappingSourceHistogram},
              ""))
          .append(kGameControlsHistogramSeparator)
          .append(kToggleOnHistogramName);

  const std::string hint_off_histogram_name =
      BuildGameControlsHistogramName(
          base::JoinString(
              std::vector<std::string>{kHintHistogramName,
                                       kToggleWithMappingSourceHistogram},
              ""))
          .append(kGameControlsHistogramSeparator)
          .append(kToggleOffHistogramName);

  // 1. Test with the default mapping.
  auto arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kEnabledPackageName);
  // Toggle hint off.
  ToggleGameControls(arc_window->GetNativeWindow(), /*is_feature=*/false);
  MapIncreaseValueByOne(expected_histogram_values_for_hint_off,
                        MappingSource::kDefault);
  VerifyHistogramValues(histograms, hint_off_histogram_name,
                        expected_histogram_values_for_hint_off);

  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/1u, /*index=*/0u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/0},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefault)}});

  // Toggle hint on.
  ToggleGameControls(arc_window->GetNativeWindow(), /*is_feature=*/false);
  MapIncreaseValueByOne(expected_histogram_values_for_hint_on,
                        MappingSource::kDefault);
  VerifyHistogramValues(histograms, hint_on_histogram_name,
                        expected_histogram_values_for_hint_on);
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/2u, /*index=*/1u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/1},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefault)}});

  // Toggle feature off.
  ToggleGameControls(arc_window->GetNativeWindow(), /*is_feature=*/true);
  MapIncreaseValueByOne(expected_histogram_values_for_feature_off,
                        MappingSource::kDefault);
  // Hint is also toggle off with feature toggle off.
  MapIncreaseValueByOne(expected_histogram_values_for_hint_off,
                        MappingSource::kDefault);
  VerifyHistogramValues(histograms, feature_off_histogram_name,
                        expected_histogram_values_for_feature_off);
  VerifyHistogramValues(histograms, hint_off_histogram_name,
                        expected_histogram_values_for_hint_off);
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/4u, /*index=*/2u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kFeature)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/0},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefault)}});
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/4u, /*index=*/3u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/0},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefault)}});
  // Toggle feature on.
  ToggleGameControls(arc_window->GetNativeWindow(), /*is_feature=*/true);
  MapIncreaseValueByOne(expected_histogram_values_for_feature_on,
                        MappingSource::kDefault);
  // Hint is also toggle on with feature toggle on.
  MapIncreaseValueByOne(expected_histogram_values_for_hint_on,
                        MappingSource::kDefault);
  VerifyHistogramValues(histograms, feature_on_histogram_name,
                        expected_histogram_values_for_feature_on);
  VerifyHistogramValues(histograms, hint_on_histogram_name,
                        expected_histogram_values_for_hint_on);
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/6u, /*index=*/4u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kFeature)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/1},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefault)}});
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/6u, /*index=*/5u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/1},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefault)}});

  // 2. Add the default mapping with extra user-added mapping.
  auto* injector = GetTouchInjector(arc_window->GetNativeWindow());
  injector->AddNewAction(ActionType::TAP,
                         arc_window->GetNativeWindow()->bounds().CenterPoint());
  // Toggle hint off.
  ToggleGameControls(arc_window->GetNativeWindow(), /*is_feature=*/false);
  MapIncreaseValueByOne(expected_histogram_values_for_hint_off,
                        MappingSource::kDefaultAndUserAdded);
  VerifyHistogramValues(histograms, hint_off_histogram_name,
                        expected_histogram_values_for_hint_off);
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/7u, /*index=*/6u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/0},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefaultAndUserAdded)}});
  // Toggle hint on.
  ToggleGameControls(arc_window->GetNativeWindow(), /*is_feature=*/false);
  MapIncreaseValueByOne(expected_histogram_values_for_hint_on,
                        MappingSource::kDefaultAndUserAdded);
  VerifyHistogramValues(histograms, hint_on_histogram_name,
                        expected_histogram_values_for_hint_on);
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/8u, /*index=*/7u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/1},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefaultAndUserAdded)}});
  // Toggle feature off.
  ToggleGameControls(arc_window->GetNativeWindow(), /*is_feature=*/true);
  MapIncreaseValueByOne(expected_histogram_values_for_feature_off,
                        MappingSource::kDefaultAndUserAdded);
  // Hint is also toggle off with feature toggle off.
  MapIncreaseValueByOne(expected_histogram_values_for_hint_off,
                        MappingSource::kDefaultAndUserAdded);
  VerifyHistogramValues(histograms, feature_off_histogram_name,
                        expected_histogram_values_for_feature_off);
  VerifyHistogramValues(histograms, hint_off_histogram_name,
                        expected_histogram_values_for_hint_off);
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/10u, /*index=*/8u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kFeature)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/0},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefaultAndUserAdded)}});
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/10u, /*index=*/9u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/0},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefaultAndUserAdded)}});

  // Toggle feature on.
  ToggleGameControls(arc_window->GetNativeWindow(), /*is_feature=*/true);
  MapIncreaseValueByOne(expected_histogram_values_for_feature_on,
                        MappingSource::kDefaultAndUserAdded);
  // Hint is also toggle on with feature toggle on.
  MapIncreaseValueByOne(expected_histogram_values_for_hint_on,
                        MappingSource::kDefaultAndUserAdded);
  VerifyHistogramValues(histograms, feature_on_histogram_name,
                        expected_histogram_values_for_feature_on);
  VerifyHistogramValues(histograms, hint_on_histogram_name,
                        expected_histogram_values_for_hint_on);
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/12u, /*index=*/10u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kFeature)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/1},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefaultAndUserAdded)}});
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/12u, /*index=*/11u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/1},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kDefaultAndUserAdded)}});

  // 3. Test with user-added mapping only.
  auto game_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(), window_bounds,
      kRandomGamePackageName);
  injector = GetTouchInjector(game_window->GetNativeWindow());
  injector->AddNewAction(
      ActionType::TAP, game_window->GetNativeWindow()->bounds().CenterPoint());
  // Toggle hint off.
  ToggleGameControls(game_window->GetNativeWindow(), /*is_feature=*/false);
  MapIncreaseValueByOne(expected_histogram_values_for_hint_off,
                        MappingSource::kUserAdded);
  VerifyHistogramValues(histograms, hint_off_histogram_name,
                        expected_histogram_values_for_hint_off);
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/13u, /*index=*/12u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/0},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kUserAdded)}});
  // Toggle hint on.
  ToggleGameControls(game_window->GetNativeWindow(), /*is_feature=*/false);
  MapIncreaseValueByOne(expected_histogram_values_for_hint_on,
                        MappingSource::kUserAdded);
  VerifyHistogramValues(histograms, hint_on_histogram_name,
                        expected_histogram_values_for_hint_on);
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/14u, /*index=*/13u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/1},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kUserAdded)}});
  // Toggle feature off.
  ToggleGameControls(game_window->GetNativeWindow(), /*is_feature=*/true);
  MapIncreaseValueByOne(expected_histogram_values_for_feature_off,
                        MappingSource::kUserAdded);
  // Hint is also toggle off with feature toggle off.
  MapIncreaseValueByOne(expected_histogram_values_for_hint_off,
                        MappingSource::kUserAdded);
  VerifyHistogramValues(histograms, feature_off_histogram_name,
                        expected_histogram_values_for_feature_off);
  VerifyHistogramValues(histograms, hint_off_histogram_name,
                        expected_histogram_values_for_hint_off);
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/16u, /*index=*/14u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kFeature)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/0},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kUserAdded)}});
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/16u, /*index=*/15u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/0},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kUserAdded)}});
  // Toggle feature on.
  ToggleGameControls(game_window->GetNativeWindow(), /*is_feature=*/true);
  MapIncreaseValueByOne(expected_histogram_values_for_feature_on,
                        MappingSource::kUserAdded);
  // Hint is also toggle on with feature toggle on.
  MapIncreaseValueByOne(expected_histogram_values_for_hint_on,
                        MappingSource::kUserAdded);
  VerifyHistogramValues(histograms, feature_on_histogram_name,
                        expected_histogram_values_for_feature_on);
  VerifyHistogramValues(histograms, hint_on_histogram_name,
                        expected_histogram_values_for_hint_on);
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/18u, /*index=*/16u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kFeature)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/1},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kUserAdded)}});
  VerifyToggleWithMappingSourceUkmEvent(
      ukm_recorder, /*expected_entry_size=*/18u, /*index=*/17u,
      {{ukm::builders::GameControls_ToggleWithMappingSource::kFunctionName,
        static_cast<int64_t>(GameControlsToggleFunction::kMappingHint)},
       {ukm::builders::GameControls_ToggleWithMappingSource::kToggleOnName,
        /*toggle_on=*/1},
       {ukm::builders::GameControls_ToggleWithMappingSource::kMappingSourceName,
        static_cast<int64_t>(MappingSource::kUserAdded)}});
}

INSTANTIATE_TEST_SUITE_P(All,
                         VersionArcInputOverlayManagerTest,
                         ::testing::Bool());

}  // namespace arc::input_overlay
