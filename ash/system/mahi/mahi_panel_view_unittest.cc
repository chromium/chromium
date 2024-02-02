// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/style/icon_button.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/components/mahi/public/cpp/fake_mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
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

class MahiPanelViewTest : public AshTestBase {
 public:
  MahiPanelViewTest() = default;
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

    fake_mahi_manager_ = std::make_unique<chromeos::FakeMahiManager>();
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

  chromeos::FakeMahiManager* fake_mahi_manager() {
    return fake_mahi_manager_.get();
  }

  MahiPanelView* panel_view() { return panel_view_; }

  views::Widget* widget() { return widget_.get(); }

 private:
  std::unique_ptr<chromeos::FakeMahiManager> fake_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
  raw_ptr<MahiPanelView> panel_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MockNewWindowDelegate> new_window_delegate_;
  std::unique_ptr<TestNewWindowDelegateProvider> delegate_provider_;
};

// Makes sure that the summary text is set correctly in ctor with different
// texts.
TEST_F(MahiPanelViewTest, SummaryText) {
  auto* test_text1 = u"test summary text 1";
  fake_mahi_manager()->set_summary_text(test_text1);
  auto mahi_view1 = std::make_unique<MahiPanelView>();
  auto* summary_label1 = static_cast<views::Label*>(
      mahi_view1->GetViewByID(MahiPanelView::ViewId::kSummaryLabel));
  EXPECT_EQ(test_text1, summary_label1->GetText());

  auto* test_text2 = u"test summary text 2";
  fake_mahi_manager()->set_summary_text(test_text2);
  auto mahi_view2 = std::make_unique<MahiPanelView>();
  auto* summary_label2 = static_cast<views::Label*>(
      mahi_view2->GetViewByID(MahiPanelView::ViewId::kSummaryLabel));
  EXPECT_EQ(test_text2, summary_label2->GetText());
}

TEST_F(MahiPanelViewTest, FeedbackButtons) {
  base::HistogramTester histogram_tester;

  LeftClickOn(
      panel_view()->GetViewByID(MahiPanelView::ViewId::kThumbsUpButton));
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 0);

  LeftClickOn(
      panel_view()->GetViewByID(MahiPanelView::ViewId::kThumbsDownButton));
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 1);
}

TEST_F(MahiPanelViewTest, CloseButton) {
  EXPECT_FALSE(widget()->IsClosed());

  LeftClickOn(panel_view()->GetViewByID(MahiPanelView::ViewId::kCloseButton));

  EXPECT_TRUE(widget()->IsClosed());
}

TEST_F(MahiPanelViewTest, LearnMoreLink) {
  EXPECT_CALL(new_window_delegate(),
              OpenUrl(GURL(mahi_constants::kLearnMorePage),
                      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      NewWindowDelegate::Disposition::kNewForegroundTab));
  LeftClickOn(panel_view()->GetViewByID(MahiPanelView::ViewId::kLearnMoreLink));
}

}  // namespace
}  // namespace ash
