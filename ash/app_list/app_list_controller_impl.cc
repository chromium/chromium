// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_controller_impl.h"

#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_presenter_delegate_impl.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "extensions/common/constants.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

using chromeos::assistant::AssistantEntryPoint;
using chromeos::assistant::AssistantExitPoint;

namespace {

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

TabletModeAnimationTransition CalculateAnimationTransitionForMetrics(
    HomeScreenDelegate::AnimationTrigger trigger,
    bool launcher_should_show) {
  switch (trigger) {
    case HomeScreenDelegate::AnimationTrigger::kHideForWindow:
      return TabletModeAnimationTransition::kHideHomeLauncherForWindow;
    case HomeScreenDelegate::AnimationTrigger::kLauncherButton:
      return TabletModeAnimationTransition::kHomeButtonShow;
    case HomeScreenDelegate::AnimationTrigger::kDragRelease:
      return launcher_should_show
                 ? TabletModeAnimationTransition::kDragReleaseShow
                 : TabletModeAnimationTransition::kDragReleaseHide;
    case HomeScreenDelegate::AnimationTrigger::kOverviewModeSlide:
      return launcher_should_show
                 ? TabletModeAnimationTransition::kExitOverviewMode
                 : TabletModeAnimationTransition::kEnterOverviewMode;
    case HomeScreenDelegate::AnimationTrigger::kOverviewModeFade:
      return launcher_should_show
                 ? TabletModeAnimationTransition::kFadeOutOverview
                 : TabletModeAnimationTransition::kFadeInOverview;
  }
}

int GetAssistantPrivacyInfoShownCount() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return prefs->GetInteger(prefs::kAssistantPrivacyInfoShownInLauncher);
}

void SetAssistantPrivacyInfoShownCount(int count) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetInteger(prefs::kAssistantPrivacyInfoShownInLauncher, count);
}

bool IsAssistantPrivacyInfoDismissed() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return prefs->GetBoolean(prefs::kAssistantPrivacyInfoDismissedInLauncher);
}

void SetAssistantPrivacyInfoDismissed() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetBoolean(prefs::kAssistantPrivacyInfoDismissedInLauncher, true);
}

int GetSuggestedContentInfoShownCount() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return prefs->GetInteger(prefs::kSuggestedContentInfoShownInLauncher);
}

void SetSuggestedContentInfoShownCount(int count) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetInteger(prefs::kSuggestedContentInfoShownInLauncher, count);
}

bool IsSuggestedContentInfoDismissed() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return prefs->GetBoolean(prefs::kSuggestedContentInfoDismissedInLauncher);
}

void SetSuggestedContentInfoDismissed() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetBoolean(prefs::kSuggestedContentInfoDismissedInLauncher, true);
}

bool IsSuggestedContentEnabled() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return prefs->GetBoolean(chromeos::prefs::kSuggestedContentEnabled);
}

// Gets the MRU window shown over the applist when in tablet mode.
// Returns nullptr if no windows are shown over the applist.
aura::Window* GetTopVisibleWindow() {
  std::vector<aura::Window*> window_list =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          DesksMruType::kActiveDesk);
  for (auto* window : window_list) {
    if (window->TargetVisibility() && !WindowState::Get(window)->IsMinimized())
      return window;
  }
  return nullptr;
}

void LogAppListShowSource(AppListShowSource show_source) {
  UMA_HISTOGRAM_ENUMERATION(kAppListToggleMethodHistogram, show_source);
}

base::Optional<TabletModeAnimationTransition>
GetTransitionFromMetricsAnimationInfo(
    base::Optional<HomeScreenDelegate::AnimationInfo> animation_info) {
  if (!animation_info.has_value())
    return base::nullopt;

  return CalculateAnimationTransitionForMetrics(animation_info->trigger,
                                                animation_info->showing);
}

}  // namespace

AppListControllerImpl::AppListControllerImpl()
    : model_(std::make_unique<AppListModel>()),
      color_provider_(AppListColorProviderImpl()),
      presenter_(std::make_unique<AppListPresenterDelegateImpl>(this)),
      is_notification_indicator_enabled_(
          ::features::IsNotificationIndicatorEnabled()) {
  model_->AddObserver(this);
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  session_controller->AddObserver(this);

  // In case of crash-and-restart case where session state starts with ACTIVE
  // and does not change to trigger OnSessionStateChanged(), notify the current
  // session state here to ensure that the app list is shown.
  OnSessionStateChanged(session_controller->GetSessionState());

  Shell* shell = Shell::Get();
  shell->tablet_mode_controller()->AddObserver(this);
  shell->wallpaper_controller()->AddObserver(this);
  shell->AddShellObserver(this);
  shell->overview_controller()->AddObserver(this);
  keyboard::KeyboardUIController::Get()->AddObserver(this);
  AssistantState::Get()->AddObserver(this);
  shell->window_tree_host_manager()->AddObserver(this);
  shell->mru_window_tracker()->AddObserver(this);
  AssistantController::Get()->AddObserver(this);
  AssistantUiController::Get()->GetModel()->AddObserver(this);
  message_center::MessageCenter::Get()->AddObserver(this);
}

AppListControllerImpl::~AppListControllerImpl() {
  if (tracked_app_window_) {
    tracked_app_window_->RemoveObserver(this);
    tracked_app_window_ = nullptr;
  }

  // If this is being destroyed before the Shell starts shutting down, first
  // remove this from objects it's observing.
  if (!is_shutdown_)
    Shutdown();

  if (client_)
    client_->OnAppListControllerDestroyed();
}

// static
void AppListControllerImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kAssistantPrivacyInfoShownInLauncher, 0);
  registry->RegisterBooleanPref(
      prefs::kAssistantPrivacyInfoDismissedInLauncher, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      prefs::kSuggestedContentInfoShownInLauncher, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kSuggestedContentInfoDismissedInLauncher, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

void AppListControllerImpl::SetClient(AppListClient* client) {
  client_ = client;
}

AppListClient* AppListControllerImpl::GetClient() {
  DCHECK(client_);
  return client_;
}

AppListModel* AppListControllerImpl::GetModel() {
  return model_.get();
}

SearchModel* AppListControllerImpl::GetSearchModel() {
  return &search_model_;
}

AppListNotifier* AppListControllerImpl::GetNotifier() {
  if (!client_)
    return nullptr;
  return client_->GetNotifier();
}

void AppListControllerImpl::AddItem(
    std::unique_ptr<AppListItemMetadata> item_data) {
  const std::string folder_id = item_data->folder_id;
  if (folder_id.empty())
    model_->AddItem(CreateAppListItem(std::move(item_data)));
  else
    AddItemToFolder(std::move(item_data), folder_id);
}

void AppListControllerImpl::AddItemToFolder(
    std::unique_ptr<AppListItemMetadata> item_data,
    const std::string& folder_id) {
  // When we're setting a whole model of a profile, each item may have its
  // folder id set properly. However, |AppListModel::AddItemToFolder| requires
  // the item to add is not in the target folder yet, and sets its folder id
  // later. So we should clear the folder id here to avoid breaking checks.
  item_data->folder_id.clear();
  model_->AddItemToFolder(CreateAppListItem(std::move(item_data)), folder_id);
}

void AppListControllerImpl::RemoveItem(const std::string& id) {
  model_->DeleteItem(id);
}

void AppListControllerImpl::RemoveUninstalledItem(const std::string& id) {
  model_->DeleteUninstalledItem(id);
}

void AppListControllerImpl::MoveItemToFolder(const std::string& id,
                                             const std::string& folder_id) {
  AppListItem* item = model_->FindItem(id);
  model_->MoveItemToFolder(item, folder_id);
}

void AppListControllerImpl::SetStatus(AppListModelStatus status) {
  model_->SetStatus(status);
}

void AppListControllerImpl::SetSearchEngineIsGoogle(bool is_google) {
  search_model_.SetSearchEngineIsGoogle(is_google);
}

void AppListControllerImpl::UpdateSearchBox(const base::string16& text,
                                            bool initiated_by_user) {
  search_model_.search_box()->Update(text, initiated_by_user);
}

void AppListControllerImpl::PublishSearchResults(
    std::vector<std::unique_ptr<SearchResultMetadata>> results) {
  std::vector<std::unique_ptr<SearchResult>> new_results;
  for (auto& result_metadata : results) {
    std::unique_ptr<SearchResult> result = std::make_unique<SearchResult>();
    result->SetMetadata(std::move(result_metadata));
    new_results.push_back(std::move(result));
  }
  search_model_.PublishResults(std::move(new_results));
}

void AppListControllerImpl::SetItemMetadata(
    const std::string& id,
    std::unique_ptr<AppListItemMetadata> data) {
  AppListItem* item = model_->FindItem(id);
  if (!item)
    return;

  // data may not contain valid position or icon. Preserve it in this case.
  if (!data->position.IsValid())
    data->position = item->position();

  // Update the item's position and name based on the metadata.
  if (!data->position.Equals(item->position()))
    model_->SetItemPosition(item, data->position);

  if (data->short_name.empty()) {
    if (data->name != item->name()) {
      model_->SetItemName(item, data->name);
    }
  } else {
    if (data->name != item->name() || data->short_name != item->short_name()) {
      model_->SetItemNameAndShortName(item, data->name, data->short_name);
    }
  }

  // Folder icon is generated on ash side and chrome side passes a null
  // icon here. Skip it.
  if (data->icon.isNull())
    data->icon = item->GetIcon(AppListConfigType::kShared);

  item->SetMetadata(std::move(data));
}

void AppListControllerImpl::SetItemIcon(const std::string& id,
                                        const gfx::ImageSkia& icon) {
  AppListItem* item = model_->FindItem(id);
  if (item)
    item->SetIcon(AppListConfigType::kShared, icon);
}

void AppListControllerImpl::SetModelData(
    int profile_id,
    std::vector<std::unique_ptr<AppListItemMetadata>> apps,
    bool is_search_engine_google) {
  // Clear old model data.
  model_->DeleteAllItems();
  search_model_.DeleteAllResults();

  profile_id_ = profile_id;

  // Populate new models. First populate folders and then other items to avoid
  // automatically creating folder items in |AddItemToFolder|.
  for (auto& app : apps) {
    if (!app->is_folder)
      continue;
    DCHECK(app->folder_id.empty());
    AddItem(std::move(app));
  }
  for (auto& app : apps) {
    if (!app)
      continue;
    AddItem(std::move(app));
  }
  search_model_.SetSearchEngineIsGoogle(is_search_engine_google);
}

void AppListControllerImpl::SetSearchResultMetadata(
    std::unique_ptr<SearchResultMetadata> metadata) {
  SearchResult* result = search_model_.FindSearchResult(metadata->id);
  if (result)
    result->SetMetadata(std::move(metadata));
}

void AppListControllerImpl::GetIdToAppListIndexMap(
    GetIdToAppListIndexMapCallback callback) {
  base::flat_map<std::string, uint16_t> id_to_app_list_index;
  for (size_t i = 0; i < model_->top_level_item_list()->item_count(); ++i)
    id_to_app_list_index[model_->top_level_item_list()->item_at(i)->id()] = i;
  std::move(callback).Run(id_to_app_list_index);
}

void AppListControllerImpl::FindOrCreateOemFolder(
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preferred_oem_position,
    FindOrCreateOemFolderCallback callback) {
  AppListFolderItem* oem_folder = model_->FindFolderItem(kOemFolderId);
  if (!oem_folder) {
    std::unique_ptr<AppListFolderItem> new_folder =
        std::make_unique<AppListFolderItem>(kOemFolderId);
    syncer::StringOrdinal oem_position = preferred_oem_position.IsValid()
                                             ? preferred_oem_position
                                             : GetOemFolderPos();
    // Do not create a sync item for the OEM folder here, do it in
    // ResolveFolderPositions() when the item position is finalized.
    oem_folder =
        static_cast<AppListFolderItem*>(model_->AddItem(std::move(new_folder)));
    model_->SetItemPosition(oem_folder, oem_position);
  }
  model_->SetItemName(oem_folder, oem_folder_name);
  std::move(callback).Run();
}

void AppListControllerImpl::ResolveOemFolderPosition(
    const syncer::StringOrdinal& preferred_oem_position,
    ResolveOemFolderPositionCallback callback) {
  // In ash:
  AppListFolderItem* ash_oem_folder = FindFolderItem(kOemFolderId);
  std::unique_ptr<AppListItemMetadata> metadata;
  if (ash_oem_folder) {
    const syncer::StringOrdinal& oem_folder_pos =
        preferred_oem_position.IsValid() ? preferred_oem_position
                                         : GetOemFolderPos();
    model_->SetItemPosition(ash_oem_folder, oem_folder_pos);
    metadata = ash_oem_folder->CloneMetadata();
  }
  std::move(callback).Run(std::move(metadata));
}

void AppListControllerImpl::NotifyProcessSyncChangesFinished() {
  // When there are incompatible apps on different devices under the same
  // user account, it is possible that moving or adding an app on an empty
  // spot on a page of a different type of device (e.g. Device 1) may cause app
  // overflow on another device (e.g. Device 2) since it may have more apps on
  // the same page. See details in http://crbug.com/1098174.
  // When the change is synced to the Device 2, paged view structure may load
  // meta data and detect a full page of apps without a page break item
  // at the end of the overflowed page. Therefore, after the sync service has
  // finished processing sync change, SaveToMetaData should be called to insert
  // page break items if there are any missing at the end of full pages.
  AppListView* const app_list_view = presenter_.GetView();
  if (app_list_view) {
    app_list_view->app_list_main_view()
        ->contents_view()
        ->apps_container_view()
        ->apps_grid_view()
        ->UpdatePagedViewStructure();
  }
}

void AppListControllerImpl::DismissAppList() {
  if (tracked_app_window_) {
    tracked_app_window_->RemoveObserver(this);
    tracked_app_window_ = nullptr;
  }

  presenter_.Dismiss(base::TimeTicks());
}

void AppListControllerImpl::GetAppInfoDialogBounds(
    GetAppInfoDialogBoundsCallback callback) {
  AppListView* app_list_view = presenter_.GetView();
  gfx::Rect bounds = gfx::Rect();
  if (app_list_view)
    bounds = app_list_view->GetAppInfoDialogBounds();
  std::move(callback).Run(bounds);
}

void AppListControllerImpl::ShowAppList() {
  presenter_.Show(GetDisplayIdToShowAppListOn(), base::TimeTicks());
}

aura::Window* AppListControllerImpl::GetWindow() {
  return presenter_.GetWindow();
}

bool AppListControllerImpl::IsVisible(
    const base::Optional<int64_t>& display_id) {
  return last_visible_ && (!display_id.has_value() ||
                           display_id.value() == last_visible_display_id_);
}

////////////////////////////////////////////////////////////////////////////////
// AppListModelObserver:

void AppListControllerImpl::OnAppListItemAdded(AppListItem* item) {
  client_->OnItemAdded(profile_id_, item->CloneMetadata());

  if (is_notification_indicator_enabled_ && cache_ &&
      notification_badging_pref_enabled_.value_or(false)) {
    // Update the notification badge indicator for the newly added app list
    // item.
    cache_->ForOneApp(item->id(), [item](const apps::AppUpdate& update) {
      item->UpdateBadge(update.HasBadge() == apps::mojom::OptionalBool::kTrue);
    });
  }
}

void AppListControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (is_notification_indicator_enabled_) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(pref_service);

    pref_change_registrar_->Add(
        prefs::kAppNotificationBadgingEnabled,
        base::BindRepeating(&AppListControllerImpl::UpdateAppBadging,
                            base::Unretained(this)));

    // Observe AppRegistryCache for the current active account to get
    // notification updates.
    AccountId account_id =
        Shell::Get()->session_controller()->GetActiveAccountId();
    cache_ =
        apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id);
    Observe(cache_);

    // Resetting the recorded pref forces the next call to UpdateAppBadging()
    // to update notification badging for every app item.
    notification_badging_pref_enabled_.reset();
    UpdateAppBadging();
  }

  if (!IsTabletMode()) {
    DismissAppList();
    return;
  }

  // The app list is not dismissed before switching user, suggestion chips will
  // not be shown. So reset app list state and trigger an initial search here to
  // update the suggestion results.
  if (presenter_.GetView()) {
    presenter_.GetView()->CloseOpenedPage();
    presenter_.GetView()->search_box_view()->ClearSearch();
  }
}

void AppListControllerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (!IsTabletMode())
    return;

  if (state != session_manager::SessionState::ACTIVE)
    return;

  // Show the app list after signing in in tablet mode. For metrics, the app
  // list is not considered shown since the browser window is shown over app
  // list upon login.
  if (!presenter_.GetTargetVisibility())
    Shell::Get()->home_screen_controller()->Show();

  // Hide app list UI initially to prevent app list from flashing in background
  // while the initial app window is being shown.
  if (!last_target_visible_ && !ShouldHomeLauncherBeVisible())
    presenter_.SetViewVisibility(false);
  else
    OnVisibilityChanged(true, last_visible_display_id_);
}

void AppListControllerImpl::OnAppListItemWillBeDeleted(AppListItem* item) {
  if (!client_)
    return;

  if (item->is_folder())
    client_->OnFolderDeleted(profile_id_, item->CloneMetadata());

  if (item->is_page_break())
    client_->OnPageBreakItemDeleted(profile_id_, item->id());
}

void AppListControllerImpl::OnAppListItemUpdated(AppListItem* item) {
  if (client_)
    client_->OnItemUpdated(profile_id_, item->CloneMetadata());
}

void AppListControllerImpl::OnAppListStateChanged(AppListState new_state,
                                                  AppListState old_state) {
  UpdateLauncherContainer();

  if (new_state == AppListState::kStateEmbeddedAssistant) {
    // ShowUi() will be no-op if the Assistant UI is already visible.
    AssistantUiController::Get()->ShowUi(AssistantEntryPoint::kUnspecified);
    return;
  }

  if (old_state == AppListState::kStateEmbeddedAssistant) {
    // CloseUi() will be no-op if the Assistant UI is already closed.
    AssistantUiController::Get()->CloseUi(AssistantExitPoint::kBackInLauncher);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Methods used in Ash

bool AppListControllerImpl::GetTargetVisibility(
    const base::Optional<int64_t>& display_id) const {
  return last_target_visible_ &&
         (!display_id.has_value() ||
          display_id.value() == last_visible_display_id_);
}

void AppListControllerImpl::Show(int64_t display_id,
                                 base::Optional<AppListShowSource> show_source,
                                 base::TimeTicks event_time_stamp) {
  if (show_source.has_value())
    LogAppListShowSource(show_source.value());

  presenter_.Show(display_id, event_time_stamp);

  // AppListControllerImpl::Show is called in ash at the first time of showing
  // app list view. So check whether the expand arrow view should be visible.
  UpdateExpandArrowVisibility();
}

void AppListControllerImpl::UpdateYPositionAndOpacity(
    int y_position_in_screen,
    float background_opacity) {
  // Avoid changing app list opacity and position when homecher is enabled.
  if (IsTabletMode())
    return;
  presenter_.UpdateYPositionAndOpacity(y_position_in_screen,
                                       background_opacity);
}

void AppListControllerImpl::EndDragFromShelf(AppListViewState app_list_state) {
  // Avoid dragging app list when homecher is enabled.
  if (IsTabletMode())
    return;
  presenter_.EndDragFromShelf(app_list_state);
}

void AppListControllerImpl::ProcessMouseWheelEvent(
    const ui::MouseWheelEvent& event) {
  presenter_.ProcessMouseWheelOffset(event.offset());
}

ShelfAction AppListControllerImpl::ToggleAppList(
    int64_t display_id,
    AppListShowSource show_source,
    base::TimeTicks event_time_stamp) {
  if (IsTabletMode()) {
    bool handled = Shell::Get()->home_screen_controller()->GoHome(display_id);

    // Perform the "back" action for the app list.
    if (!handled) {
      Back();
      return SHELF_ACTION_APP_LIST_BACK;
    }

    LogAppListShowSource(show_source);
    return SHELF_ACTION_APP_LIST_SHOWN;
  }

  base::AutoReset<bool> auto_reset(&should_dismiss_immediately_,
                                   display_id != last_visible_display_id_);
  ShelfAction action =
      presenter_.ToggleAppList(display_id, show_source, event_time_stamp);
  UpdateExpandArrowVisibility();
  if (action == SHELF_ACTION_APP_LIST_SHOWN)
    LogAppListShowSource(show_source);
  return action;
}

AppListViewState AppListControllerImpl::GetAppListViewState() {
  return model_->state_fullscreen();
}

bool AppListControllerImpl::ShouldHomeLauncherBeVisible() const {
  if (!IsTabletMode())
    return false;

  if (home_launcher_transition_state_ ==
      HomeLauncherTransitionState::kMostlyShown)
    return true;

  return !Shell::Get()->overview_controller()->InOverviewSession() &&
         !GetTopVisibleWindow();
}

void AppListControllerImpl::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  if (!IsTabletMode())
    DismissAppList();
}

void AppListControllerImpl::OnShellDestroying() {
  // Stop observing at the beginning of ~Shell to avoid unnecessary work during
  // Shell shutdown.
  Shutdown();
}

void AppListControllerImpl::OnOverviewModeStarting() {
  if (IsTabletMode()) {
    const int64_t display_id = last_visible_display_id_;
    OnVisibilityWillChange(false /*shown*/, display_id);
  } else {
    DismissAppList();
  }
}

void AppListControllerImpl::OnOverviewModeStartingAnimationComplete(
    bool canceled) {
  if (!IsTabletMode())
    return;

  // If overview start was canceled, overview end animations are about to start.
  // Preemptively update the target app list visibility.
  if (canceled) {
    OnVisibilityWillChange(!GetTopVisibleWindow(), last_visible_display_id_);
    return;
  }

  OnVisibilityChanged(false /* shown */, last_visible_display_id_);
}

void AppListControllerImpl::OnOverviewModeEnding(OverviewSession* session) {
  if (!IsTabletMode())
    return;

  // Overview state might end during home launcher transition - if that is the
  // case, respect the final state set by in-progress home launcher transition.
  if (home_launcher_transition_state_ != HomeLauncherTransitionState::kFinished)
    return;

  OnVisibilityWillChange(!GetTopVisibleWindow() /*shown*/,
                         last_visible_display_id_);
}

void AppListControllerImpl::OnOverviewModeEnded() {
  if (!IsTabletMode())
    return;
  // Overview state might end during home launcher transition - if that is the
  // case, respect the final state set by in-progress home launcher transition.
  if (home_launcher_transition_state_ != HomeLauncherTransitionState::kFinished)
    return;
  OnVisibilityChanged(!GetTopVisibleWindow(), last_visible_display_id_);
}

void AppListControllerImpl::OnTabletModeStarted() {
  const AppListView* app_list_view = presenter_.GetView();
  // In tablet mode shelf orientation is always "bottom". Dismiss app list if
  // switching to tablet mode from side shelf app list, to ensure the app list
  // is re-shown and laid out with correct "side shelf" value.
  if (app_list_view && app_list_view->is_side_shelf())
    DismissAppList();

  presenter_.OnTabletModeChanged(true);

  // Show the app list if the tablet mode starts.
  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::ACTIVE) {
    Shell::Get()->home_screen_controller()->Show();
  }
  UpdateLauncherContainer();

  // If the app list is visible before the transition to tablet mode,
  // AppListPresenter relies on the active window change to detect the app list
  // view got hidden behind a window. Though, app list UI moving behind an app
  // window does not always cause an active window change:
  // *   If the app list is still being shown - given that app list takes focus
  //     from the top window only when it's fully shown, the focus will remain
  //     within the app window throughout the tablet mode transition.
  // *   If the assistant UI is visible before the tablet mode transition - the
  //     assistant will keep the focus during transition, even though the app
  //     window will be shown over the app list view.
  // Ensure the app list visibility is properly updated if the app list is
  // hidden behind a window at this point.
  if (last_target_visible_ && !ShouldHomeLauncherBeVisible())
    OnVisibilityChanged(false, last_visible_display_id_);
}

void AppListControllerImpl::OnTabletModeEnded() {
  aura::Window* window = presenter_.GetWindow();
  base::AutoReset<bool> auto_reset(
      &should_dismiss_immediately_,
      window && RootWindowController::ForWindow(window)
                    ->GetShelfLayoutManager()
                    ->HasVisibleWindow());
  presenter_.OnTabletModeChanged(false);
  UpdateLauncherContainer();

  // Dismiss the app list if the tablet mode ends.
  DismissAppList();
}

void AppListControllerImpl::OnWallpaperColorsChanged() {
  if (IsVisible(last_visible_display_id_))
    presenter_.GetView()->OnWallpaperColorsChanged();
}

void AppListControllerImpl::OnKeyboardVisibilityChanged(const bool is_visible) {
  onscreen_keyboard_shown_ = is_visible;
  AppListView* app_list_view = presenter_.GetView();
  if (app_list_view)
    app_list_view->OnScreenKeyboardShown(is_visible);
}

void AppListControllerImpl::OnAssistantStatusChanged(
    chromeos::assistant::AssistantStatus status) {
  UpdateAssistantVisibility();
}

void AppListControllerImpl::OnAssistantSettingsEnabled(bool enabled) {
  UpdateAssistantVisibility();
}

void AppListControllerImpl::OnAssistantFeatureAllowedChanged(
    chromeos::assistant::AssistantAllowedState state) {
  UpdateAssistantVisibility();
}

void AppListControllerImpl::OnDisplayConfigurationChanged() {
  // Entering tablet mode triggers a display configuration change when we
  // automatically switch to mirror mode. Switching to mirror mode happens
  // asynchronously (see DisplayConfigurationObserver::OnTabletModeStarted()).
  // This may result in the removal of a window tree host, as in the example of
  // switching to tablet mode while Unified Desktop mode is on; the Unified host
  // will be destroyed and the Home Launcher (which was created earlier when we
  // entered tablet mode) will be dismissed.
  // To avoid crashes, we must ensure that the Home Launcher shown status is as
  // expected if it's enabled and we're still in tablet mode.
  // https://crbug.com/900956.
  const bool should_be_shown =
      IsTabletMode() && Shell::Get()->session_controller()->GetSessionState() ==
                            session_manager::SessionState::ACTIVE;

  if (!should_be_shown || should_be_shown == presenter_.GetTargetVisibility())
    return;

  Shell::Get()->home_screen_controller()->Show();
}

void AppListControllerImpl::OnWindowUntracked(aura::Window* untracked_window) {
  UpdateExpandArrowVisibility();
}

void AppListControllerImpl::OnAssistantReady() {
  UpdateAssistantVisibility();
}

void AppListControllerImpl::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  switch (new_visibility) {
    case AssistantVisibility::kVisible:
      if (!IsVisible(base::nullopt)) {
        Show(GetDisplayIdToShowAppListOn(), kAssistantEntryPoint,
             base::TimeTicks());
      }

      if (!IsShowingEmbeddedAssistantUI())
        presenter_.ShowEmbeddedAssistantUI(true);

      // Make sure that app list views are visible - they might get hidden
      // during session startup, and the app list visibility might not have yet
      // changed to visible by this point.
      presenter_.SetViewVisibility(true);
      break;
    case AssistantVisibility::kClosed:
      if (!IsShowingEmbeddedAssistantUI())
        break;

      // When Launcher is closing, we do not want to call
      // |ShowEmbeddedAssistantUI(false)|, which will show previous state page
      // in Launcher and make the UI flash.
      if (IsTabletMode()) {
        base::Optional<ContentsView::ScopedSetActiveStateAnimationDisabler>
            set_active_state_animation_disabler;
        // When taking a screenshot by Assistant, we do not want to animate to
        // the final state. Otherwise the screenshot may have tansient state
        // during the animation. In tablet mode, we want to go back to
        // kStateApps immediately, i.e. skipping the animation in
        // |SetActiveStateInternal|, which are called from
        // |ShowEmbeddedAssistantUI(false)| and
        // |ClearSearchAndDeactivateSearchBox()|.
        if (exit_point == AssistantExitPoint::kScreenshot) {
          set_active_state_animation_disabler.emplace(
              presenter_.GetView()->app_list_main_view()->contents_view());
        }

        presenter_.ShowEmbeddedAssistantUI(false);

        if (exit_point != AssistantExitPoint::kBackInLauncher) {
          presenter_.GetView()
              ->search_box_view()
              ->ClearSearchAndDeactivateSearchBox();
        }
      } else if (exit_point != AssistantExitPoint::kBackInLauncher) {
        // Similarly, when taking a screenshot by Assistant in clamshell mode,
        // we do not want to dismiss launcher with animation. Otherwise the
        // screenshot may have tansient state during the animation.
        base::AutoReset<bool> auto_reset(
            &should_dismiss_immediately_,
            exit_point == AssistantExitPoint::kScreenshot);
        DismissAppList();
      }

      break;
  }
}

base::ScopedClosureRunner
AppListControllerImpl::DisableHomeScreenBackgroundBlur() {
  AppListView* const app_list_view = presenter_.GetView();
  if (!app_list_view)
    return base::ScopedClosureRunner(base::DoNothing());
  return app_list_view->app_list_main_view()
      ->contents_view()
      ->apps_container_view()
      ->DisableSuggestionChipsBlur();
}

void AppListControllerImpl::OnHomeLauncherAnimationComplete(
    bool shown,
    int64_t display_id) {
  // Stop disabling background blur in home screen when the home screen
  // transition ends.
  home_screen_blur_disabler_.reset();

  home_launcher_transition_state_ = HomeLauncherTransitionState::kFinished;

  AssistantUiController::Get()->CloseUi(
      shown ? AssistantExitPoint::kLauncherOpen
            : AssistantExitPoint::kLauncherClose);

  // Animations can be reversed (e.g. in a drag). Let's ensure the target
  // visibility is correct first.
  OnVisibilityChanged(shown, display_id);

  if (!home_launcher_animation_callback_.is_null())
    home_launcher_animation_callback_.Run(shown);
}

void AppListControllerImpl::OnHomeLauncherPositionChanged(int percent_shown,
                                                          int64_t display_id) {
  // Disable home screen background blur if the home launcher transition is
  // staring - the blur disabler will be reset when the transition ends (in
  // OnHomeLauncherAnimationComplete()).
  if (home_launcher_transition_state_ == HomeLauncherTransitionState::kFinished)
    home_screen_blur_disabler_ = DisableHomeScreenBackgroundBlur();

  const bool mostly_shown = percent_shown >= 50;
  home_launcher_transition_state_ =
      mostly_shown ? HomeLauncherTransitionState::kMostlyShown
                   : HomeLauncherTransitionState::kMostlyHidden;
  OnVisibilityWillChange(mostly_shown, display_id);
}

void AppListControllerImpl::ShowHomeScreenView() {
  DCHECK(IsTabletMode());

  // App list is only considered shown for metrics if there are currently no
  // other visible windows shown over the app list after the tablet transition.
  base::Optional<AppListShowSource> show_source;
  if (!GetTopVisibleWindow())
    show_source = kTabletMode;

  Show(GetDisplayIdToShowAppListOn(), show_source, base::TimeTicks());
}

aura::Window* AppListControllerImpl::GetHomeScreenWindow() {
  return presenter_.GetWindow();
}

void AppListControllerImpl::UpdateYPositionAndOpacityForHomeLauncher(
    int y_position_in_screen,
    float opacity,
    base::Optional<AnimationInfo> animation_info,
    UpdateAnimationSettingsCallback callback) {
  DCHECK(!animation_info.has_value() || !callback.is_null());

  presenter_.UpdateYPositionAndOpacityForHomeLauncher(
      y_position_in_screen, opacity,
      GetTransitionFromMetricsAnimationInfo(std::move(animation_info)),
      std::move(callback));
}

void AppListControllerImpl::UpdateScaleAndOpacityForHomeLauncher(
    float scale,
    float opacity,
    base::Optional<AnimationInfo> animation_info,
    UpdateAnimationSettingsCallback callback) {
  DCHECK(!animation_info.has_value() || !callback.is_null());

  presenter_.UpdateScaleAndOpacityForHomeLauncher(
      scale, opacity,
      GetTransitionFromMetricsAnimationInfo(std::move(animation_info)),
      std::move(callback));
}

base::Optional<base::TimeDelta>
AppListControllerImpl::GetOptionalAnimationDuration() {
  if (model_->state() == AppListState::kStateEmbeddedAssistant) {
    // If Assistant is shown, we don't want any delay in animation transitions
    // since the launcher is already shown.
    return base::TimeDelta::Min();
  }
  return base::nullopt;
}

void AppListControllerImpl::Back() {
  presenter_.GetView()->Back();
}

void AppListControllerImpl::SetKeyboardTraversalMode(bool engaged) {
  if (keyboard_traversal_engaged_ == engaged)
    return;

  keyboard_traversal_engaged_ = engaged;

  views::View* focused_view =
      presenter_.GetView()->GetFocusManager()->GetFocusedView();

  if (!focused_view)
    return;

  // When the search box has focus, it is actually the textfield that has focus.
  // As such, the |SearchBoxView| must be told to repaint directly.
  if (focused_view == presenter_.GetView()->search_box_view()->search_box())
    presenter_.GetView()->search_box_view()->SchedulePaint();
  else
    focused_view->SchedulePaint();
}

bool AppListControllerImpl::IsShowingEmbeddedAssistantUI() const {
  return presenter_.IsShowingEmbeddedAssistantUI();
}

void AppListControllerImpl::UpdateExpandArrowVisibility() {
  bool should_show = false;

  // Hide the expand arrow view when in tablet mode and an activatable window
  // cannot be dragged from top of the screen. This will be the case if:
  // *   there is no activatable window on the current active desk, or
  // *   kDragFromShelfToHomeOrOverview feature is enabled, in which app window
  //     drag from top of home screen is disabled.
  if (IsTabletMode()) {
    should_show = !features::IsDragFromShelfToHomeOrOverviewEnabled() &&
                  !Shell::Get()
                       ->mru_window_tracker()
                       ->BuildWindowForCycleList(kActiveDesk)
                       .empty();
  } else {
    should_show = true;
  }

  presenter_.SetExpandArrowViewVisibility(should_show);
}

AppListViewState AppListControllerImpl::CalculateStateAfterShelfDrag(
    const ui::LocatedEvent& event_in_screen,
    float launcher_above_shelf_bottom_amount) const {
  if (presenter_.GetView())
    return presenter_.GetView()->CalculateStateAfterShelfDrag(
        event_in_screen, launcher_above_shelf_bottom_amount);
  return AppListViewState::kClosed;
}

void AppListControllerImpl::SetAppListModelForTest(
    std::unique_ptr<AppListModel> model) {
  model_->RemoveObserver(this);
  model_ = std::move(model);
  model_->AddObserver(this);
}

void AppListControllerImpl::SetStateTransitionAnimationCallbackForTesting(
    StateTransitionAnimationCallback callback) {
  state_transition_animation_callback_ = std::move(callback);
}

void AppListControllerImpl::SetHomeLauncherAnimationCallbackForTesting(
    HomeLauncherAnimationCallback callback) {
  home_launcher_animation_callback_ = std::move(callback);
}

void AppListControllerImpl::RecordShelfAppLaunched() {
  RecordAppListAppLaunched(
      AppListLaunchedFrom::kLaunchedFromShelf,
      recorded_app_list_view_state_.value_or(GetAppListViewState()),
      IsTabletMode(), recorded_app_list_visibility_.value_or(last_visible_));
  recorded_app_list_view_state_ = base::nullopt;
  recorded_app_list_visibility_ = base::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
// Methods of |client_|:

void AppListControllerImpl::StartAssistant() {
  AssistantUiController::Get()->ShowUi(
      AssistantEntryPoint::kLauncherSearchBoxIcon);
}

void AppListControllerImpl::StartSearch(const base::string16& raw_query) {
  if (client_) {
    base::string16 query;
    base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
    client_->StartSearch(query);
    auto* notifier = GetNotifier();
    if (notifier)
      notifier->NotifySearchQueryChanged(raw_query);
  }
}

void AppListControllerImpl::OpenSearchResult(const std::string& result_id,
                                             int event_flags,
                                             AppListLaunchedFrom launched_from,
                                             AppListLaunchType launch_type,
                                             int suggestion_index,
                                             bool launch_as_default) {
  SearchResult* result = search_model_.FindSearchResult(result_id);
  if (!result)
    return;

  if (launch_type == AppListLaunchType::kAppSearchResult) {
    switch (launched_from) {
      case AppListLaunchedFrom::kLaunchedFromSearchBox:
      case AppListLaunchedFrom::kLaunchedFromSuggestionChip:
        RecordAppLaunched(launched_from);
        break;
      case AppListLaunchedFrom::kLaunchedFromGrid:
      case AppListLaunchedFrom::kLaunchedFromShelf:
        break;
    }
  }

  UMA_HISTOGRAM_ENUMERATION(kSearchResultOpenDisplayTypeHistogram,
                            result->display_type(),
                            SearchResultDisplayType::kLast);

  // Suggestion chips are not represented to the user as search results, so do
  // not record search result metrics for them.
  if (launched_from != AppListLaunchedFrom::kLaunchedFromSuggestionChip) {
    base::RecordAction(base::UserMetricsAction("AppList_OpenSearchResult"));

    UMA_HISTOGRAM_COUNTS_100(kSearchQueryLength, GetLastQueryLength());
    if (IsTabletMode()) {
      UMA_HISTOGRAM_COUNTS_100(kSearchQueryLengthInTablet,
                               GetLastQueryLength());
    } else {
      UMA_HISTOGRAM_COUNTS_100(kSearchQueryLengthInClamshell,
                               GetLastQueryLength());
    }
  }

  auto* notifier = GetNotifier();
  if (notifier) {
    // Special-case chip results, because the display type of app results
    // doesn't account for whether it's being displayed in the suggestion chips
    // or app tiles.
    AppListNotifier::Result notifier_result(result->id(),
                                            result->metrics_type());
    if (launched_from == AppListLaunchedFrom::kLaunchedFromSuggestionChip) {
      notifier->NotifyLaunched(SearchResultDisplayType::kChip, notifier_result);
    } else {
      notifier->NotifyLaunched(result->display_type(), notifier_result);
    }
  }

  if (client_) {
    client_->OpenSearchResult(result_id, event_flags, launched_from,
                              launch_type, suggestion_index, launch_as_default);
  }

  ResetHomeLauncherIfShown();
}

void AppListControllerImpl::LogResultLaunchHistogram(
    SearchResultLaunchLocation launch_location,
    int suggestion_index) {
  RecordSearchLaunchIndexAndQueryLength(launch_location, GetLastQueryLength(),
                                        suggestion_index);
}

void AppListControllerImpl::LogSearchAbandonHistogram() {
  RecordSearchAbandonWithQueryLengthHistogram(GetLastQueryLength());
}

void AppListControllerImpl::InvokeSearchResultAction(
    const std::string& result_id,
    int action_index,
    int event_flags) {
  if (client_)
    client_->InvokeSearchResultAction(result_id, action_index, event_flags);
}

void AppListControllerImpl::GetSearchResultContextMenuModel(
    const std::string& result_id,
    GetContextMenuModelCallback callback) {
  if (client_)
    client_->GetSearchResultContextMenuModel(result_id, std::move(callback));
}

void AppListControllerImpl::ViewShown(int64_t display_id) {
  UpdateAssistantVisibility();

  if (client_)
    client_->ViewShown(display_id);

  Shell::Get()->home_screen_controller()->OnAppListViewShown();

  // Ensure search box starts fresh with no ring each time it opens.
  keyboard_traversal_engaged_ = false;
}

bool AppListControllerImpl::AppListTargetVisibility() const {
  return last_target_visible_;
}

void AppListControllerImpl::ViewClosing() {
  if (presenter_.GetView()->search_box_view()->is_search_box_active()) {
    // Close the virtual keyboard before the app list view is dismissed.
    // Otherwise if the browser is behind the app list view, after the latter is
    // closed, IME is updated because of the changed focus. Consequently,
    // the virtual keyboard is hidden for the wrong IME instance, which may
    // bring troubles when restoring the virtual keyboard (see
    // https://crbug.com/944233).
    keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();
  }

  AssistantUiController::Get()->CloseUi(AssistantExitPoint::kLauncherClose);

  if (client_)
    client_->ViewClosing();

  if (Shell::Get()->home_screen_controller())
    Shell::Get()->home_screen_controller()->OnAppListViewClosing();
}

const std::vector<SkColor>&
AppListControllerImpl::GetWallpaperProminentColors() {
  return Shell::Get()->wallpaper_controller()->GetWallpaperColors();
}

void AppListControllerImpl::ActivateItem(const std::string& id,
                                         int event_flags,
                                         AppListLaunchedFrom launched_from) {
  RecordAppLaunched(launched_from);

  if (client_)
    client_->ActivateItem(profile_id_, id, event_flags);

  ResetHomeLauncherIfShown();
}

void AppListControllerImpl::GetContextMenuModel(
    const std::string& id,
    GetContextMenuModelCallback callback) {
  if (client_)
    client_->GetContextMenuModel(profile_id_, id, std::move(callback));
}

ui::ImplicitAnimationObserver* AppListControllerImpl::GetAnimationObserver(
    AppListViewState target_state) {
  // |presenter_| observes the close animation only.
  if (target_state == AppListViewState::kClosed)
    return &presenter_;
  return nullptr;
}

void AppListControllerImpl::ShowWallpaperContextMenu(
    const gfx::Point& onscreen_location,
    ui::MenuSourceType source_type) {
  Shell::Get()->ShowContextMenu(onscreen_location, source_type);
}

bool AppListControllerImpl::ProcessHomeLauncherGesture(
    ui::GestureEvent* event) {
  if (features::IsDragFromShelfToHomeOrOverviewEnabled())
    return false;

  HomeLauncherGestureHandler* home_launcher_gesture_handler =
      Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
  const gfx::PointF event_location =
      event->details().bounding_box_f().CenterPoint();
  switch (event->type()) {
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_SCROLL_BEGIN:
      return home_launcher_gesture_handler->OnPressEvent(
          HomeLauncherGestureHandler::Mode::kSlideDownToHide, event_location);
    case ui::ET_GESTURE_SCROLL_UPDATE:
      return home_launcher_gesture_handler->OnScrollEvent(
          event_location, event->details().scroll_x(),
          event->details().scroll_y());
    case ui::ET_GESTURE_END:
      return home_launcher_gesture_handler->OnReleaseEvent(
          event_location,
          /*velocity_y=*/base::nullopt);
    default:
      break;
  }

  NOTREACHED();
  return false;
}

bool AppListControllerImpl::KeyboardTraversalEngaged() {
  return keyboard_traversal_engaged_;
}

bool AppListControllerImpl::CanProcessEventsOnApplistViews() {
  // Do not allow processing events during overview or while overview is
  // finished but still animating out.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession() ||
      overview_controller->IsCompletingShutdownAnimations()) {
    return false;
  }

  HomeScreenController* home_screen_controller =
      Shell::Get()->home_screen_controller();
  return home_screen_controller &&
         home_screen_controller->home_launcher_gesture_handler()->mode() !=
             HomeLauncherGestureHandler::Mode::kSlideUpToShow;
}

bool AppListControllerImpl::ShouldDismissImmediately() {
  if (should_dismiss_immediately_)
    return true;

  DCHECK(Shell::HasInstance());
  const int ideal_shelf_y =
      Shelf::ForWindow(presenter_.GetView()->GetWidget()->GetNativeView())
          ->GetIdealBounds()
          .y();

  const int current_y =
      presenter_.GetView()->GetWidget()->GetNativeWindow()->bounds().y();
  return current_y > ideal_shelf_y;
}

void AppListControllerImpl::GetNavigableContentsFactory(
    mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver) {
  if (client_)
    client_->GetNavigableContentsFactory(std::move(receiver));
}

int AppListControllerImpl::GetTargetYForAppListHide(aura::Window* root_window) {
  DCHECK(Shell::HasInstance());
  gfx::Point top_center =
      Shelf::ForWindow(root_window)->GetShelfBoundsInScreen().top_center();
  wm::ConvertPointFromScreen(root_window, &top_center);
  return top_center.y();
}

AssistantViewDelegate* AppListControllerImpl::GetAssistantViewDelegate() {
  return Shell::Get()->assistant_controller()->view_delegate();
}

void AppListControllerImpl::OnSearchResultVisibilityChanged(
    const std::string& id,
    bool visibility) {
  if (client_)
    client_->OnSearchResultVisibilityChanged(id, visibility);
}

void AppListControllerImpl::NotifySearchResultsForLogging(
    const base::string16& raw_query,
    const SearchResultIdWithPositionIndices& results,
    int position_index) {
  if (client_) {
    base::string16 query;
    base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
    client_->NotifySearchResultsForLogging(query, results, position_index);
  }
}

void AppListControllerImpl::MaybeIncreasePrivacyInfoShownCounts() {
  if (ShouldShowAssistantPrivacyInfo()) {
    const int count = GetAssistantPrivacyInfoShownCount();
    SetAssistantPrivacyInfoShownCount(count + 1);
  } else if (ShouldShowSuggestedContentInfo()) {
    const int count = GetSuggestedContentInfoShownCount();
    SetSuggestedContentInfoShownCount(count + 1);
  }
}

bool AppListControllerImpl::IsAssistantAllowedAndEnabled() const {
  if (!Shell::Get()->assistant_controller()->IsAssistantReady())
    return false;

  auto* state = AssistantState::Get();
  return state->settings_enabled().value_or(false) &&
         state->allowed_state() ==
             chromeos::assistant::AssistantAllowedState::ALLOWED &&
         state->assistant_status() !=
             chromeos::assistant::AssistantStatus::NOT_READY;
}

bool AppListControllerImpl::ShouldShowAssistantPrivacyInfo() const {
  if (!IsAssistantAllowedAndEnabled())
    return false;

  if (!app_list_features::IsAssistantSearchEnabled())
    return false;

  const bool dismissed = IsAssistantPrivacyInfoDismissed();
  if (dismissed)
    return false;

  const int count = GetAssistantPrivacyInfoShownCount();
  constexpr int kThresholdToShow = 6;
  return count >= 0 && count <= kThresholdToShow;
}

void AppListControllerImpl::MarkAssistantPrivacyInfoDismissed() {
  // User dismissed the privacy info view. Will not show the view again.
  SetAssistantPrivacyInfoDismissed();
}

bool AppListControllerImpl::ShouldShowSuggestedContentInfo() const {
  if (!base::FeatureList::IsEnabled(
          chromeos::features::kSuggestedContentToggle)) {
    return false;
  }

  if (!IsSuggestedContentEnabled()) {
    // Don't show if user has interacted with the setting already.
    SetSuggestedContentInfoDismissed();
    return false;
  }

  if (IsSuggestedContentInfoDismissed()) {
    return false;
  }

  const int count = GetSuggestedContentInfoShownCount();
  constexpr int kThresholdToShow = 3;
  return count >= 0 && count <= kThresholdToShow;
}

void AppListControllerImpl::MarkSuggestedContentInfoDismissed() {
  // User dismissed the privacy info view. Will not show the view again.
  SetSuggestedContentInfoDismissed();
}

void AppListControllerImpl::OnStateTransitionAnimationCompleted(
    AppListViewState state) {
  if (!state_transition_animation_callback_.is_null())
    state_transition_animation_callback_.Run(state);
}

void AppListControllerImpl::OnViewStateChanged(AppListViewState state) {
  auto* notifier = GetNotifier();
  if (notifier)
    notifier->NotifyUIStateChanged(state);
}

void AppListControllerImpl::GetAppLaunchedMetricParams(
    AppLaunchedMetricParams* metric_params) {
  metric_params->app_list_view_state = GetAppListViewState();
  metric_params->is_tablet_mode = IsTabletMode();
  metric_params->home_launcher_shown = last_visible_;
}

gfx::Rect AppListControllerImpl::SnapBoundsToDisplayEdge(
    const gfx::Rect& bounds) {
  AppListView* app_list_view = presenter_.GetView();
  DCHECK(app_list_view && app_list_view->GetWidget());
  aura::Window* window = app_list_view->GetWidget()->GetNativeView();
  return screen_util::SnapBoundsToDisplayEdge(bounds, window);
}

int AppListControllerImpl::GetShelfSize() {
  return ShelfConfig::Get()->system_shelf_size();
}

bool AppListControllerImpl::IsInTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

AppListColorProviderImpl* AppListControllerImpl::GetColorProvider() {
  return &color_provider_;
}

void AppListControllerImpl::RecordAppLaunched(
    AppListLaunchedFrom launched_from) {
  RecordAppListAppLaunched(launched_from, GetAppListViewState(), IsTabletMode(),
                           last_visible_);
}

void AppListControllerImpl::AddObserver(AppListControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListControllerImpl::RemoveObserver(
    AppListControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListControllerImpl::OnVisibilityChanged(bool visible,
                                                int64_t display_id) {
  // Focus and app visibility changes while finishing home launcher state
  // animation may cause OnVisibilityChanged() to be called before the home
  // launcher state transition finished - delay the visibility change until the
  // home launcher stops animating, so observers do not miss the animation state
  // update.
  if (home_launcher_transition_state_ !=
      HomeLauncherTransitionState::kFinished) {
    OnVisibilityWillChange(visible, display_id);
    return;
  }

  bool real_visibility = visible;
  // HomeLauncher is only visible when no other app windows are visible,
  // unless we are in the process of animating to (or dragging) the home
  // launcher.
  if (IsTabletMode()) {
    UpdateTrackedAppWindow();

    if (tracked_app_window_)
      real_visibility = false;
  }

  aura::Window* app_list_window = GetWindow();
  real_visibility &= app_list_window && app_list_window->TargetVisibility();

  OnVisibilityWillChange(real_visibility, display_id);

  // Skip adjacent same changes.
  if (last_visible_ == real_visibility &&
      last_visible_display_id_ == display_id) {
    return;
  }

  last_visible_display_id_ = display_id;

  AppListView* const app_list_view = presenter_.GetView();
  app_list_view->UpdatePageResetTimer(real_visibility);

  if (!real_visibility) {
    app_list_view->search_box_view()->ClearSearchAndDeactivateSearchBox();
    // Reset the app list contents state, so the app list is in initial state
    // when the app list visibility changes again.
    app_list_view->app_list_main_view()->contents_view()->ResetForShow();
  }

  // Notify chrome of visibility changes.
  if (last_visible_ != real_visibility) {
    // When showing the launcher with the virtual keyboard enabled, one feature
    // called "transient blur" (which means that if focus was lost but regained
    // a few seconds later, we would show the virtual keyboard again) may show
    // the virtual keyboard, which is not what we want. So hide the virtual
    // keyboard explicitly when the launcher shows.
    if (real_visibility)
      keyboard::KeyboardUIController::Get()->HideKeyboardExplicitlyBySystem();

    if (client_)
      client_->OnAppListVisibilityChanged(real_visibility);

    last_visible_ = real_visibility;

    // We could make Assistant sub-controllers an AppListControllerObserver, but
    // we do not want to introduce new dependency of AppListController to
    // Assistant.
    GetAssistantViewDelegate()->OnHostViewVisibilityChanged(real_visibility);
    for (auto& observer : observers_)
      observer.OnAppListVisibilityChanged(real_visibility, display_id);
  }
}

void AppListControllerImpl::OnWindowVisibilityChanging(aura::Window* window,
                                                       bool visible) {
  if (visible || window != tracked_app_window_)
    return;

  UpdateTrackedAppWindow();

  if (!tracked_app_window_ && ShouldHomeLauncherBeVisible())
    OnVisibilityChanged(true, last_visible_display_id_);
}

void AppListControllerImpl::OnWindowDestroyed(aura::Window* window) {
  if (window != tracked_app_window_)
    return;

  tracked_app_window_ = nullptr;
}

void AppListControllerImpl::OnVisibilityWillChange(bool visible,
                                                   int64_t display_id) {
  bool real_target_visibility = visible;
  // HomeLauncher is only visible when no other app windows are visible,
  // unless we are in the process of animating to (or dragging) the home
  // launcher.
  if (IsTabletMode() && home_launcher_transition_state_ ==
                            HomeLauncherTransitionState::kFinished) {
    real_target_visibility &= !GetTopVisibleWindow();
  }

  // Skip adjacent same changes.
  if (last_target_visible_ == real_target_visibility &&
      last_target_visible_display_id_ == display_id) {
    return;
  }

  // Notify chrome of target visibility changes.
  if (last_target_visible_ != real_target_visibility) {
    last_target_visible_ = real_target_visibility;
    last_target_visible_display_id_ = display_id;

    if (real_target_visibility && IsTabletMode()) {
      // Update the arrow visibility when starting to show the home screen
      // (presumably, the visibility has already been updated if home is being
      // hidden).
      UpdateExpandArrowVisibility();
    }

    if (real_target_visibility && presenter_.GetView())
      presenter_.SetViewVisibility(true);

    if (client_)
      client_->OnAppListVisibilityWillChange(real_target_visibility);

    for (auto& observer : observers_) {
      observer.OnAppListVisibilityWillChange(real_target_visibility,
                                             display_id);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Private used only:

void AppListControllerImpl::OnHomeLauncherDragStart() {
  AppListView* app_list_view = presenter_.GetView();
  DCHECK(app_list_view);
  app_list_view->OnHomeLauncherDragStart();
}

void AppListControllerImpl::OnHomeLauncherDragInProgress() {
  AppListView* app_list_view = presenter_.GetView();
  DCHECK(app_list_view);
  app_list_view->OnHomeLauncherDragInProgress();
}

void AppListControllerImpl::OnHomeLauncherDragEnd() {
  AppListView* app_list_view = presenter_.GetView();
  DCHECK(app_list_view);
  app_list_view->OnHomeLauncherDragEnd();
}

syncer::StringOrdinal AppListControllerImpl::GetOemFolderPos() {
  // Place the OEM folder just after the web store, which should always be
  // followed by a pre-installed app (e.g. Search), so the poosition should be
  // stable. TODO(stevenjb): consider explicitly setting the OEM folder location
  // along with the name in ServicesCustomizationDocument::SetOemFolderName().
  AppListItemList* item_list = model_->top_level_item_list();
  if (!item_list->item_count()) {
    LOG(ERROR) << "No top level item was found. "
               << "Placing OEM folder at the beginning.";
    return syncer::StringOrdinal::CreateInitialOrdinal();
  }

  size_t web_store_app_index;
  if (!item_list->FindItemIndex(extensions::kWebStoreAppId,
                                &web_store_app_index)) {
    LOG(ERROR) << "Web store position is not found it top items. "
               << "Placing OEM folder at the end.";
    return item_list->item_at(item_list->item_count() - 1)
        ->position()
        .CreateAfter();
  }

  // Skip items with the same position.
  const AppListItem* web_store_app_item =
      item_list->item_at(web_store_app_index);
  for (size_t j = web_store_app_index + 1; j < item_list->item_count(); ++j) {
    const AppListItem* next_item = item_list->item_at(j);
    DCHECK(next_item->position().IsValid());
    if (!next_item->position().Equals(web_store_app_item->position())) {
      const syncer::StringOrdinal oem_ordinal =
          web_store_app_item->position().CreateBetween(next_item->position());
      VLOG(1) << "Placing OEM Folder at: " << j
              << " position: " << oem_ordinal.ToDebugString();
      return oem_ordinal;
    }
  }

  const syncer::StringOrdinal oem_ordinal =
      web_store_app_item->position().CreateAfter();
  VLOG(1) << "Placing OEM Folder at: " << item_list->item_count()
          << " position: " << oem_ordinal.ToDebugString();
  return oem_ordinal;
}

std::unique_ptr<AppListItem> AppListControllerImpl::CreateAppListItem(
    std::unique_ptr<AppListItemMetadata> metadata) {
  std::unique_ptr<AppListItem> app_list_item =
      metadata->is_folder ? std::make_unique<AppListFolderItem>(metadata->id)
                          : std::make_unique<AppListItem>(metadata->id);
  app_list_item->SetMetadata(std::move(metadata));
  return app_list_item;
}

AppListFolderItem* AppListControllerImpl::FindFolderItem(
    const std::string& folder_id) {
  return model_->FindFolderItem(folder_id);
}

void AppListControllerImpl::UpdateAssistantVisibility() {
  GetSearchModel()->search_box()->SetShowAssistantButton(
      IsAssistantAllowedAndEnabled());
}

int64_t AppListControllerImpl::GetDisplayIdToShowAppListOn() {
  if (IsTabletMode() && !Shell::Get()->display_manager()->IsInUnifiedMode()) {
    return display::Display::HasInternalDisplay()
               ? display::Display::InternalDisplayId()
               : display::Screen::GetScreen()->GetPrimaryDisplay().id();
  }

  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(Shell::GetRootWindowForNewWindows())
      .id();
}

void AppListControllerImpl::ResetHomeLauncherIfShown() {
  if (!IsTabletMode() || !presenter_.IsVisibleDeprecated())
    return;

  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsKeyboardVisible())
    keyboard_controller->HideKeyboardByUser();
  presenter_.GetView()->CloseOpenedPage();

  // Refresh the suggestion chips with empty query.
  StartSearch(base::string16());
}

void AppListControllerImpl::UpdateLauncherContainer(
    base::Optional<int64_t> display_id) {
  aura::Window* window = presenter_.GetWindow();
  if (!window)
    return;

  aura::Window* parent_window = GetContainerForDisplayId(display_id);
  if (parent_window && !parent_window->Contains(window)) {
    parent_window->AddChild(window);
    // Release focus if the launcher is moving behind apps, and there is app
    // window showing. Note that the app list can be shown behind apps in tablet
    // mode only.
    if (IsTabletMode() && !ShouldHomeLauncherBeVisible()) {
      WindowState* const window_state = WindowState::Get(window);
      if (window_state->IsActive())
        window_state->Deactivate();
    }
  }
}

int AppListControllerImpl::GetContainerId() const {
  return ShouldLauncherShowBehindApps() ? kShellWindowId_HomeScreenContainer
                                        : kShellWindowId_AppListContainer;
}

aura::Window* AppListControllerImpl::GetContainerForDisplayId(
    base::Optional<int64_t> display_id) {
  aura::Window* root_window = nullptr;
  if (display_id.has_value()) {
    root_window = Shell::GetRootWindowForDisplayId(display_id.value());
  } else if (presenter_.GetWindow()) {
    root_window = presenter_.GetWindow()->GetRootWindow();
  }

  return root_window ? root_window->GetChildById(GetContainerId()) : nullptr;
}

bool AppListControllerImpl::ShouldLauncherShowBehindApps() const {
  return IsTabletMode() &&
         model_->state() != AppListState::kStateEmbeddedAssistant;
}

int AppListControllerImpl::GetLastQueryLength() {
  base::string16 query;
  base::TrimWhitespace(search_model_.search_box()->text(), base::TRIM_ALL,
                       &query);
  return query.length();
}

void AppListControllerImpl::Shutdown() {
  DCHECK(!is_shutdown_);
  is_shutdown_ = true;

  Shell* shell = Shell::Get();
  message_center::MessageCenter::Get()->RemoveObserver(this);
  AssistantController::Get()->RemoveObserver(this);
  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
  shell->mru_window_tracker()->RemoveObserver(this);
  shell->window_tree_host_manager()->RemoveObserver(this);
  AssistantState::Get()->RemoveObserver(this);
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  shell->overview_controller()->RemoveObserver(this);
  shell->RemoveShellObserver(this);
  shell->wallpaper_controller()->RemoveObserver(this);
  shell->tablet_mode_controller()->RemoveObserver(this);
  shell->session_controller()->RemoveObserver(this);
  model_->RemoveObserver(this);
}

bool AppListControllerImpl::IsHomeScreenVisible() {
  return IsTabletMode() && IsVisible(base::nullopt);
}

gfx::Rect AppListControllerImpl::GetInitialAppListItemScreenBoundsForWindow(
    aura::Window* window) {
  if (!presenter_.GetView())
    return gfx::Rect();
  std::string* app_id = window->GetProperty(kAppIDKey);
  return presenter_.GetView()->GetItemScreenBoundsInFirstGridPage(
      app_id ? *app_id : std::string());
}

void AppListControllerImpl::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.HasBadgeChanged() &&
      notification_badging_pref_enabled_.value_or(false) &&
      !quiet_mode_enabled_.value_or(false)) {
    UpdateItemNotificationBadge(update.AppId(), update.HasBadge());
  }
}

void AppListControllerImpl::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void AppListControllerImpl::OnQuietModeChanged(bool in_quiet_mode) {
  UpdateAppBadging();
}

void AppListControllerImpl::UpdateTrackedAppWindow() {
  aura::Window* top_window = GetTopVisibleWindow();
  if (tracked_app_window_ == top_window)
    return;

  if (tracked_app_window_)
    tracked_app_window_->RemoveObserver(this);
  tracked_app_window_ = top_window;
  if (tracked_app_window_)
    tracked_app_window_->AddObserver(this);
}

void AppListControllerImpl::RecordAppListState() {
  recorded_app_list_view_state_ = GetAppListViewState();
  recorded_app_list_visibility_ = last_visible_;
}

void AppListControllerImpl::UpdateItemNotificationBadge(
    const std::string& app_id,
    apps::mojom::OptionalBool has_badge) {
  AppListItem* item = model_->FindItem(app_id);
  if (item)
    item->UpdateBadge(has_badge == apps::mojom::OptionalBool::kTrue);
}

void AppListControllerImpl::UpdateAppBadging() {
  bool new_badging_enabled = pref_change_registrar_
                                 ? pref_change_registrar_->prefs()->GetBoolean(
                                       prefs::kAppNotificationBadgingEnabled)
                                 : false;
  bool new_quiet_mode_enabled =
      message_center::MessageCenter::Get()->IsQuietMode();

  if (notification_badging_pref_enabled_.has_value() &&
      notification_badging_pref_enabled_.value() == new_badging_enabled &&
      quiet_mode_enabled_.has_value() &&
      quiet_mode_enabled_.value() == new_quiet_mode_enabled) {
    return;
  }
  notification_badging_pref_enabled_ = new_badging_enabled;
  quiet_mode_enabled_ = new_quiet_mode_enabled;

  if (cache_) {
    cache_->ForEachApp([this](const apps::AppUpdate& update) {
      // Set the app notification badge hidden when the pref is disabled.
      apps::mojom::OptionalBool has_badge =
          notification_badging_pref_enabled_.value() &&
                  !quiet_mode_enabled_.value() &&
                  (update.HasBadge() == apps::mojom::OptionalBool::kTrue)
              ? apps::mojom::OptionalBool::kTrue
              : apps::mojom::OptionalBool::kFalse;
      UpdateItemNotificationBadge(update.AppId(), has_badge);
    });
  }
}

}  // namespace ash
