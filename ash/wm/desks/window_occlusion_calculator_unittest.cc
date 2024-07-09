// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/window_occlusion_calculator.h"

#include "ash/public/cpp/window_properties.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::Mock;

class MockObserver : public WindowOcclusionCalculator::Observer {
 public:
  MockObserver() = default;
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  // WindowOcclusionCalculator::Observer:
  MOCK_METHOD(void,
              OnWindowOcclusionChanged,
              (aura::Window * window),
              (override));
};

class ScopedMockObserver : public MockObserver {
 public:
  ScopedMockObserver(WindowOcclusionCalculator* occlusion_calculator,
                     const aura::Window::Windows& parent_windows_to_track)
      : occlusion_calculator_(occlusion_calculator) {
    CHECK(occlusion_calculator_);
    occlusion_calculator_->AddObserver(parent_windows_to_track, this);
  }
  ScopedMockObserver(const ScopedMockObserver&) = delete;
  ScopedMockObserver& operator=(const ScopedMockObserver&) = delete;
  ~ScopedMockObserver() override {
    occlusion_calculator_->RemoveObserver(this);
  }

 private:
  const raw_ptr<WindowOcclusionCalculator> occlusion_calculator_;
};

class WindowOcclusionCalculatorTest : public aura::test::AuraTestBase {
 protected:
  // Reflects typical construction/destruction order in reality since the
  // `WindowOcclusionCalculator` is transient.
  void SetUp() override {
    aura::test::AuraTestBase::SetUp();
    occlusion_calculator_ = std::make_unique<WindowOcclusionCalculator>();
  }

  void TearDown() override {
    occlusion_calculator_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  aura::Window* CreateWindow(const gfx::Rect& bounds, aura::Window* parent) {
    ++id_assigner_;
    return CreateNormalWindow(id_assigner_, parent, /*delegate=*/nullptr);
  }

  std::unique_ptr<WindowOcclusionCalculator> occlusion_calculator_;
  int id_assigner_ = 1000;
};

// Note these tests are not intended to test every possible window occlusion
// case. That is the job of the `aura::WindowOcclusionTracker`. Beyond a few
// basic cases, these tests mainly verify that the "plumbing" of the occlusion
// state is done correctly.
TEST_F(WindowOcclusionCalculatorTest, BasicOcclusionStateIsCorrect) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* occluded_window =
      CreateWindow(gfx::Rect(20, 20), parent_window);
  aura::Window* visible_window =
      CreateWindow(gfx::Rect(50, 100), parent_window);
  aura::Window* hidden_window = CreateWindow(gfx::Rect(50, 100), parent_window);

  parent_window->StackChildAtTop(hidden_window);
  parent_window->StackChildAbove(visible_window, occluded_window);

  hidden_window->Hide();

  ScopedMockObserver observer(occlusion_calculator_.get(), {parent_window});
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(parent_window),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(occluded_window),
            aura::Window::OcclusionState::OCCLUDED);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(visible_window),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(hidden_window),
            aura::Window::OcclusionState::HIDDEN);
}

TEST_F(WindowOcclusionCalculatorTest,
       UnknownOcclusionStateForUntrackedWindows) {
  aura::Window* parent_window_1 =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* parent_window_2 =
      CreateWindow(parent_window_1->bounds(), root_window());
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(parent_window_1),
            aura::Window::OcclusionState::UNKNOWN);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(parent_window_2),
            aura::Window::OcclusionState::UNKNOWN);

  ScopedMockObserver observer(occlusion_calculator_.get(), {parent_window_1});
  EXPECT_NE(occlusion_calculator_->GetOcclusionState(parent_window_1),
            aura::Window::OcclusionState::UNKNOWN);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(parent_window_2),
            aura::Window::OcclusionState::UNKNOWN);
}

TEST_F(WindowOcclusionCalculatorTest,
       UnknownOcclusionStateForDestroyedWindows) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());

  ScopedMockObserver observer(occlusion_calculator_.get(), {parent_window});
  ASSERT_NE(occlusion_calculator_->GetOcclusionState(parent_window),
            aura::Window::OcclusionState::UNKNOWN);
  delete parent_window;
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(parent_window),
            aura::Window::OcclusionState::UNKNOWN);
}

TEST_F(WindowOcclusionCalculatorTest, ForcesObservedParentWindowsVisible) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* child_window_visible =
      CreateWindow(parent_window->bounds(), parent_window);
  aura::Window* child_window_occluded =
      CreateWindow(parent_window->bounds(), parent_window);
  parent_window->StackChildAtTop(child_window_visible);
  parent_window->Hide();

  ScopedMockObserver observer(occlusion_calculator_.get(), {parent_window});
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(parent_window),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_visible),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_occluded),
            aura::Window::OcclusionState::OCCLUDED);
}

TEST_F(WindowOcclusionCalculatorTest, NotifiesObserverOfOcclusionChange) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* child_window_1 =
      CreateWindow(parent_window->bounds(), parent_window);
  aura::Window* child_window_2 =
      CreateWindow(parent_window->bounds(), parent_window);
  parent_window->StackChildAtTop(child_window_1);

  ScopedMockObserver observer(occlusion_calculator_.get(), {parent_window});
  ASSERT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1),
            aura::Window::OcclusionState::VISIBLE);
  ASSERT_EQ(occlusion_calculator_->GetOcclusionState(child_window_2),
            aura::Window::OcclusionState::OCCLUDED);

  EXPECT_CALL(observer, OnWindowOcclusionChanged(child_window_1));
  EXPECT_CALL(observer, OnWindowOcclusionChanged(child_window_2));
  parent_window->StackChildAtTop(child_window_2);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1),
            aura::Window::OcclusionState::OCCLUDED);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_2),
            aura::Window::OcclusionState::VISIBLE);
}

TEST_F(WindowOcclusionCalculatorTest,
       DoesNotNotifyObserverOfUnrelatedOcclusionChanges) {
  aura::Window* parent_window_1 =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* parent_window_2 =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* child_window_1 =
      CreateWindow(parent_window_1->bounds(), parent_window_1);
  aura::Window* child_window_2 =
      CreateWindow(parent_window_2->bounds(), parent_window_1);

  ScopedMockObserver observer(occlusion_calculator_.get(), {parent_window_2});
  EXPECT_CALL(observer, OnWindowOcclusionChanged(_)).Times(0);
  parent_window_1->StackChildAtTop(child_window_1);
  parent_window_1->StackChildAtTop(child_window_2);
  Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(WindowOcclusionCalculatorTest, MultipleObserversDifferentWindows) {
  aura::Window* parent_window_1 =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* parent_window_2 =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* child_window_1_a =
      CreateWindow(parent_window_1->bounds(), parent_window_1);
  aura::Window* child_window_1_b =
      CreateWindow(parent_window_1->bounds(), parent_window_1);
  parent_window_1->StackChildAtTop(child_window_1_a);

  aura::Window* child_window_2_a =
      CreateWindow(parent_window_2->bounds(), parent_window_2);
  aura::Window* child_window_2_b =
      CreateWindow(parent_window_2->bounds(), parent_window_2);
  parent_window_2->StackChildAtTop(child_window_2_a);

  ScopedMockObserver observer_1(occlusion_calculator_.get(), {parent_window_1});
  ScopedMockObserver observer_2(occlusion_calculator_.get(), {parent_window_2});
  EXPECT_CALL(observer_1, OnWindowOcclusionChanged(child_window_1_a));
  EXPECT_CALL(observer_1, OnWindowOcclusionChanged(child_window_1_b));
  EXPECT_CALL(observer_2, OnWindowOcclusionChanged(_)).Times(0);
  parent_window_1->StackChildAtTop(child_window_1_b);
  Mock::VerifyAndClearExpectations(&observer_1);
  Mock::VerifyAndClearExpectations(&observer_2);

  EXPECT_CALL(observer_2, OnWindowOcclusionChanged(child_window_2_a));
  EXPECT_CALL(observer_2, OnWindowOcclusionChanged(child_window_2_b));
  EXPECT_CALL(observer_1, OnWindowOcclusionChanged(_)).Times(0);
  parent_window_2->StackChildAtTop(child_window_2_b);
  Mock::VerifyAndClearExpectations(&observer_1);
  Mock::VerifyAndClearExpectations(&observer_2);

  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1_a),
            aura::Window::OcclusionState::OCCLUDED);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1_b),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_2_a),
            aura::Window::OcclusionState::OCCLUDED);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_2_b),
            aura::Window::OcclusionState::VISIBLE);
}

TEST_F(WindowOcclusionCalculatorTest, MultipleObserversSameWindow) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* child_window_1 =
      CreateWindow(parent_window->bounds(), parent_window);
  aura::Window* child_window_2 =
      CreateWindow(parent_window->bounds(), parent_window);
  parent_window->StackChildAtTop(child_window_1);

  ScopedMockObserver observer_1(occlusion_calculator_.get(), {parent_window});
  ScopedMockObserver observer_2(occlusion_calculator_.get(), {parent_window});

  EXPECT_CALL(observer_1, OnWindowOcclusionChanged(child_window_1));
  EXPECT_CALL(observer_1, OnWindowOcclusionChanged(child_window_2));
  EXPECT_CALL(observer_2, OnWindowOcclusionChanged(child_window_1));
  EXPECT_CALL(observer_2, OnWindowOcclusionChanged(child_window_2));
  parent_window->StackChildAtTop(child_window_2);
  Mock::VerifyAndClearExpectations(&observer_1);
  Mock::VerifyAndClearExpectations(&observer_2);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1),
            aura::Window::OcclusionState::OCCLUDED);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_2),
            aura::Window::OcclusionState::VISIBLE);
}

TEST_F(WindowOcclusionCalculatorTest, MultipleObserversDescendantWindow) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* child_window =
      CreateWindow(parent_window->bounds(), parent_window);
  aura::Window* grandchild_window_1 =
      CreateWindow(parent_window->bounds(), child_window);
  aura::Window* grandchild_window_2 =
      CreateWindow(parent_window->bounds(), child_window);
  child_window->StackChildAtTop(grandchild_window_1);

  ScopedMockObserver parent_observer(occlusion_calculator_.get(),
                                     {parent_window});
  ScopedMockObserver child_observer(occlusion_calculator_.get(),
                                    {child_window});

  EXPECT_CALL(parent_observer, OnWindowOcclusionChanged(grandchild_window_1));
  EXPECT_CALL(parent_observer, OnWindowOcclusionChanged(grandchild_window_2));
  EXPECT_CALL(child_observer, OnWindowOcclusionChanged(grandchild_window_1));
  EXPECT_CALL(child_observer, OnWindowOcclusionChanged(grandchild_window_2));
  child_window->StackChildAtTop(grandchild_window_2);
  Mock::VerifyAndClearExpectations(&parent_observer);
  Mock::VerifyAndClearExpectations(&child_observer);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(grandchild_window_1),
            aura::Window::OcclusionState::OCCLUDED);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(grandchild_window_2),
            aura::Window::OcclusionState::VISIBLE);
}

TEST_F(WindowOcclusionCalculatorTest, OneObserverMultipleWindows) {
  aura::Window* parent_window_1 =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* parent_window_2 =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* child_window_1_a =
      CreateWindow(parent_window_1->bounds(), parent_window_1);
  aura::Window* child_window_1_b =
      CreateWindow(parent_window_1->bounds(), parent_window_1);
  parent_window_1->StackChildAtTop(child_window_1_a);

  aura::Window* child_window_2_a =
      CreateWindow(parent_window_2->bounds(), parent_window_2);
  aura::Window* child_window_2_b =
      CreateWindow(parent_window_2->bounds(), parent_window_2);
  parent_window_2->StackChildAtTop(child_window_2_a);

  ScopedMockObserver observer(occlusion_calculator_.get(),
                              {parent_window_1, parent_window_2});
  EXPECT_CALL(observer, OnWindowOcclusionChanged(child_window_1_a));
  EXPECT_CALL(observer, OnWindowOcclusionChanged(child_window_1_b));
  EXPECT_CALL(observer, OnWindowOcclusionChanged(child_window_2_a));
  EXPECT_CALL(observer, OnWindowOcclusionChanged(child_window_2_b));
  parent_window_1->StackChildAtTop(child_window_1_b);
  parent_window_2->StackChildAtTop(child_window_2_b);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1_a),
            aura::Window::OcclusionState::OCCLUDED);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1_b),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_2_a),
            aura::Window::OcclusionState::OCCLUDED);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_2_b),
            aura::Window::OcclusionState::VISIBLE);
}

TEST_F(WindowOcclusionCalculatorTest, RemoveObserverStopsNotifications) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());

  MockObserver observer;
  occlusion_calculator_->AddObserver({parent_window}, &observer);
  ASSERT_EQ(occlusion_calculator_->GetOcclusionState(parent_window),
            aura::Window::OcclusionState::VISIBLE);
  occlusion_calculator_->RemoveObserver(&observer);

  EXPECT_CALL(observer, OnWindowOcclusionChanged(_)).Times(0);
  parent_window->Hide();
  Mock::VerifyAndClearExpectations(&observer);
  ASSERT_EQ(occlusion_calculator_->GetOcclusionState(parent_window),
            aura::Window::OcclusionState::HIDDEN);
}

TEST_F(WindowOcclusionCalculatorTest, DoesNotMutateWindowOcclusionState) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* occluded_window =
      CreateWindow(gfx::Rect(20, 20), parent_window);
  aura::Window* visible_window =
      CreateWindow(gfx::Rect(50, 100), parent_window);

  ScopedMockObserver observer(occlusion_calculator_.get(), {parent_window});
  EXPECT_EQ(parent_window->GetOcclusionState(),
            aura::Window::OcclusionState::UNKNOWN);
  EXPECT_EQ(occluded_window->GetOcclusionState(),
            aura::Window::OcclusionState::UNKNOWN);
  EXPECT_EQ(visible_window->GetOcclusionState(),
            aura::Window::OcclusionState::UNKNOWN);
}

TEST_F(WindowOcclusionCalculatorTest, ExcludesHiddenMiniViewWindows) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* on_bottom_window =
      CreateWindow(gfx::Rect(20, 20), parent_window);
  aura::Window* on_top_window =
      CreateWindow(parent_window->bounds(), parent_window);
  on_top_window->SetProperty(kHideInDeskMiniViewKey, true);

  parent_window->StackChildAtTop(on_top_window);

  ScopedMockObserver observer(occlusion_calculator_.get(), {parent_window});
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_bottom_window),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_top_window),
            aura::Window::OcclusionState::HIDDEN);

  on_top_window->SetProperty(kHideInDeskMiniViewKey, false);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_bottom_window),
            aura::Window::OcclusionState::OCCLUDED);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_top_window),
            aura::Window::OcclusionState::VISIBLE);

  on_top_window->SetProperty(kHideInDeskMiniViewKey, true);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_bottom_window),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_top_window),
            aura::Window::OcclusionState::HIDDEN);
}

TEST_F(WindowOcclusionCalculatorTest,
       HandlesWindowsBeingAddedWhileBeingTracked) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());
  ScopedMockObserver observer(occlusion_calculator_.get(), {parent_window});

  aura::Window* on_bottom_window = CreateWindow(gfx::Rect(20, 20), nullptr);
  aura::Window* on_bottom_child_window_1 =
      CreateWindow(gfx::Rect(10, 10), on_bottom_window);
  EXPECT_CALL(observer, OnWindowOcclusionChanged(on_bottom_window));
  EXPECT_CALL(observer, OnWindowOcclusionChanged(on_bottom_child_window_1));
  parent_window->AddChild(on_bottom_window);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_bottom_window),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_bottom_child_window_1),
            aura::Window::OcclusionState::VISIBLE);

  // `on_bottom_child_window_2` should occlude `on_bottom_child_window_1`.
  aura::Window* on_bottom_child_window_2 =
      CreateWindow(on_bottom_child_window_1->bounds(), nullptr);
  EXPECT_CALL(observer, OnWindowOcclusionChanged(on_bottom_child_window_1));
  EXPECT_CALL(observer, OnWindowOcclusionChanged(on_bottom_child_window_2));
  on_bottom_window->AddChild(on_bottom_child_window_2);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_bottom_window),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_bottom_child_window_1),
            aura::Window::OcclusionState::OCCLUDED);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_bottom_child_window_2),
            aura::Window::OcclusionState::VISIBLE);

  aura::Window* on_top_window = CreateWindow(parent_window->bounds(), nullptr);
  on_top_window->SetProperty(kHideInDeskMiniViewKey, true);
  parent_window->AddChild(on_top_window);

  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_bottom_window),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_bottom_child_window_1),
            aura::Window::OcclusionState::OCCLUDED);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_bottom_child_window_2),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(on_top_window),
            aura::Window::OcclusionState::HIDDEN);

  // Destroying tracked windows should not cause crashes.
  delete on_top_window;
  delete on_bottom_child_window_2;
  delete on_bottom_child_window_1;
  delete on_bottom_window;
}

TEST_F(WindowOcclusionCalculatorTest,
       HandlesWindowsBeingRemovedWhileBeingTracked) {
  aura::Window* tracked_parent_window =
      CreateWindow(root_window()->bounds(), root_window());
  aura::Window* child_window_1 =
      CreateWindow(tracked_parent_window->bounds(), tracked_parent_window);
  aura::Window* child_window_2 =
      CreateWindow(tracked_parent_window->bounds(), tracked_parent_window);
  tracked_parent_window->StackChildAtTop(child_window_2);
  aura::Window* untracked_parent_window =
      CreateWindow(root_window()->bounds(), root_window());

  ScopedMockObserver observer(occlusion_calculator_.get(),
                              {tracked_parent_window});

  ASSERT_EQ(occlusion_calculator_->GetOcclusionState(tracked_parent_window),
            aura::Window::OcclusionState::VISIBLE);
  ASSERT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1),
            aura::Window::OcclusionState::OCCLUDED);
  ASSERT_EQ(occlusion_calculator_->GetOcclusionState(child_window_2),
            aura::Window::OcclusionState::VISIBLE);

  EXPECT_CALL(observer, OnWindowOcclusionChanged(child_window_1));
  untracked_parent_window->AddChild(child_window_2);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(tracked_parent_window),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1),
            aura::Window::OcclusionState::VISIBLE);

  untracked_parent_window->AddChild(child_window_1);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(tracked_parent_window),
            aura::Window::OcclusionState::VISIBLE);

  // Since the window occlusion changes are no longer happening in the tracked
  // parent window's hierarchy, there should be no observer notifications.
  EXPECT_CALL(observer, OnWindowOcclusionChanged(_)).Times(0);
  root_window()->StackChildAtTop(untracked_parent_window);
  untracked_parent_window->StackChildAtTop(child_window_2);
  untracked_parent_window->StackChildAtTop(child_window_1);
  Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(WindowOcclusionCalculatorTest, SnapshotsWindows) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* child_window_1 =
      CreateWindow(parent_window->bounds(), parent_window);
  aura::Window* child_window_2 =
      CreateWindow(parent_window->bounds(), parent_window);
  parent_window->StackChildAtTop(child_window_1);

  occlusion_calculator_->SnapshotOcclusionStateForWindows({parent_window});
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_2),
            aura::Window::OcclusionState::OCCLUDED);

  ScopedMockObserver observer(occlusion_calculator_.get(), {parent_window});

  EXPECT_CALL(observer, OnWindowOcclusionChanged(_)).Times(0);
  parent_window->StackChildAtTop(child_window_2);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1),
            aura::Window::OcclusionState::VISIBLE);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_2),
            aura::Window::OcclusionState::OCCLUDED);
}

TEST_F(WindowOcclusionCalculatorTest, PauseAndResume) {
  aura::Window* parent_window =
      CreateWindow(gfx::Rect(100, 100), root_window());
  aura::Window* child_window_1 =
      CreateWindow(parent_window->bounds(), parent_window);
  aura::Window* child_window_2 =
      CreateWindow(parent_window->bounds(), parent_window);
  parent_window->StackChildAtTop(child_window_1);

  ScopedMockObserver observer(occlusion_calculator_.get(), {parent_window});
  ASSERT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1),
            aura::Window::OcclusionState::VISIBLE);

  auto scoped_pause = occlusion_calculator_->Pause();

  EXPECT_CALL(observer, OnWindowOcclusionChanged(_)).Times(0);
  parent_window->StackChildAtTop(child_window_2);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1),
            aura::Window::OcclusionState::VISIBLE);

  parent_window->StackChildAtTop(child_window_1);
  scoped_pause.reset();
  EXPECT_CALL(observer, OnWindowOcclusionChanged(child_window_1));
  EXPECT_CALL(observer, OnWindowOcclusionChanged(child_window_2));
  parent_window->StackChildAtTop(child_window_2);
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_EQ(occlusion_calculator_->GetOcclusionState(child_window_1),
            aura::Window::OcclusionState::OCCLUDED);
}

}  // namespace
}  // namespace ash
