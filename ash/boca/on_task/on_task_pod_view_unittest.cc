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

namespace ash {
namespace {

// Mock implementation of the `OnTaskPodController`.
class MockOnTaskPodController : public OnTaskPodController {
 public:
  MockOnTaskPodController() = default;
  ~MockOnTaskPodController() override = default;

  MOCK_METHOD(void, ReloadCurrentPage, (), (override));
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

}  // namespace
}  // namespace ash
