// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_test_view_delegate.h"

#include <string>
#include <utility>

#include "ash/app_list/model/app_list_model.h"
#include "base/functional/callback.h"

namespace ash {
namespace test {

AppListTestViewDelegate::AppListTestViewDelegate()
    : model_(std::make_unique<AppListTestModel>()),
      search_model_(std::make_unique<SearchModel>()) {
  model_provider_.SetActiveModel(model_.get(), search_model_.get());
}

AppListTestViewDelegate::~AppListTestViewDelegate() = default;

bool AppListTestViewDelegate::KeyboardTraversalEngaged() {
  return true;
}

void AppListTestViewDelegate::StartZeroStateSearch(base::OnceClosure callback,
                                                   base::TimeDelta timeout) {
  std::move(callback).Run();
}

void AppListTestViewDelegate::OpenSearchResult(
    const std::string& result_id,
    int event_flags,
    ash::AppListLaunchedFrom launched_from,
    ash::AppListLaunchType launch_type,
    int suggestion_index,
    bool launch_as_default) {
  const SearchModel::SearchResults* results = search_model_->results();
  for (size_t i = 0; i < results->item_count(); ++i) {
    if (results->GetItemAt(i)->id() == result_id) {
      open_search_result_counts_[i]++;
      if (results->GetItemAt(i)->is_omnibox_search()) {
        ++open_assistant_ui_count_;
      }
      break;
    }
  }
  ++open_search_result_count_;

  if (launch_type == ash::AppListLaunchType::kAppSearchResult) {
    switch (launched_from) {
      case ash::AppListLaunchedFrom::kLaunchedFromSearchBox:
      case ash::AppListLaunchedFrom::kLaunchedFromSuggestionChip:
      case ash::AppListLaunchedFrom::kLaunchedFromRecentApps:
        RecordAppLaunched(launched_from);
        return;
      case ash::AppListLaunchedFrom::kLaunchedFromGrid:
      case ash::AppListLaunchedFrom::kLaunchedFromShelf:
      case ash::AppListLaunchedFrom::kLaunchedFromContinueTask:
        return;
    }
  }
}

void AppListTestViewDelegate::DismissAppList() {
  ++dismiss_count_;
}

void AppListTestViewDelegate::ReplaceTestModel(int item_count) {
  search_model_ = std::make_unique<SearchModel>();
  model_ = std::make_unique<AppListTestModel>();
  model_->PopulateApps(item_count);
  model_provider_.SetActiveModel(model_.get(), search_model_.get());
}

void AppListTestViewDelegate::SetSearchEngineIsGoogle(bool is_google) {
  search_model_->SetSearchEngineIsGoogle(is_google);
}

void AppListTestViewDelegate::SetIsTabletModeEnabled(bool is_tablet_mode) {
  is_tablet_mode_ = is_tablet_mode;
}

void AppListTestViewDelegate::ActivateItem(
    const std::string& id,
    int event_flags,
    ash::AppListLaunchedFrom launched_from) {
  AppListItem* item = model_->FindItem(id);
  if (!item)
    return;
  DCHECK(!item->is_folder());
  static_cast<AppListTestModel::AppListTestItem*>(item)->Activate(event_flags);
  RecordAppLaunched(launched_from);
}

void AppListTestViewDelegate::GetContextMenuModel(
    const std::string& id,
    AppListItemContext item_context,
    GetContextMenuModelCallback callback) {
  AppListItem* item = model_->FindItem(id);
  // TODO(stevenjb/jennyz): Implement this for folder items
  std::unique_ptr<ui::SimpleMenuModel> menu_model;
  if (item && !item->is_folder()) {
    menu_model = static_cast<AppListTestModel::AppListTestItem*>(item)
                     ->CreateContextMenuModel();
  }
  std::move(callback).Run(std::move(menu_model));
}

ui::ImplicitAnimationObserver* AppListTestViewDelegate::GetAnimationObserver(
    ash::AppListViewState target_state) {
  return nullptr;
}

void AppListTestViewDelegate::ShowWallpaperContextMenu(
    const gfx::Point& onscreen_location,
    ui::MenuSourceType source_type) {
  ++show_wallpaper_context_menu_count_;
}

bool AppListTestViewDelegate::CanProcessEventsOnApplistViews() {
  return true;
}

bool AppListTestViewDelegate::ShouldDismissImmediately() {
  return false;
}

bool AppListTestViewDelegate::HasValidProfile() const {
  return true;
}

bool AppListTestViewDelegate::ShouldHideContinueSection() const {
  return false;
}

void AppListTestViewDelegate::SetHideContinueSection(bool hide) {}

ash::AssistantViewDelegate*
AppListTestViewDelegate::GetAssistantViewDelegate() {
  return nullptr;
}

void AppListTestViewDelegate::OnSearchResultVisibilityChanged(
    const std::string& id,
    bool visibility) {}

bool AppListTestViewDelegate::IsAssistantAllowedAndEnabled() const {
  return false;
}

void AppListTestViewDelegate::OnStateTransitionAnimationCompleted(
    AppListViewState state,
    bool was_animation_interrupted) {}

AppListState AppListTestViewDelegate::GetCurrentAppListPage() const {
  return app_list_page_;
}

void AppListTestViewDelegate::OnAppListPageChanged(AppListState page) {
  app_list_page_ = page;
}

AppListViewState AppListTestViewDelegate::GetAppListViewState() const {
  return app_list_view_state_;
}

void AppListTestViewDelegate::OnViewStateChanged(AppListViewState state) {
  app_list_view_state_ = state;
}

void AppListTestViewDelegate::GetAppLaunchedMetricParams(
    AppLaunchedMetricParams* metric_params) {}

gfx::Rect AppListTestViewDelegate::SnapBoundsToDisplayEdge(
    const gfx::Rect& bounds) {
  return bounds;
}

int AppListTestViewDelegate::GetShelfSize() {
  // TODO(mmourgos): change this to 48 once shelf-hotseat flag is enabled.
  // Return the height of the shelf when clamshell mode is active.
  return 56;
}

bool AppListTestViewDelegate::AppListTargetVisibility() const {
  return true;
}

bool AppListTestViewDelegate::IsInTabletMode() {
  return is_tablet_mode_;
}

AppListNotifier* AppListTestViewDelegate::GetNotifier() {
  return nullptr;
}

void AppListTestViewDelegate::RecordAppLaunched(
    ash::AppListLaunchedFrom launched_from) {
  RecordAppListAppLaunched(launched_from, app_list_view_state_,
                           false /*tablet mode*/,
                           false /*home launcher shown*/);
}

bool AppListTestViewDelegate::IsCommandIdChecked(int command_id) const {
  return true;
}

bool AppListTestViewDelegate::IsCommandIdEnabled(int command_id) const {
  return true;
}

void AppListTestViewDelegate::ExecuteCommand(int command_id, int event_flags) {}

}  // namespace test
}  // namespace ash
