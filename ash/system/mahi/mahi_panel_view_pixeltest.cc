// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_view.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Pixel tests for Chrome OS Status Area. This relates to all tray buttons in
// the bottom right corner.
class MahiPanelViewPixelTest : public AshTestBase {
 public:
  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kMahi, chromeos::features::kFeatureManagementMahi},
        {});
    AshTestBase::SetUp();

    scoped_setter_ = std::make_unique<chromeos::ScopedMahiManagerSetter>(
        &mock_mahi_manager_);

    CreatePanelWidget();
  }

  void TearDown() override {
    panel_view_ = nullptr;
    widget_.reset();
    scoped_setter_.reset();

    AshTestBase::TearDown();
  }

  void CreatePanelWidget() {
    widget_ = CreateFramelessTestWidget();
    widget_->SetBounds(
        gfx::Rect(/*x=*/0, /*y=*/0,
                  /*width=*/mahi_constants::kPanelDefaultWidth,
                  /*height=*/mahi_constants::kPanelDefaultHeight));
    panel_view_ = widget_->SetContentsView(
        std::make_unique<MahiPanelView>(&ui_controller_));
  }

  void RecreatePanelWidget() {
    panel_view_ = nullptr;
    widget_.reset();

    CreatePanelWidget();
  }

  // Scroll the scroll view inside Mahi panel to the bottom.
  void ScrollToBottom() {
    auto* scroll_view = views::AsViewClass<views::ScrollView>(
        panel_view()->GetViewByID(mahi_constants::ViewId::kScrollView));
    ASSERT_TRUE(scroll_view);
    scroll_view->vertical_scroll_bar()->ScrollByAmount(
        views::ScrollBar::ScrollAmount::kEnd);
  }

  MockMahiManager& mock_mahi_manager() { return mock_mahi_manager_; }

  MahiUiController* ui_controller() { return &ui_controller_; }

  MahiPanelView* panel_view() { return panel_view_; }

  views::Widget* widget() { return widget_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::NiceMock<MockMahiManager> mock_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
  MahiUiController ui_controller_;
  raw_ptr<MahiPanelView> panel_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(MahiPanelViewPixelTest, MainPanel) {
  ON_CALL(mock_mahi_manager(), GetContentTitle)
      .WillByDefault(testing::Return(u"Test content title"));
  ON_CALL(mock_mahi_manager(), GetContentIcon)
      .WillByDefault(testing::Return(
          gfx::test::CreateImageSkia(/*size=*/128, SK_ColorBLUE)));

  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([](chromeos::MahiManager::MahiSummaryCallback callback) {
        std::move(callback).Run(
            base::StrCat(std::vector<std::u16string>(30, u"Summary text ")),
            chromeos::MahiResponseStatus::kSuccess);
      });

  ui_controller()->RefreshContents();
  views::test::RunScheduledLayout(widget());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "panel_view", /*revision_number=*/7, panel_view()));
}

TEST_F(MahiPanelViewPixelTest, ContentSourceButton) {
  ON_CALL(mock_mahi_manager(), GetContentTitle)
      .WillByDefault(testing::Return(base::StrCat(
          std::vector<std::u16string>(3, u"Long content title "))));
  ON_CALL(mock_mahi_manager(), GetContentIcon)
      .WillByDefault(testing::Return(
          gfx::test::CreateImageSkia(/*size=*/200, SK_ColorRED)));

  ui_controller()->RefreshContents();
  views::test::RunScheduledLayout(widget());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "content_source", /*revision_number=*/2,
      panel_view()->GetViewByID(mahi_constants::ViewId::kContentSourceButton)));
}

TEST_F(MahiPanelViewPixelTest, SummaryView) {
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([](chromeos::MahiManager::MahiSummaryCallback callback) {
        std::move(callback).Run(
            base::StrCat(std::vector<std::u16string>(35, u"Summary text ")),
            chromeos::MahiResponseStatus::kSuccess);
      });

  ui_controller()->RefreshContents();
  views::test::RunScheduledLayout(widget());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "summary_view", /*revision_number=*/4,
      panel_view()->GetViewByID(mahi_constants::ViewId::kScrollView)));
}

TEST_F(MahiPanelViewPixelTest, PanelWithoutFeedbackButtons) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
      prefs::kHmrFeedbackAllowed, false);
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([](chromeos::MahiManager::MahiSummaryCallback callback) {
        std::move(callback).Run(
            base::StrCat(std::vector<std::u16string>(35, u"Summary text ")),
            chromeos::MahiResponseStatus::kSuccess);
      });

  RecreatePanelWidget();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "scroll_view", /*revision_number=*/0,
      panel_view()->GetViewByID(mahi_constants::ViewId::kScrollView)));

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "footer", /*revision_number=*/0,
      panel_view()->GetViewByID(mahi_constants::ViewId::kFooterLabel)));
}

TEST_F(MahiPanelViewPixelTest, QuestionAnswerViewBasic) {
  auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  auto* const question_textfield = views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));

  const std::u16string answer(u"test answer");
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [&answer](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(answer,
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  // Set a valid text in the question textfield.
  const std::u16string question(u"question");
  question_textfield->SetText(question);

  // Pressing the send button should create a question and answer text bubble.
  LeftClickOn(send_button);

  views::test::RunScheduledLayout(widget());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "question_answer_view_basic", /*revision_number=*/5,
      panel_view()->GetViewByID(mahi_constants::ViewId::kScrollView)));
}

TEST_F(MahiPanelViewPixelTest, QuestionAnswerViewLongText) {
  auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  auto* const question_textfield = views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));

  const std::u16string answer =
      base::StrCat(std::vector<std::u16string>(25, u"Long Answer "));
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [&answer](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(answer,
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  // Set a valid text in the question textfield.
  const std::u16string question =
      base::StrCat(std::vector<std::u16string>(25, u"Long Question "));
  question_textfield->SetText(question);

  // Pressing the send button should create a question and answer text bubble.
  LeftClickOn(send_button);

  views::test::RunScheduledLayout(widget());

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "question_answer_view_long_text", /*revision_number=*/7,
      panel_view()->GetViewByID(mahi_constants::ViewId::kScrollView)));
}

TEST_F(MahiPanelViewPixelTest, SummaryViewScrollToBottom) {
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([](chromeos::MahiManager::MahiSummaryCallback callback) {
        std::move(callback).Run(
            base::StrCat(std::vector<std::u16string>(60, u"Summary text ")),
            chromeos::MahiResponseStatus::kSuccess);
      });

  ui_controller()->RefreshContents();
  views::test::RunScheduledLayout(widget());

  ScrollToBottom();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "summary_view_bottom", /*revision_number=*/3,
      panel_view()->GetViewByID(mahi_constants::ViewId::kScrollView)));
}

TEST_F(MahiPanelViewPixelTest, QuestionAnswerViewScrollToBottom) {
  const std::u16string answer =
      base::StrCat(std::vector<std::u16string>(35, u"Long Answer "));
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [&answer](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(answer,
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  // Set a valid text in the question textfield.
  const std::u16string question =
      base::StrCat(std::vector<std::u16string>(25, u"Long Question "));
  views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield))
      ->SetText(question);

  // Pressing the send button should create a question and answer text bubble.
  LeftClickOn(panel_view()->GetViewByID(
      mahi_constants::ViewId::kAskQuestionSendButton));

  views::test::RunScheduledLayout(widget());

  ScrollToBottom();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "question_answer_bottom", /*revision_number=*/4,
      panel_view()->GetViewByID(mahi_constants::ViewId::kScrollView)));
}

}  // namespace ash
