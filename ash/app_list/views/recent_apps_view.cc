// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/recent_apps_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"
#include "url/gurl.h"

namespace ash {
namespace {

// Horizontal space between apps in dips.
constexpr int kHorizontalSpacing = 8;

// Sorts increasing by display index, then decreasing by position priority.
struct CompareByDisplayIndexAndPositionPriority {
  bool operator()(const SearchResult* result1,
                  const SearchResult* result2) const {
    SearchResultDisplayIndex index1 = result1->display_index();
    SearchResultDisplayIndex index2 = result2->display_index();
    if (index1 != index2)
      return index1 < index2;
    return result1->position_priority() > result2->position_priority();
  }
};

// Converts a search result app ID to an app list item ID.
std::string ItemIdFromAppId(const std::string& app_id) {
  // Convert chrome-extension://<id> to just <id>.
  if (base::StartsWith(app_id, extensions::kExtensionScheme)) {
    GURL url(app_id);
    return url.host();
  }
  return app_id;
}

// Returns true if `type` is an application result.
bool IsAppType(AppListSearchResultType type) {
  return type == AppListSearchResultType::kInstalledApp ||
         type == AppListSearchResultType::kPlayStoreApp ||
         type == AppListSearchResultType::kInstantApp ||
         type == AppListSearchResultType::kInternalApp;
}

// Returns a list of recent apps by filtering suggestion chip data.
// TODO(crbug.com/1216662): Replace with a real implementation after the ML team
// gives us a way to query directly for recent apps.
std::vector<std::string> GetRecentAppIdsFromSuggestionChips(
    SearchModel* search_model) {
  SearchModel::SearchResults* results = search_model->results();
  auto is_app_suggestion = [](const SearchResult& r) -> bool {
    return IsAppType(r.result_type()) &&
           r.display_type() == SearchResultDisplayType::kChip;
  };
  std::vector<SearchResult*> app_suggestion_results =
      SearchModel::FilterSearchResultsByFunction(
          results, base::BindRepeating(is_app_suggestion),
          /*max_results=*/5);

  std::sort(app_suggestion_results.begin(), app_suggestion_results.end(),
            CompareByDisplayIndexAndPositionPriority());

  std::vector<std::string> app_ids;
  for (SearchResult* result : app_suggestion_results)
    app_ids.push_back(result->id());
  return app_ids;
}

}  // namespace

// The grid delegate for each AppListItemView. Recent app icons cannot be
// dragged, so this implementation is mostly a stub.
class RecentAppsView::GridDelegateImpl : public AppListItemView::GridDelegate {
 public:
  explicit GridDelegateImpl(AppListViewDelegate* view_delegate)
      : view_delegate_(view_delegate) {}
  GridDelegateImpl(const GridDelegateImpl&) = delete;
  GridDelegateImpl& operator=(const GridDelegateImpl&) = delete;
  ~GridDelegateImpl() override = default;

  // AppListItemView::GridDelegate:
  bool IsInFolder() const override { return false; }
  void SetSelectedView(AppListItemView* view) override {
    DCHECK(view);
    if (view == selected_view_)
      return;
    // Ensure the translucent background of the previous selection goes away.
    if (selected_view_)
      selected_view_->SchedulePaint();
    selected_view_ = view;
    // Ensure the translucent background of this selection is painted.
    selected_view_->SchedulePaint();
  }
  void ClearSelectedView() override { selected_view_ = nullptr; }
  bool IsSelectedView(const AppListItemView* view) const override {
    return view == selected_view_;
  }
  bool InitiateDrag(AppListItemView* view,
                    const gfx::Point& location,
                    const gfx::Point& root_location,
                    base::OnceClosure drag_start_callback,
                    base::OnceClosure drag_end_callback) override {
    return false;
  }
  void StartDragAndDropHostDragAfterLongPress() override {}
  bool UpdateDragFromItem(bool is_touch,
                          const ui::LocatedEvent& event) override {
    return false;
  }
  void EndDrag(bool cancel) override {}
  void OnAppListItemViewActivated(AppListItemView* pressed_item_view,
                                  const ui::Event& event) override {
    // TODO(crbug.com/1216594): Add a new launch type for "recent apps".
    // NOTE: Avoid using |item->id()| as the parameter. In some rare situations,
    // activating the item may destruct it. Using the reference to an object
    // which may be destroyed during the procedure as the function parameter
    // may bring the crash like https://crbug.com/990282.
    const std::string id = pressed_item_view->item()->id();
    view_delegate_->ActivateItem(
        id, event.flags(), AppListLaunchedFrom::kLaunchedFromSuggestionChip);
    // `this` may be deleted.
  }
  const AppListConfig& GetAppListConfig() const override {
    // TODO(crbug.com/1211592): Eliminate this method and use the real config.
    return *AppListConfigProvider::Get().GetConfigForType(
        AppListConfigType::kLarge, /*can_create=*/true);
  }

 private:
  AppListViewDelegate* const view_delegate_;
  AppListItemView* selected_view_ = nullptr;
};

RecentAppsView::RecentAppsView(AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate),
      grid_delegate_(std::make_unique<GridDelegateImpl>(view_delegate_)) {
  DCHECK(view_delegate_);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kHorizontalSpacing));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  std::vector<std::string> app_ids =
      GetRecentAppIdsFromSuggestionChips(view_delegate_->GetSearchModel());

  AppListModel* model = view_delegate_->GetModel();
  for (const std::string& app_id : app_ids) {
    std::string item_id = ItemIdFromAppId(app_id);
    AppListItem* item = model->FindItem(item_id);
    if (item) {
      // NOTE: If you change the view structure, update GetItemForTest() as
      // well.
      AddChildView(std::make_unique<AppListItemView>(grid_delegate_.get(), item,
                                                     view_delegate_));
    }
  }
}

RecentAppsView::~RecentAppsView() = default;

AppListItemView* RecentAppsView::GetItemViewForTest(int index) {
  return static_cast<AppListItemView*>(children()[index]);
}

BEGIN_METADATA(RecentAppsView, views::View)
END_METADATA

}  // namespace ash
