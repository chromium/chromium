// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_menu_model_adapter.h"

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {
using ::testing::Bool;
using ::testing::Combine;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

using crosapi::mojom::ClipboardHistoryControllerShowSource;

namespace {

ClipboardHistoryControllerImpl* GetClipboardHistoryController() {
  return Shell::Get()->clipboard_history_controller();
}

std::vector<ClipboardHistoryControllerShowSource>
GetClipboardHistoryShowSources() {
  std::vector<ClipboardHistoryControllerShowSource> sources;
  for (int i =
           static_cast<int>(ClipboardHistoryControllerShowSource::kMinValue);
       i <= static_cast<int>(ClipboardHistoryControllerShowSource::kMaxValue);
       ++i) {
    sources.push_back(static_cast<ClipboardHistoryControllerShowSource>(i));
  }
  return sources;
}

}  // namespace

// Base class for `ClipboardHistoryMenuModelAdapter` tests that verify the
// presence of a menu header, a menu footer, both, or neither.
class ClipboardHistoryMenuModelAdapterMenuItemTest
    : public AshTestBase,
      public WithParamInterface<std::tuple<ClipboardHistoryControllerShowSource,
                                           /*enable_refresh=*/bool>> {
 public:
  ClipboardHistoryMenuModelAdapterMenuItemTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kClipboardHistoryLongpress,
          IsClipboardHistoryLongpressEnabled()},
         {chromeos::features::kClipboardHistoryRefresh,
          IsClipboardHistoryRefreshEnabled()},
         {chromeos::features::kJelly, IsClipboardHistoryRefreshEnabled()}});
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    GetClipboardHistoryController()->set_confirmed_operation_callback_for_test(
        operation_confirmed_future_.GetRepeatingCallback());
  }

  void WriteTextToClipboardAndConfirm(const std::u16string& str) {
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteText(str);
    }
    EXPECT_TRUE(operation_confirmed_future_.Take());
  }

  ClipboardHistoryControllerShowSource GetSource() const {
    return std::get<0>(GetParam());
  }

  bool IsClipboardHistoryLongpressEnabled() {
    return GetSource() ==
           ClipboardHistoryControllerShowSource::kControlVLongpress;
  }

  bool IsClipboardHistoryRefreshEnabled() { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TestFuture<bool> operation_confirmed_future_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryMenuModelAdapterMenuItemTest,
                         Combine(ValuesIn(GetClipboardHistoryShowSources()),
                                 /*enable_refresh=*/Bool()));

TEST_P(ClipboardHistoryMenuModelAdapterMenuItemTest,
       HeaderAndFooterConditionallyPresent) {
  // Write items to clipboard history so that the menu can show.
  WriteTextToClipboardAndConfirm(u"A");
  WriteTextToClipboardAndConfirm(u"B");
  auto* const controller = GetClipboardHistoryController();
  ASSERT_TRUE(controller);
  EXPECT_EQ(controller->history()->GetItems().size(), 2u);

  // Show the clipboard history menu.
  EXPECT_TRUE(controller->ShowMenu(
      gfx::Rect(), ui::MenuSourceType::MENU_SOURCE_NONE, GetSource()));
  EXPECT_TRUE(controller->IsMenuShowing());

  // Verify the number of items in the menu model.
  const bool has_header = IsClipboardHistoryRefreshEnabled();
  const bool has_footer = IsClipboardHistoryLongpressEnabled();
  size_t expected_menu_item_count = controller->history()->GetItems().size();
  if (has_header) {
    // The menu's first item should be a header.
    ++expected_menu_item_count;
  }
  if (has_footer) {
    // The menu's last item should be a footer.
    ++expected_menu_item_count;
  }
  const auto* const model =
      controller->context_menu_for_test()->GetModelForTest();
  ASSERT_TRUE(model);
  ASSERT_EQ(model->GetItemCount(), expected_menu_item_count);

  // Verify that the first item is a header iff the UI refresh is enabled.
  EXPECT_EQ(model->GetTypeAt(0u), has_header
                                      ? ui::MenuModel::ItemType::TYPE_TITLE
                                      : ui::MenuModel::ItemType::TYPE_COMMAND);

  // Verify that the last item is a footer iff the menu was shown via Ctrl+V
  // long-press.
  EXPECT_EQ(model->GetTypeAt(model->GetItemCount() - 1u),
            has_footer ? ui::MenuModel::ItemType::TYPE_TITLE
                       : ui::MenuModel::ItemType::TYPE_COMMAND);
}

}  // namespace ash
