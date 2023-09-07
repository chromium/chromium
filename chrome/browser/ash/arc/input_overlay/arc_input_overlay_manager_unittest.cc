// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_manager.h"

#include <memory>

#include "ash/components/arc/test/fake_compatibility_mode_instance.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/arc_test_window.h"
#include "chrome/browser/ash/arc/input_overlay/test/event_capturer.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
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
constexpr char kEnabledPackageName[] = "org.chromium.arc.testapp.inputoverlay";
constexpr char kRandomPackageName[] =
    "org.chromium.arc.testapp.inputoverlay_no_data";
constexpr char kRandomGamePackageName[] =
    "org.chromium.arc.testapp.inputoverlay_game";
constexpr const float kTolerance = 0.999f;

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

  std::unique_ptr<ArcInputOverlayManager> arc_test_input_overlay_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  // ash::AshTestBase:
  void TearDown() override {
    arc_test_input_overlay_manager_->Shutdown();
    arc_test_input_overlay_manager_.reset();
    ash::AshTestBase::TearDown();
  }
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
      scoped_feature_list_.InitWithFeatures(
          {ash::features::kGameDashboard, ash::features::kArcInputOverlayBeta},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {});
    }
  }

 protected:
  bool IsBetaVersion() const { return GetParam(); }
};

TEST_P(VersionArcInputOverlayManagerTest, TestPropertyChangeAndWindowDestroy) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  // Test app with input overlay data.
  auto arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kEnabledPackageName);
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
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kRandomPackageName);
  EXPECT_FALSE(IsObserving(arc_window_ptr));
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window_no_data->GetNativeWindow()));
}

TEST_P(VersionArcInputOverlayManagerTest, TestWindowDestroyNoWait) {
  // This test is to check UAF issue reported in crbug.com/1363030.
  task_environment()->RunUntilIdle();
  auto arc_window =
      CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                      gfx::Rect(10, 10, 100, 100), kEnabledPackageName);
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
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kEnabledPackageName);
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
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kEnabledPackageName);
  auto arc_window_no_data = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kRandomPackageName);
  EXPECT_EQ(1, EnabledWindows());

  auto* injector = GetTouchInjector(arc_window->GetNativeWindow());
  EXPECT_TRUE(injector);
  // The action number should be adjusted with the data in the
  // org.chromium.arc.testapp.inputoverlay.json.
  EXPECT_EQ(3, (int)injector->actions().size());

  EXPECT_TRUE(!GetRegisteredWindow() && !GetDisplayOverlayController());
  focus_client->FocusWindow(arc_window->GetNativeWindow());
  EXPECT_EQ(arc_window->GetNativeWindow(), GetRegisteredWindow());
  EXPECT_TRUE(GetDisplayOverlayController());
  focus_client->FocusWindow(arc_window_no_data->GetNativeWindow());
  EXPECT_TRUE(!GetRegisteredWindow() && !GetDisplayOverlayController());
}

TEST_P(VersionArcInputOverlayManagerTest, TestTabletMode) {
  // Launch app in tablet mode and switch to desktop mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  auto arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kEnabledPackageName);
  EXPECT_TRUE(IsInputOverlayEnabled(arc_window->GetNativeWindow()));
  EXPECT_FALSE(GetRegisteredWindow());
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_TRUE(GetRegisteredWindow());
  arc_window.reset();

  // Launch app in desktop mode and switch to tablet mode.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kEnabledPackageName);
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
  auto arc_window = CreateArcWindow(
      root_windows[1], gfx::Rect(1010, 910, 100, 100), kEnabledPackageName);
  arc_window->GetNativeWindow()->SetBoundsInScreen(
      gfx::Rect(1010, 910, 100, 100), display1);
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
  arc_window->GetNativeWindow()->SetBoundsInScreen(gfx::Rect(10, 10, 100, 100),
                                                   display0);
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  // Move back to the secondary display.
  arc_window->GetNativeWindow()->SetBoundsInScreen(
      gfx::Rect(1010, 910, 100, 100), display1);
  EXPECT_TRUE(GetKeyEventSourceRewriter());
  arc_window.reset();

  // Test when launching input overlay window on the primary display, there
  // shouldn't be `key_event_source_rewriter_`.
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kEnabledPackageName);
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  // Move to the secondary display.
  arc_window->GetNativeWindow()->SetBoundsInScreen(gfx::Rect(10, 10, 100, 100),
                                                   display1);
  EXPECT_TRUE(GetKeyEventSourceRewriter());
  // When losing focus, `key_event_source_rewriter_` should be destroyed too.
  focus_client->FocusWindow(nullptr);
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  arc_window.reset();

  // Test when this is non input overlay window launched on the secondry
  // display, there shouldn't be `key_event_source_rewriter_`.
  auto arc_window_no_data = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kRandomPackageName);
  focus_client->FocusWindow(arc_window_no_data->GetNativeWindow());
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  arc_window_no_data.reset();

  // Test with no text input focused, when input overlay window on the secondary
  // root window is registered/focused, primary window shouldn't receive any key
  // events. When input overlay window on the secondary root window is not
  // registered/not focused, primary window should receive key events.
  root_windows[0]->AddPostTargetHandler(&event_capturer);
  arc_window = CreateArcWindowSyncAndWait(task_environment(), root_windows[1],
                                          gfx::Rect(1010, 910, 100, 100),
                                          kEnabledPackageName);
  arc_window->GetNativeWindow()->SetBoundsInScreen(
      gfx::Rect(1010, 910, 100, 100), display1);
  arc_window_no_data = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kRandomPackageName);
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
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kEnabledPackageName);
  // Make sure to dismiss the educational dialog in beforehand.
  auto* injector = GetTouchInjector(arc_window->GetNativeWindow());
  DCHECK(injector);
  focus_client->FocusWindow(arc_window->GetNativeWindow());
  int caption_height = -arc_window->non_client_view()
                            ->frame_view()
                            ->GetWindowBoundsForClientBounds(gfx::Rect())
                            .y();
  EXPECT_EQ(injector->content_bounds_f(),
            gfx::RectF(10, 10 + caption_height, 100, 100 - caption_height));
  EXPECT_POINTF_NEAR(
      injector->actions()[0]->touch_down_positions()[0],
      gfx::PointF(60, (100 - caption_height) * 0.5 + 10 + caption_height),
      kTolerance);
  EXPECT_POINTF_NEAR(
      injector->actions()[1]->touch_down_positions()[0],
      gfx::PointF(100, (100 - caption_height) * 0.9 + 10 + caption_height),
      kTolerance);

  // Confirm the content bounds and touch down positions are updated after
  // window bounds changed.
  auto display = display::Screen::GetScreen()->GetDisplayMatching(
      ash::Shell::GetPrimaryRootWindow()->GetBoundsInScreen());
  arc_window->GetNativeWindow()->SetBoundsInScreen(gfx::Rect(10, 10, 150, 150),
                                                   display);
  EXPECT_EQ(injector->content_bounds_f(),
            gfx::RectF(10, 10 + caption_height, 150, 150 - caption_height));
  EXPECT_POINTF_NEAR(
      injector->actions()[0]->touch_down_positions()[0],
      gfx::PointF(85, (150 - caption_height) * 0.5 + 10 + caption_height),
      kTolerance);
  EXPECT_POINTF_NEAR(
      injector->actions()[1]->touch_down_positions()[0],
      gfx::PointF(145, (150 - caption_height) * 0.9 + 10 + caption_height),
      kTolerance);
}

TEST_P(VersionArcInputOverlayManagerTest, TestDisplayRotationChanged) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  auto arc_window = CreateArcWindowSyncAndWait(
      task_environment(), ash::Shell::GetPrimaryRootWindow(),
      gfx::Rect(10, 10, 100, 100), kEnabledPackageName);
  // Make sure to dismiss the educational dialog in beforehand.
  auto* injector = GetTouchInjector(arc_window->GetNativeWindow());
  DCHECK(injector);
  focus_client->FocusWindow(arc_window->GetNativeWindow());
  EXPECT_FALSE(injector->rotation_transform());
  int caption_height = -arc_window->non_client_view()
                            ->frame_view()
                            ->GetWindowBoundsForClientBounds(gfx::Rect())
                            .y();
  auto expect_bounds =
      gfx::RectF(10, 10 + caption_height, 100, 100 - caption_height);
  EXPECT_EQ(injector->content_bounds_f(), expect_bounds);
  auto expect_touch_a =
      gfx::PointF(60, (100 - caption_height) * 0.5 + 10 + caption_height);
  EXPECT_POINTF_NEAR(injector->actions()[0]->touch_down_positions()[0],
                     expect_touch_a, kTolerance);
  auto expect_touch_b =
      gfx::PointF(100, (100 - caption_height) * 0.9 + 10 + caption_height);
  EXPECT_POINTF_NEAR(injector->actions()[1]->touch_down_positions()[0],
                     expect_touch_b, kTolerance);

  // Confirm the touch down positions are updated after display rotated.
  UpdateDisplay("800x600/r");
  EXPECT_TRUE(injector->rotation_transform());
  EXPECT_EQ(injector->content_bounds_f(), expect_bounds);
  expect_touch_a = injector->rotation_transform()->MapPoint(expect_touch_a);
  EXPECT_POINTF_NEAR(injector->actions()[0]->touch_down_positions()[0],
                     expect_touch_a, kTolerance);
  expect_touch_b = injector->rotation_transform()->MapPoint(expect_touch_b);
  EXPECT_POINTF_NEAR(injector->actions()[1]->touch_down_positions()[0],
                     expect_touch_b, kTolerance);
}

TEST_P(VersionArcInputOverlayManagerTest, TestNonGameApp) {
  // Test a random non-game app.
  auto window =
      CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                      gfx::Rect(10, 10, 100, 100), kRandomPackageName);
  task_environment()->FastForwardBy(kIORead);
  auto* injector = GetTouchInjector(window->GetNativeWindow());
  EXPECT_FALSE(injector);
  window.reset();
}

TEST_P(VersionArcInputOverlayManagerTest, TestGameWithDefaultMapping) {
  // Test with a game with default mapping.
  auto window =
      CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                      gfx::Rect(10, 10, 100, 100), kEnabledPackageName);
  task_environment()->FastForwardBy(kIORead);
  auto* injector = GetTouchInjector(window->GetNativeWindow());
  EXPECT_TRUE(injector);
  size_t actions_size = injector->actions().size();

  if (IsBetaVersion()) {
    // Add two new actions.
    injector->AddNewAction(ActionType::TAP);
    injector->AddNewAction(ActionType::TAP);
    EXPECT_EQ(actions_size + 2, injector->actions().size());
  }

  // Relaunch the game to check whether previous customized data is loaded.
  window.reset();
  window = CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                           gfx::Rect(10, 10, 100, 100), kEnabledPackageName);
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
  std::unique_ptr<TestingProfile> profile = std::make_unique<TestingProfile>();
  ArcAppTest arc_app_test;
  arc_app_test.set_wait_compatibility_mode(true);
  arc_app_test.SetUp(profile.get());

  // Test with a random non-O4C game.
  arc_app_test.compatibility_mode_instance()->set_is_gio_applicable(
      IsBetaVersion());
  task_environment()->RunUntilIdle();
  auto game_window =
      CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                      gfx::Rect(10, 10, 100, 100), kRandomGamePackageName);
  task_environment()->FastForwardBy(kIORead);
  auto* injector = GetTouchInjector(game_window->GetNativeWindow());
  if (IsBetaVersion()) {
    EXPECT_TRUE(injector);
    EXPECT_EQ(0u, injector->actions().size());
    // Add two new actions.
    injector->AddNewAction(ActionType::TAP);
    injector->AddNewAction(ActionType::TAP);
    injector->OnBindingSave();
  } else {
    EXPECT_FALSE(injector);
  }

  // Relaunch the game to check whether previous customized_data is loaded.
  game_window.reset();
  task_environment()->RunUntilIdle();
  game_window =
      CreateArcWindow(ash::Shell::GetPrimaryRootWindow(),
                      gfx::Rect(10, 10, 100, 100), kRandomGamePackageName);
  task_environment()->FastForwardBy(kIORead);
  injector = GetTouchInjector(game_window->GetNativeWindow());
  if (IsBetaVersion()) {
    EXPECT_TRUE(injector);
    EXPECT_EQ(2u, injector->actions().size());
  } else {
    EXPECT_FALSE(injector);
  }

  game_window.reset();
  arc_app_test.TearDown();
}

INSTANTIATE_TEST_SUITE_P(All,
                         VersionArcInputOverlayManagerTest,
                         ::testing::Bool());

}  // namespace arc::input_overlay
