// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APPS_COLLECTION_SECTION_VIEW_H_
#define ASH_APP_LIST_VIEWS_APPS_COLLECTION_SECTION_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_collections_constants.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace ash {
class AppListConfig;
class AppListModel;
class AppListItemView;
class AppListViewDelegate;

// An app collection section. These sections are shown on the
// AppListBubbleAppsCollectionPage. Each section contains a label with the
// name of the collection and a grid of apps that belong to that collection.
class ASH_EXPORT AppsCollectionSectionView : public AppListModelObserver,
                                             public views::View {
  METADATA_HEADER(AppsCollectionSectionView, views::View)

 public:
  AppsCollectionSectionView(AppCollection collection,
                            AppListViewDelegate* view_delegate);
  AppsCollectionSectionView(const AppsCollectionSectionView&) = delete;
  AppsCollectionSectionView& operator=(const AppsCollectionSectionView&) =
      delete;
  ~AppsCollectionSectionView() override;

  // Sets the `AppListConfig` that should be used to configure layout of
  // `AppListItemViews` shown within this view.
  void UpdateAppListConfig(const AppListConfig* app_list_config);

  void UpdateAppsForCollection();

  void SetModel(AppListModel* model);

  // Returns the number of AppListItemView children.
  size_t GetItemViewCount() const;

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // AppListModelObserver:
  void OnAppListModelStatusChanged() override;
  void OnAppListItemAdded(AppListItem* item) override;
  void OnAppListItemWillBeDeleted(AppListItem* item) override;

  AppCollection collection() { return collection_; }

 private:
  friend class AppsCollectionSectionViewTest;

  // Calculates how much padding is assigned to the AppListItemView.
  int CalculateTilePadding() const;

  // Returns the index of the AppListItemView within `item_views_` that
  // corresponds to the `item_id`. If the `item_id` does not appear on
  // `item_views_`, the return value will be null.
  std::optional<size_t> GetViewIndexForItem(const std::string& item_id);

  const AppCollection collection_ = AppCollection::kUnknown;
  const raw_ptr<AppListViewDelegate> view_delegate_;
  raw_ptr<const AppListConfig> app_list_config_ = nullptr;

  raw_ptr<views::View> apps_container_ = nullptr;
  raw_ptr<AppListModel> model_ = nullptr;

  // The grid delegate for each AppListItemView.
  class GridDelegateImpl;
  std::unique_ptr<GridDelegateImpl> grid_delegate_;

  // The recent app items. Stored here because this view has child views for
  // spacing that are not AppListItemViews.
  views::ViewModelT<AppListItemView> item_views_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APPS_COLLECTION_SECTION_VIEW_H_
