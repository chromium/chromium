// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_zero_state_view.h"

#include <memory>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/test_support/clipboard_history_item_builder.h"
#include "ash/clipboard/test_support/mock_clipboard_history_controller.h"
#include "ash/picker/picker_test_util.h"
#include "ash/picker/views/picker_caps_nudge_view.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/test/view_drawn_waiter.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Key;
using ::testing::Not;
using ::testing::Property;

constexpr int kPickerWidth = 320;

class PickerZeroStateViewTest : public views::ViewsTestBase {
 private:
  AshColorProvider ash_color_provider_;
};

TEST_F(PickerZeroStateViewTest, CreatesCategorySections) {
  PickerZeroStateView view(kPickerWidth, base::DoNothing(), base::DoNothing());

  EXPECT_THAT(view.section_views_for_testing(),
              ElementsAre(Key(PickerCategoryType::kExpressions),
                          Key(PickerCategoryType::kLinks),
                          Key(PickerCategoryType::kFiles)));
  EXPECT_THAT(view.SuggestedSectionForTesting(), IsNull());
}

TEST_F(PickerZeroStateViewTest, LeftClickSelectsCategory) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->SetFullscreen(true);
  base::test::TestFuture<PickerCategory> future;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      kPickerWidth, future.GetRepeatingCallback(), base::DoNothing()));
  widget->Show();
  ASSERT_THAT(view->section_views_for_testing(),
              Contains(Key(PickerCategoryType::kExpressions)));
  ASSERT_THAT(view->section_views_for_testing()
                  .find(PickerCategoryType::kExpressions)
                  ->second->item_views_for_testing(),
              Not(IsEmpty()));

  PickerItemView* category_view = view->section_views_for_testing()
                                      .find(PickerCategoryType::kExpressions)
                                      ->second->item_views_for_testing()[0];
  ViewDrawnWaiter().Wait(category_view);
  LeftClickOn(*category_view);

  EXPECT_EQ(future.Get(), PickerCategory::kEmojis);
}

TEST_F(PickerZeroStateViewTest, ClickingOkInCapsNudgeHidesCapsNudge) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->SetFullscreen(true);
  base::test::TestFuture<PickerCategory> future;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      kPickerWidth, future.GetRepeatingCallback(), base::DoNothing()));
  widget->Show();

  auto* caps_nudge_view = view->CapsNudgeViewForTesting();
  EXPECT_THAT(caps_nudge_view, Property(&views::View::GetVisible, true));

  LeftClickOn(*caps_nudge_view->GetOkButtonForTesting());

  EXPECT_EQ(view->CapsNudgeViewForTesting(), nullptr);
}

TEST_F(PickerZeroStateViewTest, ShowsClipboardItems) {
  base::UnguessableToken item_id;
  testing::StrictMock<MockClipboardHistoryController> mock_clipboard;
  EXPECT_CALL(mock_clipboard, GetHistoryValues)
      .WillOnce(
          [&item_id](
              ClipboardHistoryController::GetHistoryValuesCallback callback) {
            ClipboardHistoryItemBuilder builder;
            builder.SetFormat(ui::ClipboardInternalFormat::kText);
            ClipboardHistoryItem item = builder.Build();
            item_id = item.id();
            std::move(callback).Run({std::move(item)});
          });

  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->SetFullscreen(true);
  base::test::TestFuture<const PickerSearchResult&> future;
  auto* view = widget->SetContentsView(std::make_unique<PickerZeroStateView>(
      kPickerWidth, base::DoNothing(), future.GetRepeatingCallback()));
  widget->Show();

  EXPECT_THAT(view->SuggestedSectionForTesting(), Not(IsNull()));
  PickerItemView* item_view =
      view->SuggestedSectionForTesting()->item_views_for_testing()[0];

  ViewDrawnWaiter().Wait(item_view);
  LeftClickOn(*item_view);

  EXPECT_EQ(future.Get(), PickerSearchResult::Clipboard(item_id));
}

}  // namespace
}  // namespace ash
