// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_menu_model_adapter.h"

#include <string>

#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/ui/clipboard_history/clipboard_history_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view_utils.h"

namespace ash {

using ::testing::AllOf;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Conditional;
using ::testing::Eq;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::Values;
using ::testing::ValuesIn;
using ::testing::WithParamInterface;

using crosapi::mojom::ClipboardHistoryControllerShowSource;

namespace {

// Helpers ---------------------------------------------------------------------

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

void FlushMessageLoop() {
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Matchers --------------------------------------------------------------------

template <typename ViewType, typename MatcherType>
auto GetViewById(int id, MatcherType m) {
  return ResultOf(
      [id](const auto* arg) {
        return views::AsViewClass<ViewType>(arg->GetViewByID(id));
      },
      m);
}

}  // namespace

// ClipboardHistoryMenuModelAdapterRefreshTest ---------------------------------

// Base class for `ClipboardHistoryMenuModelAdapter` tests whose only required
// parameterization is whether the clipboard history refresh is enabled.
class ClipboardHistoryMenuModelAdapterRefreshTest
    : public AshTestBase,
      public WithParamInterface</*enable_refresh=*/bool> {
 public:
  ClipboardHistoryMenuModelAdapterRefreshTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{chromeos::features::kClipboardHistoryRefresh,
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
    EXPECT_FALSE(operation_confirmed_future_.IsReady());
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteText(str);
    }
    EXPECT_TRUE(operation_confirmed_future_.Take());
  }

  bool IsClipboardHistoryRefreshEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TestFuture<bool> operation_confirmed_future_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryMenuModelAdapterRefreshTest,
                         /*enable_refresh=*/Bool());

TEST_P(ClipboardHistoryMenuModelAdapterRefreshTest, FirstItemShowsCtrlVLabel) {
  // Write items to clipboard history so that the menu can show.
  WriteTextToClipboardAndConfirm(u"A");
  WriteTextToClipboardAndConfirm(u"B");
  WriteTextToClipboardAndConfirm(u"C");

  // Show the clipboard history menu and wait for the first item to be selected
  // so that item selection does not interfere with removing items later.
  auto* const controller = GetClipboardHistoryController();
  ASSERT_TRUE(controller);
  base::RunLoop run_loop;
  controller->set_initial_item_selected_callback_for_test(
      run_loop.QuitClosure());
  EXPECT_TRUE(controller->ShowMenu(
      gfx::Rect(), ui::MenuSourceType::MENU_SOURCE_NONE,
      ClipboardHistoryControllerShowSource::kDefaultValue));
  run_loop.Run();
  EXPECT_TRUE(controller->IsMenuShowing());

  // Verify the number of items in the menu.
  auto* const adapter = controller->context_menu_for_test();
  ASSERT_EQ(adapter->GetMenuItemsCount(), 3u);

  // Get handles to three clipboard history items' Ctrl+V labels, noting that
  // the items' indices will be offset by 1 if the menu has a header.
  const size_t offset = IsClipboardHistoryRefreshEnabled() ? 1u : 0u;
  const auto* const ctrl_v_label1 =
      adapter->GetMenuItemViewAtForTest(0u + offset)
          ->GetViewByID(clipboard_history_util::kCtrlVLabelID);
  ASSERT_EQ(!!ctrl_v_label1, IsClipboardHistoryRefreshEnabled());
  const auto* const ctrl_v_label2 =
      adapter->GetMenuItemViewAtForTest(1u + offset)
          ->GetViewByID(clipboard_history_util::kCtrlVLabelID);
  ASSERT_EQ(!!ctrl_v_label2, IsClipboardHistoryRefreshEnabled());
  const auto* const ctrl_v_label3 =
      adapter->GetMenuItemViewAtForTest(2u + offset)
          ->GetViewByID(clipboard_history_util::kCtrlVLabelID);
  ASSERT_EQ(!!ctrl_v_label3, IsClipboardHistoryRefreshEnabled());

  // If the labels do exist, test that only the first item's label is visible.
  if (IsClipboardHistoryRefreshEnabled()) {
    // Initially, the first item's label should be visible.
    EXPECT_TRUE(ctrl_v_label1->GetVisible());
    EXPECT_FALSE(ctrl_v_label2->GetVisible());
    EXPECT_FALSE(ctrl_v_label3->GetVisible());

    // Remove the first item. Now the second item's label should be visible.
    adapter->RemoveMenuItemWithCommandId(
        clipboard_history_util::kFirstItemCommandId);
    FlushMessageLoop();
    EXPECT_TRUE(ctrl_v_label2->GetVisible());
    EXPECT_FALSE(ctrl_v_label3->GetVisible());

    // Remove the second item. Now the third item's label should be visible.
    adapter->RemoveMenuItemWithCommandId(
        clipboard_history_util::kFirstItemCommandId + 1);
    FlushMessageLoop();
    EXPECT_TRUE(ctrl_v_label3->GetVisible());
  }
}

TEST_P(ClipboardHistoryMenuModelAdapterRefreshTest,
       TextItemHasExpectedDisplayTextLabel) {
  // Write items to clipboard history so that the menu can show.
  WriteTextToClipboardAndConfirm(u"A");
  WriteTextToClipboardAndConfirm(u"https://google.com/");

  // Show the clipboard history menu.
  auto* const controller = GetClipboardHistoryController();
  ASSERT_TRUE(controller);
  EXPECT_TRUE(controller->ShowMenu(
      gfx::Rect(), ui::MenuSourceType::MENU_SOURCE_NONE,
      ClipboardHistoryControllerShowSource::kDefaultValue));
  EXPECT_TRUE(controller->IsMenuShowing());

  // Verify the number of items in the menu.
  const auto* const adapter = controller->context_menu_for_test();
  ASSERT_THAT(
      adapter,
      Property(&ClipboardHistoryMenuModelAdapter::GetMenuItemsCount, Eq(2u)));

  // Verify expected display text labels.
  const size_t offset = IsClipboardHistoryRefreshEnabled() ? 1u : 0u;
  for (size_t i = 0u; i < 2u; ++i) {
    const views::Label* display_text_label = views::AsViewClass<views::Label>(
        adapter->GetMenuItemViewAtForTest(i + offset)
            ->GetViewByID(clipboard_history_util::kDisplayTextLabelID));

    gfx::ElideBehavior elide_behavior = gfx::ELIDE_TAIL;
    size_t max_lines = 1u;

    if (IsClipboardHistoryRefreshEnabled()) {
      if (chromeos::clipboard_history::IsUrl(display_text_label->GetText())) {
        elide_behavior = gfx::ELIDE_MIDDLE;
      } else {
        max_lines = ClipboardHistoryViews::kTextItemMaxLines;
      }
    }

    EXPECT_THAT(
        display_text_label,
        AllOf(Property(&views::Label::GetElideBehavior, Eq(elide_behavior)),
              Property(&views::Label::GetMaxLines, Eq(max_lines)),
              Property(&views::Label::GetMultiLine, Eq(max_lines > 1u))));
  }
}

// Base class for `ClipboardHistoryMenuModelAdapter` tests that verify the
// presence of a menu header, a menu footer, both, or neither.
class ClipboardHistoryMenuModelAdapterMenuItemTest
    : public AshTestBase,
      public WithParamInterface<
          std::tuple<ClipboardHistoryControllerShowSource,
                     /*time_since_menu_shown=*/std::optional<base::TimeDelta>,
                     /*time_since_nudge_shown=*/std::optional<base::TimeDelta>,
                     /*enable_refresh=*/bool>> {
 public:
  ClipboardHistoryMenuModelAdapterMenuItemTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
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

    auto* session_controller = Shell::Get()->session_controller();
    ASSERT_TRUE(session_controller);
    auto* prefs = session_controller->GetLastActiveUserPrefService();
    ASSERT_TRUE(prefs);

    // Set nudge last time shown.
    if (const auto& time_since_nudge_shown = GetTimeSinceNudgeShown()) {
      ClipboardHistoryController::Get()->OnScreenshotNotificationCreated();
      task_environment()->FastForwardBy(*time_since_nudge_shown);
    }

    // Set menu last time shown.
    if (const auto& time_since_menu_shown = GetTimeSinceMenuShown()) {
      prefs->SetTime(prefs::kMultipasteMenuLastTimeShown,
                     base::Time::Now() - time_since_menu_shown.value());
    }
  }

  void WriteTextToClipboardAndFlushMessageLoop(const std::u16string& str) {
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteText(str);
    }
    FlushMessageLoop();
  }

  ClipboardHistoryControllerShowSource GetSource() const {
    return std::get<0>(GetParam());
  }

  const std::optional<base::TimeDelta>& GetTimeSinceMenuShown() const {
    return std::get<1>(GetParam());
  }

  const std::optional<base::TimeDelta>& GetTimeSinceNudgeShown() const {
    return std::get<2>(GetParam());
  }

  bool IsClipboardHistoryLongpressEnabled() const {
    return GetSource() ==
           ClipboardHistoryControllerShowSource::kControlVLongpress;
  }

  bool IsClipboardHistoryRefreshEnabled() const {
    return std::get<3>(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ClipboardHistoryMenuModelAdapterMenuItemTest,
                         Combine(ValuesIn(GetClipboardHistoryShowSources()),
                                 /*time_since_menu_shown=*/
                                 Values(std::make_optional(base::Days(60)),
                                        std::make_optional(base::Days(59)),
                                        std::nullopt),
                                 /*time_since_nudge_shown=*/
                                 Values(std::make_optional(base::Seconds(61)),
                                        std::make_optional(base::Seconds(60)),
                                        std::nullopt),
                                 /*enable_refresh=*/Bool()));

TEST_P(ClipboardHistoryMenuModelAdapterMenuItemTest,
       HeaderAndFooterConditionallyPresent) {
  // Write items to clipboard history so that the menu can show.
  WriteTextToClipboardAndFlushMessageLoop(u"A");
  WriteTextToClipboardAndFlushMessageLoop(u"B");
  auto* const controller = GetClipboardHistoryController();
  ASSERT_TRUE(controller);
  EXPECT_EQ(controller->history()->GetItems().size(), 2u);

  // Show the clipboard history menu.
  EXPECT_TRUE(controller->ShowMenu(
      gfx::Rect(), ui::MenuSourceType::MENU_SOURCE_NONE, GetSource()));
  EXPECT_TRUE(controller->IsMenuShowing());

  const auto time_since_menu_shown =
      GetTimeSinceMenuShown().value_or(base::TimeDelta::Max());
  const auto time_since_nudge_shown =
      GetTimeSinceNudgeShown().value_or(base::TimeDelta::Max());

  const bool has_header = IsClipboardHistoryRefreshEnabled();
  const bool has_footer = IsClipboardHistoryLongpressEnabled() ||
                          (IsClipboardHistoryRefreshEnabled() &&
                           ((time_since_menu_shown >= base::Days(60)) ||
                            (time_since_nudge_shown <= base::Seconds(60))));

  // Verify the number of items in the menu model.
  size_t expected_menu_item_count = controller->history()->GetItems().size();
  if (has_header) {
    // The menu's first item should be a header.
    ++expected_menu_item_count;
  }
  if (has_footer) {
    // The menu's last item should be a footer.
    ++expected_menu_item_count;
  }
  const auto* const adapter = controller->context_menu_for_test();
  const auto* const model = adapter->GetModelForTest();
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

  if (!has_footer) {
    return;
  }

  // Verify that footer content is of the expected version.
  const int footer_index = model->GetItemCount() - 1u;
  const auto* const footer = adapter->GetMenuItemViewAtForTest(footer_index);
  EXPECT_THAT(
      footer->GetViewByID(clipboard_history_util::kFooterContentViewID),
      Conditional(IsClipboardHistoryRefreshEnabled(), IsNull(), NotNull()));
  EXPECT_THAT(
      footer->GetViewByID(clipboard_history_util::kFooterContentV2ViewID),
      Conditional(
          IsClipboardHistoryRefreshEnabled(),
          GetViewById<views::StyledLabel>(
              clipboard_history_util::kFooterContentV2LabelID,
              Property(
                  &views::StyledLabel::GetText,
                  Conditional(
                      IsClipboardHistoryLongpressEnabled(),
                      l10n_util::GetStringUTF16(
                          IDS_ASH_CLIPBOARD_HISTORY_CONTROL_V_LONGPRESS_FOOTER),
                      l10n_util::GetStringFUTF16(
                          IDS_ASH_CLIPBOARD_HISTORY_FOOTER,
                          clipboard_history_util::GetShortcutKeyName())))),
          IsNull()));
}

}  // namespace ash
