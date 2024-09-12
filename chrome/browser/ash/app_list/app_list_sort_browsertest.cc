// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/safe_sprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

namespace {

using AcceleratorAction = ash::AcceleratorAction;
using MenuType = ash::AppListTestApi::MenuType;
using ReorderAnimationEndState = ash::AppListTestApi::ReorderAnimationEndState;

// An app manifest file's data. The app name and icon paths wait for filling.
constexpr char kManifestData[] =
    R"({ )"
    /**/ R"("description": "fake",)"
    /**/ R"("name": "%s",)"
    /**/ R"("manifest_version": 2,)"
    /**/ R"("version": "0",)"
    /**/ R"("icons": %s,)"
    /**/ R"("app": { )"
    /**/ R"("launch": {)"
    /****/ R"("web_url": "https://www.google.com/")"
    /****/ R"(})"
    /**/ R"(})"
    R"(})";

gfx::ImageSkia CreateImageSkia(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Test app icon loader that generates icons for apps in tests. The app icons
// are monochromatic, and icon colors can be configured per app using
// `SetAppIconColor()`. By default, the loader will generate white icons.
class FakeIconLoader : public apps::IconLoader {
 public:
  FakeIconLoader() = default;
  FakeIconLoader(const FakeIconLoader&) = delete;
  FakeIconLoader& operator=(const FakeIconLoader&) = delete;
  ~FakeIconLoader() override = default;

  void SetAppIconColor(const std::string& app_id, SkColor color) {
    app_icon_colors_[app_id] = color;
  }

  std::unique_ptr<apps::IconLoader::Releaser> LoadIconFromIconKey(
      const std::string& id,
      const apps::IconKey& icon_key,
      apps::IconType icon_type,
      int32_t size_hint_in_dip,
      bool allow_placeholder_icon,
      apps::LoadIconCallback callback) override {
    auto iv = std::make_unique<apps::IconValue>();
    iv->icon_type = icon_type;
    iv->uncompressed = CreateImageSkia(16, 16, GetIconColor(id, SK_ColorWHITE));
    iv->is_placeholder_icon = false;

    std::move(callback).Run(std::move(iv));
    return nullptr;
  }

 private:
  SkColor GetIconColor(const std::string& app_id, SkColor default_color) {
    const auto& color_override = app_icon_colors_.find(app_id);
    if (color_override == app_icon_colors_.end()) {
      return default_color;
    }

    return color_override->second;
  }

  // Contains icon colors registered using `SetAppIconColor()`.
  std::map<std::string, SkColor> app_icon_colors_;
};

}  // namespace

class AppListSortBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  AppListSortBrowserTest() = default;
  AppListSortBrowserTest(const AppListSortBrowserTest&) = delete;
  AppListSortBrowserTest& operator=(const AppListSortBrowserTest&) = delete;
  ~AppListSortBrowserTest() override = default;

 protected:
  void ReorderTopLevelAppsGridAndWaitForCompletion(ash::AppListSortOrder order,
                                                   MenuType menu_type) {
    ReorderAnimationEndState actual_state;
    app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
        order, menu_type, event_generator_.get(),
        /*target_state=*/ReorderAnimationEndState::kCompleted, &actual_state);
    EXPECT_EQ(ReorderAnimationEndState::kCompleted, actual_state);
  }

  // Verifies that app list fade out animation is in progress then runs a
  // closure to interrupt reorder.
  void VerifyFadeOutInProgressAndRunInterruptClosure(
      const std::vector<std::string>& expected_ids_in_order,
      base::OnceClosure interrupt_closure) {
    // Verify that there is active reorder animations.
    EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

    // The app order does not change because the fade out animation is running.
    EXPECT_EQ(GetAppIdsInOrdinalOrder(), expected_ids_in_order);

    // Run the closure to interrupt the fade out animation.
    std::move(interrupt_closure).Run();
  }

  // Registers a closure that turns on/off the tablet mode at the start of the
  // fade out animation to interrupt app list reorder. `tablet_mode_enabled` is
  // true when switching to the tablet mode; `tablet_mode_enabled` is false when
  // switching to the clamshell mode. The tablet mode changes synchronously on
  // animation start to avoid the race condition between the fade out animation
  // completion and the task to change the tablet mode state.
  // See https://crbug.com/1302924
  void RegisterModeSwitchClosureOnFadeOutStarted(bool tablet_mode_enabled) {
    auto switch_mode_closure = base::BindOnce(
        [](bool tablet_mode_enabled) {
          ash::ShellTestApi().SetTabletModeEnabledForTest(tablet_mode_enabled);
        },
        tablet_mode_enabled);

    // NOTE: When the fade out animation is interrupted, the app order should
    // not change.
    app_list_test_api_.AddFadeOutAnimationStartClosure(base::BindOnce(
        &AppListSortBrowserTest::VerifyFadeOutInProgressAndRunInterruptClosure,
        weak_factory_.GetWeakPtr(),
        /*expected_ids_in_order=*/GetAppIdsInOrdinalOrder(),
        std::move(switch_mode_closure)));
  }

  // Returns a list of app ids following the ordinal increasing order.
  std::vector<std::string> GetAppIdsInOrdinalOrder(
      const std::initializer_list<std::string>& ids) {
    AppListModelUpdater* model_updater =
        test::GetModelUpdater(AppListClientImpl::GetInstance());
    std::vector<std::string> copy_ids(ids);
    std::sort(copy_ids.begin(), copy_ids.end(),
              [model_updater](const std::string& id1, const std::string& id2) {
                return model_updater->FindItem(id1)->position().LessThan(
                    model_updater->FindItem(id2)->position());
              });
    return copy_ids;
  }

  std::vector<std::string> GetAppIdsInOrdinalOrder() {
    return GetAppIdsInOrdinalOrder({app1_id_, app2_id_, app3_id_});
  }

  // Similar to `GetAppIdsInOrdinalOrder()` but app ordinal positions are
  // fetched from the app list syncable service rather than the model updater.
  std::vector<std::string> GetAppIdsInPermanentOrdinalOrder() {
    app_list::AppListSyncableService* app_list_syncable_service =
        app_list::AppListSyncableServiceFactory::GetForProfile(profile());
    std::vector<std::string> ids({app1_id_, app2_id_, app3_id_});
    std::sort(
        ids.begin(), ids.end(),
        [app_list_syncable_service](const std::string& id1,
                                    const std::string& id2) {
          return app_list_syncable_service->GetSyncItem(id1)
              ->item_ordinal.LessThan(
                  app_list_syncable_service->GetSyncItem(id2)->item_ordinal);
        });
    return ids;
  }

  ash::AppListSortOrder GetPermanentSortingOrder() {
    return static_cast<ash::AppListSortOrder>(
        profile()->GetPrefs()->GetInteger(prefs::kAppListPreferredOrder));
  }

  // extensions::ExtensionBrowserTest:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    AppListClientImpl* client = AppListClientImpl::GetInstance();
    ASSERT_TRUE(client);
    client->UpdateProfile();

    // Start in tablet mode.
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);
    WaitForAppListTransitionAnimation();

    // Ensure async callbacks are run.
    base::RunLoop().RunUntilIdle();

    // Shows the app list which is initially behind a window in tablet mode.
    ash::AcceleratorController::Get()->PerformActionIfEnabled(
        AcceleratorAction::kToggleAppList, {});

    const int default_app_count = app_list_test_api_.GetTopListItemCount();

    // Assume that there are two default apps.
    ASSERT_EQ(2, app_list_test_api_.GetTopListItemCount());

    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->OverrideInnerIconLoaderForTesting(&icon_loader_);

    app1_id_ = LoadExtension(test_data_dir_.AppendASCII("app1"))->id();
    ASSERT_FALSE(app1_id_.empty());
    SetTestAppIconColor(app1_id_, SK_ColorBLUE);

    app2_id_ = LoadExtension(test_data_dir_.AppendASCII("app2"))->id();
    ASSERT_FALSE(app2_id_.empty());
    SetTestAppIconColor(app2_id_, SK_ColorRED);

    app3_id_ = LoadExtension(test_data_dir_.AppendASCII("app3"))->id();
    ASSERT_FALSE(app3_id_.empty());
    SetTestAppIconColor(app3_id_, SK_ColorGREEN);

    EXPECT_EQ(default_app_count + 3, app_list_test_api_.GetTopListItemCount());

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());
  }

  void TearDownOnMainThread() override {
    app_list_test_api_.VerifyTopLevelItemVisibility();
    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  void SetTestAppIconColor(const std::string& app_id, SkColor color) {
    icon_loader_.SetAppIconColor(app_id, color);
    // Force icon reload after setting the test color.
    // We cannot call LoadAppIcon directly because we need to invalidate the
    // icon color cache. So we use `IncrementIconVersion()` to remove the
    // icon color cache entry and trigger icon loading.
    test::GetModelUpdater(AppListClientImpl::GetInstance())
        ->FindItem(app_id)
        ->IncrementIconVersion();
  }

  // Helps to prevent flakiness due to conflicting animations (`AppListView`
  // closing animation aborts already started bubble launcher sort animation
  // indirectly via app list service layer).
  void WaitForAppListTransitionAnimation() {
    ui::LayerAnimationStoppedWaiter().Wait(
        app_list_test_api_.GetAppListViewLayer());
  }

  ash::AppListTestApi app_list_test_api_;
  std::string app1_id_;
  std::string app2_id_;
  std::string app3_id_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  FakeIconLoader icon_loader_;

  base::WeakPtrFactory<AppListSortBrowserTest> weak_factory_{this};
};

// Verifies that the apps in the top level apps grid can be arranged in the
// alphabetical order or sorted by the apps' icon colors using the context menu
// in apps grid view.
// TODO(crbug.com/1267369): Also add a test that verifies the behavior in tablet
// mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, ContextMenuSortItemsInTopLevel) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  base::HistogramTester histograms;
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical, MenuType::kAppListPageMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  histograms.ExpectBucketCount(ash::kClamshellReorderActionHistogram,
                               ash::AppListSortOrder::kNameAlphabetical, 1);
  histograms.ExpectTotalCount(ash::kAppListSortDiscoveryDurationAfterNudge, 1);
  histograms.ExpectTotalCount(ash::kAppListSortDiscoveryDurationAfterActivation,
                              1);

  ReorderTopLevelAppsGridAndWaitForCompletion(ash::AppListSortOrder::kColor,
                                              MenuType::kAppListPageMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
  histograms.ExpectBucketCount(ash::kClamshellReorderActionHistogram,
                               ash::AppListSortOrder::kColor, 1);
}

// Verifies that clearing pref order by moving an item works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, ClearPrefOrderByItemMove) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::ShellTestApi().drag_drop_controller()->SetDisableNestedLoopForTesting(
      true);

  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical, MenuType::kAppListPageMenu);

  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  EXPECT_EQ(ash::AppListToastType::kReorderUndo,
            app_list_test_api_.GetToastType());

  std::string app4_id = LoadExtension(test_data_dir_.AppendASCII("app4"))->id();
  ASSERT_FALSE(app4_id.empty());
  EXPECT_EQ(GetAppIdsInOrdinalOrder({app1_id_, app2_id_, app3_id_, app4_id}),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_, app4_id}));

  EXPECT_EQ(ash::AppListToastType::kNone, app_list_test_api_.GetToastType());
  EXPECT_EQ(ash::AppListSortOrder::kNameAlphabetical,
            GetPermanentSortingOrder());

  base::HistogramTester histograms;
  app_list_test_api_.ReorderItemInRootByDragAndDrop(/*source_index=*/0,
                                                    /*target_index=*/1);
  EXPECT_EQ(ash::AppListSortOrder::kCustom, GetPermanentSortingOrder());
  histograms.ExpectBucketCount(ash::kClamshellPrefOrderClearActionHistogram,
                               ash::AppListOrderUpdateEvent::kItemMoved, 1);
}

// Verifies that the apps in a folder can be arranged in the alphabetical order
// or sorted by the apps' icon colors using the context menu in apps grid view.
// TODO(crbug.com/1267369): Also add a test that verifies the behavior in tablet
// mode.
// Flaky. See https://crbug.com/1423200
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       DISABLED_ContextMenuSortItemsInFolder) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Move apps to one folder.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_, app3_id_});

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical, MenuType::kAppListPageMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderTopLevelAppsGridAndWaitForCompletion(ash::AppListSortOrder::kColor,
                                              MenuType::kAppListPageMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verifies that the apps in the top level apps grid can be arranged in the
// alphabetical order or sorted by the apps' icon colors using the context menu
// in app list item view.
// TODO(crbug.com/1267369): Also add a test that verifies the behavior in tablet
// mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       ContextMenuOnAppListItemSortItemsInTopLevel) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verifies that the apps in a folder can be arranged in the alphabetical order
// or sorted by the apps' icon colors using the context menu in app list item
// view.
// TODO(crbug.com/1267369): Also add a test that verifies the behavior in tablet
// mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       ContextMenuOnAppListItemSortItemsInFolder) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Move apps to one folder.
  app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_, app3_id_});
  views::test::RunScheduledLayout(app_list_test_api_.GetTopLevelAppsGridView());
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verifies that the apps can be arranged in the alphabetical order or sorted by
// the apps' icon colors using the context menu on folder item view.
// TODO(crbug.com/1267369): Also add a test that verifies the behavior in tablet
// mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       ContextMenuOnFolderItemSortItems) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Move apps to one folder.
  app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_, app3_id_});
  views::test::RunScheduledLayout(app_list_test_api_.GetTopLevelAppsGridView());

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderTopLevelAppsGridAndWaitForCompletion(ash::AppListSortOrder::kColor,
                                              MenuType::kAppListFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       SortUsingContextMenuOnFolderChildViewClamshell) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Create an app list folder.
  app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_});
  ash::AppsGridView* top_level_grid =
      app_list_test_api_.GetTopLevelAppsGridView();
  views::test::RunScheduledLayout(top_level_grid);

  // Click on the folder to open it.
  base::RunLoop run_loop;
  app_list_test_api_.SetFolderViewAnimationCallback(run_loop.QuitClosure());

  ash::AppListItemView* folder_item_view =
      app_list_test_api_.FindTopLevelFolderItemView();
  ASSERT_TRUE(folder_item_view);
  event_generator_->MoveMouseTo(
      folder_item_view->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  run_loop.Run();

  ash::AppsGridView* folder_grid = app_list_test_api_.GetFolderAppsGridView();
  EXPECT_TRUE(folder_grid->IsDrawn());
  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtContextMenuInAppsGrid(
      folder_grid, ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu, event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kCompleted, &actual_state);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  EXPECT_FALSE(app_list_test_api_.GetFolderAppsGridView()->IsDrawn());
}

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       FolderNotClosedIfTemporarySortIsCommittedClamshell) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Create an app list folder.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_});
  ash::AppsGridView* top_level_grid =
      app_list_test_api_.GetTopLevelAppsGridView();
  views::test::RunScheduledLayout(top_level_grid);

  // Order apps grid to transition to temporary sort order.
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Click on the folder item to open it.
  base::RunLoop run_loop;
  app_list_test_api_.SetFolderViewAnimationCallback(run_loop.QuitClosure());

  ash::AppListItemView* folder_item_view =
      app_list_test_api_.FindTopLevelFolderItemView();
  ASSERT_TRUE(folder_item_view);
  event_generator_->MoveMouseTo(
      folder_item_view->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  run_loop.Run();

  ash::AppsGridView* folder_grid = app_list_test_api_.GetFolderAppsGridView();
  EXPECT_TRUE(folder_grid->IsDrawn());
  EXPECT_EQ(ash::AppListToastType::kReorderUndo,
            app_list_test_api_.GetToastType());

  // Rename folder to commit the sort order - verify that the folder remained
  // open.
  ash::AppListModelProvider::Get()->model()->delegate()->RequestFolderRename(
      folder_id, "Test folder");
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  EXPECT_TRUE(app_list_test_api_.GetFolderAppsGridView()->IsDrawn());
  EXPECT_EQ(ash::AppListToastType::kNone, app_list_test_api_.GetToastType());
}

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       SortUsingContextMenuOnFolderChildViewTablet) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Create an app list folder.
  app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_});
  ash::AppsGridView* top_level_grid =
      app_list_test_api_.GetTopLevelAppsGridView();
  views::test::RunScheduledLayout(top_level_grid);

  // Click on the folder to open it.
  base::RunLoop run_loop;
  app_list_test_api_.SetFolderViewAnimationCallback(run_loop.QuitClosure());

  ash::AppListItemView* folder_item_view =
      app_list_test_api_.FindTopLevelFolderItemView();
  ASSERT_TRUE(folder_item_view);
  event_generator_->MoveMouseTo(
      folder_item_view->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  run_loop.Run();

  ash::AppsGridView* folder_grid = app_list_test_api_.GetFolderAppsGridView();
  EXPECT_TRUE(folder_grid->IsDrawn());
  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtContextMenuInAppsGrid(
      folder_grid, ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu, event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kCompleted, &actual_state);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  EXPECT_FALSE(app_list_test_api_.GetFolderAppsGridView()->IsDrawn());
}

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       FolderNotClosedIfTemporarySortIsCommittedTablet) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Create an app list folder.
  const std::string folder_id =
      app_list_test_api_.CreateFolderWithApps({app1_id_, app2_id_});
  ash::AppsGridView* top_level_grid =
      app_list_test_api_.GetTopLevelAppsGridView();
  views::test::RunScheduledLayout(top_level_grid);

  // Order apps grid to transition to temporary sort order.
  base::HistogramTester histograms;
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  histograms.ExpectTotalCount(ash::kAppListSortDiscoveryDurationAfterNudge, 1);

  // Click on the folder item to open it.
  base::RunLoop run_loop;
  app_list_test_api_.SetFolderViewAnimationCallback(run_loop.QuitClosure());

  ash::AppListItemView* folder_item_view =
      app_list_test_api_.FindTopLevelFolderItemView();
  ASSERT_TRUE(folder_item_view);
  event_generator_->MoveMouseTo(
      folder_item_view->GetBoundsInScreen().CenterPoint());
  event_generator_->ClickLeftButton();

  run_loop.Run();

  ash::AppsGridView* folder_grid = app_list_test_api_.GetFolderAppsGridView();
  EXPECT_TRUE(folder_grid->IsDrawn());
  EXPECT_EQ(ash::AppListToastType::kReorderUndo,
            app_list_test_api_.GetToastType());

  // Rename folder to commit the sort order - verify that the folder remained
  // open.
  ash::AppListModelProvider::Get()->model()->delegate()->RequestFolderRename(
      folder_id, "Test folder");
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  EXPECT_TRUE(app_list_test_api_.GetFolderAppsGridView()->IsDrawn());
  EXPECT_EQ(ash::AppListToastType::kNone, app_list_test_api_.GetToastType());
}

// Verify that starting a new reorder before the old animation completes works
// as expected.
IN_PROC_BROWSER_TEST_F(
    AppListSortBrowserTest,
    ContextMenuOnAppListItemSortItemsInTopLevelWithoutWaiting) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Trigger name alphabetical sorting.
  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kNameAlphabetical, MenuType::kAppListPageMenu,
      event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kFadeOutAborted,
      &actual_state);

  // Verify that the app order does not change because the animation is ongoing.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Trigger another reorder animation without waiting for the current one and
  // wait until the new animation finishes. The previous animation should be
  // aborted.
  ReorderTopLevelAppsGridAndWaitForCompletion(ash::AppListSortOrder::kColor,
                                              MenuType::kAppListPageMenu);
  EXPECT_EQ(ReorderAnimationEndState::kFadeOutAborted, actual_state);

  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that deleting an item during reorder animation works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       DeleteItemDuringReorderingAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Trigger name alphabetical sorting.
  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kNameAlphabetical, MenuType::kAppListPageMenu,
      event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kFadeOutAborted,
      &actual_state);

  // Verify that the app order does not change because the animation is ongoing.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  UninstallExtension(app3_id_);
  base::RunLoop().RunUntilIdle();

  // Uninstallation should abort the ongoing fade out animation.
  EXPECT_EQ(ReorderAnimationEndState::kFadeOutAborted, actual_state);

  AppListModelUpdater* model_updater =
      test::GetModelUpdater(AppListClientImpl::GetInstance());

  // Verify that `app3_id_` cannot be found from `model_updater_`.
  EXPECT_FALSE(model_updater->FindItem(app3_id_));

  // Note that the temporary sorting state ends when uninstalling an app.
  // Therefore the remaining apps are placed following the alphabetical order.
  EXPECT_TRUE(model_updater->FindItem(app2_id_)->position().GreaterThan(
      model_updater->FindItem(app1_id_)->position()));

  app_list_test_api_.VerifyTopLevelItemVisibility();
}

// Verifies that clicking at the reorder undo toast should revert the temporary
// sorting order in bubble launcher.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, UndoTemporarySortingClamshell) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  base::HistogramTester histograms;
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical, MenuType::kAppListPageMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Wait for one additional frame so that the metric data is collected.
  ui::Compositor* compositor =
      app_list_test_api_.GetTopLevelAppsGridView()->layer()->GetCompositor();
  base::IgnoreResult(
      ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(300)));

  histograms.ExpectTotalCount(
      ash::kClamshellReorderAnimationSmoothnessHistogram, 1);

  // Ensure that the reorder undo toast's bounds update.
  views::test::RunScheduledLayout(app_list_test_api_.GetTopLevelAppsGridView());

  // The toast should be visible.
  EXPECT_EQ(ash::AppListToastType::kReorderUndo,
            app_list_test_api_.GetToastType());

  app_list_test_api_.ClickOnRedoButtonAndWaitForAnimation(
      event_generator_.get());

  // Wait for the metric data to be collected.
  base::IgnoreResult(
      ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(300)));

  // Smoothness of the reorder animation triggered by undo button is recorded.
  histograms.ExpectTotalCount(
      ash::kClamshellReorderAnimationSmoothnessHistogram, 2);

  // Verify that the default app order is recovered.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // The toast should be hidden.
  EXPECT_EQ(ash::AppListToastType::kNone, app_list_test_api_.GetToastType());
}

// Verifies that clicking at the reorder undo toast should revert the temporary
// sorting order in tablet mode.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, UndoTemporarySortingTablet) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  base::HistogramTester histograms;
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  histograms.ExpectBucketCount(ash::kTabletReorderActionHistogram,
                               ash::AppListSortOrder::kNameAlphabetical, 1);

  // The toast should be visible.
  EXPECT_EQ(ash::AppListToastType::kReorderUndo,
            app_list_test_api_.GetToastType());

  // Wait for one additional frame so that the metric data is collected.
  ui::Compositor* compositor =
      app_list_test_api_.GetTopLevelAppsGridView()->layer()->GetCompositor();
  base::IgnoreResult(
      ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(300)));

  histograms.ExpectTotalCount(ash::kTabletReorderAnimationSmoothnessHistogram,
                              1);

  app_list_test_api_.ClickOnRedoButtonAndWaitForAnimation(
      event_generator_.get());

  // Verify that the default app order is recovered.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // The toast should be hidden.
  EXPECT_EQ(ash::AppListToastType::kNone, app_list_test_api_.GetToastType());

  // Wait for the metric data to be collected.
  base::IgnoreResult(
      ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(300)));

  // Smoothness of the reorder animation triggered by undo button is recorded.
  histograms.ExpectTotalCount(ash::kTabletReorderAnimationSmoothnessHistogram,
                              2);
}

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, TransitionToTabletCommitsSort) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Ensure that the reorder undo toast's bounds update.
  views::test::RunScheduledLayout(app_list_test_api_.GetTopLevelAppsGridView());

  // The toast should be visible.
  EXPECT_EQ(ash::AppListToastType::kReorderUndo,
            app_list_test_api_.GetToastType());

  // Transition to tablet mode - verify that the fullscreen launcher does not
  // have undo toast, and that the order of apps is still sorted.
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);
  EXPECT_EQ(ash::AppListToastType::kNone, app_list_test_api_.GetToastType());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Transition back to clamshell, and verify the bubble launcher undo toast is
  // now hidden.
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  EXPECT_EQ(ash::AppListToastType::kNone, app_list_test_api_.GetToastType());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
}

IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       TransitionToClamshellCommitsSort) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // The toast should be visible.
  EXPECT_EQ(ash::AppListToastType::kReorderUndo,
            app_list_test_api_.GetToastType());

  // Transition to clamshell mode - verify that the bubble launcher does not
  // have undo toast, and that the order of apps is still sorted.
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  EXPECT_EQ(ash::AppListToastType::kNone, app_list_test_api_.GetToastType());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Transition back to tablet mode, and verify the fullscreen launcher undo
  // toast is now hidden.
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  EXPECT_EQ(ash::AppListToastType::kNone, app_list_test_api_.GetToastType());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
}

// Verify that switching to tablet mode when the fade out animation in clamshell
// mode is running works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       DISABLED_TransitionToTabletModeDuringFadeOutAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  RegisterModeSwitchClosureOnFadeOutStarted(/*tablet_mode_enabled=*/true);
  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kNameAlphabetical, MenuType::kAppListPageMenu,
      event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kFadeOutAborted,
      &actual_state);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify that the reorder animation is aborted.
  EXPECT_EQ(ReorderAnimationEndState::kFadeOutAborted, actual_state);

  // When switching to the tablet mode, the app list is closed so the
  // temporary sorting order should be committed.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Verify that reordering in tablet mode works.
  base::HistogramTester histograms;
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  histograms.ExpectBucketCount(ash::kTabletReorderActionHistogram,
                               ash::AppListSortOrder::kColor, 1);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that switching to clamshell mode when the fade out animation in tablet
// mode is running works as expected.
// TODO(crbug.com/40217187): Flaky.
IN_PROC_BROWSER_TEST_F(
    AppListSortBrowserTest,
    DISABLED_TransitionToClamshellModeDuringFadeOutAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  RegisterModeSwitchClosureOnFadeOutStarted(/*tablet_mode_enabled=*/false);
  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu, event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kFadeOutAborted,
      &actual_state);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify that the reorder animation is aborted.
  EXPECT_EQ(ReorderAnimationEndState::kFadeOutAborted, actual_state);

  // Before switching to the tablet mode, the app list is closed so the
  // temporary sorting order is committed.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Verify that reordering in tablet mode works.
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that switching to tablet mode when the fade in animation in clamshell
// is running works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       TransitionToTabletModeDuringFadeInAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kNameAlphabetical, MenuType::kAppListPageMenu,
      event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kFadeInAborted, &actual_state);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order should change because the fade out animation ends.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify that the reorder animation is aborted.
  EXPECT_EQ(ReorderAnimationEndState::kFadeInAborted, actual_state);

  // When switching to the tablet mode, the app list is closed so the
  // temporary sorting order should be committed.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Verify that reordering in tablet mode works.
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verifies that clicking at the toast close button to commit the temporary sort
// order works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       CommitTemporaryOrderByClickingAtToastCloseButton) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Before committing the temporary order, the permanent ordinal order should
  // not change.
  EXPECT_EQ(GetAppIdsInPermanentOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Commit the temporary order by clicking at the close button. Check that
  // the permanent ordinal order changes accordingly.
  app_list_test_api_.ClickOnCloseButtonAndWaitForToastAnimation(
      event_generator_.get());
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
  EXPECT_EQ(GetAppIdsInPermanentOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));
}

// Verify that switching to clamshell mode when the fade in animation in tablet
// mode is running works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       TransitionToClamshellModeDuringFadeInAnimation) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu, event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kFadeInAborted, &actual_state);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order should change because the fade out animation ends.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();
  EXPECT_NE(ReorderAnimationEndState::kFadeOutAborted, actual_state);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // When switching out of the tablet mode, the tablet mode app list gets
  // closed so the temporary sorting order is committed.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that switching to clamshell mode when the fade in animation in tablet
// mode is running, and gets aborted during tablet mode transition works as
// expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       TransitionToClamshellModeDuringAbortedFadeInAnimation) {
  ash::TabletModeControllerTestApi().EnterTabletMode();

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu, event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kFadeInAborted, &actual_state);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order should change because the fade out animation ends.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ash::TabletModeControllerTestApi().LeaveTabletMode();

  // Progress tablet mode animation to the end before item fade in animation
  // completes - this should hide the tablet mode app list and abort the fade in
  // aniamtion.
  app_list_test_api_.GetAppListViewLayer()->GetAnimator()->StopAnimating();
  EXPECT_EQ(ReorderAnimationEndState::kFadeInAborted, actual_state);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // When switching out of the tablet mode, the tablet mode app list gets
  // closed so the temporary sorting order is committed.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));
}

// Verify that in clamshell interrupting a fade out animation by starting
// another reorder animation works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       InterruptReorderFadeOutAnimationClamshellMode) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu, event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kFadeOutAborted,
      &actual_state);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order does not change because the fade out animation is running.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Trigger another app list reorder.
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));

  // Verify that the previous reorder animation is aborted.
  EXPECT_EQ(ReorderAnimationEndState::kFadeOutAborted, actual_state);
}

// Verify that in tablet interrupting a fade out animation by starting another
// reorder animation works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       InterruptReorderFadeOutAnimationTabletMode) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu, event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kFadeOutAborted,
      &actual_state);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order does not change because the fade out animation is running.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  // Trigger another app list reorder.
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));

  // Verify that the previous reorder animation is aborted.
  EXPECT_EQ(ReorderAnimationEndState::kFadeOutAborted, actual_state);
}

// Verify that in clamshell interrupting a fade in animation by starting another
// reorder animation works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       InterruptReorderFadeInAnimationClamshellMode) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu, event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kFadeInAborted, &actual_state);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order should change because the fade out animation ends.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Trigger another app list reorder.
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));

  // Verify that the previous reorder animation is aborted.
  EXPECT_EQ(ReorderAnimationEndState::kFadeInAborted, actual_state);
}

// Verify that in tablet interrupting a fade in animation by starting another
// reorder animation works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest,
                       InterruptReorderFadeInAnimationTabletMode) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForAppListShowAnimation(/*is_bubble_window=*/false);

  // Verify the default app order.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app3_id_, app2_id_, app1_id_}));

  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kNameAlphabetical,
      MenuType::kAppListNonFolderItemMenu, event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kFadeInAborted, &actual_state);

  // Verify that there is active reorder animations.
  EXPECT_TRUE(app_list_test_api_.HasAnyWaitingReorderDoneCallback());

  // The app order should change because the fade out animation ends.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app1_id_, app2_id_, app3_id_}));

  // Trigger another app list reorder.
  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));

  // Verify that the previous reorder animation is aborted.
  EXPECT_EQ(ReorderAnimationEndState::kFadeInAborted, actual_state);
}

// Verifies that changing an app's icon under color sort works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortBrowserTest, SetIconUnderColorSort) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app3_id_, app1_id_}));

  // Set the app 3's icon color to be black.
  auto* model_updater = test::GetModelUpdater(AppListClientImpl::GetInstance());
  const syncer::StringOrdinal position_before_setting_black =
      model_updater->FindItem(app3_id_)->position();
  SetTestAppIconColor(app3_id_, SK_ColorBLACK);
  const syncer::StringOrdinal position_after_setting_black =
      model_updater->FindItem(app3_id_)->position();

  // Verify that the color order is still maintained.
  EXPECT_EQ(GetAppIdsInOrdinalOrder(),
            std::vector<std::string>({app2_id_, app1_id_, app3_id_}));

  // Verify that the app 3's position changes.
  EXPECT_FALSE(
      position_after_setting_black.Equals(position_before_setting_black));

  // Set the app 3's icon color to be magenta.
  const std::vector<const ChromeAppListItem*> items_before_setting_magenta =
      model_updater->GetItems();
  SetTestAppIconColor(app3_id_, SK_ColorMAGENTA);

  // Verify that there is no position changes. Because after setting the app3
  // should still be placed at the end.
  EXPECT_EQ(items_before_setting_magenta.size(), model_updater->ItemCount());
  for (const ChromeAppListItem* item : items_before_setting_magenta) {
    EXPECT_EQ(item->position(),
              model_updater->FindItem(item->id())->position());
  }

  // Set the app 1's icon color to be white. But the icon is labeled as a
  // placeholder.
  const std::vector<const ChromeAppListItem*> items_before_setting_white =
      model_updater->GetItems();
  SetTestAppIconColor(app1_id_, SK_ColorWHITE);

  // Verify that there is no position changes because setting a placeholder icon
  // should not update item positions.
  EXPECT_EQ(items_before_setting_white.size(), model_updater->ItemCount());
  for (const ChromeAppListItem* item : items_before_setting_white) {
    EXPECT_EQ(item->position(),
              model_updater->FindItem(item->id())->position());
  }
}

// Verifies color sort features by providing an app with the specified icon.
class AppListSortColorOrderBrowserTest : public AppListSortBrowserTest {
 public:
  AppListSortColorOrderBrowserTest() = default;
  AppListSortColorOrderBrowserTest(const AppListSortColorOrderBrowserTest&) =
      delete;
  AppListSortColorOrderBrowserTest& operator=(
      const AppListSortColorOrderBrowserTest&) = delete;
  ~AppListSortColorOrderBrowserTest() override = default;

  // AppListSortBrowserTest:
  void SetUpOnMainThread() override {
    AppListSortBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(extension_data_directory_.CreateUniqueTempDir());
    extension_path_ = SetUpFakeAppWithPureColorIcon(
        /*app_name=*/"yellow_app", /*icon_color=*/SK_ColorYELLOW);
  }

  base::FilePath extension_path_;

 private:
  // Sets up the resources of a fake app with the specified name and icon color.
  // Returns the path to the fake app data.
  base::FilePath SetUpFakeAppWithPureColorIcon(const std::string& app_name,
                                               SkColor icon_color) {
    // The export directory for an extension.
    const base::FilePath extension_path =
        extension_data_directory_.GetPath().Append(app_name);
    base::CreateDirectory(extension_path);

    // Prepare an icon file.
    constexpr char icon_file_name[] = "icon.png";
    base::FilePath icon_path = extension_path.AppendASCII(icon_file_name);
    base::File icon_file(icon_path,
                         base::File::FLAG_CREATE | base::File::FLAG_WRITE);

    // Write the data of a circular icon in pure color into the icon file.
    constexpr int icon_size = 128;
    gfx::ImageSkia icon;
    icon = gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
        icon_size / 2, icon_color, icon);
    const sk_sp<SkImage> image = SkImages::RasterFromBitmap(*icon.bitmap());
    const sk_sp<SkData> png_data =
        SkPngEncoder::Encode(nullptr, image.get(), {});
    icon_file.Write(0, (const char*)png_data->data(), png_data->size());
    icon_file.Close();

    // Prepare the app manifest file.
    base::FilePath manifest_path =
        extension_path.Append("manifest").AddExtension(".json");
    base::File manifest_file(manifest_path,
                             base::File::FLAG_CREATE | base::File::FLAG_WRITE);

    // Write data into the manifest file.
    char json_buffer[30];
    constexpr char icon_json[] = R"({"%d": "%s"})";
    base::strings::SafeSPrintf(json_buffer, icon_json, icon_size,
                               icon_file_name);
    char manifest_buffer[300];
    int count = base::strings::SafeSPrintf(manifest_buffer, kManifestData,
                                           app_name.c_str(), json_buffer);
    EXPECT_EQ(count, manifest_file.Write(0, manifest_buffer, count));
    manifest_file.Close();

    return extension_path;
  }

  // A temporary directory acting as a root directory for extension data.
  base::ScopedTempDir extension_data_directory_;
};

// Verify that installing an app under color sort works as expected.
IN_PROC_BROWSER_TEST_F(AppListSortColorOrderBrowserTest,
                       InstallAppUnderColorSort) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(false);
  WaitForAppListTransitionAnimation();

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  app_list_test_api_.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  ReorderTopLevelAppsGridAndWaitForCompletion(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu);

  std::string yellow_app_id = LoadExtension(extension_path_)->id();
  EXPECT_FALSE(yellow_app_id.empty());
  SetTestAppIconColor(yellow_app_id, SK_ColorYELLOW);

  // Verify that the new app's position follows the color order.
  EXPECT_EQ(
      GetAppIdsInOrdinalOrder({app1_id_, app2_id_, app3_id_, yellow_app_id}),
      std::vector<std::string>({app2_id_, yellow_app_id, app3_id_, app1_id_}));
}

class AppListSortLoginTest
    : public ash::LoginManagerTest,
      public ::testing::WithParamInterface</*in_tablet=*/bool> {
 public:
  AppListSortLoginTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;
  }
  ~AppListSortLoginTest() override = default;

  void SetUpOnMainThread() override {
    ash::ShellTestApi().SetTabletModeEnabledForTest(GetParam());
    ash::LoginManagerTest::SetUpOnMainThread();
  }

  AccountId account_id1_;
  AccountId account_id2_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

INSTANTIATE_TEST_SUITE_P(All, AppListSortLoginTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(AppListSortLoginTest,
                       RecordPrefSortOrderOnSessionStart) {
  // Verify that the pref sort order is recorded when a primary user logs in.
  base::HistogramTester histogram;
  LoginUser(account_id1_);
  const char* histogram_name =
      GetParam() ? ash::kTabletAppListSortOrderOnSessionStartHistogram
                 : ash::kClamshellAppListSortOrderOnSessionStartHistogram;
  histogram.ExpectBucketCount(histogram_name, ash::AppListSortOrder::kCustom,
                              1);

  // Verify that the pref sort order is recorded when a secondary user logs in.
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  histogram.ExpectBucketCount(histogram_name, ash::AppListSortOrder::kCustom,
                              2);

  // Switch back to the primary user. Verify that the pref sort order is not
  // recorded again.
  user_manager::UserManager::Get()->SwitchActiveUser(account_id1_);
  histogram.ExpectBucketCount(histogram_name, ash::AppListSortOrder::kCustom,
                              2);
}

// Verifies that the app list sort discovery duration after the education nudge
// shows is recorded as expected.
// TODO(crbug.com/328928228): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_VerifySortAfterNudgeShowMetric \
  DISABLED_VerifySortAfterNudgeShowMetric
#else
#define MAYBE_VerifySortAfterNudgeShowMetric VerifySortAfterNudgeShowMetric
#endif
IN_PROC_BROWSER_TEST_P(AppListSortLoginTest,
                       MAYBE_VerifySortAfterNudgeShowMetric) {
  LoginUser(account_id1_);

  ash::AcceleratorController::Get()->PerformActionIfEnabled(
      AcceleratorAction::kToggleAppList, {});
  const bool is_in_tablet = GetParam();
  ash::AppListTestApi app_list_test_api;
  if (is_in_tablet)
    app_list_test_api.WaitForAppListShowAnimation(/*is_bubble_window=*/false);
  else
    app_list_test_api.WaitForBubbleWindow(/*wait_for_opening_animation=*/true);

  // Reorder the app list.
  ReorderAnimationEndState actual_state;
  base::HistogramTester histogram;
  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      ash::Shell::GetPrimaryRootWindow());
  app_list_test_api.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu,
      event_generator.get(),
      /*target_state=*/ReorderAnimationEndState::kCompleted, &actual_state);
  EXPECT_EQ(ReorderAnimationEndState::kCompleted, actual_state);

  // Verify that the data is reported with the correct histogram.
  histogram.ExpectTotalCount(
      ash::kAppListSortDiscoveryDurationAfterNudgeClamshell, !is_in_tablet);
  histogram.ExpectTotalCount(ash::kAppListSortDiscoveryDurationAfterNudgeTablet,
                             is_in_tablet);
}

class AppListSortLoginTalbetTest : public ash::LoginManagerTest {
 public:
  AppListSortLoginTalbetTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
    account_id1_ = login_mixin_.users()[0].account_id;
    account_id2_ = login_mixin_.users()[1].account_id;
  }
  AppListSortLoginTalbetTest(const AppListSortLoginTalbetTest&) = delete;
  AppListSortLoginTalbetTest& operator=(const AppListSortLoginTalbetTest&) =
      delete;
  ~AppListSortLoginTalbetTest() override = default;

  void SetUpOnMainThread() override {
    ash::ShellTestApi().SetTabletModeEnabledForTest(true);
    ash::LoginManagerTest::SetUpOnMainThread();
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());
  }

  ash::AppListTestApi app_list_test_api_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  AccountId account_id1_;
  AccountId account_id2_;
  ash::LoginManagerMixin login_mixin_{&mixin_host_};
};

// TODO(crbug.com/40890115): Flaky test.
IN_PROC_BROWSER_TEST_F(AppListSortLoginTalbetTest,
                       DISABLED_PRE_SwitchUnderTemporarySort) {
  LoginUser(account_id1_);

  // Because Account 1 is new, the reorder education nudge should show.
  EXPECT_EQ(ash::AppListToastType::kReorderNudge,
            app_list_test_api_.GetToastType());

  // Reorder the app list.
  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu,
      event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kCompleted, &actual_state);
  EXPECT_EQ(ReorderAnimationEndState::kCompleted, actual_state);

  // Verify that the reorder undo toast shows.
  EXPECT_EQ(ash::AppListToastType::kReorderUndo,
            app_list_test_api_.GetToastType());
}

// Verifies that the active account switch works as expected when the app list
// is under temporary sort.
//
// TODO(crbug.com/40890115): Flaky test.
IN_PROC_BROWSER_TEST_F(AppListSortLoginTalbetTest,
                       DISABLED_SwitchUnderTemporarySort) {
  LoginUser(account_id1_);

  // Reorder has been triggered in the pretest so the toast should not show.
  EXPECT_EQ(ash::AppListToastType::kNone, app_list_test_api_.GetToastType());

  // Switch to Account 2.
  ash::UserAddingScreen::Get()->Start();
  AddUser(account_id2_);
  EXPECT_EQ(ash::AppListToastType::kReorderNudge,
            app_list_test_api_.GetToastType());

  // Reorder the app list and check that the undo toast shows.
  ReorderAnimationEndState actual_state;
  app_list_test_api_.ReorderByMouseClickAtToplevelAppsGridMenu(
      ash::AppListSortOrder::kColor, MenuType::kAppListNonFolderItemMenu,
      event_generator_.get(),
      /*target_state=*/ReorderAnimationEndState::kCompleted, &actual_state);
  EXPECT_EQ(ReorderAnimationEndState::kCompleted, actual_state);
  EXPECT_EQ(ash::AppListToastType::kReorderUndo,
            app_list_test_api_.GetToastType());

  // Switch back to Account 1. Verify that the toast should not show.
  user_manager::UserManager::Get()->SwitchActiveUser(account_id1_);
  EXPECT_EQ(account_id1_,
            user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
  EXPECT_EQ(ash::AppListToastType::kNone, app_list_test_api_.GetToastType());
}
