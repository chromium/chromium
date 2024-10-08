// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/url_constants.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_textfield.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_content_source_button.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ash/system/mahi/mahi_utils.h"
#include "ash/system/mahi/test/mahi_test_util.h"
#include "ash/system/mahi/test/mock_mahi_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Aliases ---------------------------------------------------------------------

using chromeos::MahiResponseStatus;
using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;

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
// NOTE: `MahiResponseStatus::kLowQuota` is a warning instead of an error.
std::vector<MahiResponseStatus> GetMahiErrors() {
  std::vector<MahiResponseStatus> errors;
  for (size_t status_value = 0;
       status_value <= static_cast<size_t>(MahiResponseStatus::kMaxValue);
       ++status_value) {
    MahiResponseStatus status = static_cast<MahiResponseStatus>(status_value);
    if (status != MahiResponseStatus::kSuccess &&
        status != MahiResponseStatus::kLowQuota) {
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
    chromeos::MahiManager::MahiAnswerQuestionCallback callback,
    base::TimeDelta delay = base::TimeDelta()) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure unblock_closure, MahiResponseStatus status,
             chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(u"fake answer", status);
            std::move(unblock_closure).Run();
          },
          waiter.GetCallback(), status, std::move(callback)),
      delay);
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
            std::move(callback).Run(mahi_test_util::GetDefaultFakeOutlines(),
                                    status);
            std::move(unblock_closure).Run();
          },
          waiter.GetCallback(), status, std::move(callback)));
}

// Returns a fake summary asyncly with the specified `status`. Use `waiter` to
// wait for the response.
void ReturnDefaultSummaryAsyncly(
    base::test::TestFuture<void>& waiter,
    MahiResponseStatus status,
    chromeos::MahiManager::MahiSummaryCallback callback,
    base::TimeDelta delay = base::TimeDelta()) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure unblock_closure, MahiResponseStatus status,
             chromeos::MahiManager::MahiSummaryCallback callback) {
            std::move(callback).Run(u"fake summary", status);
            std::move(unblock_closure).Run();
          },
          waiter.GetCallback(), status, std::move(callback)),
      delay);
}

// Returns a long summary.
void ReturnLongSummary(chromeos::MahiManager::MahiSummaryCallback callback) {
  std::move(callback).Run(
      base::StrCat(std::vector<std::u16string>(100, u"Long Summary\n")),
      MahiResponseStatus::kSuccess);
}

const std::u16string& GetContentSourceTitle(views::View* mahi_view) {
  return views::AsViewClass<MahiContentSourceButton>(
             mahi_view->GetViewByID(
                 mahi_constants::ViewId::kContentSourceButton))
      ->GetText();
}

gfx::ImageSkia GetContentSourceIcon(views::View* mahi_view) {
  return views::AsViewClass<MahiContentSourceButton>(
             mahi_view->GetViewByID(
                 mahi_constants::ViewId::kContentSourceButton))
      ->GetImage(views::Button::STATE_NORMAL);
}

views::Label* GetSummaryLabel(views::View* mahi_view) {
  return views::AsViewClass<views::Label>(
      mahi_view->GetViewByID(mahi_constants::ViewId::kSummaryLabel));
}

// Generates a random string, given the maximum amount of words the string can
// have.
std::u16string GetRandomString(int max_words_count) {
  int string_length = base::RandInt(1, max_words_count);
  std::vector<char> random_chars;
  for (int string_index = 0; string_index < string_length; string_index++) {
    int word_length = base::RandInt(1, 10);
    for (int word_index = 0; word_index < word_length; word_index++) {
      // Add a random character from 'a' to 'z' to the string.
      random_chars.push_back(base::RandInt('a', 'z'));
    }

    // Add a space between each word.
    random_chars.push_back(0x20);
  }

  return std::u16string(random_chars.begin(), random_chars.end());
}

}  // namespace

class MahiPanelViewTest : public AshTestBase {
 public:
  MahiPanelViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  MockMahiManager& mock_mahi_manager() { return mock_mahi_manager_; }

  MahiUiController* ui_controller() { return &ui_controller_; }

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

  MahiPanelView* panel_view() { return panel_view_; }

  views::Widget* widget() { return widget_.get(); }

 protected:
  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kMahi,
                              chromeos::features::kFeatureManagementMahi},
        /*disabled_features=*/{});

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

  // Creates a widget hosting `MahiPanelView`. Recreates if there is one.
  void CreatePanelWidget() {
    ResetPanelWidget();
    widget_ = CreateFramelessTestWidget();
    widget_->SetBounds(
        gfx::Rect(/*x=*/0, /*y=*/0,
                  /*width=*/mahi_constants::kPanelDefaultWidth,
                  /*height=*/mahi_constants::kPanelDefaultHeight));
    panel_view_ = widget_->SetContentsView(
        std::make_unique<MahiPanelView>(&ui_controller_));
  }

  void ResetPanelWidget() {
    // Avoid creating a dangling pointer.
    panel_view_ = nullptr;

    widget_.reset();
  }

  // Submit a test question by setting the text in the textfield and click send
  // button.
  void SubmitTestQuestion(const std::u16string& question = u"fake question") {
    auto* const question_textfield = views::AsViewClass<views::Textfield>(
        panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));
    ASSERT_TRUE(question_textfield);
    question_textfield->SetText(question);

    auto* const send_button = panel_view()->GetViewByID(
        mahi_constants::ViewId::kAskQuestionSendButton);
    ASSERT_TRUE(send_button);
    LeftClickOn(send_button);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  NiceMock<MockMahiManager> mock_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
  MahiUiController ui_controller_;
  raw_ptr<MahiPanelView> panel_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  MockNewWindowDelegate new_window_delegate_;
};

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

TEST_F(MahiPanelViewTest, ThumbsUpFeedbackButton) {
  base::HistogramTester histogram_tester;

  // Pressing thumbs up should toggle the button on and update the feedback
  // histogram.
  EXPECT_CALL(mock_mahi_manager(), OpenFeedbackDialog).Times(0);
  IconButton* thumbs_up_button = views::AsViewClass<IconButton>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsUpButton));
  LeftClickOn(thumbs_up_button);
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  EXPECT_TRUE(thumbs_up_button->toggled());
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 0);

  // Pressing thumbs up again should just toggle the button off.
  EXPECT_CALL(mock_mahi_manager(), OpenFeedbackDialog).Times(0);
  LeftClickOn(thumbs_up_button);
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  EXPECT_FALSE(thumbs_up_button->toggled());
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 0);

  // Pressing thumbs up should toggle the button on and update the histogram
  // again.
  EXPECT_CALL(mock_mahi_manager(), OpenFeedbackDialog).Times(0);
  LeftClickOn(thumbs_up_button);
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  EXPECT_TRUE(thumbs_up_button->toggled());
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 2);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 0);
}

TEST_F(MahiPanelViewTest, ThumbsDownFeedbackButton) {
  base::HistogramTester histogram_tester;

  // Pressing thumbs down the first time should open the feedback dialog, toggle
  // the button off and update the feedback histogram.
  EXPECT_CALL(mock_mahi_manager(), OpenFeedbackDialog).Times(1);
  IconButton* thumbs_down_button = views::AsViewClass<IconButton>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsDownButton));
  LeftClickOn(thumbs_down_button);
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  EXPECT_TRUE(thumbs_down_button->toggled());
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 0);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 1);

  // Pressing thumbs down again should just toggle the button off.
  EXPECT_CALL(mock_mahi_manager(), OpenFeedbackDialog).Times(0);
  LeftClickOn(thumbs_down_button);
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  EXPECT_FALSE(thumbs_down_button->toggled());
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 0);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 1);

  // Pressing thumbs down should toggle the button on and update the histogram
  // again.
  EXPECT_CALL(mock_mahi_manager(), OpenFeedbackDialog).Times(0);
  LeftClickOn(thumbs_down_button);
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  EXPECT_TRUE(thumbs_down_button->toggled());
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 0);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 2);
}

TEST_F(MahiPanelViewTest, CloseButton) {
  EXPECT_FALSE(widget()->IsClosed());

  LeftClickOn(panel_view()->GetViewByID(mahi_constants::ViewId::kCloseButton));

  EXPECT_TRUE(widget()->IsClosed());
}

TEST_F(MahiPanelViewTest, LearnMoreLink) {
  auto* learn_more_link =
      panel_view()->GetViewByID(mahi_constants::ViewId::kLearnMoreLink);
  // Run layout so the link updates its size and becomes clickable.
  views::test::RunScheduledLayout(widget());

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(chrome::kHelpMeReadWriteLearnMoreURL),
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
      .WillByDefault(mahi_test_util::ReturnDefaultOutlines);

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
      .WillByDefault(mahi_test_util::ReturnDefaultOutlines);

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
  ResetPanelWidget();
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

TEST_F(MahiPanelViewTest, SummaryLoadingAnimationsMetricsRecord) {
  // Reset the default panel to avoid unnecessary histogram record.
  ResetPanelWidget();

  base::HistogramTester histogram_tester;
  auto delay_time = base::Milliseconds(100);

  // Config the mock mahi manager to return a summary asyncly.
  base::test::TestFuture<void> summary_waiter;
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault([&summary_waiter, delay_time](
                         chromeos::MahiManager::MahiSummaryCallback callback) {
        ReturnDefaultSummaryAsyncly(
            summary_waiter, MahiResponseStatus::kSuccess, std::move(callback),
            /*delay=*/delay_time);
      });

  MahiPanelView mahi_view(ui_controller());

  histogram_tester.ExpectTimeBucketCount(
      mahi_constants::kSummaryLoadingTimeHistogramName, delay_time, 0);

  // Test that loading time metrics is recorded when summary is loaded.
  ASSERT_TRUE(summary_waiter.WaitAndClear());
  histogram_tester.ExpectTimeBucketCount(
      mahi_constants::kSummaryLoadingTimeHistogramName, delay_time, 1);

  // Test that loading time metrics is recorded when the content is refreshed.
  ui_controller()->RefreshContents();
  ASSERT_TRUE(summary_waiter.Wait());
  histogram_tester.ExpectTimeBucketCount(
      mahi_constants::kSummaryLoadingTimeHistogramName, delay_time, 2);
}

TEST_F(MahiPanelViewTest, AnswerLoadingAnimationsMetricsRecord) {
  base::HistogramTester histogram_tester;
  auto delay_time = base::Milliseconds(100);

  // Config the mock mahi manager to return an answer asyncly.
  base::test::TestFuture<void> answer_waiter;
  EXPECT_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillOnce(
          [&answer_waiter, delay_time](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            ReturnDefaultAnswerAsyncly(
                answer_waiter, MahiResponseStatus::kSuccess,
                std::move(callback), /*delay=*/delay_time);
          });

  SubmitTestQuestion();

  histogram_tester.ExpectTimeBucketCount(
      mahi_constants::kAnswerLoadingTimeHistogramName, delay_time, 0);

  // Test that loading time metrics is recorded when a question is answered.
  ASSERT_TRUE(answer_waiter.Wait());
  histogram_tester.ExpectTimeBucketCount(
      mahi_constants::kAnswerLoadingTimeHistogramName, delay_time, 1);
}

// Tests that pressing on the send button with a valid textfield takes the user
// to the Q&A View and the back to summary outlines button that appears on top
// takes the user back to the main view.
TEST_F(MahiPanelViewTest, TransitionToQuestionAnswerView) {
  // Initially the Summary Outlines section is visible.
  const auto* const summary_outlines_section = panel_view()->GetViewByID(
      mahi_constants::ViewId::kSummaryOutlinesSection);
  ASSERT_TRUE(summary_outlines_section);
  EXPECT_TRUE(summary_outlines_section->GetVisible());

  const auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  ASSERT_TRUE(question_answer_view);
  EXPECT_FALSE(question_answer_view->GetVisible());

  const auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  ASSERT_TRUE(send_button);
  EXPECT_TRUE(send_button->GetVisible());

  const auto* const go_to_summary_outlines_button = panel_view()->GetViewByID(
      mahi_constants::ViewId::kGoToSummaryOutlinesButton);
  ASSERT_TRUE(go_to_summary_outlines_button);
  EXPECT_FALSE(go_to_summary_outlines_button->GetVisible());

  auto* const question_textfield = views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));
  ASSERT_TRUE(question_textfield);
  EXPECT_TRUE(question_textfield->GetVisible());

  const auto* const go_to_question_answer_button = panel_view()->GetViewByID(
      mahi_constants::ViewId::kGoToQuestionAndAnswerButton);
  ASSERT_TRUE(go_to_question_answer_button);
  EXPECT_FALSE(go_to_question_answer_button->GetVisible());

  // Provide a valid input in the textfield so it can be sent as a question.
  question_textfield->SetText(u"input");

  // Pressing the send button with a valid input in the textfield should take
  // the user to the Q&A view.
  LeftClickOn(send_button);
  EXPECT_FALSE(summary_outlines_section->GetVisible());
  EXPECT_TRUE(question_answer_view->GetVisible());
  EXPECT_TRUE(go_to_summary_outlines_button->GetVisible());
  EXPECT_TRUE(send_button->GetVisible());

  // Run layout so the back to summary outlines button updates its size and
  // becomes clickable.
  views::test::RunScheduledLayout(widget());

  // Pressing the back to summary outlines button should take the user back to
  // the main view.
  LeftClickOn(go_to_summary_outlines_button);
  EXPECT_TRUE(summary_outlines_section->GetVisible());
  EXPECT_FALSE(question_answer_view->GetVisible());
  EXPECT_FALSE(go_to_summary_outlines_button->GetVisible());
  EXPECT_TRUE(send_button->GetVisible());
  views::test::RunScheduledLayout(widget());

  // "Back to QA" button should now be visible and clickable.
  EXPECT_TRUE(go_to_question_answer_button->GetVisible());
  LeftClickOn(go_to_question_answer_button);
  EXPECT_FALSE(summary_outlines_section->GetVisible());
  EXPECT_TRUE(question_answer_view->GetVisible());

  // Refreshing the summary contents should clear the Q&A view and make the
  // "Back to QA button" invisible.
  ui_controller()->RefreshContents();
  EXPECT_TRUE(summary_outlines_section->GetVisible());
  EXPECT_FALSE(question_answer_view->GetVisible());
  EXPECT_FALSE(go_to_question_answer_button->GetVisible());
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
  LeftClickOn(panel_view()->GetViewByID(
      mahi_constants::ViewId::kGoToSummaryOutlinesButton));

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
  // TODO(b/334117521): Create a test API and use it instead of using
  // `children()`.
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

// Tests the animated image showing when answer is loading.
TEST_F(MahiPanelViewTest, AnswerLoadingAnimation) {
  // Config the mock mahi manager to answer a question asyncly.
  base::test::TestFuture<void> answer_waiter;
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [&answer_waiter](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            ReturnDefaultAnswerAsyncly(answer_waiter,
                                       MahiResponseStatus::kSuccess,
                                       std::move(callback));
          });

  SubmitTestQuestion();

  // After a question is posted and before an answer is loaded, the Q&A view
  // should show.
  auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  ASSERT_TRUE(question_answer_view);

  // When the answer is loading, the view should show the question and the
  // loading image.
  EXPECT_EQ(question_answer_view->children().size(), 2u);
  EXPECT_TRUE(question_answer_view->children()[0]->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel));
  EXPECT_TRUE(question_answer_view->children()[1]->GetViewByID(
      mahi_constants::ViewId::kAnswerLoadingAnimatedImage));

  // After the answer is loaded, the view should show the question and answer
  // labels, without the loading image.
  ASSERT_TRUE(answer_waiter.WaitAndClear());
  EXPECT_EQ(question_answer_view->children().size(), 2u);
  EXPECT_TRUE(question_answer_view->children()[0]->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel));
  EXPECT_TRUE(question_answer_view->children()[1]->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel));
  EXPECT_FALSE(panel_view()->GetViewByID(
      mahi_constants::ViewId::kAnswerLoadingAnimatedImage));

  // Test submitting another question.
  SubmitTestQuestion();

  EXPECT_EQ(question_answer_view->children().size(), 4u);
  EXPECT_TRUE(question_answer_view->children()[2]->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel));
  EXPECT_TRUE(question_answer_view->children()[3]->GetViewByID(
      mahi_constants::ViewId::kAnswerLoadingAnimatedImage));

  // After 2nd answer is loaded.
  ASSERT_TRUE(answer_waiter.WaitAndClear());
  EXPECT_EQ(question_answer_view->children().size(), 4u);
  EXPECT_TRUE(question_answer_view->children()[2]->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel));
  EXPECT_TRUE(question_answer_view->children()[3]->GetViewByID(
      mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel));
  EXPECT_FALSE(panel_view()->GetViewByID(
      mahi_constants::ViewId::kAnswerLoadingAnimatedImage));
}

// Tests that the content scroll view scrolls to the bottom or top when being
// laid out based on the following:
// 1. Scroll to bottom when switching to the Q&A view, or when a new
//    question/answer is added.
// 2. Scroll to top when switching to the summary & outlines section, or
//    when refreshing the summary contents.
TEST_F(MahiPanelViewTest, ScrollViewScrollsAfterLayout) {
  ON_CALL(mock_mahi_manager(), GetSummary).WillByDefault(ReturnLongSummary);
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [](const std::u16string& question, bool current_panel_content,
             chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(/*answer=*/u"answer",
                                    MahiResponseStatus::kSuccess);
          });

  // Recreate panel widget so it can return a long summary.
  CreatePanelWidget();

  // Check that the Summary view with a long summary has a scroll bar.
  const auto* const summary_outlines_section = panel_view()->GetViewByID(
      mahi_constants::ViewId::kSummaryOutlinesSection);
  ASSERT_TRUE(summary_outlines_section);
  EXPECT_TRUE(summary_outlines_section->GetVisible());

  auto* const scroll_view = views::AsViewClass<views::ScrollView>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kScrollView));
  ASSERT_TRUE(scroll_view);
  auto* const scroll_bar = scroll_view->vertical_scroll_bar();
  ASSERT_TRUE(scroll_bar);
  EXPECT_TRUE(scroll_bar->GetVisible());

  // Switch to the Q&A view, which should initially not be scrollable.
  auto* const question_textfield = views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));
  ASSERT_TRUE(question_textfield);
  question_textfield->SetText(u"question");

  const auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  ASSERT_TRUE(send_button);
  LeftClickOn(send_button);

  const auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  ASSERT_TRUE(question_answer_view);
  ASSERT_TRUE(question_answer_view->GetVisible());

  views::test::RunScheduledLayout(widget());
  EXPECT_FALSE(scroll_bar->GetVisible());

  // Add enough questions to make the view scrollable.
  while (!scroll_bar->GetVisible()) {
    question_textfield->SetText(u"question");
    LeftClickOn(send_button);
    views::test::RunScheduledLayout(widget());
  }

  // Ensure that the view scrolls down after a new question is added.
  int previous_scroll_bar_position = scroll_bar->GetPosition();
  question_textfield->SetText(u"question");
  LeftClickOn(send_button);
  views::test::RunScheduledLayout(widget());
  EXPECT_GT(scroll_bar->GetPosition(), previous_scroll_bar_position);

  // Ensure that the scroll bar is at the end position.
  previous_scroll_bar_position = scroll_bar->GetPosition();
  scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kEnd);
  EXPECT_EQ(scroll_bar->GetPosition(), previous_scroll_bar_position);

  // The view scrolls up after switching to the summary section.
  const auto* const go_to_summary_outlines_button = panel_view()->GetViewByID(
      mahi_constants::ViewId::kGoToSummaryOutlinesButton);
  ASSERT_TRUE(go_to_summary_outlines_button);
  LeftClickOn(go_to_summary_outlines_button);
  views::test::RunScheduledLayout(widget());
  EXPECT_TRUE(summary_outlines_section->GetVisible());

  // Ensure that the scroll bar is visible.
  ASSERT_TRUE(scroll_bar->GetVisible());
  EXPECT_LT(scroll_bar->GetPosition(), previous_scroll_bar_position);

  // Ensure that the scroll bar is at the start position.
  previous_scroll_bar_position = scroll_bar->GetPosition();
  scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kStart);
  EXPECT_EQ(scroll_bar->GetPosition(), previous_scroll_bar_position);

  // Scroll down to the end position to test that refreshing the summary
  // contents scrolls to the start position.
  scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kEnd);
  EXPECT_GT(scroll_bar->GetPosition(), previous_scroll_bar_position);
  previous_scroll_bar_position = scroll_bar->GetPosition();
  ui_controller()->RefreshContents();
  views::test::RunScheduledLayout(widget());
  EXPECT_LT(scroll_bar->GetPosition(), previous_scroll_bar_position);

  // Ensure again that the scroll bar is at the start position.
  previous_scroll_bar_position = scroll_bar->GetPosition();
  scroll_bar->ScrollByAmount(views::ScrollBar::ScrollAmount::kStart);
  EXPECT_EQ(scroll_bar->GetPosition(), previous_scroll_bar_position);
}

// Verifies the mahi panel view when loading an answer with an error by
// iterating all possible errors.
TEST_F(MahiPanelViewTest, FailToGetAnswer) {
  for (MahiResponseStatus error : GetMahiErrors()) {
    // Configs the mock mahi manager to return answer with an `error` asyncly.
    base::test::TestFuture<void> answer_waiter;
    EXPECT_CALL(mock_mahi_manager(), AnswerQuestion)
        .WillOnce(
            [&answer_waiter, error](
                const std::u16string& question, bool current_panel_content,
                chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
              ReturnDefaultAnswerAsyncly(answer_waiter, error,
                                         std::move(callback));
            });

    base::HistogramTester histogram_tester;
    const std::u16string question(u"A question that brings errors");
    SubmitTestQuestion(question);
    histogram_tester.ExpectBucketCount(
        mahi_constants::kMahiQuestionSourceHistogramName,
        MahiUiController::QuestionSource::kPanel, 1);
    Mock::VerifyAndClearExpectations(&mock_mahi_manager());

    // After a question is posted and before an answer is loaded, the Q&A view
    // should show.
    const auto* const question_answer_view =
        panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
    CHECK(question_answer_view);
    EXPECT_TRUE(question_answer_view->GetVisible());
    EXPECT_TRUE(question_answer_view->GetViewByID(
        mahi_constants::ViewId::kAnswerLoadingAnimatedImage));

    const auto* const summary_outlines_section = panel_view()->GetViewByID(
        mahi_constants::ViewId::kSummaryOutlinesSection);
    CHECK(summary_outlines_section);
    EXPECT_FALSE(summary_outlines_section->GetVisible());

    auto* error_label_view =
        views::AsViewClass<views::Label>(panel_view()->GetViewByID(
            mahi_constants::ViewId::kQuestionAnswerErrorLabel));
    EXPECT_EQ(nullptr, error_label_view);

    // Waits until an answer is loaded with an error.
    ASSERT_TRUE(answer_waiter.WaitAndClear());

    EXPECT_TRUE(question_answer_view->GetVisible());
    EXPECT_FALSE(summary_outlines_section->GetVisible());

    // Checks the contents of `error_status_label`. The error should show
    // inline.
    error_label_view =
        views::AsViewClass<views::Label>(panel_view()->GetViewByID(
            mahi_constants::ViewId::kQuestionAnswerErrorLabel));
    EXPECT_TRUE(error_label_view->GetVisible());
    EXPECT_EQ(
        error_label_view->GetText(),
        l10n_util::GetStringUTF16(mahi_utils::GetErrorStatusViewTextId(error)));

    auto* const send_button = panel_view()->GetViewByID(
        mahi_constants::ViewId::kAskQuestionSendButton);
    EXPECT_TRUE(send_button->GetEnabled());

    EXPECT_FALSE(question_answer_view->GetViewByID(
        mahi_constants::ViewId::kAnswerLoadingAnimatedImage));

    // Configs the mock mahi manager to return an answer in success.
    EXPECT_CALL(mock_mahi_manager(), AnswerQuestion)
        .WillOnce(
            [&answer_waiter](
                const std::u16string& question, bool current_panel_content,
                chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
              ReturnDefaultAnswerAsyncly(answer_waiter,
                                         MahiResponseStatus::kSuccess,
                                         std::move(callback));
            });

    // Asks another question.
    auto* const question_textfield = views::AsViewClass<views::Textfield>(
        panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield));
    question_textfield->SetText(u"A new question");
    LeftClickOn(send_button);
    Mock::VerifyAndClearExpectations(&mock_mahi_manager());

    // Loading animated image should show again.
    EXPECT_TRUE(question_answer_view->GetViewByID(
        mahi_constants::ViewId::kAnswerLoadingAnimatedImage));

    // The error image view and the error label view should still exist.
    EXPECT_TRUE(panel_view()->GetViewByID(
        mahi_constants::ViewId::kQuestionAnswerErrorImage));
    EXPECT_TRUE(panel_view()->GetViewByID(
        mahi_constants::ViewId::kQuestionAnswerErrorLabel));

    // Waits for the answer to load. Both the error image view and the error
    // label view should still exist.
    ASSERT_TRUE(answer_waiter.WaitAndClear());
    EXPECT_TRUE(question_answer_view->GetVisible());
    EXPECT_TRUE(panel_view()->GetViewByID(
        mahi_constants::ViewId::kQuestionAnswerErrorImage));
    EXPECT_TRUE(panel_view()->GetViewByID(
        mahi_constants::ViewId::kQuestionAnswerErrorLabel));
    EXPECT_EQ(question_answer_view->children().size(), 4u);
    EXPECT_EQ(views::AsViewClass<views::Label>(
                  question_answer_view->children()[3]->GetViewByID(
                      mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                  ->GetText(),
              u"fake answer");

    // Configs the mock mahi manager to return answer with an `error` again.
    EXPECT_CALL(mock_mahi_manager(), AnswerQuestion)
        .WillOnce(
            [&answer_waiter, error](
                const std::u16string& question, bool current_panel_content,
                chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
              ReturnDefaultAnswerAsyncly(answer_waiter, error,
                                         std::move(callback));
            });
    const std::u16string question2(u"A new question that brings errors");
    SubmitTestQuestion(question2);
    Mock::VerifyAndClearExpectations(&mock_mahi_manager());

    // Shows the new error message inline.
    ASSERT_TRUE(answer_waiter.WaitAndClear());
    EXPECT_EQ(question_answer_view->children().size(), 6u);
    EXPECT_EQ(question_answer_view->children()[5]->children()[0]->GetID(),
              mahi_constants::ViewId::kQuestionAnswerErrorImage);
    EXPECT_EQ(question_answer_view->children()[5]->children()[1]->GetID(),
              mahi_constants::ViewId::kQuestionAnswerErrorLabel);
    EXPECT_EQ(
        views::AsViewClass<views::Label>(
            question_answer_view->children()[5]->GetViewByID(
                mahi_constants::ViewId::kQuestionAnswerErrorLabel))
            ->GetText(),
        l10n_util::GetStringUTF16(mahi_utils::GetErrorStatusViewTextId(error)));

    CreatePanelWidget();
  }
}

// Verifies the mahi panel view when loading an answer with a low quota warning.
TEST_F(MahiPanelViewTest, GetAnswerWithLowQuotaWarning) {
  // Config the mock mahi manager to return an answer with a low quota warning.
  base::test::TestFuture<void> answer_waiter;
  EXPECT_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillOnce(
          [&answer_waiter](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            ReturnDefaultAnswerAsyncly(answer_waiter,
                                       MahiResponseStatus::kLowQuota,
                                       std::move(callback));
          });

  SubmitTestQuestion();

  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  // After a question is posted and before an answer is loaded, the Q&A view
  // should show.
  const auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  CHECK(question_answer_view);
  EXPECT_TRUE(question_answer_view->GetVisible());
  EXPECT_TRUE(question_answer_view->GetViewByID(
      mahi_constants::ViewId::kAnswerLoadingAnimatedImage));

  const auto* const summary_outlines_section = panel_view()->GetViewByID(
      mahi_constants::ViewId::kSummaryOutlinesSection);
  CHECK(summary_outlines_section);
  EXPECT_FALSE(summary_outlines_section->GetVisible());

  const auto* const error_status_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kErrorStatusView);
  CHECK(error_status_view);
  EXPECT_FALSE(error_status_view->GetVisible());

  // Wait until an answer is loaded.
  ASSERT_TRUE(answer_waiter.Wait());

  // `question_answer_view` should still be visible because
  // `MahiResponseStatus::kLowQuota` should not block the answer.
  EXPECT_FALSE(error_status_view->GetVisible());
  EXPECT_TRUE(question_answer_view->GetVisible());
  EXPECT_FALSE(summary_outlines_section->GetVisible());

  // Check the answer bubble.
  // TODO(http://b/334117521): Add a test API instead of using `children()`.
  ASSERT_EQ(question_answer_view->children().size(), 2u);
  EXPECT_EQ(views::AsViewClass<views::Label>(
                question_answer_view->children()[1]->GetViewByID(
                    mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel))
                ->GetText(),
            u"fake answer");
}

// Verifies the mahi panel view when loading outlines with an error by
// iterating all possible errors.
TEST_F(MahiPanelViewTest, FailToGetOutlines) {
  for (MahiResponseStatus error : GetMahiErrors()) {
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

    // Wait until outlines are loaded with an error.
    ASSERT_TRUE(outlines_waiter.Wait());

    EXPECT_TRUE(error_status_view->GetVisible());
    EXPECT_FALSE(question_answer_view->GetVisible());
    EXPECT_FALSE(summary_outlines_section->GetVisible());

    // Check the contents of `error_status_label`.
    EXPECT_EQ(
        error_status_label->GetText(),
        l10n_util::GetStringUTF16(mahi_utils::GetErrorStatusViewTextId(error)));

    const auto* const retry_link =
        panel_view()->GetViewByID(mahi_constants::kErrorStatusRetryLink);
    ASSERT_TRUE(retry_link);
    EXPECT_EQ(retry_link->GetVisible(),
              mahi_utils::CalculateRetryLinkVisible(error));

    if (retry_link->GetVisible()) {
      // Click the `retry_link`. The mock mahi manager should be requested for a
      // summary and outlines again.
      views::test::RunScheduledLayout(widget());
      GetEventGenerator()->MoveMouseTo(
          retry_link->GetBoundsInScreen().CenterPoint());
      EXPECT_CALL(mock_mahi_manager(), GetSummary);
      EXPECT_CALL(mock_mahi_manager(), GetOutlines);
      EXPECT_CALL(mock_mahi_manager(), AnswerQuestion).Times(0);
      GetEventGenerator()->ClickLeftButton();
      Mock::VerifyAndClear(&mock_mahi_manager());
    }
  }
}

// Verifies the mahi panel view when loading outlines with a low quota warning.
TEST_F(MahiPanelViewTest, GetOutlinesWithLowQuotaWarning) {
  // Config the mock mahi manager to return outlines with a low quota warning.
  base::test::TestFuture<void> outlines_waiter;
  EXPECT_CALL(mock_mahi_manager(), GetOutlines)
      .WillOnce([&outlines_waiter](
                    chromeos::MahiManager::MahiOutlinesCallback callback) {
        ReturnDefaultOutlinesAsyncly(outlines_waiter,
                                     MahiResponseStatus::kLowQuota,
                                     std::move(callback));
      });

  CreatePanelWidget();
  Mock::VerifyAndClear(&mock_mahi_manager());

  // Before outlines are loaded, the summary & outlines section should show.
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

  // Wait until outlines are loaded.
  ASSERT_TRUE(outlines_waiter.Wait());

  // `summary_outlines_section` should still be visible because
  // `MahiResponseStatus::kLowQuota` should not block the outlines.
  // TODO(http://b/330643995): Check the outlines container is visible.
  EXPECT_FALSE(error_status_view->GetVisible());
  EXPECT_FALSE(question_answer_view->GetVisible());
  EXPECT_TRUE(summary_outlines_section->GetVisible());
}

// Verifies the mahi panel view when loading summary with an error by iterating
// all possible errors.
TEST_F(MahiPanelViewTest, FailToGetSummary) {
  for (MahiResponseStatus error : GetMahiErrors()) {
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

    // Wait until the summary is loaded with an error.
    ASSERT_TRUE(summary_waiter.Wait());

    EXPECT_TRUE(error_status_view->GetVisible());
    EXPECT_FALSE(question_answer_view->GetVisible());
    EXPECT_FALSE(summary_outlines_section->GetVisible());

    // Check the contents of `error_status_label`.
    EXPECT_EQ(
        error_status_label->GetText(),
        l10n_util::GetStringUTF16(mahi_utils::GetErrorStatusViewTextId(error)));

    const auto* const retry_link =
        panel_view()->GetViewByID(mahi_constants::kErrorStatusRetryLink);
    ASSERT_TRUE(retry_link);
    EXPECT_EQ(retry_link->GetVisible(),
              mahi_utils::CalculateRetryLinkVisible(error));

    if (retry_link->GetVisible()) {
      // Click the `retry_link`. The mock mahi manager should be requested for a
      // summary and outlines again.
      views::test::RunScheduledLayout(widget());
      GetEventGenerator()->MoveMouseTo(
          retry_link->GetBoundsInScreen().CenterPoint());
      EXPECT_CALL(mock_mahi_manager(), GetSummary);
      EXPECT_CALL(mock_mahi_manager(), GetOutlines);
      EXPECT_CALL(mock_mahi_manager(), AnswerQuestion).Times(0);
      GetEventGenerator()->ClickLeftButton();
      Mock::VerifyAndClear(&mock_mahi_manager());
    }
  }
}

// Verifies the mahi panel view when loading a summary with a low quota warning.
TEST_F(MahiPanelViewTest, GetSummaryWithLowQuotaWarning) {
  // Config the mock mahi manager to return a summary with a low quota warning.
  base::test::TestFuture<void> summary_waiter;
  EXPECT_CALL(mock_mahi_manager(), GetSummary)
      .WillOnce([&summary_waiter](
                    chromeos::MahiManager::MahiSummaryCallback callback) {
        ReturnDefaultSummaryAsyncly(
            summary_waiter, MahiResponseStatus::kLowQuota, std::move(callback));
      });

  CreatePanelWidget();
  Mock::VerifyAndClear(&mock_mahi_manager());

  // Before the summary is loaded, the summary & outlines section should show.
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

  // Wait until the summary is loaded.
  ASSERT_TRUE(summary_waiter.Wait());

  // `summary_outlines_section` should still be visible because
  // `MahiResponseStatus::kLowQuota` should not block the summary.
  EXPECT_FALSE(error_status_view->GetVisible());
  EXPECT_FALSE(question_answer_view->GetVisible());
  EXPECT_TRUE(summary_outlines_section->GetVisible());

  const auto* const summary_label = GetSummaryLabel(panel_view());
  ASSERT_TRUE(summary_label);
  EXPECT_TRUE(summary_label->GetVisible());
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

  EXPECT_EQ(GetContentSourceTitle(&mahi_view), title1);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *GetContentSourceIcon(&mahi_view).bitmap(),
      *image_util::ResizeAndCropImage(icon1, mahi_constants::kContentIconSize)
           .bitmap()));
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

  EXPECT_EQ(GetContentSourceTitle(&mahi_view), title2);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(
      *GetContentSourceIcon(&mahi_view).bitmap(),
      *image_util::ResizeAndCropImage(icon2, mahi_constants::kContentIconSize)
           .bitmap()));
  EXPECT_EQ(GetSummaryLabel(&mahi_view)->GetText(), summary2);
}

// Tests that clicking the content source button opens the source url
// corresponding to the refreshed content shown on the Mahi panel.
TEST_F(MahiPanelViewTest, ContentSourceButtonUrlAfterRefresh) {
  const GURL test_url1("https://www.google.com");
  ON_CALL(mock_mahi_manager(), GetContentUrl).WillByDefault(Return(test_url1));

  CreatePanelWidget();

  EXPECT_CALL(
      new_window_delegate(),
      OpenUrl(test_url1, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
              NewWindowDelegate::Disposition::kSwitchToTab));
  LeftClickOn(
      panel_view()->GetViewByID(mahi_constants::ViewId::kContentSourceButton));
  Mock::VerifyAndClearExpectations(&new_window_delegate());

  const GURL test_url2("https://en.wikipedia.org");
  ON_CALL(mock_mahi_manager(), GetContentUrl).WillByDefault(Return(test_url2));

  ui_controller()->RefreshContents();

  EXPECT_CALL(
      new_window_delegate(),
      OpenUrl(test_url2, NewWindowDelegate::OpenUrlFrom::kUserInteraction,
              NewWindowDelegate::Disposition::kSwitchToTab));
  LeftClickOn(
      panel_view()->GetViewByID(mahi_constants::ViewId::kContentSourceButton));
  Mock::VerifyAndClearExpectations(&new_window_delegate());
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

  // Transition to Q&A view by asking a question.
  SubmitTestQuestion();
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

// TODO(crbug.com/333800096): Re-enable this test
TEST_F(MahiPanelViewTest, DISABLED_ClickMetrics) {
  base::HistogramTester histogram;

  // Learn more button.
  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kLearnMoreLink, 0);
  LeftClickOn(
      panel_view()->GetViewByID(mahi_constants::ViewId::kLearnMoreLink));
  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kLearnMoreLink, 1);
  histogram.ExpectTotalCount(mahi_constants::kMahiButtonClickHistogramName, 1);

  auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  auto* const back_to_summary_outlines_button = panel_view()->GetViewByID(
      mahi_constants::ViewId::kGoToSummaryOutlinesButton);
  auto* const back_to_question_answer_button = panel_view()->GetViewByID(
      mahi_constants::ViewId::kGoToQuestionAndAnswerButton);
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
  EXPECT_FALSE(back_to_summary_outlines_button->GetVisible());
  const std::u16string question(u"question text");
  question_textfield->SetText(question);
  LeftClickOn(send_button);
  histogram.ExpectBucketCount(
      mahi_constants::kMahiButtonClickHistogramName,
      mahi_constants::PanelButton::kAskQuestionSendButton, 1);
  histogram.ExpectTotalCount(mahi_constants::kMahiButtonClickHistogramName, 2);

  // Now the back to summary outlines button is visible.
  EXPECT_TRUE(back_to_summary_outlines_button->GetVisible());

  // Back to summary outlines button.
  views::test::RunScheduledLayout(widget());
  histogram.ExpectBucketCount(
      mahi_constants::kMahiButtonClickHistogramName,
      mahi_constants::PanelButton::kGoToSummaryOutlinesButton, 0);
  LeftClickOn(back_to_summary_outlines_button);
  histogram.ExpectBucketCount(
      mahi_constants::kMahiButtonClickHistogramName,
      mahi_constants::PanelButton::kGoToSummaryOutlinesButton, 1);
  histogram.ExpectTotalCount(mahi_constants::kMahiButtonClickHistogramName, 3);

  // Back to Q&A button.
  views::test::RunScheduledLayout(widget());
  EXPECT_TRUE(back_to_question_answer_button->GetVisible());
  histogram.ExpectBucketCount(
      mahi_constants::kMahiButtonClickHistogramName,
      mahi_constants::PanelButton::kGoToQuestionAndAnswerButton, 0);
  LeftClickOn(back_to_question_answer_button);
  histogram.ExpectBucketCount(
      mahi_constants::kMahiButtonClickHistogramName,
      mahi_constants::PanelButton::kGoToQuestionAndAnswerButton, 1);

  // Close button.
  views::test::RunScheduledLayout(widget());
  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kCloseButton, 0);
  LeftClickOn(panel_view()->GetViewByID(mahi_constants::ViewId::kCloseButton));
  histogram.ExpectBucketCount(mahi_constants::kMahiButtonClickHistogramName,
                              mahi_constants::PanelButton::kCloseButton, 1);
  histogram.ExpectTotalCount(mahi_constants::kMahiButtonClickHistogramName, 5);
}

TEST_F(MahiPanelViewTest, UserJourneyTimeMetrics) {
  base::HistogramTester histogram;
  histogram.ExpectTimeBucketCount(
      mahi_constants::kMahiUserJourneyTimeHistogramName, base::Seconds(3),
      /*expected_count=*/0);

  task_environment()->AdvanceClock(base::Seconds(3));

  CreatePanelWidget();
  histogram.ExpectTimeBucketCount(
      mahi_constants::kMahiUserJourneyTimeHistogramName, base::Seconds(3),
      /*expected_count=*/1);

  task_environment()->AdvanceClock(base::Minutes(3));

  CreatePanelWidget();
  histogram.ExpectTimeBucketCount(
      mahi_constants::kMahiUserJourneyTimeHistogramName, base::Minutes(3),
      /*expected_count=*/1);

  task_environment()->AdvanceClock(base::Minutes(10));

  CreatePanelWidget();
  histogram.ExpectTimeBucketCount(
      mahi_constants::kMahiUserJourneyTimeHistogramName, base::Minutes(10),
      /*expected_count=*/1);
}

TEST_F(MahiPanelViewTest, ReportQuestionCountWhenRefresh) {
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [](const std::u16string& question, bool current_panel_content,
             chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(u"answer",
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  // Ask one question then refresh. Verify that the recorded question count
  // in this Mahi session should be one.
  base::HistogramTester histogram_tester;
  SubmitTestQuestion();
  histogram_tester.ExpectBucketCount(
      mahi_constants::kQuestionCountPerMahiSessionHistogramName, /*sample=*/1,
      /*expected_count=*/0);
  ui_controller()->RefreshContents();
  histogram_tester.ExpectBucketCount(
      mahi_constants::kQuestionCountPerMahiSessionHistogramName, /*sample=*/1,
      /*expected_count=*/1);

  // Ask two questions then refresh. Verify that the recorded question count
  // in this Mahi session should be two.
  SubmitTestQuestion();
  SubmitTestQuestion();
  histogram_tester.ExpectBucketCount(
      mahi_constants::kQuestionCountPerMahiSessionHistogramName, /*sample=*/1,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      mahi_constants::kQuestionCountPerMahiSessionHistogramName, /*sample=*/2,
      /*expected_count=*/0);
  ui_controller()->RefreshContents();
  histogram_tester.ExpectBucketCount(
      mahi_constants::kQuestionCountPerMahiSessionHistogramName, /*sample=*/2,
      /*expected_count=*/1);
}

TEST_F(MahiPanelViewTest, ReportQuestionCountWhenMahiPanelDestroyed) {
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [](const std::u16string& question, bool current_panel_content,
             chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(u"answer",
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  // Ask one question then destroy the Mahi panel. Verify that the recorded
  // question count in this Mahi session should be one.
  base::HistogramTester histogram_tester;
  SubmitTestQuestion();
  histogram_tester.ExpectBucketCount(
      mahi_constants::kQuestionCountPerMahiSessionHistogramName, /*sample=*/1,
      /*expected_count=*/0);
  ResetPanelWidget();
  histogram_tester.ExpectBucketCount(
      mahi_constants::kQuestionCountPerMahiSessionHistogramName, /*sample=*/1,
      /*expected_count=*/1);

  // Ask two questions then destroy the Mahi panel. Verify that the recorded
  // question count in this Mahi session should be two.
  CreatePanelWidget();
  SubmitTestQuestion();
  SubmitTestQuestion();
  histogram_tester.ExpectBucketCount(
      mahi_constants::kQuestionCountPerMahiSessionHistogramName,
      /*sample=*/1,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      mahi_constants::kQuestionCountPerMahiSessionHistogramName,
      /*sample=*/2,
      /*expected_count=*/0);
  ResetPanelWidget();
  histogram_tester.ExpectBucketCount(
      mahi_constants::kQuestionCountPerMahiSessionHistogramName,
      /*sample=*/2,
      /*expected_count=*/1);
}

// Make sure that summary label is displayed correctly given any kind of text.
TEST_F(MahiPanelViewTest, RandomizedTextSummaryLabel) {
  auto random_string = GetRandomString(/*max_words_count=*/500);
  ON_CALL(mock_mahi_manager(), GetSummary)
      .WillByDefault(
          [random_string](chromeos::MahiManager::MahiSummaryCallback callback) {
            std::move(callback).Run(random_string,
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  ui_controller()->RefreshContents();
  views::test::RunScheduledLayout(widget());

  auto* summary_label = views::AsViewClass<views::Label>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kSummaryLabel));

  // Make sure the summary label is not clipped.
  EXPECT_FALSE(summary_label->IsDisplayTextClipped())
      << "Summary label is clipped with the text: " << random_string;

  // Make sure the label is within the bounds of its parent view.
  auto* scroll_view = views::AsViewClass<views::ScrollView>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kScrollView));
  EXPECT_LE(summary_label->width(), scroll_view->GetVisibleRect().width())
      << "Summary label width surpasses scroll view visible width: "
      << random_string;
}

// Make sure the question and answer labels are displayed correctly given any
// kind of texts.
TEST_F(MahiPanelViewTest, RandomizedTextQuestionAnswerLabels) {
  auto random_answer = GetRandomString(/*max_words_count=*/100);
  ON_CALL(mock_mahi_manager(), AnswerQuestion)
      .WillByDefault(
          [&random_answer](
              const std::u16string& question, bool current_panel_content,
              chromeos::MahiManager::MahiAnswerQuestionCallback callback) {
            std::move(callback).Run(random_answer,
                                    chromeos::MahiResponseStatus::kSuccess);
          });

  auto random_question = GetRandomString(/*max_words_count=*/100);
  views::AsViewClass<views::Textfield>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionTextfield))
      ->SetText(random_question);

  // Pressing the send button should create a question and answer text bubble.
  LeftClickOn(panel_view()->GetViewByID(
      mahi_constants::ViewId::kAskQuestionSendButton));

  views::test::RunScheduledLayout(widget());

  auto* question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  auto* question_label = views::AsViewClass<views::Label>(
      question_answer_view->children()[0]->GetViewByID(
          mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel));
  EXPECT_FALSE(question_label->IsDisplayTextClipped())
      << "Question label is clipped with the text: " << random_question;

  auto* scroll_view = views::AsViewClass<views::ScrollView>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kScrollView));
  EXPECT_LE(question_label->width(), scroll_view->GetVisibleRect().width())
      << "Question label width surpasses scroll view visible width: "
      << random_answer;

  auto* answer_label = views::AsViewClass<views::Label>(
      question_answer_view->children()[1]->GetViewByID(
          mahi_constants::ViewId::kQuestionAnswerTextBubbleLabel));

  EXPECT_FALSE(answer_label->IsDisplayTextClipped())
      << "Answer label is clipped with the text: " << random_answer;
  EXPECT_LE(answer_label->width(), scroll_view->GetVisibleRect().width())
      << "Answer label width surpasses scroll view visible width: "
      << random_answer;
}

TEST_F(MahiPanelViewTest, OnlyOneFeedbackButtonCanKeepToggled) {
  IconButton* thumbs_up_button = views::AsViewClass<IconButton>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsUpButton));
  IconButton* thumbs_down_button = views::AsViewClass<IconButton>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsDownButton));
  EXPECT_FALSE(thumbs_up_button->toggled());
  EXPECT_FALSE(thumbs_down_button->toggled());

  // Pressing thumbs up should toggle the button.
  LeftClickOn(thumbs_up_button);
  EXPECT_TRUE(thumbs_up_button->toggled());
  EXPECT_FALSE(thumbs_down_button->toggled());

  // Pressing thumbs down should just toggle down button on and up button off.
  LeftClickOn(thumbs_down_button);
  EXPECT_TRUE(thumbs_down_button->toggled());
  EXPECT_FALSE(thumbs_up_button->toggled());

  // Pressing thumbs up should just toggle up button on and down button off.
  LeftClickOn(thumbs_up_button);
  EXPECT_TRUE(thumbs_up_button->toggled());
  EXPECT_FALSE(thumbs_down_button->toggled());
}

TEST_F(MahiPanelViewTest, FeedbackButtonsAllowed) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();

  prefs->SetBoolean(prefs::kHmrFeedbackAllowed, false);
  CreatePanelWidget();
  EXPECT_FALSE(
      panel_view()
          ->GetViewByID(mahi_constants::ViewId::kFeedbackButtonsContainer)
          ->GetVisible());
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(
          IDS_ASH_MAHI_PANEL_DISCLAIMER_FEEDBACK_DISABLED,
          l10n_util::GetStringUTF16(IDS_ASH_MAHI_LEARN_MORE_LINK_LABEL_TEXT)),
      static_cast<views::StyledLabel*>(
          panel_view()->GetViewByID(mahi_constants::ViewId::kFooterLabel))
          ->GetText());

  prefs->SetBoolean(prefs::kHmrFeedbackAllowed, true);
  CreatePanelWidget();
  EXPECT_TRUE(
      panel_view()
          ->GetViewByID(mahi_constants::ViewId::kFeedbackButtonsContainer)
          ->GetVisible());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_MAHI_PANEL_DISCLAIMER),
            static_cast<views::Label*>(
                panel_view()->GetViewByID(mahi_constants::ViewId::kFooterLabel))
                ->GetText());
}

TEST_F(MahiPanelViewTest, FeedbackButtonResetWhenRefresh) {
  IconButton* thumbs_up_button = views::AsViewClass<IconButton>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsUpButton));
  IconButton* thumbs_down_button = views::AsViewClass<IconButton>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsDownButton));
  EXPECT_FALSE(thumbs_up_button->toggled());
  EXPECT_FALSE(thumbs_down_button->toggled());

  LeftClickOn(thumbs_up_button);
  EXPECT_TRUE(thumbs_up_button->toggled());
  EXPECT_FALSE(thumbs_down_button->toggled());

  // Test that the feedback button is reset when content is refreshed.
  ui_controller()->RefreshContents();
  EXPECT_FALSE(thumbs_up_button->toggled());
  EXPECT_FALSE(thumbs_down_button->toggled());

  LeftClickOn(thumbs_down_button);
  EXPECT_FALSE(thumbs_up_button->toggled());
  EXPECT_TRUE(thumbs_down_button->toggled());

  ui_controller()->RefreshContents();
  EXPECT_FALSE(thumbs_up_button->toggled());
  EXPECT_FALSE(thumbs_down_button->toggled());
}

TEST_F(MahiPanelViewTest, FeedbackButtonsOnError) {
  base::HistogramTester histogram_tester;

  base::test::TestFuture<void> summary_waiter;
  EXPECT_CALL(mock_mahi_manager(), GetSummary)
      .WillOnce([&summary_waiter](
                    chromeos::MahiManager::MahiSummaryCallback callback) {
        ReturnDefaultSummaryAsyncly(summary_waiter,
                                    MahiResponseStatus::kUnknownError,
                                    std::move(callback));
      });

  CreatePanelWidget();

  // Wait until the summary is loaded with an error.
  ASSERT_TRUE(summary_waiter.Wait());

  // Pressing thumbs up should toggle the button on and update the feedback
  // histogram.
  EXPECT_CALL(mock_mahi_manager(), OpenFeedbackDialog).Times(0);
  IconButton* thumbs_up_button = views::AsViewClass<IconButton>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsUpButton));
  LeftClickOn(thumbs_up_button);
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  EXPECT_TRUE(thumbs_up_button->toggled());
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 0);

  // Pressing thumbs down the first time should open the feedback dialog, toggle
  // the button off and update the feedback histogram.
  EXPECT_CALL(mock_mahi_manager(), OpenFeedbackDialog).Times(1);
  IconButton* thumbs_down_button = views::AsViewClass<IconButton>(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsDownButton));
  LeftClickOn(thumbs_down_button);
  Mock::VerifyAndClearExpectations(&mock_mahi_manager());

  EXPECT_TRUE(thumbs_down_button->toggled());
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 1);
}

}  // namespace ash
