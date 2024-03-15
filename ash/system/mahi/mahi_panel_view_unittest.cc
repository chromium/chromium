// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/style/icon_button.h"
#include "ash/system/mahi/fake_mahi_manager.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

}  // namespace

class MahiPanelViewTest : public AshTestBase {
 public:
  MahiPanelViewTest() = default;
  explicit MahiPanelViewTest(base::test::TaskEnvironment::TimeSource time)
      : AshTestBase(time) {}

  MahiPanelViewTest(const MahiPanelViewTest&) = delete;
  MahiPanelViewTest& operator=(const MahiPanelViewTest&) = delete;
  ~MahiPanelViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(std::move(delegate));

    AshTestBase::SetUp();

    fake_mahi_manager_ = std::make_unique<FakeMahiManager>();
    scoped_setter_ = std::make_unique<chromeos::ScopedMahiManagerSetter>(
        fake_mahi_manager_.get());

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    panel_view_ = widget_->SetContentsView(std::make_unique<MahiPanelView>());
  }

  void TearDown() override {
    panel_view_ = nullptr;
    widget_.reset();
    scoped_setter_.reset();
    fake_mahi_manager_.reset();

    AshTestBase::TearDown();

    new_window_delegate_ = nullptr;
  }

  MockNewWindowDelegate& new_window_delegate() { return *new_window_delegate_; }

  FakeMahiManager* fake_mahi_manager() { return fake_mahi_manager_.get(); }

  MahiPanelView* panel_view() { return panel_view_; }

  views::Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<FakeMahiManager> fake_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
  raw_ptr<MahiPanelView> panel_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MockNewWindowDelegate> new_window_delegate_;
  std::unique_ptr<TestNewWindowDelegateProvider> delegate_provider_;
};

// Verifies that the content title is correct when the panel is created.
TEST_F(MahiPanelViewTest, ContentTitle) {
  auto* test_title1 = u"test content title 1";
  fake_mahi_manager()->set_content_title(test_title1);
  auto mahi_view1 = std::make_unique<MahiPanelView>();
  auto* content_title_label1 = static_cast<views::Label*>(
      mahi_view1->GetViewByID(mahi_constants::ViewId::kContentTitle));
  EXPECT_EQ(content_title_label1->GetText(), test_title1);

  auto* test_title2 = u"test content title 2";
  fake_mahi_manager()->set_content_title(test_title2);
  auto mahi_view2 = std::make_unique<MahiPanelView>();
  auto* content_title_label2 = static_cast<views::Label*>(
      mahi_view2->GetViewByID(mahi_constants::ViewId::kContentTitle));
  EXPECT_EQ(content_title_label2->GetText(), test_title2);
}

// Verifies that the content icon is correct when the panel is created.
TEST_F(MahiPanelViewTest, ContentIcon) {
  auto test_icon1 = gfx::test::CreateImageSkia(128, SK_ColorRED);
  fake_mahi_manager()->set_content_icon(test_icon1);
  auto mahi_view1 = std::make_unique<MahiPanelView>();
  auto* content_icon1 = static_cast<views::ImageView*>(
      mahi_view1->GetViewByID(mahi_constants::ViewId::kContentIcon));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*content_icon1->GetImage().bitmap(),
                                         *test_icon1.bitmap()));
  EXPECT_EQ(content_icon1->GetPreferredSize(),
            mahi_constants::kContentIconSize);

  auto test_icon2 = gfx::test::CreateImageSkia(128, SK_ColorBLUE);
  fake_mahi_manager()->set_content_icon(test_icon2);
  auto mahi_view2 = std::make_unique<MahiPanelView>();
  auto* content_icon2 = static_cast<views::ImageView*>(
      mahi_view2->GetViewByID(mahi_constants::ViewId::kContentIcon));
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(*content_icon2->GetImage().bitmap(),
                                         *test_icon2.bitmap()));
  EXPECT_EQ(content_icon2->GetPreferredSize(),
            mahi_constants::kContentIconSize);
}

// Makes sure that the summary text is set correctly in ctor with different
// texts.
TEST_F(MahiPanelViewTest, SummaryText) {
  auto* test_text1 = u"test summary text 1";
  fake_mahi_manager()->set_summary_text(test_text1);
  auto mahi_view1 = std::make_unique<MahiPanelView>();
  auto* summary_label1 = static_cast<views::Label*>(
      mahi_view1->GetViewByID(mahi_constants::ViewId::kSummaryLabel));
  EXPECT_EQ(test_text1, summary_label1->GetText());

  auto* test_text2 = u"test summary text 2";
  fake_mahi_manager()->set_summary_text(test_text2);
  auto mahi_view2 = std::make_unique<MahiPanelView>();
  auto* summary_label2 = static_cast<views::Label*>(
      mahi_view2->GetViewByID(mahi_constants::ViewId::kSummaryLabel));
  EXPECT_EQ(test_text2, summary_label2->GetText());

  // Make sure the text is multiline and aligned correctly.
  EXPECT_TRUE(summary_label2->GetMultiLine());
  EXPECT_EQ(gfx::HorizontalAlignment::ALIGN_LEFT,
            summary_label2->GetHorizontalAlignment());
}

TEST_F(MahiPanelViewTest, FeedbackButtons) {
  base::HistogramTester histogram_tester;

  LeftClickOn(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsUpButton));
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 0);

  EXPECT_EQ(0, fake_mahi_manager()->open_feedback_dialog_called_count());

  LeftClickOn(
      panel_view()->GetViewByID(mahi_constants::ViewId::kThumbsDownButton));
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 1);

  // Should open feedback dialog when thumbs down button is pressed.
  EXPECT_EQ(1, fake_mahi_manager()->open_feedback_dialog_called_count());
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
}

// Make sure the `PanelContentsContainer` is larger than its contents when the
// contents are short.
TEST_F(MahiPanelViewTest, PanelContentsViewBoundsWithShortSummary) {
  auto panel_bounds = gfx::Size(300, 400);

  // Create a panel with a short summary.
  fake_mahi_manager()->set_summary_text(u"Short summary");
  auto mahi_view = std::make_unique<MahiPanelView>();
  mahi_view->SetPreferredSize(panel_bounds);
  mahi_view->SizeToPreferredSize();

  int short_content_height =
      mahi_view->GetViewByID(mahi_constants::kSummaryOutlinesSection)
          ->bounds()
          .height();
  int short_contents_container_height =
      mahi_view->GetViewByID(mahi_constants::kPanelContentsContainer)
          ->bounds()
          .height();

  // The container should be larger than the contents when the summary is short.
  EXPECT_GT(short_contents_container_height, short_content_height);
}

// Make sure the `PanelContentsContainer` is smaller than its contents when the
// contents are long.
TEST_F(MahiPanelViewTest, PanelContentsViewBoundsWithLongSummary) {
  auto panel_bounds = gfx::Size(300, 400);

  // Create a panel with a long summary.
  std::u16string long_summary;
  for (int i = 0; i < 100; i++) {
    long_summary += u"Long Summary\n";
  }
  fake_mahi_manager()->set_summary_text(long_summary);
  auto mahi_view = std::make_unique<MahiPanelView>();
  mahi_view->SetPreferredSize(panel_bounds);
  mahi_view->SizeToPreferredSize();

  int long_content_height =
      mahi_view->GetViewByID(mahi_constants::kSummaryOutlinesSection)
          ->bounds()
          .height();
  int long_contents_container_height =
      mahi_view->GetViewByID(mahi_constants::kPanelContentsContainer)
          ->bounds()
          .height();

  // The container should be smaller than the contents when the summary is long.
  EXPECT_LT(long_contents_container_height, long_content_height);
}

// Make sure the `PanelContentsContainer` is always sized to occupy the same
// amount of space in the `MahiPanelView` irrespective of its contents size.
TEST_F(MahiPanelViewTest, PanelContentsViewBoundsStayConstant) {
  auto panel_bounds = gfx::Size(300, 400);

  // Create a panel with a short summary.
  fake_mahi_manager()->set_summary_text(u"Short summary");
  auto mahi_view1 = std::make_unique<MahiPanelView>();
  mahi_view1->SetPreferredSize(panel_bounds);
  mahi_view1->SizeToPreferredSize();

  // Create a panel with a long summary.
  std::u16string long_summary;
  for (int i = 0; i < 100; i++) {
    long_summary += u"Long Summary\n";
  }
  fake_mahi_manager()->set_summary_text(long_summary);
  auto mahi_view2 = std::make_unique<MahiPanelView>();
  mahi_view2->SetPreferredSize(panel_bounds);
  mahi_view2->SizeToPreferredSize();

  int short_contents_container_height =
      mahi_view1->GetViewByID(mahi_constants::kPanelContentsContainer)
          ->bounds()
          .height();
  int long_contents_container_height =
      mahi_view2->GetViewByID(mahi_constants::kPanelContentsContainer)
          ->bounds()
          .height();

  // The container size should stay constant irrespective of summary length.
  EXPECT_EQ(short_contents_container_height, long_contents_container_height);
}

// A test class that uses a mock time task environment.
class MahiPanelViewMockTimeTest : public MahiPanelViewTest {
 public:
  MahiPanelViewMockTimeTest()
      : MahiPanelViewTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  MahiPanelViewMockTimeTest(const MahiPanelViewMockTimeTest&) = delete;
  MahiPanelViewMockTimeTest& operator=(const MahiPanelViewMockTimeTest&) =
      delete;
  ~MahiPanelViewMockTimeTest() override = default;

  // MahiPanelViewTest:
  void SetUp() override {
    MahiPanelViewTest::SetUp();
    fake_mahi_manager()->set_enable_fake_delays_for_animations(true);
  }
};

TEST_F(MahiPanelViewMockTimeTest, LoadingAnimations) {
  auto mahi_view = std::make_unique<MahiPanelView>();

  auto* summary_loading_animated_image = mahi_view->GetViewByID(
      mahi_constants::ViewId::kSummaryLoadingAnimatedImage);
  auto* outlines_loading_animated_image = mahi_view->GetViewByID(
      mahi_constants::ViewId::kOutlinesLoadingAnimatedImage);
  auto* summary_label =
      mahi_view->GetViewByID(mahi_constants::ViewId::kSummaryLabel);
  auto* outlines_container =
      mahi_view->GetViewByID(mahi_constants::ViewId::kOutlinesContainer);

  EXPECT_TRUE(summary_loading_animated_image->GetVisible());
  EXPECT_TRUE(outlines_loading_animated_image->GetVisible());
  EXPECT_FALSE(summary_label->GetVisible());
  EXPECT_FALSE(outlines_container->GetVisible());

  // Fast forward until the summary has loaded, the outline animation should
  // still be visible.
  task_environment()->FastForwardBy(
      base::Seconds(mahi_constants::kFakeMahiManagerLoadSummaryDelaySeconds));
  EXPECT_FALSE(summary_loading_animated_image->GetVisible());
  EXPECT_TRUE(outlines_loading_animated_image->GetVisible());
  EXPECT_TRUE(summary_label->GetVisible());
  EXPECT_FALSE(outlines_container->GetVisible());

  // Fast forward until everything is loaded, all animations shouldn't be
  // visible.
  task_environment()->FastForwardBy(
      base::Seconds(mahi_constants::kFakeMahiManagerLoadOutlinesDelaySeconds));
  EXPECT_FALSE(summary_loading_animated_image->GetVisible());
  EXPECT_FALSE(outlines_loading_animated_image->GetVisible());
  EXPECT_TRUE(summary_label->GetVisible());
  EXPECT_TRUE(outlines_container->GetVisible());
}

// Tests that pressing on the send button takes the user to the Q&A View and the
// back button takes the user back to the main view.
TEST_F(MahiPanelViewTest, TransitionToQuestionAnswerView) {
  const auto* const summary_outlines_section = panel_view()->GetViewByID(
      mahi_constants::ViewId::kSummaryOutlinesSection);
  const auto* const question_answer_view =
      panel_view()->GetViewByID(mahi_constants::ViewId::kQuestionAnswerView);
  const auto* const send_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kAskQuestionSendButton);
  const auto* const back_button =
      panel_view()->GetViewByID(mahi_constants::ViewId::kBackButton);

  // Assert that the views to be tested exist.
  ASSERT_TRUE(summary_outlines_section);
  ASSERT_TRUE(question_answer_view);
  ASSERT_TRUE(back_button);
  ASSERT_TRUE(send_button);

  // Initially the Summary Outlines section is visible.
  EXPECT_TRUE(summary_outlines_section->GetVisible());
  EXPECT_FALSE(question_answer_view->GetVisible());
  EXPECT_FALSE(back_button->GetVisible());
  EXPECT_TRUE(send_button->GetVisible());

  // Pressing the send button should take the user to the Q&A view.
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

}  // namespace ash
