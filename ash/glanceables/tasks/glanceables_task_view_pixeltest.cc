// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "ash/api/tasks/tasks_types.h"
#include "ash/glanceables/common/glanceables_util.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/glanceables/tasks/glanceables_task_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_tags.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

// Pixel tests for `GlanceablesTaskView` that covers all possible permutations
// of UI states.
class GlanceablesTaskViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple</*use_rtl=*/bool,
                     /*has_due_date=*/bool,
                     /*has_subtasks=*/bool,
                     /*has_notes=*/bool,
                     /*completed=*/bool,
                     /*is_in_edit_state=*/bool>> {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    glanceables_util::SetIsNetworkConnectedForTest(true);

    base::Time due_date;
    ASSERT_TRUE(base::Time::FromString("2022-12-21T00:00:00.000Z", &due_date));

    task_ = std::make_unique<api::Task>(
        "task-id", "Task title",
        has_due_date() ? std::make_optional(due_date) : std::nullopt,
        /*completed=*/false, has_subtasks(),
        /*has_email_link=*/false,
        /*has_notes=*/has_notes(), /*updated=*/base::Time(),
        /*web_view_link=*/GURL(), api::Task::OriginSurfaceType::kRegular);

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);

    auto* const container =
        widget_->SetContentsView(std::make_unique<views::BoxLayoutView>());
    container->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStart);
    view_ = container->AddChildView(std::make_unique<GlanceablesTaskView>(
        task_.get(), /*mark_as_completed_callback=*/base::DoNothing(),
        /*save_callback=*/base::DoNothing(),
        /*edit_in_browser_callback=*/base::DoNothing(),
        /*show_error_message_callback=*/base::DoNothing()));

    widget_->LayoutRootViewIfNecessary();
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = use_rtl();
    return init_params;
  }

  std::string GenerateScreenshotName() const {
    std::vector<std::string> parameters = {
        use_rtl() ? "rtl" : "ltr",
        has_due_date() ? "has_due_date=true" : "has_due_date=false",
        has_subtasks() ? "has_subtasks=true" : "has_subtasks=false",
        has_notes() ? "has_notes=true" : "has_notes=false",
        completed() ? "completed=true" : "completed=false",
        is_in_edit_state() ? "is_in_edit_state=true"
                           : "is_in_edit_state=false"};

    std::string stringified_params = base::JoinString(parameters, "|");
    return base::JoinString({"glanceables_task_view", stringified_params}, ".");
  }

  views::Widget* widget() const { return widget_.get(); }
  GlanceablesTaskView* view() const { return view_; }
  bool use_rtl() const { return std::get<0>(GetParam()); }
  bool has_due_date() const { return std::get<1>(GetParam()); }
  bool has_subtasks() const { return std::get<2>(GetParam()); }
  bool has_notes() const { return std::get<3>(GetParam()); }
  bool completed() const { return std::get<4>(GetParam()); }
  bool is_in_edit_state() const { return std::get<5>(GetParam()); }

 private:
  std::unique_ptr<api::Task> task_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<GlanceablesTaskView> view_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         GlanceablesTaskViewPixelTest,
                         testing::Combine(
                             /*use_rtl=*/testing::Bool(),
                             /*has_due_date=*/testing::Bool(),
                             /*has_subtasks=*/testing::Bool(),
                             /*has_notes=*/testing::Bool(),
                             /*completed=*/testing::Bool(),
                             /*is_in_edit_state=*/testing::Bool()));

TEST_P(GlanceablesTaskViewPixelTest, GlanceablesTaskView) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-c5b9a047-dd51-4ccd-8f47-50c0fdfae7d1");  // due date +
                                                           // description
  base::AddFeatureIdTagToTestResult(
      "screenplay-c67b5acd-d0e8-41e8-a921-8060756cd807");  // subtasks

  ASSERT_TRUE(widget());

  ASSERT_FALSE(view()->GetCompletedForTest());
  if (completed()) {
    const auto* const checkbox = view()->GetCheckButtonForTest();
    ASSERT_TRUE(checkbox);
    LeftClickOn(checkbox);
    ASSERT_TRUE(view()->GetCompletedForTest());
  }

  if (is_in_edit_state()) {
    const auto* const title_label =
        views::AsViewClass<views::Label>(view()->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleLabel)));
    ASSERT_TRUE(title_label);
    LeftClickOn(title_label);

    auto* const title_text_field =
        views::AsViewClass<views::Textfield>(view()->GetViewByID(
            base::to_underlying(GlanceablesViewId::kTaskItemTitleTextField)));
    ASSERT_TRUE(title_text_field);
    views::TextfieldTestApi(title_text_field).SetCursorLayerOpacity(0.f);
  }

  widget()->LayoutRootViewIfNecessary();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName(), /*revision_number=*/0, widget()));
}

}  // namespace ash
