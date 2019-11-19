// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/desktop_task_switch_metric_recorder.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/window_factory.h"
#include "base/test/metrics/user_action_tester.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/public/activation_client.h"

using wm::ActivationChangeObserver;

namespace ash {
namespace {

const char kDesktopTaskSwitchUserAction[] = "Desktop_SwitchTask";

// Test fixture for the DesktopTaskSwitchMetricsRecorder class. NOTE: This
// fixture extends AshTestBase so that the UserMetricsRecorder instance required
// by the test target can be obtained through Shell::Get()->metrics() and the
// test target is not the same instance as the one owned by the
// UserMetricsRecorder instance.
class DesktopTaskSwitchMetricRecorderTest : public AshTestBase {
 public:
  DesktopTaskSwitchMetricRecorderTest();
  ~DesktopTaskSwitchMetricRecorderTest() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

  // Resets the recorded user action counts.
  void ResetActionCounts();

  // Returns the number of times the "Desktop_SwitchTask" user action was
  // recorded.
  int GetActionCount() const;

  // Creates a positionable window such that
  // window_util::IsWindowUserPositionable(...) would return true.
  std::unique_ptr<aura::Window> CreatePositionableWindow() const;

  // Creates a non-positionable window such that
  // window_util::IsWindowUserPositionable(...) would return false.
  std::unique_ptr<aura::Window> CreateNonPositionableWindow() const;

  // Wrapper to notify the test target's OnWindowActivated(...) method that
  // |window| was activated due to an INPUT_EVENT.
  void ActiveTaskWindowWithUserInput(aura::Window* window);

 protected:
  // Records UMA user action counts.
  std::unique_ptr<base::UserActionTester> user_action_tester_;

  // The test target.
  std::unique_ptr<DesktopTaskSwitchMetricRecorder> metrics_recorder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopTaskSwitchMetricRecorderTest);
};

DesktopTaskSwitchMetricRecorderTest::DesktopTaskSwitchMetricRecorderTest() =
    default;

DesktopTaskSwitchMetricRecorderTest::~DesktopTaskSwitchMetricRecorderTest() =
    default;

void DesktopTaskSwitchMetricRecorderTest::SetUp() {
  AshTestBase::SetUp();
  metrics_recorder_.reset(new DesktopTaskSwitchMetricRecorder);
  user_action_tester_.reset(new base::UserActionTester);
}

void DesktopTaskSwitchMetricRecorderTest::TearDown() {
  user_action_tester_.reset();
  metrics_recorder_.reset();
  AshTestBase::TearDown();
}

void DesktopTaskSwitchMetricRecorderTest::ActiveTaskWindowWithUserInput(
    aura::Window* window) {
  metrics_recorder_->OnWindowActivated(
      ActivationChangeObserver::ActivationReason::INPUT_EVENT, window, nullptr);
}

void DesktopTaskSwitchMetricRecorderTest::ResetActionCounts() {
  user_action_tester_->ResetCounts();
}

int DesktopTaskSwitchMetricRecorderTest::GetActionCount() const {
  return user_action_tester_->GetActionCount(kDesktopTaskSwitchUserAction);
}

std::unique_ptr<aura::Window>
DesktopTaskSwitchMetricRecorderTest::CreatePositionableWindow() const {
  std::unique_ptr<aura::Window> window = window_factory::NewWindow(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate());
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_NOT_DRAWN);
  return window;
}

std::unique_ptr<aura::Window>
DesktopTaskSwitchMetricRecorderTest::CreateNonPositionableWindow() const {
  std::unique_ptr<aura::Window> window = window_factory::NewWindow(
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate());
  window->SetType(aura::client::WINDOW_TYPE_UNKNOWN);
  window->Init(ui::LAYER_NOT_DRAWN);
  return window;
}

// Verify user action is recorded when a positionable window is activated given
// that a null window was activated last.
TEST_F(DesktopTaskSwitchMetricRecorderTest,
       ActivatePositionableWindowWhenNullWindowWasActivatedLast) {
  std::unique_ptr<aura::Window> null_window;
  std::unique_ptr<aura::Window> positionable_window =
      CreatePositionableWindow();

  ActiveTaskWindowWithUserInput(null_window.get());
  ResetActionCounts();

  ActiveTaskWindowWithUserInput(positionable_window.get());
  EXPECT_EQ(1, GetActionCount());
}

// Verify user action is recorded whena positionable window is activated given
// a different positionable window was activated last.
TEST_F(
    DesktopTaskSwitchMetricRecorderTest,
    ActivatePositionableWindowWhenADifferentPositionableWindowWasActivatedLast) {
  std::unique_ptr<aura::Window> positionable_window_1 =
      CreatePositionableWindow();
  std::unique_ptr<aura::Window> positionable_window_2 =
      CreatePositionableWindow();

  ActiveTaskWindowWithUserInput(positionable_window_1.get());
  ResetActionCounts();

  ActiveTaskWindowWithUserInput(positionable_window_2.get());
  EXPECT_EQ(1, GetActionCount());
}

// Verify user action is not recorded when a positionable window is activated
// given the same positionable window was activated last.
TEST_F(
    DesktopTaskSwitchMetricRecorderTest,
    ActivatePositionableWindowWhenTheSamePositionableWindowWasActivatedLast) {
  std::unique_ptr<aura::Window> positionable_window =
      CreatePositionableWindow();

  ActiveTaskWindowWithUserInput(positionable_window.get());
  ResetActionCounts();

  ActiveTaskWindowWithUserInput(positionable_window.get());
  EXPECT_EQ(0, GetActionCount());
}

// Verify user action is recorded when a positionable window is activated given
// a non-positionable window was activated last.
TEST_F(DesktopTaskSwitchMetricRecorderTest,
       ActivatePositionableWindowWhenANonPositionableWindowWasActivatedLast) {
  std::unique_ptr<aura::Window> non_positionable_window =
      CreateNonPositionableWindow();
  std::unique_ptr<aura::Window> positionable_window =
      CreatePositionableWindow();

  ActiveTaskWindowWithUserInput(non_positionable_window.get());
  ResetActionCounts();

  ActiveTaskWindowWithUserInput(positionable_window.get());
  EXPECT_EQ(1, GetActionCount());
}

// Verify user action is not recorded when a non-positionable window is
// activated between two activations of the same positionable window.
TEST_F(DesktopTaskSwitchMetricRecorderTest,
       ActivateNonPositionableWindowBetweenTwoPositionableWindowActivations) {
  std::unique_ptr<aura::Window> positionable_window =
      CreatePositionableWindow();
  std::unique_ptr<aura::Window> non_positionable_window =
      CreateNonPositionableWindow();

  ActiveTaskWindowWithUserInput(positionable_window.get());
  ResetActionCounts();

  ActiveTaskWindowWithUserInput(non_positionable_window.get());
  EXPECT_EQ(0, GetActionCount());

  ActiveTaskWindowWithUserInput(positionable_window.get());
  EXPECT_EQ(0, GetActionCount());
}

// Verify user action is not recorded when a null window is activated.
TEST_F(DesktopTaskSwitchMetricRecorderTest, ActivateNullWindow) {
  std::unique_ptr<aura::Window> positionable_window =
      CreatePositionableWindow();
  std::unique_ptr<aura::Window> null_window = nullptr;

  ActiveTaskWindowWithUserInput(positionable_window.get());
  ResetActionCounts();

  ActiveTaskWindowWithUserInput(null_window.get());
  EXPECT_EQ(0, GetActionCount());
}

// Verify user action is not recorded when a non-positionable window is
// activated.
TEST_F(DesktopTaskSwitchMetricRecorderTest, ActivateNonPositionableWindow) {
  std::unique_ptr<aura::Window> positionable_window =
      CreatePositionableWindow();
  std::unique_ptr<aura::Window> non_positionable_window =
      CreateNonPositionableWindow();

  ActiveTaskWindowWithUserInput(positionable_window.get());
  ResetActionCounts();

  ActiveTaskWindowWithUserInput(non_positionable_window.get());
  EXPECT_EQ(0, GetActionCount());
}

// Verify user action is not recorded when the ActivationReason is not an
// INPUT_EVENT.
TEST_F(DesktopTaskSwitchMetricRecorderTest,
       ActivatePositionableWindowWithNonInputEventReason) {
  std::unique_ptr<aura::Window> positionable_window_1 =
      CreatePositionableWindow();
  std::unique_ptr<aura::Window> positionable_window_2 =
      CreatePositionableWindow();

  ActiveTaskWindowWithUserInput(positionable_window_1.get());
  ResetActionCounts();

  metrics_recorder_->OnWindowActivated(
      ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      positionable_window_2.get(), nullptr);
  EXPECT_EQ(0, GetActionCount());
}

// Test fixture to test the integration of the DesktopTaskSwitchMetricsRecorder
// class with ash::Shell environment.
class DesktopTaskSwitchMetricRecorderWithShellIntegrationTest
    : public AshTestBase {
 public:
  DesktopTaskSwitchMetricRecorderWithShellIntegrationTest();
  ~DesktopTaskSwitchMetricRecorderWithShellIntegrationTest() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

  // Returns the number of times the "Desktop_SwitchTask" user action was
  // recorded.
  int GetActionCount() const;

  // Creates a positionable window with the given |bounds| such that
  // window_util::IsWindowUserPositionable(...) would return true.
  aura::Window* CreatePositionableWindowInShellWithBounds(
      const gfx::Rect& bounds);

 protected:
  // Records UMA user action counts.
  std::unique_ptr<base::UserActionTester> user_action_tester_;

  // Delegate used when creating new windows using the
  // CreatePositionableWindowInShellWithBounds(...) method.
  aura::test::TestWindowDelegate test_window_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DesktopTaskSwitchMetricRecorderWithShellIntegrationTest);
};

DesktopTaskSwitchMetricRecorderWithShellIntegrationTest::
    DesktopTaskSwitchMetricRecorderWithShellIntegrationTest() = default;

DesktopTaskSwitchMetricRecorderWithShellIntegrationTest::
    ~DesktopTaskSwitchMetricRecorderWithShellIntegrationTest() = default;

void DesktopTaskSwitchMetricRecorderWithShellIntegrationTest::SetUp() {
  AshTestBase::SetUp();
  user_action_tester_.reset(new base::UserActionTester);
}

void DesktopTaskSwitchMetricRecorderWithShellIntegrationTest::TearDown() {
  user_action_tester_.reset();
  AshTestBase::TearDown();
}

int DesktopTaskSwitchMetricRecorderWithShellIntegrationTest::GetActionCount()
    const {
  return user_action_tester_->GetActionCount(kDesktopTaskSwitchUserAction);
}

aura::Window* DesktopTaskSwitchMetricRecorderWithShellIntegrationTest::
    CreatePositionableWindowInShellWithBounds(const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegate(&test_window_delegate_, 0, bounds);
}

// Verify a user action is recorded when a positionable window is activated by
// a INPUT_EVENT.
TEST_F(DesktopTaskSwitchMetricRecorderWithShellIntegrationTest,
       ActivatePositionableWindowWithInputEvent) {
  aura::Window* positionable_window =
      CreatePositionableWindowInShellWithBounds(gfx::Rect(0, 0, 10, 10));

  ui::test::EventGenerator event_generator(Shell::GetPrimaryRootWindow());

  event_generator.MoveMouseToCenterOf(positionable_window);
  event_generator.ClickLeftButton();

  EXPECT_EQ(1, GetActionCount());
}

// Verify a user action is not recorded when a positionable window is activated
// by a non INPUT_EVENT.
TEST_F(DesktopTaskSwitchMetricRecorderWithShellIntegrationTest,
       ActivatePositionableWindowWithNonInputEvent) {
  aura::Window* positionable_window =
      CreatePositionableWindowInShellWithBounds(gfx::Rect(0, 0, 10, 10));

  Shell::Get()->activation_client()->ActivateWindow(positionable_window);

  EXPECT_EQ(0, GetActionCount());
}

}  // namespace
}  // namespace ash
