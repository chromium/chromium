// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/drag_drop/drag_drop_controller_test_api.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class AppListItemViewPixelTestBase : public AshTestBase {
 public:
  AppListItemViewPixelTestBase(bool use_folder_icon_refresh,
                               bool use_tablet_mode,
                               bool use_dense_ui,
                               bool use_rtl,
                               bool is_new_install,
                               bool has_notification,
                               bool enable_promise_icons)
      : use_folder_icon_refresh_(use_folder_icon_refresh),
        use_tablet_mode_(use_tablet_mode),
        use_dense_ui_(use_dense_ui),
        use_rtl_(use_rtl),
        is_new_install_(is_new_install),
        has_notification_(has_notification),
        enable_promise_icons_(enable_promise_icons) {}

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.under_rtl = use_rtl();
    return init_params;
  }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureStates(
        {{ash::features::kPromiseIcons, enable_promise_icons()}});

    AshTestBase::SetUp();

    // As per `app_list_config_provider.cc`, dense values are used for screens
    // with width OR height <= 675.
    UpdateDisplay(use_dense_ui() ? "800x600" : "1200x800");
      auto* drag_controller = ShellTestApi().drag_drop_controller();
      drag_drop_controller_test_api_ =
          std::make_unique<DragDropControllerTestApi>(drag_controller);
      drag_controller->SetDisableNestedLoopForTesting(true);
  }

  void TearDown() override {
    drag_drop_controller_test_api_.reset();
    AshTestBase::TearDown();
  }

  // Creates multiple folders that contain from 1 app to `max_items` apps
  // respectively.
  void CreateFoldersContainingDifferentNumOfItems(int max_items) {
    AppListFolderItem* folder_item;
    for (int i = 1; i <= max_items; ++i) {
      if (i == 1) {
        folder_item = GetAppListTestHelper()->model()->CreateSingleItemFolder(
            "folder_id", "item_id");
      } else {
        folder_item =
            GetAppListTestHelper()->model()->CreateAndPopulateFolderWithApps(i);
      }
      // Update the notification state of the first app in the folder to
      // simulate that there exists an app with notifications in the folder.
      folder_item->item_list()->item_at(0)->UpdateNotificationBadge(
          has_notification());
    }
  }

  void CreateAppListItem(const std::string& name) {
    AppListItem* item =
        GetAppListTestHelper()->model()->CreateAndAddItem(name + "_id");
    item->SetName(name);
    item->SetIsNewInstall(is_new_install());
    item->UpdateNotificationBadge(has_notification());
  }

  void ShowAppList() {
    if (use_tablet_mode()) {
      ash::TabletModeControllerTestApi().EnterTabletMode();
    } else {
      GetAppListTestHelper()->ShowAppList();
    }
  }

  AppsGridView* GetAppsGridView() {
    if (use_tablet_mode()) {
      return GetAppListTestHelper()->GetRootPagedAppsGridView();
    }

    return GetAppListTestHelper()->GetScrollableAppsGridView();
  }

  AppListItemView* GetItemViewAt(size_t index) {
    return GetAppsGridView()->GetItemViewAt(index);
  }

  std::string GenerateScreenshotName() {
    std::vector<std::string> parameters = {
        use_tablet_mode() ? "tablet_mode" : "clamshell_mode",
        use_dense_ui() ? "dense_ui" : "regular_ui", use_rtl() ? "rtl" : "ltr",
        is_new_install() ? "new_install=true" : "new_install=false",
        has_notification() ? "has_notification=true"
                           : "has_notification=false"};
    std::string stringified_params = base::JoinString(parameters, "|");
    return base::JoinString({"app_list_item_view", stringified_params}, ".");
  }

  views::Widget* GetDraggedWidget() {
    return drag_drop_controller_test_api_->drag_image_widget();
  }

  bool use_folder_icon_refresh() const { return use_folder_icon_refresh_; }
  bool use_tablet_mode() const { return use_tablet_mode_; }
  bool use_dense_ui() const { return use_dense_ui_; }
  bool use_rtl() const { return use_rtl_; }
  bool is_new_install() const { return is_new_install_; }
  bool has_notification() const { return has_notification_; }
  bool enable_promise_icons() const { return enable_promise_icons_; }

 private:
  const bool use_folder_icon_refresh_;
  const bool use_tablet_mode_;
  const bool use_dense_ui_;
  const bool use_rtl_;
  const bool is_new_install_;
  const bool has_notification_;
  const bool enable_promise_icons_;

  std::unique_ptr<DragDropControllerTestApi> drag_drop_controller_test_api_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class AppListItemViewPixelTest
    : public AppListItemViewPixelTestBase,
      public testing::WithParamInterface<
          std::tuple</*use_folder_icon_refresh=*/bool,
                     /*use_tablet_mode=*/bool,
                     /*use_dense_ui=*/bool,
                     /*use_rtl=*/bool,
                     /*is_new_install=*/bool,
                     /*has_notification=*/bool>> {
 public:
  AppListItemViewPixelTest()
      : AppListItemViewPixelTestBase(
            /*use_folder_icon_refresh=*/std::get<0>(GetParam()),
            /*use_tablet_mode=*/std::get<1>(GetParam()),
            /*use_dense_ui=*/std::get<2>(GetParam()),
            /*use_rtl=*/std::get<3>(GetParam()),
            /*is_new_install=*/std::get<4>(GetParam()),
            /*has_notification=*/std::get<5>(GetParam()),
            /*enable_promise_icons=*/false) {}
};

INSTANTIATE_TEST_SUITE_P(All,
                         AppListItemViewPixelTest,
                         testing::Combine(
                             /*use_folder_icon_refresh=*/testing::Bool(),
                             /*use_tablet_mode=*/testing::Bool(),
                             /*use_dense_ui=*/testing::Bool(),
                             /*use_rtl=*/testing::Bool(),
                             /*is_new_install=*/testing::Bool(),
                             /*has_notification=*/testing::Bool()));

TEST_P(AppListItemViewPixelTest, AppListItemView) {
  CreateAppListItem("App");
  CreateAppListItem("App with a loooooooong name");

  ShowAppList();
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName(), /*revision_number=*/5, GetItemViewAt(0),
      GetItemViewAt(1)));
}

// Verifies the layout of the item icons inside a folder.
TEST_P(AppListItemViewPixelTest, AppListFolderItemsLayoutInIcon) {
  // Skip the case where the apps are newly installed as it doesn't change the
  // folder icons.
  if (!use_folder_icon_refresh() || is_new_install()) {
    return;
  }

  // Reset any configs set by previous tests so that
  // ItemIconInFolderIconMargin() in app_list_config.cc is correctly
  // initialized. Can be removed if folder icon refresh is set as default.
  AppListConfigProvider::Get().ResetForTesting();

  // To test the item counter on folder icons, set the maximum number of the
  // items in a folder to 5.
  const int max_items_in_folder = 5;
  CreateFoldersContainingDifferentNumOfItems(max_items_in_folder);
  ShowAppList();

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName(), /*revision_number=*/9, GetItemViewAt(0),
      GetItemViewAt(1), GetItemViewAt(2), GetItemViewAt(3), GetItemViewAt(4)));
}

// Verifies the folder icon is extended when an app is dragged upon it.
TEST_P(AppListItemViewPixelTest, AppListFolderIconExtendedState) {
  // Skip the case where the apps are newly installed as it doesn't change the
  // folder icons.
  if (!use_folder_icon_refresh() || is_new_install()) {
    return;
  }

  // Reset any configs set by previous tests so that
  // ItemIconInFolderIconMargin() in app_list_config.cc is correctly
  // initialized. Can be removed if folder icon refresh is set as default.
  AppListConfigProvider::Get().ResetForTesting();

  // To test the item counter on folder icons, set the maximum number of the
  // items in a folder to 5.
  const int max_items_in_folder = 5;
  CreateFoldersContainingDifferentNumOfItems(max_items_in_folder);
  CreateAppListItem("App");
  ShowAppList();

  // For tablet mode, simulate that a drag starts and enter the cardified state.
  if (use_tablet_mode()) {
    GetAppListTestHelper()
        ->GetRootPagedAppsGridView()
        ->MaybeStartCardifiedView();
  }

  // Simulate that there is an app dragged onto it for each folder.
  for (int i = 0; i < max_items_in_folder; ++i) {
    GetItemViewAt(i)->OnDraggedViewEnter();
  }

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName(), /*revision_number=*/10, GetItemViewAt(0),
      GetItemViewAt(1), GetItemViewAt(2), GetItemViewAt(3), GetItemViewAt(4)));

  // Reset the states.
  for (int i = 0; i < max_items_in_folder; ++i) {
    GetItemViewAt(i)->OnDraggedViewExit();
  }
  if (use_tablet_mode()) {
    GetAppListTestHelper()->GetRootPagedAppsGridView()->MaybeEndCardifiedView();
  }
}

// Vefifies the dragged folder icon proxy is correctly created.
TEST_P(AppListItemViewPixelTest, DraggedAppListFolderIcon) {
  // Skip the case where the apps are newly installed or have notifications as
  // they don't change the folder icons.
  if (!use_folder_icon_refresh() || is_new_install() || has_notification()) {
    return;
  }

  // Reset any configs set by previous tests so that
  // ItemIconInFolderIconMargin() in app_list_config.cc is correctly
  // initialized. Can be removed if folder icon refresh is set as default.
  AppListConfigProvider::Get().ResetForTesting();

  // Set the maximum number of the items in a folder that we want to test to 4.
  const int max_items_in_folder = 4;
  CreateFoldersContainingDifferentNumOfItems(max_items_in_folder);
  ShowAppList();

  auto* event_generator = GetEventGenerator();
  AppsGridView* apps_grid_view = GetAppsGridView();
  gfx::Point grid_center = apps_grid_view->GetBoundsInScreen().CenterPoint();

  // Create a folder item view list for folders with different number of items.
  // This is used instead of GetItemViewAt() to prevent reordering while
  // dragging each folder.
  std::vector<AppListItemView*> folder_list;
  for (int i = 0; i < max_items_in_folder; ++i) {
    folder_list.push_back(GetItemViewAt(i));
  }

  auto verify_folder_widget =
      [&](int number_of_items) {
        std::string filename =
            base::NumberToString(number_of_items) + "_items_folder";
        EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
            base::JoinString({GenerateScreenshotName(), filename}, "."),
            /*revision_number=*/6, GetDraggedWidget()));
      };

  for (size_t i = 0; i < max_items_in_folder; ++i) {
    gfx::Point folder_icon_center =
        folder_list[i]->GetIconBoundsInScreen().CenterPoint();

    std::list<base::OnceClosure> tasks;
    tasks.push_back(base::BindLambdaForTesting([&]() {
      if (use_tablet_mode()) {
        event_generator->PressTouch(folder_icon_center);
        folder_list[i]->FireTouchDragTimerForTest();
      } else {
        event_generator->MoveMouseTo(folder_icon_center);
        event_generator->PressLeftButton();
        folder_list[i]->FireMouseDragTimerForTest();
      }
    }));
    tasks.push_back(base::BindLambdaForTesting([&]() {
      if (use_tablet_mode()) {
        event_generator->MoveTouch(grid_center);
      } else {
        event_generator->MoveMouseTo(grid_center);
      }
      test::AppsGridViewTestApi(apps_grid_view).WaitForItemMoveAnimationDone();
    }));
    tasks.push_back(base::BindLambdaForTesting(
        [&]() { verify_folder_widget(/*number_of_items=*/i + 1); }));
    tasks.push_back(base::BindLambdaForTesting([&]() {
      if (use_tablet_mode()) {
        event_generator->ReleaseTouch();
      } else {
        event_generator->ReleaseLeftButton();
      }
    }));

    MaybeRunDragAndDropSequenceForAppList(&tasks,
                                          /*is_touch=*/use_tablet_mode());
  }
}

class AppListViewPromiseAppPixelTest
    : public AppListItemViewPixelTestBase,
      public testing::WithParamInterface<std::tuple<
          /*use_tablet_mode=*/bool,
          /*use_dense_ui=*/bool,
          /*use_rtl=*/bool>> {
 public:
  AppListViewPromiseAppPixelTest()
      : AppListItemViewPixelTestBase(
            /*use_folder_icon_refresh=*/false,
            /*use_tablet_mode=*/std::get<0>(GetParam()),
            /*use_dense_ui=*/std::get<1>(GetParam()),
            /*use_rtl=*/std::get<2>(GetParam()),
            /*is_new_install=*/false,
            /*has_notification=*/false,
            /*enable_promise_icons=*/true) {}

  AppListItem* CreateAppListPromiseItem(const std::string& name) {
    return GetAppListTestHelper()->model()->CreateAndAddPromiseItem(name +
                                                                    "_id");
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         AppListViewPromiseAppPixelTest,
                         testing::Combine(
                             /*use_tablet_mode=*/testing::Bool(),
                             /*use_dense_ui=*/testing::Bool(),
                             /*use_rtl=*/testing::Bool()));

TEST_P(AppListViewPromiseAppPixelTest, PromiseAppWaiting) {
  // Reset any configs set by previous tests so that
  // ItemIconInFolderIconMargin() in app_list_config.cc is correctly
  // initialized. Can be removed if folder icon refresh is set as default.
  AppListConfigProvider::Get().ResetForTesting();
  CreateAppListPromiseItem("PromiseApp");
  AppListItem* placeholder = CreateAppListPromiseItem("PromiseApp_placeholder");
  placeholder->SetDefaultIconAndColor(placeholder->GetDefaultIcon(),
                                      placeholder->GetDefaultIconColor(),
                                      /*is_placeholder_icon=*/true);
  ShowAppList();
  EXPECT_EQ(GetItemViewAt(0)->item()->progress(), -1.0f);
  EXPECT_EQ(GetItemViewAt(0)->item()->app_status(), AppStatus::kPending);
  EXPECT_EQ(GetItemViewAt(1)->item()->progress(), -1.0f);
  EXPECT_EQ(GetItemViewAt(1)->item()->app_status(), AppStatus::kPending);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      base::JoinString({"promise_app_waiting", GenerateScreenshotName()}, "."),
      /*revision_number=*/3, GetItemViewAt(0), GetItemViewAt(1)));
}

TEST_P(AppListViewPromiseAppPixelTest, PromiseAppInstalling) {
  // Reset any configs set by previous tests so that
  // ItemIconInFolderIconMargin() in app_list_config.cc is correctly
  // initialized. Can be removed if folder icon refresh is set as default.
  AppListConfigProvider::Get().ResetForTesting();
  AppListItem* item = CreateAppListPromiseItem("PromiseApp");
  AppListItem* placeholder = CreateAppListPromiseItem("PromiseApp_placeholder");
  placeholder->SetDefaultIconAndColor(placeholder->GetDefaultIcon(),
                                      placeholder->GetDefaultIconColor(),
                                      /*is_placeholder_icon=*/true);

  // Start install progress bar.
  item->SetAppStatus(AppStatus::kInstalling);
  item->SetProgress(0.8f);
  placeholder->SetAppStatus(AppStatus::kInstalling);
  placeholder->SetProgress(0.8f);
  ShowAppList();

  EXPECT_EQ(GetItemViewAt(0)->item()->progress(), 0.8f);
  EXPECT_EQ(GetItemViewAt(0)->item()->app_status(), AppStatus::kInstalling);
  EXPECT_EQ(GetItemViewAt(1)->item()->progress(), 0.8f);
  EXPECT_EQ(GetItemViewAt(1)->item()->app_status(), AppStatus::kInstalling);
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      base::JoinString({"promise_app_installing", GenerateScreenshotName()},
                       "."),
      /*revision_number=*/3, GetItemViewAt(0), GetItemViewAt(1)));
}

}  // namespace ash
