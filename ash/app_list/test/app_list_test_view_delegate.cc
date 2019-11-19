// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/app_list_test_view_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/model/app_list_model.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace test {

AppListTestViewDelegate::AppListTestViewDelegate()
    : model_(std::make_unique<AppListTestModel>()),
      search_model_(std::make_unique<SearchModel>()) {}

AppListTestViewDelegate::~AppListTestViewDelegate() {}

AppListModel* AppListTestViewDelegate::GetModel() {
  return model_.get();
}

SearchModel* AppListTestViewDelegate::GetSearchModel() {
  return search_model_.get();
}

bool AppListTestViewDelegate::KeyboardTraversalEngaged() {
  return true;
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
      if (app_list_features::IsAssistantLauncherUIEnabled() &&
          results->GetItemAt(i)->is_omnibox_search()) {
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
        RecordAppLaunched(launched_from);
        return;
      case ash::AppListLaunchedFrom::kLaunchedFromGrid:
      case ash::AppListLaunchedFrom::kLaunchedFromShelf:
        return;
    }
  }
}

void AppListTestViewDelegate::DismissAppList() {
  ++dismiss_count_;
}

void AppListTestViewDelegate::ReplaceTestModel(int item_count) {
  model_ = std::make_unique<AppListTestModel>();
  model_->PopulateApps(item_count);
  search_model_ = std::make_unique<SearchModel>();
}

void AppListTestViewDelegate::SetSearchEngineIsGoogle(bool is_google) {
  search_model_->SetSearchEngineIsGoogle(is_google);
}

const std::vector<SkColor>&
AppListTestViewDelegate::GetWallpaperProminentColors() {
  return wallpaper_prominent_colors_;
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

bool AppListTestViewDelegate::ProcessHomeLauncherGesture(
    ui::GestureEvent* event) {
  return false;
}

bool AppListTestViewDelegate::CanProcessEventsOnApplistViews() {
  return true;
}

bool AppListTestViewDelegate::ShouldDismissImmediately() {
  return false;
}

void AppListTestViewDelegate::GetNavigableContentsFactory(
    mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver) {
  fake_navigable_contents_factory_.BindReceiver(std::move(receiver));
}
int AppListTestViewDelegate::GetTargetYForAppListHide(
    aura::Window* root_window) {
  return 0;
}

void AppListTestViewDelegate::GetSearchResultContextMenuModel(
    const std::string& result_id,
    GetContextMenuModelCallback callback) {
  auto menu = std::make_unique<ui::SimpleMenuModel>(this);
  // Change items if needed.
  int command_id = 0;
  menu->AddItem(command_id++, base::ASCIIToUTF16("Item0"));
  menu->AddItem(command_id++, base::ASCIIToUTF16("Item1"));
  std::move(callback).Run(std::move(menu));
}

ash::AssistantViewDelegate*
AppListTestViewDelegate::GetAssistantViewDelegate() {
  return nullptr;
}

void AppListTestViewDelegate::OnSearchResultVisibilityChanged(
    const std::string& id,
    bool visibility) {}

void AppListTestViewDelegate::NotifySearchResultsForLogging(
    const base::string16& raw_query,
    const ash::SearchResultIdWithPositionIndices& results,
    int position_index) {}

bool AppListTestViewDelegate::IsAssistantAllowedAndEnabled() const {
  return false;
}

bool AppListTestViewDelegate::ShouldShowAssistantPrivacyInfo() const {
  return false;
}

void AppListTestViewDelegate::MaybeIncreaseAssistantPrivacyInfoShownCount() {}

void AppListTestViewDelegate::MarkAssistantPrivacyInfoDismissed() {}

void AppListTestViewDelegate::OnStateTransitionAnimationCompleted(
    ash::AppListViewState state) {}

void AppListTestViewDelegate::GetAppLaunchedMetricParams(
    AppLaunchedMetricParams* metric_params) {}

gfx::Rect AppListTestViewDelegate::SnapBoundsToDisplayEdge(
    const gfx::Rect& bounds) {
  return bounds;
}

int AppListTestViewDelegate::GetShelfHeight() {
  // TODO(mmourgos): change this to 48 once shelf-hotseat flag is enabled.
  // Return the height of the shelf when clamshell mode is active.
  return 56;
}

void AppListTestViewDelegate::RecordAppLaunched(
    ash::AppListLaunchedFrom launched_from) {
  RecordAppListAppLaunched(launched_from, model_->state_fullscreen(),
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
