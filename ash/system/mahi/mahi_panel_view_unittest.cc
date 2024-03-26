// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/shell.h"
#include "ash/style/system_textfield.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Aliases ---------------------------------------------------------------------

using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

// Constants -------------------------------------------------------------------

const std::vector<chromeos::MahiOutline> kFakeOutlines(
    {chromeos::MahiOutline(/*id=*/1, u"Outline 1"),
     chromeos::MahiOutline(/*id=*/2, u"Outline 2"),
     chromeos::MahiOutline(/*id=*/3, u"Outline 3"),
     chromeos::MahiOutline(/*id=*/4, u"Outline 4"),
     chromeos::MahiOutline(/*id=*/5, u"Outline 5")});

// MockNewWindowDelegate -------------------------------------------------------

class MockNewWindowDelegate : public NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

// Helpers ---------------------------------------------------------------------

// Returns `kFakeOutlines` syncly.
void ReturnDefaultOutlines(
    chromeos::MahiManager::MahiOutlinesCallback callback) {
  std::move(callback).Run(/*outlines=*/kFakeOutlines,
                          chromeos::MahiResponseStatus::kSuccess);
}

// Returns `kFakeOutlines` asyncly. Use `waiter` to wait for the response.
void ReturnDefaultOutlinesAsyncly(
    base::test::TestFuture<void>& waiter,
    chromeos::MahiManager::MahiOutlinesCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::OnceClosure unblock_closure,
                        chromeos::MahiManager::MahiOutlinesCallback callback) {
                       std::move(callback).Run(
                           kFakeOutlines,
                           chromeos::MahiResponseStatus::kSuccess);
                       std::move(unblock_closure).Run();
                     },
                     waiter.GetCallback(), std::move(callback)));
}

// Returns a fake summary asyncly. Use `waiter` to wait for the response.
void ReturnDefaultSummaryAsyncly(
    base::test::TestFuture<void>& waiter,
    chromeos::MahiManager::MahiSummaryCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::OnceClosure unblock_closure,
                        chromeos::MahiManager::MahiSummaryCallback callback) {
                       std::move(callback).Run(
                           u"fake summary",
                           chromeos::MahiResponseStatus::kSuccess);
                       std::move(unblock_closure).Run();
                     },
                     waiter.GetCallback(), std::move(callback)));
}

// Returns a long summary.
void ReturnLongSummary(chromeos::MahiManager::MahiSummaryCallback callback) {
  std::move(callback).Run(
      base::StrCat(std::vector<std::u16string>(100, u"Long Summary\n")),
      chromeos::MahiResponseStatus::kSuccess);
}

void PressEnter() {
  ui::test::EventGenerator(Shell::GetPrimaryRootWindow())
      .PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
}

}  // namespace

class MahiPanelViewTest : public AshTestBase {
 public:
  MockMahiManager& mock_mahi_manager() { return mock_mahi_manager_; }

  MockNewWindowDelegate& new_window_delegate() { return *new_window_delegate_; }

  MahiPanelView* panel_view() { return panel_view_; }

  views::Widget* widget() { return widget_.get(); }

 private:
  // AshTestBase:
  void SetUp() override {
    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(std::move(delegate));

    AshTestBase::SetUp();

    scoped_setter_ = std::make_unique<chromeos::ScopedMahiManagerSetter>(
        &mock_mahi_manager_);

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    panel_view_ = widget_->SetContentsView(std::make_unique<MahiPanelView>());
  }

  void TearDown() override {
    panel_view_ = nullptr;
    widget_.reset();
    scoped_setter_.reset();

    AshTestBase::TearDown();

    new_window_delegate_ = nullptr;
  }

  NiceMock<MockMahiManager> mock_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
  raw_ptr<MahiPanelView> panel_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MockNewWindowDelegate> new_window_delegate_;
  std::unique_ptr<TestNewWindowDelegateProvider> delegate_provider_;
};

// Verifies that the content title is correct when the panel is created.
TEST_F(MahiPanelViewTest, ContentTitle) {
  const std::u16string test_title1(u"test content title 1");
  ON_CALL(mock_mahi_manager(), GetContentTitle)
      .WillByDefault(Return(test_title1));

  MahiPanelView mahi_view1;
  const auto* const content_title_label1 = views::AsViewClass<views::Label>(
      mahi_view1.GetViewByID(mahi_constants::ViewId::kContentTitle));
  EXPECT_EQ(content_title_label1->GetText(), test_title1);

  const std::u16string test_title2(u"test content title 2");
  ON_CALL(mock_mahi_manager(), GetContentTitle)
      .WillByDefault(Return(test_title2));

  MahiPanelView mahi_view2;
  const auto* const content_title_label2 = views::AsViewClass<views::Label>(
      mahi_view2.GetViewByID(mahi_constants::ViewId::kContentTitle));
  EXPECT_EQ(content_title_label2->GetText(), test_title2);
}

// Verifies that the content icon is correct when the panel is created.
TEST_F(MahiPanelViewTest, ContentIcon) {
  const auto test_icon1 = gfx::test::CreateImageSkia(/*size=*/128, SK_ColorRED);
  ON_CALL(mock_mahi_manager(), GetContentIcon)
      .WillByDefault(Return(test_icon1));
  MahiPanelView mahi_view1;
  const auto* const content_icon1 = views::AsViewClass<views::ImageView>(
      mahi_view1.GetViewByID(mahi_constants::ViewId::kContentIcon));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*content_icon1->GetImage().bitmap(),
                                         *test_icon1.bitmap()));
  EXPECT_EQ(content_icon1->GetPreferredSize(),
            mahi_constants::kContentIconSize);

  const auto test_icon2 =
      gfx::test::CreateImageSkia(/*size=*/128, SK_ColorBLUE);
  ON_CALL(mock_mahi_manager(), GetContentIcon)
      .WillByDefault(Return(test_icon2));
  MahiPanelView mahi_view2;
  const auto* const content_icon2 = views::AsViewClass<views::ImageView>(
      mahi_view2.GetViewByID(mahi_constants::ViewId::kContentIcon));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*content_icon2->GetImage().bitmap(),
                                         *test_icon2.bitmap()));
  EXPECT_EQ(content_icon2->GetPreferredSize(),
            mahi_constants::kContentIconSize);
}

// Checks that the summary text is set correctly in ctor with different texts.
TEST_F(MahiPanelViewTest, SummaryText) {
  const std::u16string summary_text1(u"test summary text 1");
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([&summary_text1](
                         chromeos::MahiManager::MahiSummaryCallback callback) {
        std::move(callback).Run(summary_text1,
                                chromeos::MahiResponseStatus::kSuccess);
      });

  MahiPanelView mahi_view1;
  const auto* const summary_label1 = views::AsViewClass<views::Label>(
      mahi_view1.GetViewByID(mahi_constants::ViewId::kSummaryLabel));
  EXPECT_EQ(summary_text1, summary_label1->GetText());

  const std::u16string summary_text2(u"test summary text 2");
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([&summary_text2](
                         chromeos::MahiManager::MahiSummaryCallback callback) {
        std::move(callback).Run(summary_text2,
                                chromeos::MahiResponseStatus::kSuccess);
      });

  MahiPanelView mahi_view2;
  const auto* const summary_label2 = views::AsViewClass<views::Label>(
      mahi_view2.GetViewByID(mahi_constants::ViewId::kSummaryLabel));
  EXPECT_EQ(summary_text2, summary_label2->GetText());

  // Make sure the text is multiline and aligned correctly.
  EXPECT_TRUE(summary_label2->GetMultiLine());
  EXPECT_EQ(summary_label2->GetHorizontalAlignment(),
            gfx::HorizontalAlignment::ALIGN_LEFT);
}

TEST_F(MahiPanelViewTest, FeedbackButtons) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_mahi_manager(), OpenFeedbackDialog).Times(0);
  LeftClickOn(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsUpButton));
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 0);

  EXPECT_CALL(mock_mahi_manager(), OpenFeedbackDialog).Times(1);
  LeftClickOn(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsDownButton));
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 1);
}

TEST_F(MahiPanelViewTest, CloseButton) {
  EXPECT_FALSE(widget()->IsClosed());

  LeftClickOn(panel_view()->GetViewByID(mahi_constants::ViewId::kCloseButton));

  EXPECT_TRUE(widget()->IsClosed());
}

TEST_F(MahiPanelViewTest, LearnMoreLink) {
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(mahi_constants::kLearnMorePage),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  LeftClickOn(
      panel_view()->GetViewByID(mahi_constants::ViewId::kLearnMoreLink));
  Mock::VerifyAndClearExpectations(&new_window_delegate());
}

// TODO(b/330643995): Remove this test after outlines can be shown by default.
TEST_F(MahiPanelViewTest, OutlinesHiddenByDefault) {
  EXPECT_FALSE(
      panel_view()
          ->GetViewByID(mahi_constants::ViewId::kOutlinesSectionContainer)
          ->GetVisible());
}

// Make sure the `PanelContentsContainer` is larger than its contents when the
// contents are short.
TEST_F(MahiPanelViewTest, PanelContentsViewBoundsWithShortSummary) {
  ON_CALL(mock_mahi_manager(), GetContentTitle)
      .WillByDefault(Return(u"fake content title"));
  ON_CALL(mock_mahi_manager(), GetOutlines)
      .WillByDefault(ReturnDefaultOutlines);

  // Configure the mock manager to return a short summary.
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([](chromeos::MahiManager::MahiSummaryCallback callback) {
        std::move(callback).Run(/*summary=*/u"Short summary",
                                chromeos::MahiResponseStatus::kSuccess);
      });

  MahiPanelView mahi_view;

  // TODO(b/330643995): After outlines are shown by default, remove this since
  // we won't need to explicitly show the outlines section anymore.
  mahi_view.GetViewByID(mahi_constants::ViewId::kOutlinesSectionContainer)
      ->SetVisible(true);
  mahi_view.SetPreferredSize(gfx::Size(300, 400));
  mahi_view.SizeToPreferredSize();

  const int short_content_height =
      mahi_view.GetViewByID(mahi_constants::kSummaryOutlinesSection)
          ->bounds()
          .height();
  const int short_contents_container_height =
      mahi_view.GetViewByID(mahi_constants::kPanelContentsContainer)
          ->bounds()
          .height();

  // The container should be larger than the contents when the summary is short.
  EXPECT_GT(short_contents_container_height, short_content_height);
}

// Make sure the `PanelContentsContainer` is smaller than its contents when the
// contents are long.
TEST_F(MahiPanelViewTest, PanelContentsViewBoundsWithLongSummary) {
  ON_CALL(mock_mahi_manager(), GetContentTitle)
      .WillByDefault(Return(u"fake content title"));
  ON_CALL(mock_mahi_manager(), GetOutlines)
      .WillByDefault(ReturnDefaultOutlines);

  // Configure the mock manager to return a long summary.
  ON_CALL(mock_mahi_manager(), GetSummary).WillByDefault(ReturnLongSummary);

  MahiPanelView mahi_view;

  // TODO(b/330643995): After outlines are shown by default, remove this since
  // we won't need to explicitly show the outlines section anymore.
  mahi_view.GetViewByID(mahi_constants::ViewId::kOutlinesSectionContainer)
      ->SetVisible(true);
  mahi_view.SetPreferredSize(gfx::Size(300, 400));
  mahi_view.SizeToPreferredSize();

  const int long_content_height =
      mahi_view.GetViewByID(mahi_constants::kSummaryOutlinesSection)
          ->bounds()
          .height();
  const int long_contents_container_height =
      mahi_view.GetViewByID(mahi_constants::kPanelContentsContainer)
          ->bounds()
          .height();

  // The container should be smaller than the contents when the summary is long.
  EXPECT_LT(long_contents_container_height, long_content_height);
}

// Make sure the `PanelContentsContainer` is always sized to occupy the same
// amount of space in the `MahiPanelView` irrespective of its contents size.
TEST_F(MahiPanelViewTest, PanelContentsViewBoundsStayConstant) {
  // Create a panel with a short summary.
  constexpr gfx::Size kPanelBounds(/*width=*/300, /*height=*/400);
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([](chromeos::MahiManager::MahiSummaryCallback callback) {
        std::move(callback).Run(u"Short summary",
                                chromeos::MahiResponseStatus::kSuccess);
      });
  MahiPanelView mahi_view1;
  mahi_view1.SetPreferredSize(kPanelBounds);
  mahi_view1.SizeToPreferredSize();

  // Create another panel with a long summary.
  ON_CALL(mock_mahi_manager(), GetSummary).WillByDefault(ReturnLongSummary);
  MahiPanelView mahi_view2;
  mahi_view2.SetPreferredSize(kPanelBounds);
  mahi_view2.SizeToPreferredSize();

  const int short_contents_container_height =
      mahi_view1.GetViewByID(mahi_constants::kPanelContentsContainer)
          ->bounds()
          .height();
  const int long_contents_container_height =
      mahi_view2.GetViewByID(mahi_constants::kPanelContentsContainer)
          ->bounds()
          .height();

  // The container size should stay constant irrespective of summary length.
  EXPECT_EQ(short_contents_container_height, long_contents_container_height);
}

TEST_F(MahiPanelViewTest, LoadingAnimations) {
  // Config the mock mahi manager to return a summary asyncly.
  base::test::TestFuture<void> summary_waiter;
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([&summary_waiter](
                         chromeos::MahiManager::MahiSummaryCallback callback) {
        ReturnDefaultSummaryAsyncly(summary_waiter, std::move(callback));
      });

  // Config the mock mahi manager to return outlines asyncly.
  base::test::TestFuture<void> outlines_waiter;
  ON_CALL(mock_mahi_manager(), GetOutlines)
      .WillByDefault([&outlines_waiter](
                         chromeos::MahiManager::MahiOutlinesCallback callback) {
        ReturnDefaultOutlinesAsyncly(outlines_waiter, std::move(callback));
      });

  MahiPanelView mahi_view;

  // TODO(b/330643995): After outlines are shown by default, remove this since
  // we won't need to explicitly show the outlines section anymore.
  mahi_view.GetViewByID(mahi_constants::ViewId::kOutlinesSectionContainer)
      ->SetVisible(true);

  const auto* const summary_loading_animated_image = mahi_view.GetViewByID(
      mahi_constants::ViewId::kSummaryLoadingAnimatedImage);
  const auto* const outlines_loading_animated_image = mahi_view.GetViewByID(
      mahi_constants::ViewId::kOutlinesLoadingAnimatedImage);
  const auto* const summary_label =
      mahi_view.GetViewByID(mahi_constants::ViewId::kSummaryLabel);
  const auto* const outlines_container =
      mahi_view.GetViewByID(mahi_constants::ViewId::kOutlinesContainer);

  // Since the APIs that return summary & outlines are blocked, loading
  // animations should play.
  EXPECT_TRUE(summary_loading_animated_image->GetVisible());
  EXPECT_TRUE(outlines_loading_animated_image->GetVisible());
  EXPECT_FALSE(summary_label->GetVisible());
  EXPECT_FALSE(outlines_container->GetVisible());

  // Wait until summary is returned. Then check:
  // 1. The outlines loading animation is still playing.
  // 2. `summary_label` is visible.
  ASSERT_TRUE(summary_waiter.Wait());
  EXPECT_FALSE(summary_loading_animated_image->GetVisible());
  EXPECT_TRUE(outlines_loading_animated_image->GetVisible());
  EXPECT_TRUE(summary_label->GetVisible());
  EXPECT_FALSE(outlines_container->GetVisible());

  // Wait until outlines are returned. Both labels should be visible.
  ASSERT_TRUE(outlines_waiter.Wait());
  EXPECT_FALSE(summary_loading_animated_image->GetVisible());
  EXPECT_FALSE(outlines_loading_animated_image->GetVisible());
  EXPECT_TRUE(summary_label->GetVisible());
  EXPECT_TRUE(outlines_container->GetVisible());
}

// Tests that pressing on the send button with a valid textfield takes the user
// to the Q&A View and the back button takes the user back to the main view.
TEST_F(MahiPanelViewTest, TransitionToQuestionAnswerView) {
  auto* const summary_outlines_section = panel_view()->GetViewByID(
      mahi_constants::ViewId::kSummaryOutlinesSection);
  auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  auto* const back_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kBackButton);
  auto* const question_textfield = views::AsViewClass<SystemTextfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));

  // Assert that the views to be tested exist.
  ASSERT_TRUE(summary_outlines_section);
  ASSERT_TRUE(question_answer_view);
  ASSERT_TRUE(back_button);
  ASSERT_TRUE(send_button);
  ASSERT_TRUE(question_textfield);

  // Initially the Summary Outlines section is visible.
  EXPECT_TRUE(summary_outlines_section->GetVisible());
  EXPECT_FALSE(question_answer_view->GetVisible());
  EXPECT_FALSE(back_button->GetVisible());
  EXPECT_TRUE(send_button->GetVisible());
  EXPECT_TRUE(question_textfield->GetVisible());

  // Provide a valid input in the textfield so it can be sent as a question.
  question_textfield->SetText(u"input");

  // Pressing the send button with a valid input in the textfield should take
  // the user to the Q&A view.
  LeftClickOn(send_button);
  EXPECT_FALSE(summary_outlines_section->GetVisible());
  EXPECT_TRUE(question_answer_view->GetVisible());
  EXPECT_TRUE(back_button->GetVisible());
  EXPECT_TRUE(send_button->GetVisible());

  // Run layout so the back button updates its size and becomes clickable.
  views::test::RunScheduledLayout(widget());

  // Pressing the back button should take the user back to the main view.
  LeftClickOn(back_button);
  EXPECT_TRUE(summary_outlines_section->GetVisible());
  EXPECT_FALSE(question_answer_view->GetVisible());
  EXPECT_FALSE(back_button->GetVisible());
  EXPECT_TRUE(send_button->GetVisible());
}

// Tests that the question textfield accepts user input and creates a text
// bubble with the provided text by pressing the send button or enter.
TEST_F(MahiPanelViewTest, QuestionTextfield_CreateQuestion) {
  auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  auto* const question_textfield = views::AsViewClass<SystemTextfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));

  const std::u16string answer1(u"test answer1");
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [&answer1](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(answer1,
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  // Set a valid text in the question textfield.
  const std::u16string question1(u"question 1");
  question_textfield->SetText(question1);

  // Pressing the send button should create a question and answer text bubble.
  LeftClickOn(send_button);
  ASSERT_EQ(2u, question_answer_view->children().size());
  EXPECT_EQ(views::AsViewClass<views::Label>(
                question_answer_view->children()[0]->GetViewByID(
                    mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                ->GetText(),
            question1);
  EXPECT_EQ(views::AsViewClass<views::Label>(
                question_answer_view->children()[1]->GetViewByID(
                    mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                ->GetText(),
            answer1);

  // Textfield contents should be cleared after processing input.
  EXPECT_TRUE(question_textfield->GetText().empty());

  const std::u16string answer2(u"test answer2");
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [&answer2](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(answer2,
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  // Set another valid text in the question textfield.
  const std::u16string question2(u"question 2");
  question_textfield->SetText(question2);

  // Pressing the "Enter" key while the textfield is focused should create a
  // question and answer text bubble.
  question_textfield->RequestFocus();
  PressEnter();
  ASSERT_EQ(question_answer_view->children().size(), 4u);
  EXPECT_EQ(views::AsViewClass<views::Label>(
                question_answer_view->children()[2]->GetViewByID(
                    mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                ->GetText(),
            question2);
  EXPECT_EQ(views::AsViewClass<views::Label>(
                question_answer_view->children()[3]->GetViewByID(
                    mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                ->GetText(),
            answer2);

  // Textfield contents should be cleared after processing input.
  EXPECT_TRUE(question_textfield->GetText().empty());
}

// Tests that the question textfield does not process empty or blank inputs.
TEST_F(MahiPanelViewTest, QuestionTextfield_EmptyInput) {
  // Question textfield is initially empty.
  auto* const question_textfield = views::AsViewClass<SystemTextfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));
  ASSERT_TRUE(question_textfield);
  EXPECT_TRUE(question_textfield->GetText().empty());

  // Attempting to send an empty input should not process the text.
  auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  ASSERT_TRUE(send_button);
  LeftClickOn(send_button);
  const auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  ASSERT_TRUE(question_answer_view);
  EXPECT_TRUE(question_answer_view->children().empty());

  // Set a value of whitespace for the textfield.
  question_textfield->SetText(u"   ");
  EXPECT_FALSE(question_textfield->GetText().empty());

  // Attempting to send only whitespace should not process the text.
  LeftClickOn(send_button);
  EXPECT_TRUE(question_answer_view->children().empty());
}

// Tests that the question textfield trims whitespace from the front and back of
// the provided text.
TEST_F(MahiPanelViewTest, QuestionTextfield_TrimWhitespace) {
  // Set a text in the textfield with leading and trailing whitespace.
  auto* const question_textfield = views::AsViewClass<SystemTextfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));
  ASSERT_TRUE(question_textfield);
  question_textfield->SetText(u"   leading and trailing   ");

  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [](const std::u16string& question, bool current_panel_content,
             chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(/*answer=*/u"fake answer",
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  // Sending the text should create a question and answer text bubble.
  // The whitespace should be trimmed from the sides.
  auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  ASSERT_TRUE(send_button);
  LeftClickOn(send_button);
  auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  ASSERT_TRUE(question_answer_view);
  ASSERT_EQ(question_answer_view->children().size(), 2u);
  EXPECT_EQ(u"leading and trailing",
            views::AsViewClass<views::Label>(
                question_answer_view->children()[0]->GetViewByID(
                    mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                ->GetText());

  // Set a text in the textfield with whitespace between the string.
  const std::u16string question_text(u"whitespace     between");
  question_textfield->SetText(question_text);

  // Sending the text should create a question and answer text bubble.
  // The whitespace should not be trimmed if it's not on the sides.
  LeftClickOn(send_button);
  ASSERT_EQ(question_answer_view->children().size(), 4u);
  EXPECT_EQ(views::AsViewClass<views::Label>(
                question_answer_view->children()[2]->GetViewByID(
                    mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                ->GetText(),
            question_text);
}

}  // namespace ash
