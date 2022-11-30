// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/privacy_container_view.h"

#include <memory>

#include "ash/app_list/app_list_test_view_delegate.h"
#include "ash/app_list/views/suggested_content_info_view.h"
#include "ash/public/cpp/test/test_app_list_color_provider.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace test {

class PrivacyContainerViewTest : public views::ViewsTestBase {
 public:
  PrivacyContainerViewTest() = default;
  ~PrivacyContainerViewTest() override = default;
  PrivacyContainerViewTest(const PrivacyContainerViewTest&) = delete;
  PrivacyContainerViewTest& operator=(const PrivacyContainerViewTest&) = delete;

 protected:
  AppListTestViewDelegate* view_delegate() { return &view_delegate_; }
  PrivacyContainerView* view() { return view_.get(); }

  SuggestedContentInfoView* suggested_content_view() {
    return view_->suggested_content_info_view_;
  }

  void CreateView() {
    view_ = std::make_unique<PrivacyContainerView>(&view_delegate_);
    view_->Update();
  }

 private:
  TestAppListColorProvider color_provider_;  // Needed by AppListView.
  AppListTestViewDelegate view_delegate_;
  std::unique_ptr<PrivacyContainerView> view_;
};

TEST_F(PrivacyContainerViewTest, ShowSuggestedContentInfo) {
  view_delegate()->SetShouldShowSuggestedContentInfo(true);
  CreateView();

  // Only Suggested Content info should be visible.
  ASSERT_TRUE(suggested_content_view());
  EXPECT_TRUE(suggested_content_view()->GetVisible());
  EXPECT_EQ(view()->GetResultViewAt(0), suggested_content_view());

  // Disable Suggested Content info.
  view_delegate()->SetShouldShowSuggestedContentInfo(false);
  view()->Update();

  EXPECT_FALSE(suggested_content_view()->GetVisible());
  EXPECT_FALSE(view()->GetResultViewAt(0));
}

}  // namespace test
}  // namespace ash
