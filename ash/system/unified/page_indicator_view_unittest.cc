// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/page_indicator_view.h"

#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"

namespace ash {

namespace {
int kPageCount = 10;
}

class PageIndicatorViewTest : public NoSessionAshTestBase {
 public:
  PageIndicatorViewTest() = default;

  PageIndicatorViewTest(const PageIndicatorViewTest&) = delete;
  PageIndicatorViewTest& operator=(const PageIndicatorViewTest&) = delete;

  ~PageIndicatorViewTest() override = default;

  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());

    unified_view_ = std::make_unique<UnifiedSystemTrayView>(
        controller_.get(), true /* initially_expanded */);
  }

  void TearDown() override {
    controller_.reset();
    unified_view_.reset();
    model_.reset();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  int GetButtonCount() {
    return page_indicator_view()->buttons_container()->children().size();
  }

  bool IsPageSelected(int index) {
    return page_indicator_view()->IsPageSelectedForTesting(index);
  }

  PaginationModel* pagination_model() { return model_->pagination_model(); }
  PageIndicatorView* page_indicator_view() {
    return unified_view_->page_indicator_view_for_test();
  }
  UnifiedSystemTrayView* unified_view() { return unified_view_.get(); }

 private:
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<UnifiedSystemTrayView> unified_view_;
};

// Number of buttons is equal to total pages in PaginationModel.
TEST_F(PageIndicatorViewTest, ButtonForEachPage) {
  for (int i = 0; i < kPageCount; i++) {
    pagination_model()->SetTotalPages(i);
    EXPECT_EQ(i, GetButtonCount());
  }
}

// Single button corresponding to page in PaginationModel is set to selected.
TEST_F(PageIndicatorViewTest, SelectPage) {
  pagination_model()->SetTotalPages(kPageCount);

  for (int i = 0; i < kPageCount; i++) {
    pagination_model()->SelectPage(i, false /* animate */);
    EXPECT_TRUE(IsPageSelected(i));
    for (int j = 0; j < kPageCount; j++) {
      if (i == j)
        continue;

      EXPECT_FALSE(IsPageSelected(j));
    }
  }
}

TEST_F(PageIndicatorViewTest, ExpandAndCollapse) {
  int cur_height;
  int prev_height;
  double expanded_increments[] = {0.90, 0.75, 0.5, 0.25, 0.10};

  pagination_model()->SetTotalPages(kPageCount);

  // PageIndicatorView has decreasing height as the expanded amount is
  // decreased.
  prev_height = page_indicator_view()->GetContentsBounds().height();
  for (double i : expanded_increments) {
    unified_view()->SetExpandedAmount(i);
    cur_height = page_indicator_view()->GetContentsBounds().height();
    EXPECT_GE(prev_height, cur_height);
    prev_height = cur_height;
  }

  // PageIndicatorView has zero height when collapsed.
  unified_view()->SetExpandedAmount(0.00);
  EXPECT_EQ(page_indicator_view()->GetContentsBounds().height(), 0);
}

}  // namespace ash
