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

  MOCK_METHOD(void, ReloadCurrentPage, (), (override));
  MOCK_METHOD(void,
              SetSnapLocation,
              (OnTaskPodSnapLocation snap_location),
              (override));
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

TEST_F(OnTaskPodViewTest, ReloadTabButtonClickTriggersTabReload) {
  EXPECT_CALL(mock_on_task_pod_controller_, ReloadCurrentPage()).Times(1);
  LeftClickOn(on_task_pod_view_->reload_tab_button_for_testing());
}

TEST_F(OnTaskPodViewTest, SnapPodButtonClickTogglesSnapLocation) {
  Sequence s;
  EXPECT_CALL(mock_on_task_pod_controller_,
              SetSnapLocation(OnTaskPodSnapLocation::kTopRight))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(mock_on_task_pod_controller_,
              SetSnapLocation(OnTaskPodSnapLocation::kTopLeft))
      .Times(1)
      .InSequence(s);
  auto* const snap_pod_button =
      on_task_pod_view_->snap_pod_button_for_testing();
  LeftClickOn(snap_pod_button);
  LeftClickOn(snap_pod_button);
}

}  // namespace
}  // namespace ash
