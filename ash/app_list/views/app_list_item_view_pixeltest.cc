// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/strings/string_util.h"

namespace ash {

class AppListItemViewPixelTest : public AshTestBase,
                                 public testing::WithParamInterface<
                                     std::tuple</*use_tablet_mode=*/bool,
                                                /*use_dense_ui=*/bool,
                                                /*use_rtl=*/bool,
                                                /*is_new_install=*/bool,
                                                /*has_notification=*/bool>> {
 public:
  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = use_rtl();
    return init_params;
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // As per `app_list_config_provider.cc`, dense values are used for screens
    // with width OR height <= 675.
    UpdateDisplay(use_dense_ui() ? "800x600" : "1200x800");

    app_list_test_model_ = std::make_unique<test::AppListTestModel>();
    search_model_ = std::make_unique<SearchModel>();
    Shell::Get()->app_list_controller()->SetActiveModel(
        /*profile_id=*/1, app_list_test_model_.get(), search_model_.get());
  }

  void CreateAppListItem(const std::string& name) {
    AppListItem* item = app_list_test_model_->CreateAndAddItem(name + "_id");
    item->SetName(name);
    item->SetIsNewInstall(is_new_install());
    item->UpdateNotificationBadge(has_notification());
  }

  AppListItemView* GetItemViewAt(size_t index) {
    auto* const helper = GetAppListTestHelper();
    if (use_tablet_mode())
      return helper->GetRootPagedAppsGridView()->GetItemViewAt(index);
    return helper->GetScrollableAppsGridView()->GetItemViewAt(index);
  }

  std::string GenerateScreenshotName() {
    std::string stringified_params = base::JoinString(
        {use_tablet_mode() ? "tablet_mode" : "clamshell_mode",
         use_dense_ui() ? "dense_ui" : "regular_ui", use_rtl() ? "rtl" : "ltr",
         is_new_install() ? "new_install=true" : "new_install=false",
         has_notification() ? "has_notification=true"
                            : "has_notification=false"},
        "|");
    return base::JoinString({"app_list_item_view", stringified_params, "rev_0"},
                            ".");
  }

  bool use_tablet_mode() const { return std::get<0>(GetParam()); }
  bool use_dense_ui() const { return std::get<1>(GetParam()); }
  bool use_rtl() const { return std::get<2>(GetParam()); }
  bool is_new_install() const { return std::get<3>(GetParam()); }
  bool has_notification() const { return std::get<4>(GetParam()); }

 private:
  std::unique_ptr<test::AppListTestModel> app_list_test_model_;
  std::unique_ptr<SearchModel> search_model_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AppListItemViewPixelTest,
    testing::Combine(/*use_tablet_mode=*/testing::Bool(),
                     /*use_dense_ui=*/testing::Bool(),
                     /*use_rtl=*/testing::Bool(),
                     /*is_new_install=*/testing::Bool(),
                     /*has_notification=*/testing::Bool()));

TEST_P(AppListItemViewPixelTest, AppListItemView) {
  CreateAppListItem("App");
  CreateAppListItem("App with a loooooooong name");

  if (use_tablet_mode())
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  else
    GetAppListTestHelper()->ShowAppList();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName(), GetItemViewAt(0), GetItemViewAt(1)));
}

}  // namespace ash
