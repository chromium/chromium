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
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/system_textfield.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/mahi_utils.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Aliases ---------------------------------------------------------------------

using chromeos::MahiResponseStatus;
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

// Returns a comprehensive list of potential errors from Mahi backend.
std::vector<MahiResponseStatus> GetMahiErrors() {
  std::vector<MahiResponseStatus> errors;
  for (size_t status_value = 0;
       status_value <= static_cast<size_t>(MahiResponseStatus::kMax);
       ++status_value) {
    MahiResponseStatus status = static_cast<MahiResponseStatus>(status_value);
    if (status != MahiResponseStatus::kSuccess) {
      errors.push_back(status);
    }
  }
  return errors;
}

void PressEnter() {
  ui::test::EventGenerator(Shell::GetPrimaryRootWindow())
      .PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EF_NONE);
}

// Returns an answer asyncly with the specified `status`. Use `waiter` to wait
// for the response.
void ReturnDefaultAnswerAsyncly(
    base::test::TestFuture<void>& waiter,
    MahiResponseStatus status,
    chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure unblock_closure, MahiResponseStatus status,
             chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(u"fake answer", status);
            std::move(unblock_closure).Run();
          },
          waiter.GetCallback(), status, std::move(callback)));
}

// Returns `kFakeOutlines` syncly.
void ReturnDefaultOutlines(
    chromeos::MahiManager::MahiOutlinesCallback callback) {
  std::move(callback).Run(/*outlines=*/kFakeOutlines,
                          MahiResponseStatus::kSuccess);
}

// Returns `kFakeOutlines` asyncly with the specified `status`. Use `waiter` to
// wait for the response.
void ReturnDefaultOutlinesAsyncly(
    base::test::TestFuture<void>& waiter,
    MahiResponseStatus status,
    chromeos::MahiManager::MahiOutlinesCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure unblock_closure, MahiResponseStatus status,
             chromeos::MahiManager::MahiOutlinesCallback callback) {
            std::move(callback).Run(kFakeOutlines, status);
            std::move(unblock_closure).Run();
          },
          waiter.GetCallback(), status, std::move(callback)));
}

// Returns a fake summary asyncly with the specified `status`. Use `waiter` to
// wait for the response.
void ReturnDefaultSummaryAsyncly(
    base::test::TestFuture<void>& waiter,
    MahiResponseStatus status,
    chromeos::MahiManager::MahiSummaryCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure unblock_closure, MahiResponseStatus status,
             chromeos::MahiManager::MahiSummaryCallback callback) {
            std::move(callback).Run(u"fake summary", status);
            std::move(unblock_closure).Run();
          },
          waiter.GetCallback(), status, std::move(callback)));
}

// Returns a long summary.
void ReturnLongSummary(chromeos::MahiManager::MahiSummaryCallback callback) {
  std::move(callback).Run(
      base::StrCat(std::vector<std::u16string>(100, u"Long Summary\n")),
      MahiResponseStatus::kSuccess);
}

views::Label* GetContentTitle(views::View* mahi_view) {
  return views::AsViewClass<views::Label>(
      mahi_view->GetViewByID(mahi_constants::ViewId::kContentTitle));
}

views::ImageView* GetContentIcon(views::View* mahi_view) {
  return views::AsViewClass<views::ImageView>(
      mahi_view->GetViewByID(mahi_constants::ViewId::kContentIcon));
}

views::Label* GetSummaryLabel(views::View* mahi_view) {
  return views::AsViewClass<views::Label>(
      mahi_view->GetViewByID(mahi_constants::ViewId::kSummaryLabel));
}

}  // namespace

class MahiPanelViewTest : public AshTestBase {
 public:
  MockMahiManager& mock_mahi_manager() { return mock_mahi_manager_; }

  MahiUiController* ui_controller() { return &ui_controller_; }

  MockNewWindowDelegate& new_window_delegate() { return *new_window_delegate_; }

  MahiPanelView* panel_view() { return panel_view_; }

  views::Widget* widget() { return widget_.get(); }

 protected:
  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kMahi);

    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(std::move(delegate));

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

    new_window_delegate_ = nullptr;
  }

  // Creates a widget hosting `MahiPanelView`. Recreates if there is one.
  void CreatePanelWidget() {
    // Avoid creating a dangling pointer.
    panel_view_ = nullptr;

    widget_.reset();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    panel_view_ = widget_->SetContentsView(
        std::make_unique<MahiPanelView>(&ui_controller_));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  NiceMock<MockMahiManager> mock_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
  MahiUiController ui_controller_;
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

  MahiPanelView mahi_view1(ui_controller());
  const auto* const content_title_label1 = views::AsViewClass<views::Label>(
      mahi_view1.GetViewByID(mahi_constants::ViewId::kContentTitle));
  EXPECT_EQ(content_title_label1->GetText(), test_title1);

  const std::u16string test_title2(u"test content title 2");
  ON_CALL(mock_mahi_manager(), GetContentTitle)
      .WillByDefault(Return(test_title2));

  MahiPanelView mahi_view2(ui_controller());
  const auto* const content_title_label2 = views::AsViewClass<views::Label>(
      mahi_view2.GetViewByID(mahi_constants::ViewId::kContentTitle));
  EXPECT_EQ(content_title_label2->GetText(), test_title2);
}

// Verifies that the content icon is correct when the panel is created.
TEST_F(MahiPanelViewTest, ContentIcon) {
  const auto test_icon1 = gfx::test::CreateImageSkia(/*size=*/128, SK_ColorRED);
  ON_CALL(mock_mahi_manager(), GetContentIcon)
      .WillByDefault(Return(test_icon1));
  MahiPanelView mahi_view1(ui_controller());
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
  MahiPanelView mahi_view2(ui_controller());
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
        std::move(callback).Run(summary_text1, MahiResponseStatus::kSuccess);
      });

  MahiPanelView mahi_view1(ui_controller());
  const auto* const summary_label1 = views::AsViewClass<views::Label>(
      mahi_view1.GetViewByID(mahi_constants::ViewId::kSummaryLabel));
  EXPECT_EQ(summary_text1, summary_label1->GetText());

  const std::u16string summary_text2(u"test summary text 2");
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([&summary_text2](
                         chromeos::MahiManager::MahiSummaryCallback callback) {
        std::move(callback).Run(summary_text2, MahiResponseStatus::kSuccess);
      });

  MahiPanelView mahi_view2(ui_controller());
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
  auto* learn_more_link =
      panel_view()->GetViewByID(mahi_constants::ViewId::kLearnMoreLink);
  // TODO(b/333111220): Remove this when the link is visible by default.
  learn_more_link->SetVisible(true);
  // Run layout so the link updates its size and becomes clickable.
  views::test::RunScheduledLayout(widget());

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(mahi_constants::kLearnMorePage),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  LeftClickOn(learn_more_link);
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
                                MahiResponseStatus::kSuccess);
      });

  MahiPanelView mahi_view(ui_controller());

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

  MahiPanelView mahi_view(ui_controller());

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
        std::move(callback).Run(u"Short summary", MahiResponseStatus::kSuccess);
      });
  MahiPanelView mahi_view1(ui_controller());
  mahi_view1.SetPreferredSize(kPanelBounds);
  mahi_view1.SizeToPreferredSize();

  // Create another panel with a long summary.
  ON_CALL(mock_mahi_manager(), GetSummary).WillByDefault(ReturnLongSummary);
  MahiPanelView mahi_view2(ui_controller());
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
        ReturnDefaultSummaryAsyncly(
            summary_waiter, MahiResponseStatus::kSuccess, std::move(callback));
      });

  // Config the mock mahi manager to return outlines asyncly.
  base::test::TestFuture<void> outlines_waiter;
  ON_CALL(mock_mahi_manager(), GetOutlines)
      .WillByDefault([&outlines_waiter](
                         chromeos::MahiManager::MahiOutlinesCallback callback) {
        ReturnDefaultOutlinesAsyncly(
            outlines_waiter, MahiResponseStatus::kSuccess, std::move(callback));
      });

  MahiPanelView mahi_view(ui_controller());

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
  // TODO(b/330643995): Expect TRUE after outlines are shown by default.
  EXPECT_FALSE(outlines_container->GetVisible());
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
  auto* const question_textfield = views::AsViewClass<views::Textfield>(
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

TEST_F(MahiPanelViewTest, ScrollViewContentsDynamicSize) {
  auto* const summary_outlines_section = panel_view()->GetViewByID(
      mahi_constants::ViewId::kSummaryOutlinesSection);
  auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  auto* const scroll_view_contents =
      panel_view()->GetViewByID(mahi_constants::ViewId::kScrollViewContents);

  // Make sure the views have different size and their heights exceed the
  // visible rect's height of the scroll view.
  summary_outlines_section->SetPreferredSize(gfx::Size(80, 300));
  question_answer_view->SetPreferredSize(gfx::Size(80, 600));
  views::test::RunScheduledLayout(widget());

  EXPECT_TRUE(summary_outlines_section->GetVisible());
  EXPECT_FALSE(question_answer_view->GetVisible());
  EXPECT_EQ(summary_outlines_section->height() +
                mahi_constants::kScrollContentsViewBottomPadding,
            scroll_view_contents->GetPreferredSize().height());

  auto* const question_textfield = views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));
  question_textfield->SetText(u"input");

  // Transition to Q&A view. Scroll view should change its preferred height (the
  // height that the view will take when it is not constrained by `ScrollView`).
  LeftClickOn(panel_view()->GetViewByID(
      mahi_constants::ViewId::kAskQuestionSendButton));

  // Run layout so the views update their size.
  views::test::RunScheduledLayout(widget());

  EXPECT_FALSE(summary_outlines_section->GetVisible());
  EXPECT_TRUE(question_answer_view->GetVisible());

  EXPECT_EQ(question_answer_view->height() +
                mahi_constants::kScrollContentsViewBottomPadding,
            scroll_view_contents->GetPreferredSize().height());

  // Transition back to summary outlines view. Scroll view should change its
  // preferred height.(the height that the view will take when it is not
  // constrained by `ScrollView`).
  LeftClickOn(panel_view()->GetViewByID(mahi_constants::ViewId::kBackButton));

  views::test::RunScheduledLayout(widget());

  EXPECT_TRUE(summary_outlines_section->GetVisible());
  EXPECT_FALSE(question_answer_view->GetVisible());

  EXPECT_EQ(summary_outlines_section->height() +
                mahi_constants::kScrollContentsViewBottomPadding,
            scroll_view_contents->GetPreferredSize().height());
}

// Tests that the question textfield accepts user input and creates a text
// bubble with the provided text by pressing the send button or enter.
TEST_F(MahiPanelViewTest, QuestionTextfield_CreateQuestion) {
  auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  auto* const question_textfield = views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));

  const std::u16string answer1(u"test answer1");
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [&answer1](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(answer1, MahiResponseStatus::kSuccess);
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
            std::move(callback).Run(answer2, MahiResponseStatus::kSuccess);
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

// Tests that the question textfield does not send requests to the manager while
// it is waiting to load an answer.
TEST_F(MahiPanelViewTest, QuestionTextfield_InputDisabledWhileLoadingAnswer) {
  // Send button is initially enabled.
  const auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  ASSERT_TRUE(send_button);
  EXPECT_TRUE(send_button->GetEnabled());

  // Config the mock mahi manager to return an answer asyncly.
  base::test::TestFuture<void> answer_waiter;
  EXPECT_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillOnce(
          [&answer_waiter](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            ReturnDefaultAnswerAsyncly(answer_waiter,
                                       MahiResponseStatus::kSuccess,
                                       std::move(callback));
          });

  // Set up the textfield to have a valid input.
  auto* const question_textfield = views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));
  ASSERT_TRUE(question_textfield);
  const std::u16string question(u"fake question");
  question_textfield->SetText(question);

  // After a question is posted and before an answer is loaded, the send button
  // should be disabled.
  LeftClickOn(send_button);
  EXPECT_FALSE(send_button->GetEnabled());

  // Set up the textfield to have a valid input again.
  question_textfield->SetText(question);

  // Attempt sending a question while loading. It should not be processed either
  // by attempting to press the send button or by pressing `Enter`.
  LeftClickOn(send_button);
  question_textfield->RequestFocus();
  PressEnter();

  // Wait until an answer is loaded. Send button should be enabled again.
  ASSERT_TRUE(answer_waiter.Wait());
  EXPECT_TRUE(send_button->GetEnabled());
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  // Attempting to send now should process the new input.
  EXPECT_CALL(mock_mahi_manager(), AnswerQuestion);
  PressEnter();
  EXPECT_FALSE(send_button->GetEnabled());
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());
}

// Tests that the question textfield does not process empty or blank inputs.
TEST_F(MahiPanelViewTest, QuestionTextfield_EmptyInput) {
  // Question textfield is initially empty.
  auto* const question_textfield = views::AsViewClass<views::Textfield>(
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
  auto* const question_textfield = views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));
  ASSERT_TRUE(question_textfield);
  question_textfield->SetText(u"   leading and trailing   ");

  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [](const std::u16string& question, bool current_panel_content,
             chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(/*answer=*/u"fake answer",
                                    MahiResponseStatus::kSuccess);
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

// Verifies the mahi panel view when loading an answer with an error by
// iterating all possible errors.
TEST_F(MahiPanelViewTest, FailToGetAnswer) {
  for (MahiResponseStatus error : GetMahiErrors()) {
    if (error == MahiResponseStatus::kInappropriate ||
        error == MahiResponseStatus::kLowQuota) {
      // `kInappropriate` introduced by a question is presented in the Q&A view,
      // verified in its own test. `kLowQuota` triggers a warning verified in
      // its own test.
      continue;
    }

    // Config the mock mahi manager to return answer with an `error` asyncly.
    base::test::TestFuture<void> answer_waiter;
    EXPECT_CALL(mock_mahi_manager(), AnswerQuestion)
        .WillOnce(
            [&answer_waiter, error](
                const std::u16string& question, bool current_panel_content,
                chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
              ReturnDefaultAnswerAsyncly(answer_waiter, error,
                                         std::move(callback));
            });

    auto* const question_textfield = views::AsViewClass<views::Textfield>(
        panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));
    ASSERT_TRUE(question_textfield);
    question_textfield->SetText(u"fake question");

    auto* const send_button = panel_view()->GetViewByID(
        mahi_constants::ViewId::kAskQuestionSendButton);
    ASSERT_TRUE(send_button);
    LeftClickOn(send_button);
    Mock::VerifyAndClearExpectations(&mock_mahi_manager());

    // After a question is posted and before an answer is loaded, the Q&A view
    // should show.
    const auto* const question_answer_view =
        panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
    CHECK(question_answer_view);
    EXPECT_TRUE(question_answer_view->GetVisible());

    const auto* const summary_outlines_section = panel_view()->GetViewByID(
        mahi_constants::ViewId::kSummaryOutlinesSection);
    CHECK(summary_outlines_section);
    EXPECT_FALSE(summary_outlines_section->GetVisible());

    const auto* const error_status_view =
        panel_view()->GetViewByID(mahi_constants::ViewId::kErrorStatusView);
    CHECK(error_status_view);
    EXPECT_FALSE(error_status_view->GetVisible());

    const auto* const error_status_label = views::AsViewClass<views::Label>(
        panel_view()->GetViewByID(mahi_constants::ViewId::kErrorStatusLabel));
    CHECK(error_status_label);
    EXPECT_TRUE(error_status_label->GetText().empty());

    // Wait until an answer is loaded with an error. Verify views' visibility.
    ASSERT_TRUE(answer_waiter.Wait());
    EXPECT_FALSE(question_answer_view->GetVisible());
    EXPECT_FALSE(summary_outlines_section->GetVisible());
    EXPECT_TRUE(error_status_view->GetVisible());

    // Check the contents of `error_status_label`.
    EXPECT_EQ(
        error_status_label->GetText(),
        l10n_util::GetStringUTF16(mahi_utils::GetErrorStatusViewTextId(error)));

    CreatePanelWidget();
  }
}

// Verifies the mahi panel view when loading outlines with an error by
// iterating all possible errors.
TEST_F(MahiPanelViewTest, FailToGetOutlines) {
  for (MahiResponseStatus error : GetMahiErrors()) {
    if (error == MahiResponseStatus::kLowQuota) {
      // `kLowQuota` triggers a warning verified in its own test.
      continue;
    }

    // Config the mock mahi manager to return outlines with an `error` asyncly.
    base::test::TestFuture<void> outlines_waiter;
    EXPECT_CALL(mock_mahi_manager(), GetOutlines)
        .WillOnce([&outlines_waiter, error](
                      chromeos::MahiManager::MahiOutlinesCallback callback) {
          ReturnDefaultOutlinesAsyncly(outlines_waiter, error,
                                       std::move(callback));
        });

    CreatePanelWidget();
    Mock::VerifyAndClear(&mock_mahi_manager());

    // Before outlines are loaded with an error, the summary & outlines section
    // should show.
    const auto* const question_answer_view =
        panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
    CHECK(question_answer_view);
    EXPECT_FALSE(question_answer_view->GetVisible());

    const auto* const summary_outlines_section = panel_view()->GetViewByID(
        mahi_constants::ViewId::kSummaryOutlinesSection);
    CHECK(summary_outlines_section);
    EXPECT_TRUE(summary_outlines_section->GetVisible());

    const auto* const error_status_view =
        panel_view()->GetViewByID(mahi_constants::ViewId::kErrorStatusView);
    CHECK(error_status_view);
    EXPECT_FALSE(error_status_view->GetVisible());

    const auto* const error_status_label = views::AsViewClass<views::Label>(
        panel_view()->GetViewByID(mahi_constants::ViewId::kErrorStatusLabel));
    CHECK(error_status_label);
    EXPECT_TRUE(error_status_label->GetText().empty());

    // Wait until outlines are loaded with an error. Verify views' visibility.
    ASSERT_TRUE(outlines_waiter.Wait());
    EXPECT_FALSE(question_answer_view->GetVisible());
    EXPECT_FALSE(summary_outlines_section->GetVisible());
    EXPECT_TRUE(error_status_view->GetVisible());

    // Check the contents of `error_status_label`.
    EXPECT_EQ(
        error_status_label->GetText(),
        l10n_util::GetStringUTF16(mahi_utils::GetErrorStatusViewTextId(error)));
  }
}

// Verifies the mahi panel view when loading summary with an error by iterating
// all possible errors.
TEST_F(MahiPanelViewTest, FailToGetSummary) {
  for (MahiResponseStatus error : GetMahiErrors()) {
    if (error == MahiResponseStatus::kLowQuota) {
      // `kLowQuota` triggers a warning verified in its own test.
      continue;
    }

    // Config the mock mahi manager to return a summary with an `error` asyncly.
    base::test::TestFuture<void> summary_waiter;
    EXPECT_CALL(mock_mahi_manager(), GetSummary)
        .WillOnce([&summary_waiter,
                   error](chromeos::MahiManager::MahiSummaryCallback callback) {
          ReturnDefaultSummaryAsyncly(summary_waiter, error,
                                      std::move(callback));
        });

    CreatePanelWidget();
    Mock::VerifyAndClear(&mock_mahi_manager());

    // Before the summary is loaded with an error, the summary & outlines
    // section should show.
    const auto* const question_answer_view =
        panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
    CHECK(question_answer_view);
    EXPECT_FALSE(question_answer_view->GetVisible());

    const auto* const summary_outlines_section = panel_view()->GetViewByID(
        mahi_constants::ViewId::kSummaryOutlinesSection);
    CHECK(summary_outlines_section);
    EXPECT_TRUE(summary_outlines_section->GetVisible());

    const auto* const error_status_view =
        panel_view()->GetViewByID(mahi_constants::ViewId::kErrorStatusView);
    CHECK(error_status_view);
    EXPECT_FALSE(error_status_view->GetVisible());

    const auto* const error_status_label = views::AsViewClass<views::Label>(
        panel_view()->GetViewByID(mahi_constants::ViewId::kErrorStatusLabel));
    CHECK(error_status_label);
    EXPECT_TRUE(error_status_label->GetText().empty());

    // Wait until the summary is loaded with an error. Verify views' visibility.
    ASSERT_TRUE(summary_waiter.Wait());
    EXPECT_FALSE(question_answer_view->GetVisible());
    EXPECT_FALSE(summary_outlines_section->GetVisible());
    EXPECT_TRUE(error_status_view->GetVisible());

    // Check the contents of `error_status_label`.
    EXPECT_EQ(
        error_status_label->GetText(),
        l10n_util::GetStringUTF16(mahi_utils::GetErrorStatusViewTextId(error)));
  }
}

// Tests that calling `RefreshSummaryContents` will update the panel's contents
// with the new data from the manager.
TEST_F(MahiPanelViewTest, RefreshSummaryContents) {
  const std::u16string title1(u"Test content title");
  const std::u16string summary1(u"Short summary");
  const auto icon1(gfx::test::CreateImageSkia(/*size=*/128, SK_ColorBLUE));

  ON_CALL(mock_mahi_manager(), GetContentTitle).WillByDefault(Return(title1));
  ON_CALL(mock_mahi_manager(), GetContentIcon).WillByDefault(Return(icon1));
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault(
          [&summary1](chromeos::MahiManager::MahiSummaryCallback callback) {
            std::move(callback).Run(summary1,
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  MahiPanelView mahi_view(ui_controller());

  EXPECT_EQ(GetContentTitle(&mahi_view)->GetText(), title1);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *GetContentIcon(&mahi_view)->GetImage().bitmap(), *icon1.bitmap()));
  EXPECT_EQ(GetSummaryLabel(&mahi_view)->GetText(), summary1);

  const std::u16string title2(u"Test content title 2");
  const std::u16string summary2(u"Short summary 2");
  const auto icon2(gfx::test::CreateImageSkia(/*size=*/128, SK_ColorRED));

  ON_CALL(mock_mahi_manager(), GetContentTitle).WillByDefault(Return(title2));
  ON_CALL(mock_mahi_manager(), GetContentIcon).WillByDefault(Return(icon2));
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault(
          [&summary2](chromeos::MahiManager::MahiSummaryCallback callback) {
            std::move(callback).Run(summary2,
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  ui_controller()->RefreshContents();

  EXPECT_EQ(GetContentTitle(&mahi_view)->GetText(), title2);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *GetContentIcon(&mahi_view)->GetImage().bitmap(), *icon2.bitmap()));
  EXPECT_EQ(GetSummaryLabel(&mahi_view)->GetText(), summary2);
}

// Tests that refreshing Summary contents will bring the user to the Summary
// View and clear all previously added Q&A text bubbles.
TEST_F(MahiPanelViewTest, RefreshSummaryContents_TransitionToSummaryView) {
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [](const std::u16string& question, bool current_panel_content,
             chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(u"answer",
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  const auto* const summary_outlines_section = panel_view()->GetViewByID(
      mahi_constants::ViewId::kSummaryOutlinesSection);
  const auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  const auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  auto* const question_textfield = views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));

  // Transition to Q&A view by asking a question.
  question_textfield->SetText(u"question");
  LeftClickOn(send_button);
  EXPECT_FALSE(summary_outlines_section->GetVisible());
  EXPECT_TRUE(question_answer_view->GetVisible());
  EXPECT_EQ(question_answer_view->children().size(), 2u);

  // Refreshing summary contents should clear all Q&A contents and transition to
  // the summary view.
  ui_controller()->RefreshContents();
  EXPECT_TRUE(summary_outlines_section->GetVisible());
  EXPECT_FALSE(question_answer_view->GetVisible());
  EXPECT_TRUE(question_answer_view->children().empty());
}

// Verifies that the error introduced by an inappropriate question is presented
// as expected.
TEST_F(MahiPanelViewTest, InappropriateQuestionError) {
  // Config the mock mahi manager to return `MahiResponseStatus::kInappropriate`
  // when handling a question.
  base::test::TestFuture<void> answer_waiter;
  EXPECT_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillOnce(
          [&answer_waiter](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            ReturnDefaultAnswerAsyncly(answer_waiter,
                                       MahiResponseStatus::kInappropriate,
                                       std::move(callback));
          });

  auto* const question_textfield = views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));
  ASSERT_TRUE(question_textfield);
  question_textfield->SetText(u"fake question");

  const auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  ASSERT_TRUE(send_button);
  LeftClickOn(send_button);
  EXPECT_FALSE(send_button->GetEnabled());
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  // After a question is posted and before an answer is loaded:
  // 1. The Q&A view should show.
  // 2. The error image/label should not exist.
  const auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  CHECK(question_answer_view);
  EXPECT_TRUE(question_answer_view->GetVisible());
  EXPECT_FALSE(panel_view()->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerErrorImage));
  EXPECT_FALSE(panel_view()->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerErrorLabel));

  // Wait for the answer to be loaded. Verify:
  // 1. `question_answer_view` shows.
  // 2. `error_image_view` shows.
  // 3. `error_label_view` shows with the expected label.
  // 4. `send_button` is re-enabled.

  ASSERT_TRUE(answer_waiter.WaitAndClear());
  EXPECT_TRUE(question_answer_view->GetVisible());

  const auto* const error_image_view = panel_view()->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerErrorImage);
  ASSERT_TRUE(error_image_view);
  EXPECT_TRUE(error_image_view->GetVisible());

  const auto* const error_label_view =
      views::AsViewClass<views::Label>(panel_view()->GetViewByID(
          mahi_constants::ViewId::kQuestionAnswerErrorLabel));
  ASSERT_TRUE(error_label_view);
  EXPECT_TRUE(error_label_view->GetVisible());
  EXPECT_EQ(error_label_view->GetText(),
            l10n_util::GetStringUTF16(
                IDS_ASH_MAHI_RESPONSE_STATUS_INAPPROPRIATE_LABEL_TEXT));

  EXPECT_TRUE(send_button->GetEnabled());

  // Config the mock mahi manager to return an answer in success.
  EXPECT_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillOnce(
          [&answer_waiter](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            ReturnDefaultAnswerAsyncly(answer_waiter,
                                       MahiResponseStatus::kSuccess,
                                       std::move(callback));
          });

  // Ask another question.
  question_textfield->SetText(u"A new question");
  LeftClickOn(send_button);
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  // Before the answer loaded, both the error image view and the error label
  // view should not exist since asking a new question should remove the error
  // introduced by the previous question.
  EXPECT_FALSE(panel_view()->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerErrorImage));
  EXPECT_FALSE(panel_view()->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerErrorLabel));

  // Wait for the answer to load. Both the error image view and the error label
  // view should not exist since the answer is loaded in success.
  ASSERT_TRUE(answer_waiter.Wait());
  EXPECT_TRUE(question_answer_view->GetVisible());
  EXPECT_FALSE(panel_view()->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerErrorImage));
  EXPECT_FALSE(panel_view()->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerErrorLabel));
}

TEST_F(MahiPanelViewTest, ClickMetrics) {
  base::HistogramTester histogram;
  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kCloseButton, 0);
  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kLearnMoreLink, 0);
  histogram.ExpectBucketCount(
      mahi_constants::kMahiButtonClickHistogramName,
      mahi_constants::PanelButton::kAskQuestionSendButton, 0);
  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kBackButton, 0);

  // Learn more button.
  LeftClickOn(
      panel_view()->GetViewByID(mahi_constants::ViewId::kLearnMoreLink));
  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kLearnMoreLink, 1);
  histogram.ExpectTotalCount(mahi_constants::kMahiButtonClickHistogramName, 1);

  auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  auto* const back_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kBackButton);
  auto* const question_textfield = views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));

  // Send question button.
  // Should not send question when the question text is empty.
  views::test::RunScheduledLayout(widget());
  LeftClickOn(send_button);
  histogram.ExpectBucketCount(
      mahi_constants::kMahiButtonClickHistogramName,
      mahi_constants::PanelButton::kAskQuestionSendButton, 0);
  histogram.ExpectTotalCount(mahi_constants::kMahiButtonClickHistogramName, 1);
  // Should send question when the question text is not empty.
  EXPECT_FALSE(back_button->GetVisible());
  const std::u16string question(u"question text");
  question_textfield->SetText(question);
  LeftClickOn(send_button);
  histogram.ExpectBucketCount(
      mahi_constants::kMahiButtonClickHistogramName,
      mahi_constants::PanelButton::kAskQuestionSendButton, 1);
  histogram.ExpectTotalCount(mahi_constants::kMahiButtonClickHistogramName, 2);

  // Now the back button is visible.
  EXPECT_TRUE(back_button->GetVisible());

  // Back button.
  views::test::RunScheduledLayout(widget());
  LeftClickOn(back_button);
  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kBackButton, 1);
  histogram.ExpectTotalCount(mahi_constants::kMahiButtonClickHistogramName, 3);

  // Close button.
  views::test::RunScheduledLayout(widget());
  LeftClickOn(panel_view()->GetViewByID(mahi_constants::ViewId::kCloseButton));
  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kCloseButton, 1);
  histogram.ExpectTotalCount(mahi_constants::kMahiButtonClickHistogramName, 4);
}

}  // namespace ash
