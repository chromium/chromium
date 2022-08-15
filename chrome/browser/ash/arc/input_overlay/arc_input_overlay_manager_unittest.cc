// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_manager.h"

#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_util.h"
#include "chrome/browser/ash/arc/input_overlay/test/arc_test_window.h"
#include "chrome/browser/ash/arc/input_overlay/test/event_capturer.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/views/widget/widget.h"

namespace arc {
namespace {
// I/O time to wait.
constexpr base::TimeDelta kIORead = base::Milliseconds(50);

// Package names for testing.
constexpr char kEnabledPackageName[] = "org.chromium.arc.testapp.inputoverlay";
constexpr char kRandomPackageName[] =
    "org.chromium.arc.testapp.inputoverlay_no_data";

}  // namespace
class ArcInputOverlayManagerTest : public exo::test::ExoTestBase {
 public:
  ArcInputOverlayManagerTest()
      : exo::test::ExoTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

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

  input_overlay::TouchInjector* GetTouchInjector(aura::Window* window) {
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

  input_overlay::DisplayOverlayController* GetDisplayOverlayController() {
    return arc_test_input_overlay_manager_->display_overlay_controller_.get();
  }

  // TODO(djacobo): Maybe move all tests inside input_overlay namespace.
  void DismissEducationalDialog(input_overlay::TouchInjector* injector) {
    injector->GetControllerForTesting()->DismissEducationalViewForTesting();
  }

 protected:
  std::unique_ptr<ArcInputOverlayManager> arc_test_input_overlay_manager_;

 private:
  aura::test::TestWindowDelegate dummy_delegate_;

  void SetUp() override {
    exo::test::ExoTestBase::SetUp();
    arc_test_input_overlay_manager_ =
        base::WrapUnique(new ArcInputOverlayManager(nullptr, nullptr));
  }

  void TearDown() override {
    arc_test_input_overlay_manager_->Shutdown();
    arc_test_input_overlay_manager_.reset();
    exo::test::ExoTestBase::TearDown();
  }
};

TEST_F(ArcInputOverlayManagerTest, TestPropertyChangeAndWindowDestroy) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  // Test app with input overlay data.
  auto arc_window = std::make_unique<input_overlay::test::ArcTestWindow>(
      exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
      kEnabledPackageName);
  // I/O takes time here.
  task_environment()->FastForwardBy(kIORead);
  EXPECT_TRUE(IsInputOverlayEnabled(arc_window->GetWindow()));
  // Input overlay registers the window after reading the data when the window
  // is still focused. In the test, the arc_window is considered as focused now.
  EXPECT_TRUE(GetRegisteredWindow());
  focus_client->FocusWindow(arc_window->GetWindow());
  EXPECT_TRUE(GetRegisteredWindow());

  // Test app with input overlay data when window is destroyed.
  auto* arc_window_ptr = arc_window->GetWindow();
  arc_window.reset();
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window_ptr));

  // Test app without input overlay data.
  auto arc_window_no_data =
      std::make_unique<input_overlay::test::ArcTestWindow>(
          exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
          kRandomPackageName);
  EXPECT_FALSE(IsInputOverlayEnabled(arc_window_no_data->GetWindow()));
}

TEST_F(ArcInputOverlayManagerTest, TestInputMethodObsever) {
  ASSERT_FALSE(GetInputMethod());
  ASSERT_FALSE(IsTextInputActive());
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  auto arc_window = std::make_unique<input_overlay::test::ArcTestWindow>(
      exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
      kEnabledPackageName);
  // I/O takes time here.
  task_environment()->FastForwardBy(kIORead);
  focus_client->FocusWindow(arc_window->GetWindow());
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

TEST_F(ArcInputOverlayManagerTest, TestWindowFocusChange) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  auto arc_window = std::make_unique<input_overlay::test::ArcTestWindow>(
      exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
      kEnabledPackageName);
  // Add a deley until I/O operations finish.
  task_environment()->FastForwardBy(kIORead);
  auto arc_window_no_data =
      std::make_unique<input_overlay::test::ArcTestWindow>(
          exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
          kRandomPackageName);
  EXPECT_EQ(1, EnabledWindows());

  auto* injector = GetTouchInjector(arc_window->GetWindow());
  EXPECT_TRUE(injector);
  // The action number should be adjusted with the data in the
  // org.chromium.arc.testapp.inputoverlay.json.
  EXPECT_EQ(3, (int)injector->actions().size());

  EXPECT_TRUE(!GetRegisteredWindow() && !GetDisplayOverlayController());
  focus_client->FocusWindow(arc_window->GetWindow());
  EXPECT_EQ(arc_window->GetWindow(), GetRegisteredWindow());
  EXPECT_TRUE(GetDisplayOverlayController());
  focus_client->FocusWindow(arc_window_no_data->GetWindow());
  EXPECT_TRUE(!GetRegisteredWindow() && !GetDisplayOverlayController());
}

TEST_F(ArcInputOverlayManagerTest, TestTabletMode) {
  // Launch app in tablet mode and switch to desktop mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  auto arc_window = std::make_unique<input_overlay::test::ArcTestWindow>(
      exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
      kEnabledPackageName);
  // I/O takes time here.
  task_environment()->FastForwardBy(kIORead);
  EXPECT_TRUE(IsInputOverlayEnabled(arc_window->GetWindow()));
  EXPECT_FALSE(GetRegisteredWindow());
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_TRUE(GetRegisteredWindow());
  arc_window.reset();

  // Launch app in desktop mode and switch to tablet mode.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  arc_window = std::make_unique<input_overlay::test::ArcTestWindow>(
      exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
      kEnabledPackageName);
  // I/O takes time here.
  task_environment()->FastForwardBy(kIORead);
  EXPECT_TRUE(IsInputOverlayEnabled(arc_window->GetWindow()));
  EXPECT_TRUE(GetRegisteredWindow());
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(GetRegisteredWindow());
}

TEST_F(ArcInputOverlayManagerTest, TestKeyEventSourceRewriterForMultiDisplay) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  UpdateDisplay("1000x900,1000x900");
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  display::Display display0 = display::Screen::GetScreen()->GetDisplayMatching(
      root_windows[0]->GetBoundsInScreen());
  display::Display display1 = display::Screen::GetScreen()->GetDisplayMatching(
      root_windows[1]->GetBoundsInScreen());

  // Test when launching input overlay window on the secondary display, there
  // should be |key_event_source_rewriter_| registered on the primary root
  // window.
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  auto arc_window = std::make_unique<input_overlay::test::ArcTestWindow>(
      exo_test_helper(), root_windows[1], kEnabledPackageName);
  // I/O takes time here.
  task_environment()->FastForwardBy(kIORead);
  // Make sure to dismiss the educational dialog in beforehand.
  auto* injector = GetTouchInjector(arc_window->GetWindow());
  EXPECT_TRUE(injector);
  focus_client->FocusWindow(arc_window->GetWindow());
  DismissEducationalDialog(injector);
  EXPECT_TRUE(GetKeyEventSourceRewriter());
  // Simulate the fact that key events are only sent to primary root window
  // when there is no text input focus. Make sure the input overlay window can
  // receive simulated touch events on the secondary window.
  auto event_generator =
      std::make_unique<ui::test::EventGenerator>(root_windows[0]);
  input_overlay::test::EventCapturer event_capturer;
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
  arc_window->SetBounds(display0, gfx::Rect(10, 10, 100, 100));
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  // Move back to the secondary display.
  arc_window->SetBounds(display1, gfx::Rect(1010, 910, 100, 100));
  EXPECT_TRUE(GetKeyEventSourceRewriter());
  arc_window.reset();

  // Test when launching input overlay window on the primary display, there
  // shouldn't be |key_event_source_rewriter_|.
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  arc_window = std::make_unique<input_overlay::test::ArcTestWindow>(
      exo_test_helper(), root_windows[0], kEnabledPackageName);
  // Add a deley until I/O operations finish.
  task_environment()->FastForwardBy(kIORead);
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  // Move to the secondary display.
  arc_window->SetBounds(display1, gfx::Rect(10, 10, 100, 100));
  EXPECT_TRUE(GetKeyEventSourceRewriter());
  // When losing focus, |key_event_source_rewriter_| should be destroyed too.
  focus_client->FocusWindow(nullptr);
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  arc_window.reset();

  // Test when this is non input overlay window launched on the secondry
  // display, there shouldn't be |key_event_source_rewriter_|.
  auto arc_window_no_data =
      std::make_unique<input_overlay::test::ArcTestWindow>(
          exo_test_helper(), root_windows[1], kRandomPackageName);
  focus_client->FocusWindow(arc_window_no_data->GetWindow());
  EXPECT_FALSE(GetKeyEventSourceRewriter());
  arc_window_no_data.reset();

  // Test with no text input focused, when input overlay window on the secondary
  // root window is registered/focused, primary window shouldn't receive any key
  // events. When input overlay window on the secondary root window is not
  // registered/not focused, primary window should receive key events.
  root_windows[0]->AddPostTargetHandler(&event_capturer);
  arc_window = std::make_unique<input_overlay::test::ArcTestWindow>(
      exo_test_helper(), root_windows[1], kEnabledPackageName);
  // I/O takes time here.
  task_environment()->FastForwardBy(kIORead);
  arc_window_no_data = std::make_unique<input_overlay::test::ArcTestWindow>(
      exo_test_helper(), root_windows[0], kRandomPackageName);
  // Focus on window without input overlay.
  focus_client->FocusWindow(arc_window_no_data->GetWindow());
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  EXPECT_EQ(2u, event_capturer.key_events().size());
  event_capturer.Clear();
  // Focus input overlay window.
  focus_client->FocusWindow(arc_window->GetWindow());
  EXPECT_TRUE(GetKeyEventSourceRewriter());
  event_generator->PressKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  event_generator->ReleaseKey(ui::VKEY_A, ui::EF_NONE, 1 /* keyboard id */);
  EXPECT_TRUE(event_capturer.key_events().empty());
  event_capturer.Clear();
  root_windows[0]->RemovePostTargetHandler(&event_capturer);
}

TEST_F(ArcInputOverlayManagerTest, TestWindowBoundsChanged) {
  auto* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  auto arc_window = std::make_unique<input_overlay::test::ArcTestWindow>(
      exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
      kEnabledPackageName);
  // I/O takes time here.
  task_environment()->FastForwardBy(kIORead);
  // Make sure to dismiss the educational dialog in beforehand.
  auto* injector = GetTouchInjector(arc_window->GetWindow());
  DCHECK(injector);
  focus_client->FocusWindow(arc_window->GetWindow());
  DismissEducationalDialog(injector);
  EXPECT_EQ(injector->content_bounds(), gfx::RectF(10, 10, 100, 100));
  EXPECT_EQ(injector->actions()[0]->touch_down_positions()[0],
            gfx::PointF(60, 60));
  EXPECT_EQ(injector->actions()[1]->touch_down_positions()[0],
            gfx::PointF(100, 100));

  // Confirm the content bounds and touch down positions are updated after
  // window bounds changed.
  auto display = display::Screen::GetScreen()->GetDisplayMatching(
      ash::Shell::GetPrimaryRootWindow()->GetBoundsInScreen());
  arc_window->SetBounds(display, gfx::Rect(10, 10, 150, 150));
  EXPECT_EQ(injector->content_bounds(), gfx::RectF(10, 10, 150, 150));
  EXPECT_EQ(injector->actions()[0]->touch_down_positions()[0],
            gfx::PointF(85, 85));
  EXPECT_EQ(injector->actions()[1]->touch_down_positions()[0],
            gfx::PointF(145, 145));
}

TEST_F(ArcInputOverlayManagerTest, TestDisplayRotationChanged) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  auto arc_window = std::make_unique<input_overlay::test::ArcTestWindow>(
      exo_test_helper(), ash::Shell::GetPrimaryRootWindow(),
      kEnabledPackageName);
  // I/O takes time here.
  task_environment()->FastForwardBy(kIORead);
  // Make sure to dismiss the educational dialog in beforehand.
  auto* injector = GetTouchInjector(arc_window->GetWindow());
  DCHECK(injector);
  focus_client->FocusWindow(arc_window->GetWindow());
  DismissEducationalDialog(injector);
  EXPECT_FALSE(injector->rotation_transform());
  EXPECT_EQ(injector->content_bounds(), gfx::RectF(10, 10, 100, 100));
  EXPECT_EQ(injector->actions()[0]->touch_down_positions()[0],
            gfx::PointF(60, 60));
  EXPECT_EQ(injector->actions()[1]->touch_down_positions()[0],
            gfx::PointF(100, 100));

  // Confirm the touch down positions are updated after display rotated.
  UpdateDisplay("800x600/r");
  EXPECT_TRUE(injector->rotation_transform());
  EXPECT_EQ(injector->content_bounds(), gfx::RectF(10, 10, 100, 100));
  auto expect_pos = gfx::PointF(60, 60);
  injector->rotation_transform()->TransformPoint(&expect_pos);
  EXPECT_EQ(injector->actions()[0]->touch_down_positions()[0], expect_pos);
  expect_pos = gfx::PointF(100, 100);
  injector->rotation_transform()->TransformPoint(&expect_pos);
  EXPECT_EQ(injector->actions()[1]->touch_down_positions()[0], expect_pos);
}

}  // namespace arc
