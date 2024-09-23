// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/quick_settings_notice_view.h"

#include <memory>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class TestQuickSettingsNoticeView : public QuickSettingsNoticeView {
 public:
  explicit TestQuickSettingsNoticeView()
      : QuickSettingsNoticeView(
            VIEW_ID_QS_EOL_NOTICE_BUTTON,
            QsButtonCatalogName::kEolNoticeButton,
            IDS_ASH_QUICK_SETTINGS_BUBBLE_EOL_NOTICE,
            kUpgradeIcon,
            base::BindRepeating(&TestQuickSettingsNoticeView::OnPressed,
                                base::Unretained(this))) {}

  ~TestQuickSettingsNoticeView() override = default;

  int pressed_count() const { return pressed_count_; }

 protected:
  int GetShortTextId() const override {
    return IDS_ASH_QUICK_SETTINGS_BUBBLE_EOL_NOTICE_SHORT;
  }

 private:
  void OnPressed(const ui::Event& event) { pressed_count_++; }

  int pressed_count_ = 0;
};

class QuickSettingsNoticeViewTest : public AshTestBase {
 protected:
  void SetUp() override {
    AshTestBase::SetUp();

    // Place the view in a large views::Widget so the buttons are clickable.
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    view_ = widget_->SetContentsView(
        std::make_unique<TestQuickSettingsNoticeView>());
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();

    AshTestBase::TearDown();
  }

  base::HistogramTester histogram_tester_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<TestQuickSettingsNoticeView> view_ = nullptr;
};

TEST_F(QuickSettingsNoticeViewTest, ButtonPress) {
  EXPECT_TRUE(view_->GetVisible());
  EXPECT_EQ(view_->pressed_count(), 0);
  histogram_tester_.ExpectUniqueSample("Ash.QuickSettings.Button.Activated",
                                       QsButtonCatalogName::kEolNoticeButton,
                                       view_->pressed_count());

  LeftClickOn(view_);
  EXPECT_EQ(view_->pressed_count(), 1);
  histogram_tester_.ExpectUniqueSample("Ash.QuickSettings.Button.Activated",
                                       QsButtonCatalogName::kEolNoticeButton,
                                       view_->pressed_count());

  LeftClickOn(view_);
  EXPECT_EQ(view_->pressed_count(), 2);
  histogram_tester_.ExpectUniqueSample("Ash.QuickSettings.Button.Activated",
                                       QsButtonCatalogName::kEolNoticeButton,
                                       view_->pressed_count());
}

TEST_F(QuickSettingsNoticeViewTest, ShortText) {
  EXPECT_TRUE(view_->GetVisible());
  EXPECT_EQ(view_->GetText(), l10n_util::GetStringUTF16(
                                  IDS_ASH_QUICK_SETTINGS_BUBBLE_EOL_NOTICE));

  view_->SetNarrowLayout(true);
  EXPECT_EQ(view_->GetText(),
            l10n_util::GetStringUTF16(
                IDS_ASH_QUICK_SETTINGS_BUBBLE_EOL_NOTICE_SHORT));

  view_->SetNarrowLayout(false);
  EXPECT_EQ(view_->GetText(), l10n_util::GetStringUTF16(
                                  IDS_ASH_QUICK_SETTINGS_BUBBLE_EOL_NOTICE));
}

}  // namespace ash
