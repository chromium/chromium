// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/boca/on_task/on_task_pod_view.h"

#include <memory>

#include "ash/boca/on_task/on_task_pod_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

using ::testing::Sequence;

namespace ash {
namespace {

// Mock implementation of the `OnTaskPodController`.
class MockOnTaskPodController : public OnTaskPodController {
 public:
  MockOnTaskPodController() = default;
  ~MockOnTaskPodController() override = default;

  MOCK_METHOD(void, MaybeNavigateToPreviousPage, (), (override));
  MOCK_METHOD(void, MaybeNavigateToNextPage, (), (override));
  MOCK_METHOD(void, ReloadCurrentPage, (), (override));
  MOCK_METHOD(void, ToggleTabStripVisibility, (bool, bool), (override));
  MOCK_METHOD(void,
              SetSnapLocation,
              (OnTaskPodSnapLocation snap_location),
              (override));
  MOCK_METHOD(void, OnPauseModeChanged, (bool), (override));
  MOCK_METHOD(void, OnPageNavigationContextChanged, (), (override));
  MOCK_METHOD(bool, CanNavigateToPreviousPage, (), (override));
  MOCK_METHOD(bool, CanNavigateToNextPage, (), (override));
  MOCK_METHOD(bool, CanToggleTabStripVisibility, (), (override));
};

class OnTaskPodViewTest : public AshTestBase {
 protected:
  void SetUp() override {
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    auto on_task_pod_view =
        std::make_unique<OnTaskPodView>(&mock_on_task_pod_controller_);
    on_task_pod_view_ = widget_->SetContentsView(std::move(on_task_pod_view));
  }

  MockOnTaskPodController mock_on_task_pod_controller_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<OnTaskPodView> on_task_pod_view_;
};

TEST_F(OnTaskPodViewTest, OrientationAndAlignment) {
  EXPECT_EQ(on_task_pod_view_->GetOrientation(),
            views::BoxLayout::Orientation::kHorizontal);
  EXPECT_EQ(on_task_pod_view_->GetMainAxisAlignment(),
            views::BoxLayout::MainAxisAlignment::kStart);
  EXPECT_EQ(on_task_pod_view_->GetCrossAxisAlignment(),
            views::BoxLayout::CrossAxisAlignment::kStart);
}

TEST_F(OnTaskPodViewTest, NavigateBackButtonClickNavigatesToPreviousPage) {
  EXPECT_CALL(mock_on_task_pod_controller_, MaybeNavigateToPreviousPage())
      .Times(1);
  LeftClickOn(on_task_pod_view_->get_back_button_for_testing());
}

TEST_F(OnTaskPodViewTest, BackButtonEnabledWhenCanNavigateToPreviousPage) {
  EXPECT_CALL(mock_on_task_pod_controller_, CanNavigateToPreviousPage())
      .WillOnce(testing::Return(true));
  on_task_pod_view_->OnPageNavigationContextUpdate();
  EXPECT_TRUE(on_task_pod_view_->get_back_button_for_testing()->GetEnabled());
}

TEST_F(OnTaskPodViewTest, BackButtonDisabledWhenCannotNavigateToPreviousPage) {
  EXPECT_CALL(mock_on_task_pod_controller_, CanNavigateToPreviousPage())
      .WillOnce(testing::Return(false));
  on_task_pod_view_->OnPageNavigationContextUpdate();
  EXPECT_FALSE(on_task_pod_view_->get_back_button_for_testing()->GetEnabled());
}

TEST_F(OnTaskPodViewTest, NavigateForwardButtonClickNavigatesToNextPage) {
  EXPECT_CALL(mock_on_task_pod_controller_, MaybeNavigateToNextPage()).Times(1);
  LeftClickOn(on_task_pod_view_->get_forward_button_for_testing());
}

TEST_F(OnTaskPodViewTest, ForwardButtonEnabledWhenCanNavigateToNextPage) {
  EXPECT_CALL(mock_on_task_pod_controller_, CanNavigateToNextPage())
      .WillOnce(testing::Return(true));
  on_task_pod_view_->OnPageNavigationContextUpdate();
  EXPECT_TRUE(
      on_task_pod_view_->get_forward_button_for_testing()->GetEnabled());
}

TEST_F(OnTaskPodViewTest, ForwardButtonDisabledWhenCannotNavigateToNextPage) {
  EXPECT_CALL(mock_on_task_pod_controller_, CanNavigateToNextPage())
      .WillOnce(testing::Return(false));
  on_task_pod_view_->OnPageNavigationContextUpdate();
  EXPECT_FALSE(
      on_task_pod_view_->get_forward_button_for_testing()->GetEnabled());
}

TEST_F(OnTaskPodViewTest, ReloadTabButtonClickTriggersTabReload) {
  EXPECT_CALL(mock_on_task_pod_controller_, ReloadCurrentPage()).Times(1);
  LeftClickOn(on_task_pod_view_->reload_tab_button_for_testing());
}

TEST_F(OnTaskPodViewTest,
       HidePinTabStripButtonWhenCannotEnableShowOrHideTabStrip) {
  EXPECT_CALL(mock_on_task_pod_controller_, CanToggleTabStripVisibility())
      .WillOnce(testing::Return(false));
  on_task_pod_view_->OnLockedModeUpdate();
  EXPECT_FALSE(
      on_task_pod_view_->pin_tab_strip_button_for_testing()->GetVisible());
}

TEST_F(OnTaskPodViewTest, PinTabStripButtonClickTriggersShowOrHideTabStrip) {
  Sequence s;
  EXPECT_CALL(mock_on_task_pod_controller_, CanToggleTabStripVisibility())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_on_task_pod_controller_,
              ToggleTabStripVisibility(false, false))
      .Times(1)
      .InSequence(s);
  on_task_pod_view_->OnLockedModeUpdate();

  // Resize the widget to fit the contents view, and apply the new layout within
  // the widget. Otherwise, we are not able to click on pin_tab_strip_button.
  widget_->SetSize(on_task_pod_view_->GetPreferredSize());
  widget_->LayoutRootViewIfNecessary();
  auto* const pin_tab_strip_button =
      on_task_pod_view_->pin_tab_strip_button_for_testing();
  ASSERT_TRUE(pin_tab_strip_button->GetVisible());

  EXPECT_CALL(mock_on_task_pod_controller_,
              ToggleTabStripVisibility(true, true))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(mock_on_task_pod_controller_,
              ToggleTabStripVisibility(false, true))
      .Times(1)
      .InSequence(s);
  LeftClickOn(pin_tab_strip_button);
  EXPECT_TRUE(pin_tab_strip_button->toggled());
  LeftClickOn(pin_tab_strip_button);
  EXPECT_FALSE(pin_tab_strip_button->toggled());
}

TEST_F(OnTaskPodViewTest, PodPositionSliderButtonClickChangesPodLocation) {
  Sequence s;
  EXPECT_CALL(mock_on_task_pod_controller_,
              SetSnapLocation(OnTaskPodSnapLocation::kTopRight))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(mock_on_task_pod_controller_,
              SetSnapLocation(OnTaskPodSnapLocation::kTopLeft))
      .Times(1)
      .InSequence(s);
  auto* const dock_right_button =
      on_task_pod_view_->dock_right_button_for_testing();
  auto* const dock_left_button =
      on_task_pod_view_->dock_left_button_for_testing();
  LeftClickOn(dock_right_button);
  LeftClickOn(dock_left_button);
}

}  // namespace
}  // namespace ash
