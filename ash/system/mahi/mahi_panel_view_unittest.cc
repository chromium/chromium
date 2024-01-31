// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <memory>

#include "ash/style/icon_button.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/components/mahi/public/cpp/fake_mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class MahiPanelViewTest : public AshTestBase {
 public:
  MahiPanelViewTest() = default;
  MahiPanelViewTest(const MahiPanelViewTest&) = delete;
  MahiPanelViewTest& operator=(const MahiPanelViewTest&) = delete;
  ~MahiPanelViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
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
  }

  chromeos::FakeMahiManager* fake_mahi_manager() {
    return fake_mahi_manager_.get();
  }

  MahiPanelView* panel_view() { return panel_view_; }

 private:
  std::unique_ptr<chromeos::FakeMahiManager> fake_mahi_manager_;
  std::unique_ptr<chromeos::ScopedMahiManagerSetter> scoped_setter_;
  raw_ptr<MahiPanelView> panel_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
};

// Makes sure that the summary text is set correctly in ctor with different
// texts.
TEST_F(MahiPanelViewTest, SummaryText) {
  auto* test_text1 = u"test summary text 1";
  fake_mahi_manager()->set_summary_text(test_text1);
  auto mahi_view1 = std::make_unique<MahiPanelView>();
  EXPECT_EQ(test_text1, mahi_view1->summary_label_for_test()->GetText());

  auto* test_text2 = u"test summary text 2";
  fake_mahi_manager()->set_summary_text(test_text2);
  auto mahi_view2 = std::make_unique<MahiPanelView>();
  EXPECT_EQ(test_text2, mahi_view2->summary_label_for_test()->GetText());
}

TEST_F(MahiPanelViewTest, FeedbackButtons) {
  base::HistogramTester histogram_tester;

  LeftClickOn(panel_view()->thumbs_up_button());
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 0);

  LeftClickOn(panel_view()->thumbs_down_button());
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     true, 1);
  histogram_tester.ExpectBucketCount(mahi_constants::kMahiFeedbackHistogramName,
                                     false, 1);
}

}  // namespace
}  // namespace ash
