// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_collection_section_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_collections_constants.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/apps_collections_controller.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

namespace ash {
// Parameterized to test apps collections in the app list bubble apps
// collection page.
class AppsCollectionSectionViewTest : public AshTestBase {
 public:
  AppsCollectionSectionViewTest() = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({app_list_features::kAppsCollections},
                                          {});
    AshTestBase::SetUp();
    AppsCollectionsController::Get()->ForceAppsCollectionsForTesting(
        /*force=*/true);
  }

  void ShowAppList() {
    Shell::Get()->app_list_controller()->ShowAppList(
        AppListShowSource::kSearchKey);
  }

  void AddAppListItemWithCollection(const std::string& id,
                                    AppCollection collection_id) {
    AppListModel* model = AppListModelProvider::Get()->model();
    auto item = std::make_unique<AppListItem>(id);
    item->SetAppCollectionId(collection_id);
    AppListItem* item_ptr = model->AddItem(std::move(item));

    // Give each item a name so that the accessibility paint checks pass.
    // (Focusable items should have accessible names.)
    model->SetItemName(item_ptr, item_ptr->id());
  }

  AppsCollectionSectionView* GetViewForCollection(AppCollection id) {
    views::View* collections_container =
        GetAppListTestHelper()->GetAppCollectionsSectionsContainer();
    for (views::View* child : collections_container->children()) {
      AppsCollectionSectionView* collection =
          views::AsViewClass<AppsCollectionSectionView>(child);
      if (collection->collection() == id) {
        return collection;
      }
    }
    return nullptr;
  }

  AppListItemView* GetAppItemAtIndex(AppsCollectionSectionView* collection,
                                     size_t index) {
    return index < collection->item_views_.view_size()
               ? collection->item_views_.view_at(index)
               : nullptr;
  }

  void RemoveApp(const std::string& id) {
    AppListModelProvider::Get()->model()->DeleteItem(id);
  }

 protected:
  AppListItemView::DragState GetDragState(AppListItemView* view) {
    return view->drag_state_;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppsCollectionSectionViewTest, CreatesIconsForAppsWithinCollection) {
  AddAppListItemWithCollection("id1", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id2", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id3", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id4", AppCollection::kEntertainment);

  AddAppListItemWithCollection("id5", AppCollection::kProductivity);
  AddAppListItemWithCollection("id6", AppCollection::kProductivity);

  ShowAppList();

  ASSERT_TRUE(GetViewForCollection(AppCollection::kEntertainment));
  EXPECT_EQ(
      GetViewForCollection(AppCollection::kEntertainment)->GetItemViewCount(),
      4u);
  ASSERT_TRUE(GetViewForCollection(AppCollection::kProductivity));
  EXPECT_EQ(
      GetViewForCollection(AppCollection::kProductivity)->GetItemViewCount(),
      2u);
}

TEST_F(AppsCollectionSectionViewTest, DoesNotCreateIconsForEmptyCollection) {
  AddAppListItemWithCollection("id1", AppCollection::kProductivity);
  AddAppListItemWithCollection("id2", AppCollection::kProductivity);

  ShowAppList();

  AppsCollectionSectionView* entertainment_collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(entertainment_collection);
  EXPECT_FALSE(entertainment_collection->GetVisible());
  EXPECT_EQ(entertainment_collection->GetItemViewCount(), 0u);

  AppsCollectionSectionView* productivity_collection =
      GetViewForCollection(AppCollection::kProductivity);
  ASSERT_TRUE(productivity_collection);
  EXPECT_TRUE(productivity_collection->GetVisible());
  EXPECT_EQ(productivity_collection->GetItemViewCount(), 2u);
}

TEST_F(AppsCollectionSectionViewTest, ClickOrTapOnCollectionApp) {
  AddAppListItemWithCollection("id1", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id2", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id3", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id4", AppCollection::kEntertainment);

  ShowAppList();

  AppsCollectionSectionView* collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(collection);
  ASSERT_GT(collection->GetItemViewCount(), 0u);

  // Click or tap on the first icon.
  views::View* icon = GetAppItemAtIndex(collection, 0);

  GetEventGenerator()->MoveMouseTo(icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // The item was activated.
  EXPECT_EQ(1, GetTestAppListClient()->activate_item_count());
  EXPECT_EQ("id1", GetTestAppListClient()->activate_item_last_id());
}

TEST_F(AppsCollectionSectionViewTest, AccessibleDescription) {
  AddAppListItemWithCollection("id1", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id2", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id3", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id4", AppCollection::kEntertainment);

  ShowAppList();

  AppsCollectionSectionView* collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(collection);
  ASSERT_GT(collection->GetItemViewCount(), 0u);

  views::View* view = GetAppItemAtIndex(collection, 0);

  EXPECT_EQ(view->GetViewAccessibility().GetCachedDescription(),
            l10n_util::GetStringUTF16(
                IDS_ASH_LAUNCHER_APPS_COLLECTIONS_ENTERTAINMENT_NAME));
}

TEST_F(AppsCollectionSectionViewTest, AttemptTouchDragApp) {
  AddAppListItemWithCollection("id1", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id2", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id3", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id4", AppCollection::kEntertainment);

  ShowAppList();

  AppsCollectionSectionView* collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(collection);
  ASSERT_GT(collection->GetItemViewCount(), 0u);

  // Click or tap on the first icon.
  AppListItemView* view = GetAppItemAtIndex(collection, 0);
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  auto* generator = GetEventGenerator();
  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();

  // Attempt to fire the touch drag timer. The grid view should not trigger
  // the timer.
  EXPECT_FALSE(view->FireTouchDragTimerForTest());

  // Verify the apps did not enter dragged state.
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_TRUE(view->title()->GetVisible());
}

TEST_F(AppsCollectionSectionViewTest, AttemptMouseDragApp) {
  AddAppListItemWithCollection("id1", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id2", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id3", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id4", AppCollection::kEntertainment);

  ShowAppList();

  AppsCollectionSectionView* collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(collection);
  ASSERT_GT(collection->GetItemViewCount(), 0u);

  // Click or tap on the first icon.
  AppListItemView* view = GetAppItemAtIndex(collection, 0);
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  auto* generator = GetEventGenerator();
  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(from);
  generator->PressLeftButton();

  // Attempt to fire the mouse drag timer. The grid view should not trigger
  // the timer.
  EXPECT_FALSE(view->FireMouseDragTimerForTest());

  // Verify the apps did not enter dragged state.
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_TRUE(view->title()->GetVisible());
}

TEST_F(AppsCollectionSectionViewTest, RemoveAppFromModel) {
  AddAppListItemWithCollection("id1", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id2", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id3", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id4", AppCollection::kEntertainment);

  ShowAppList();

  AppsCollectionSectionView* collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(collection);
  EXPECT_EQ(collection->GetItemViewCount(), 4u);

  RemoveApp("id2");

  ASSERT_TRUE(collection);
  EXPECT_EQ(collection->GetItemViewCount(), 3u);
}

TEST_F(AppsCollectionSectionViewTest, AddAppToModel) {
  AddAppListItemWithCollection("id1", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id2", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id3", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id4", AppCollection::kEntertainment);

  ShowAppList();

  AppsCollectionSectionView* collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(collection);
  EXPECT_EQ(collection->GetItemViewCount(), 4u);

  AddAppListItemWithCollection("id5", AppCollection::kEntertainment);

  ASSERT_TRUE(collection);
  EXPECT_EQ(collection->GetItemViewCount(), 5u);
}

TEST_F(AppsCollectionSectionViewTest, AddAppToModelOnDifferentCollection) {
  AddAppListItemWithCollection("id1", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id2", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id3", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id4", AppCollection::kEntertainment);

  ShowAppList();

  AppsCollectionSectionView* collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(collection);
  EXPECT_EQ(collection->GetItemViewCount(), 4u);

  AddAppListItemWithCollection("id5", AppCollection::kProductivity);

  ASSERT_TRUE(collection);
  EXPECT_EQ(collection->GetItemViewCount(), 4u);
}

TEST_F(AppsCollectionSectionViewTest, RecordMetricsForAppLaunchByEntity) {
  base::HistogramTester histograms;
  AddAppListItemWithCollection("id1", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id2", AppCollection::kUnknown);

  ShowAppList();

  AppsCollectionSectionView* entertainment_collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(entertainment_collection);
  EXPECT_EQ(entertainment_collection->GetItemViewCount(), 1u);

  AppsCollectionSectionView* unknown_collection =
      GetViewForCollection(AppCollection::kUnknown);
  ASSERT_TRUE(unknown_collection);
  EXPECT_EQ(unknown_collection->GetItemViewCount(), 1u);

  histograms.ExpectUniqueSample(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByEntity",
      AppEntity::kDefaultApp, 0);
  histograms.ExpectUniqueSample(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByEntity",
      AppEntity::kThirdPartyApp, 0);

  LeftClickOn(GetAppItemAtIndex(entertainment_collection, 0));
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByEntity",
      AppEntity::kDefaultApp, 1);
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByEntity",
      AppEntity::kThirdPartyApp, 0);

  LeftClickOn(GetAppItemAtIndex(unknown_collection, 0));
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByEntity",
      AppEntity::kDefaultApp, 1);
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByEntity",
      AppEntity::kThirdPartyApp, 1);
}

TEST_F(AppsCollectionSectionViewTest, RecordMetricsForAppLaunchByCategory) {
  base::HistogramTester histograms;
  AddAppListItemWithCollection("id1", AppCollection::kEntertainment);
  AddAppListItemWithCollection("id2", AppCollection::kProductivity);
  AddAppListItemWithCollection("id3", AppCollection::kProductivity);
  AddAppListItemWithCollection("id4", AppCollection::kUnknown);

  ShowAppList();

  AppsCollectionSectionView* entertainment_collection =
      GetViewForCollection(AppCollection::kEntertainment);
  ASSERT_TRUE(entertainment_collection);
  ASSERT_EQ(entertainment_collection->GetItemViewCount(), 1u);

  AppsCollectionSectionView* productivity_collection =
      GetViewForCollection(AppCollection::kProductivity);
  ASSERT_TRUE(productivity_collection);
  ASSERT_EQ(productivity_collection->GetItemViewCount(), 2u);

  AppsCollectionSectionView* unknown_collection =
      GetViewForCollection(AppCollection::kUnknown);
  ASSERT_TRUE(unknown_collection);
  ASSERT_EQ(unknown_collection->GetItemViewCount(), 1u);

  histograms.ExpectTotalCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory", 0);

  // TODO(anasalazar): Investigate why after adding margin to the
  // AppsCollections apps container, this tests fails to click on apps unless we
  // request focus here.
  GetAppItemAtIndex(entertainment_collection, 0)->RequestFocus();

  LeftClickOn(GetAppItemAtIndex(entertainment_collection, 0));
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kEntertainment, 1);
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kProductivity, 0);
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kUnknown, 0);

  LeftClickOn(GetAppItemAtIndex(productivity_collection, 0));
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kEntertainment, 1);
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kProductivity, 1);
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kUnknown, 0);

  LeftClickOn(GetAppItemAtIndex(unknown_collection, 0));
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kEntertainment, 1);
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kProductivity, 1);
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kUnknown, 1);

  LeftClickOn(GetAppItemAtIndex(productivity_collection, 1));
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kEntertainment, 1);
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kProductivity, 2);
  histograms.ExpectBucketCount(
      "Apps.AppListBubble.AppsCollectionsPage.AppLaunchesByCategory",
      AppCollection::kUnknown, 1);
}

}  // namespace ash
