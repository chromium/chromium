// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_
#define ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_color_provider_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/home_launcher_animation_info.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/ash_export.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell_observer.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/splitview/split_view_observer.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/sync/model/string_ordinal.h"
#include "ui/aura/window_observer.h"
#include "ui/display/types/display_constants.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace ui {
class MouseWheelEvent;
}  // namespace ui

namespace ash {

class AppListControllerObserver;

// Ash's AppListController owns the AppListModel and implements interface
// functions that allow Chrome to modify and observe the Shelf and AppListModel
// state. It also controls the "home launcher", the tablet mode app list.
class ASH_EXPORT AppListControllerImpl
    : public AppListController,
      public SessionObserver,
      public AppListModelObserver,
      public AppListViewDelegate,
      public ShellObserver,
      public OverviewObserver,
      public SplitViewObserver,
      public TabletModeObserver,
      public KeyboardControllerObserver,
      public WallpaperControllerObserver,
      public AssistantStateObserver,
      public WindowTreeHostManager::Observer,
      public aura::WindowObserver,
      public AssistantControllerObserver,
      public AssistantUiModelObserver,
      public apps::AppRegistryCache::Observer {
 public:
  AppListControllerImpl();
  ~AppListControllerImpl() override;

  enum HomeLauncherTransitionState {
    kFinished,      // No drag or animation is in progress
    kMostlyShown,   // The home launcher occupies more than half of the screen
    kMostlyHidden,  // The home launcher occupies less than half of the screen
  };

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  AppListPresenterImpl* presenter() { return &presenter_; }

  // AppListController:
  void SetClient(AppListClient* client) override;
  AppListClient* GetClient() override;
  void AddObserver(AppListControllerObserver* observer) override;
  void RemoveObserver(AppListControllerObserver* obsever) override;
  void AddItem(std::unique_ptr<AppListItemMetadata> app_item) override;
  void AddItemToFolder(std::unique_ptr<AppListItemMetadata> app_item,
                       const std::string& folder_id) override;
  void RemoveItem(const std::string& id) override;
  void RemoveUninstalledItem(const std::string& id) override;
  void MoveItemToFolder(const std::string& id,
                        const std::string& folder_id) override;
  void SetStatus(AppListModelStatus status) override;
  void SetSearchEngineIsGoogle(bool is_google) override;
  void UpdateSearchBox(const std::u16string& text,
                       bool initiated_by_user) override;
  void PublishSearchResults(
      std::vector<std::unique_ptr<SearchResultMetadata>> results) override;
  void SetItemMetadata(const std::string& id,
                       std::unique_ptr<AppListItemMetadata> data) override;
  void SetItemIcon(const std::string& id, const gfx::ImageSkia& icon) override;
  void SetItemNotificationBadgeColor(const std::string& id,
                                     const SkColor color) override;
  void SetModelData(int profile_id,
                    std::vector<std::unique_ptr<AppListItemMetadata>> apps,
                    bool is_search_engine_google) override;

  void SetSearchResultMetadata(
      std::unique_ptr<SearchResultMetadata> metadata) override;

  void GetIdToAppListIndexMap(GetIdToAppListIndexMapCallback callback) override;
  void FindOrCreateOemFolder(
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preferred_oem_position,
      FindOrCreateOemFolderCallback callback) override;
  void ResolveOemFolderPosition(
      const syncer::StringOrdinal& preferred_oem_position,
      ResolveOemFolderPositionCallback callback) override;
  void NotifyProcessSyncChangesFinished() override;
  void DismissAppList() override;
  void GetAppInfoDialogBounds(GetAppInfoDialogBoundsCallback callback) override;
  void ShowAppList() override;
  aura::Window* GetWindow() override;
  bool IsVisible(const base::Optional<int64_t>& display_id) override;

  // AppListModelObserver:
  void OnAppListItemAdded(AppListItem* item) override;
  void OnAppListItemWillBeDeleted(AppListItem* item) override;
  void OnAppListItemUpdated(AppListItem* item) override;
  void OnAppListStateChanged(AppListState new_state,
                             AppListState old_state) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // Methods used in ash:
  bool GetTargetVisibility(const base::Optional<int64_t>& display_id) const;
  void Show(int64_t display_id,
            base::Optional<AppListShowSource> show_source,
            base::TimeTicks event_time_stamp);
  void UpdateYPositionAndOpacity(int y_position_in_screen,
                                 float background_opacity);
  void EndDragFromShelf(AppListViewState app_list_state);
  void ProcessMouseWheelEvent(const ui::MouseWheelEvent& event,
                              bool from_touchpad = false);

  // In tablet mode, takes the user to the home screen, either by ending
  // Overview Mode/Split View Mode or by minimizing the other windows. Returns
  // false if there was nothing to do because the given display was already
  // "home". Illegal to call in clamshell mode.
  bool GoHome(int64_t display_id);

  // Toggles app list visibility. In tablet mode, this can only show the app
  // list (by hiding any windows that might be shown over the homde launcher).
  // |display_id| is the id of display where app list should toggle.
  // |show_source| is the source of the event. |event_time_stamp| records the
  // event timestamp.
  ShelfAction ToggleAppList(int64_t display_id,
                            AppListShowSource show_source,
                            base::TimeTicks event_time_stamp);
  AppListViewState GetAppListViewState();
  // Returns whether the home launcher should be visible.
  bool ShouldHomeLauncherBeVisible() const;

  // AppListViewDelegate:
  AppListModel* GetModel() override;
  SearchModel* GetSearchModel() override;
  AppListNotifier* GetNotifier() override;
  void StartAssistant() override;
  void StartSearch(const std::u16string& raw_query) override;
  void OpenSearchResult(const std::string& result_id,
                        int event_flags,
                        AppListLaunchedFrom launched_from,
                        AppListLaunchType launch_type,
                        int suggestion_index,
                        bool launch_as_default) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index) override;
  using GetContextMenuModelCallback =
      AppListViewDelegate::GetContextMenuModelCallback;
  void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) override;
  void ViewShown(int64_t display_id) override;
  bool AppListTargetVisibility() const override;
  void ViewClosing() override;
  void ViewClosed() override {}
  const std::vector<SkColor>& GetWallpaperProminentColors() override;
  void ActivateItem(const std::string& id,
                    int event_flags,
                    AppListLaunchedFrom launched_from) override;
  void GetContextMenuModel(const std::string& id,
                           GetContextMenuModelCallback callback) override;
  ui::ImplicitAnimationObserver* GetAnimationObserver(
      AppListViewState target_state) override;
  void ShowWallpaperContextMenu(const gfx::Point& onscreen_location,
                                ui::MenuSourceType source_type) override;
  bool KeyboardTraversalEngaged() override;
  bool CanProcessEventsOnApplistViews() override;
  bool ShouldDismissImmediately() override;
  int GetTargetYForAppListHide(aura::Window* root_window) override;
  AssistantViewDelegate* GetAssistantViewDelegate() override;
  void OnSearchResultVisibilityChanged(const std::string& id,
                                       bool visibility) override;
  void NotifySearchResultsForLogging(
      const std::u16string& raw_query,
      const SearchResultIdWithPositionIndices& results,
      int position_index) override;
  void MaybeIncreaseSuggestedContentInfoShownCount() override;
  bool IsAssistantAllowedAndEnabled() const override;
  bool ShouldShowSuggestedContentInfo() const override;
  void MarkSuggestedContentInfoDismissed() override;
  void OnStateTransitionAnimationCompleted(AppListViewState state) override;
  void OnViewStateChanged(AppListViewState state) override;

  void GetAppLaunchedMetricParams(
      AppLaunchedMetricParams* metric_params) override;
  gfx::Rect SnapBoundsToDisplayEdge(const gfx::Rect& bounds) override;
  int GetShelfSize() override;
  bool IsInTabletMode() override;
  AppListColorProviderImpl* GetColorProvider();

  // Notifies observers of AppList visibility changes.
  void OnVisibilityChanged(bool visible, int64_t display_id);
  void OnVisibilityWillChange(bool visible, int64_t display_id);

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;
  void OnShellDestroying() override;

  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeStartingAnimationComplete(bool canceled) override;
  void OnOverviewModeEnding(OverviewSession* session) override;
  void OnOverviewModeEnded() override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // SplitViewObserver:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // KeyboardControllerObserver:
  void OnKeyboardVisibilityChanged(bool is_visible) override;

  // WallpaperControllerObserver:
  void OnWallpaperColorsChanged() override;
  void OnWallpaperPreviewStarted() override;
  void OnWallpaperPreviewEnded() override;

  // AssistantStateObserver:
  void OnAssistantStatusChanged(
      chromeos::assistant::AssistantStatus status) override;
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantFeatureAllowedChanged(
      chromeos::assistant::AssistantAllowedState state) override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowDestroyed(aura::Window* window) override;

  // AssistantControllerObserver:
  void OnAssistantReady() override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // Gets the home screen window, if available, or null if the home screen
  // window is being hidden for effects (e.g. when dragging windows or
  // previewing the wallpaper).
  aura::Window* GetHomeScreenWindow() const;

  // Scales the home launcher view maintaining the view center point, and
  // updates its opacity. If |callback| is non-null, the update should be
  // animated, and the |callback| should be called with the animation settings.
  // |animation_info| - Information about the transition trigger that will be
  // used to report animation metrics. Should be set only if |callback| is
  // not null (otherwise the transition will not be animated).
  using UpdateAnimationSettingsCallback =
      base::RepeatingCallback<void(ui::ScopedLayerAnimationSettings* settings)>;
  void UpdateScaleAndOpacityForHomeLauncher(
      float scale,
      float opacity,
      base::Optional<HomeLauncherAnimationInfo> animation_info,
      UpdateAnimationSettingsCallback callback);

  // Disables background blur in home screen UI while the returned
  // ScopedClosureRunner is in scope.
  base::ScopedClosureRunner DisableHomeScreenBackgroundBlur();

  // Called when the HomeLauncher positional animation has completed.
  void OnHomeLauncherAnimationComplete(bool shown, int64_t display_id);

  // Called when the HomeLauncher has changed its position on the screen,
  // during either an animation or a drag.
  void OnHomeLauncherPositionChanged(int percent_shown, int64_t display_id);

  // True if home screen is visible.
  bool IsHomeScreenVisible();

  // Returns bounds rect in screen coordinates for the app list item associated
  // with the provided window in the apps grid shown in the home screen,
  // assuming the initial app list grid page is selected.
  // If the window is not associated with an app, or the app item is not shown
  // in the initial home screen page, it returns 1x1 rectangle centered in the
  // home screen's apps grid.
  // If the home screen is not yet shown, returns an empty rect.
  gfx::Rect GetInitialAppListItemScreenBoundsForWindow(aura::Window* window);

  // Called when a window starts/ends dragging. If the home screen is shown, we
  // should hide it during dragging a window and reshow it when the drag ends.
  void OnWindowDragStarted();

  // If |animate| is true, scale-in-to-show home screen if home screen should
  // be shown after drag ends.
  void OnWindowDragEnded(bool animate);

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  bool onscreen_keyboard_shown() const { return onscreen_keyboard_shown_; }

  // Performs the 'back' action for the active page.
  void Back();

  void SetKeyboardTraversalMode(bool engaged);

  // Returns current visibility of the Assistant page.
  bool IsShowingEmbeddedAssistantUI() const;

  // Get updated app list view state after dragging from shelf.
  AppListViewState CalculateStateAfterShelfDrag(
      const ui::LocatedEvent& event_in_screen,
      float launcher_above_shelf_bottom_amount) const;

  void SetAppListModelForTest(std::unique_ptr<AppListModel> model);

  using StateTransitionAnimationCallback =
      base::RepeatingCallback<void(AppListViewState)>;

  void SetStateTransitionAnimationCallbackForTesting(
      StateTransitionAnimationCallback callback);

  using HomeLauncherAnimationCallback =
      base::RepeatingCallback<void(bool shown)>;
  void SetHomeLauncherAnimationCallbackForTesting(
      HomeLauncherAnimationCallback callback);

  void RecordShelfAppLaunched();

  // Updates which container the launcher window should be in.
  void UpdateLauncherContainer(
      base::Optional<int64_t> display_id = base::nullopt);

  // Gets the container which should contain the AppList.
  int GetContainerId() const;

  // Returns whether the launcher should show behinds apps or infront of them.
  bool ShouldLauncherShowBehindApps() const;

  // Returns the parent window of the applist for a |display_id|.
  aura::Window* GetContainerForDisplayId(
      base::Optional<int64_t> display_id = base::nullopt);

  // Methods for recording the state of the app list before it changes in order
  // to record metrics.
  void RecordAppListState();

 private:
  syncer::StringOrdinal GetOemFolderPos();
  std::unique_ptr<AppListItem> CreateAppListItem(
      std::unique_ptr<AppListItemMetadata> metadata);
  AppListFolderItem* FindFolderItem(const std::string& folder_id);

  // Update the visibility of Assistant functionality.
  void UpdateAssistantVisibility();

  int64_t GetDisplayIdToShowAppListOn();

  void ResetHomeLauncherIfShown();

  void ShowHomeScreen();

  // Updates the visibility of the home screen based on e.g. if the device is
  // in overview mode.
  void UpdateHomeScreenVisibility();

  // Returns true if home screen should be shown based on the current
  // configuration.
  bool ShouldShowHomeScreen() const;

  // Updates home launcher scale and opacity when the overview mode state
  // changes. `show_home_launcher` - whether the home launcher should be shown.
  // `animate` - whether the transition should be animated.
  void UpdateForOverviewModeChange(bool show_home_launcher, bool animate);

  // Returns the length of the most recent query.
  int GetLastQueryLength();

  // Shuts down the AppListControllerImpl, removing itself as an observer.
  void Shutdown();

  // Record the app launch for AppListAppLaunchedV2 metric.
  void RecordAppLaunched(AppListLaunchedFrom launched_from);

  // Updates the window that is tracked as |tracked_app_window_|.
  void UpdateTrackedAppWindow();

  // Updates whether a notification badge is shown for the AppListItemView
  // corresponding with the |app_id|.
  void UpdateItemNotificationBadge(const std::string& app_id,
                                   apps::mojom::OptionalBool has_badge);

  // Checks the notification badging pref and then updates whether a
  // notification badge is shown for each AppListItem.
  void UpdateAppNotificationBadging();

  // Responsible for starting or stopping |smoothness_tracker_|.
  void StartTrackingAnimationSmoothness(int64_t display_id);
  void RecordAnimationSmoothness();

  // Called when all the window minimize animations triggered by a tablet mode
  // "Go Home" have ended. |display_id| is the home screen display ID.
  void OnGoHomeWindowAnimationsEnded(int64_t display_id);

  // Whether the home launcher is
  // * being shown (either through an animation or a drag)
  // * being hidden (either through an animation or a drag)
  // * not animating nor being dragged.
  // In the case where the home launcher is being dragged, the gesture can
  // reverse direction at any point during the drag, in which case the only
  // information given by "showing" versus "hiding" is the starting point of
  // the drag and the assumed final state (which won't be accurate if the
  // gesture is reversed).
  HomeLauncherTransitionState home_launcher_transition_state_ = kFinished;

  AppListClient* client_ = nullptr;

  std::unique_ptr<AppListModel> model_;
  SearchModel search_model_;

  // Used to fetch colors from AshColorProvider. Should be destructed after
  // |presenter_| and UI.
  AppListColorProviderImpl color_provider_;

  // |presenter_| should be put below |client_| and |model_| to prevent a crash
  // in destruction.
  AppListPresenterImpl presenter_;

  // True if the on-screen keyboard is shown.
  bool onscreen_keyboard_shown_ = false;

  // True if the most recent event handled by |presenter_| was a key event.
  bool keyboard_traversal_engaged_ = false;

  // True if Shutdown() has been called.
  bool is_shutdown_ = false;

  // Whether to immediately dismiss the AppListView.
  bool should_dismiss_immediately_ = false;

  // The last target visibility change and its display id.
  bool last_target_visible_ = false;
  int64_t last_target_visible_display_id_ = display::kInvalidDisplayId;

  // The last visibility change and its display id.
  bool last_visible_ = false;
  int64_t last_visible_display_id_ = display::kInvalidDisplayId;

  // Used in mojo callings to specify the profile whose app list data is
  // read/written by Ash side through IPC. Notice that in multi-profile mode,
  // each profile has its own AppListModelUpdater to manipulate app list items.
  int profile_id_ = kAppListInvalidProfileID;

  // Used when tablet mode is active to track the MRU window among the windows
  // that were obscuring the home launcher when the home launcher visibility was
  // last calculated.
  // This window changing it's visibility to false is used as a signal that the
  // home launcher visibility should be recalculated.
  aura::Window* tracked_app_window_ = nullptr;

  // A callback that can be registered by a test to wait for the app list state
  // transition animation to finish.
  StateTransitionAnimationCallback state_transition_animation_callback_;

  // A callback that can be registered by a test to wait for the home launcher
  // visibility animation to finish. Should only be used in tablet mode.
  HomeLauncherAnimationCallback home_launcher_animation_callback_;

  // The AppListViewState at the moment it was recorded, used to record app
  // launching metrics. This allows an accurate AppListViewState to be recorded
  // before AppListViewState changes.
  base::Optional<AppListViewState> recorded_app_list_view_state_;

  // Whether the applist was shown at the moment it was recorded, used to record
  // app launching metrics. This is recorded because AppList visibility can
  // change before the metric is recorded.
  base::Optional<bool> recorded_app_list_visibility_;

  // ScopedClosureRunner which while in scope keeps background blur in home
  // screen (in particular, apps container suggestion chips background)
  // disabled. Set while home screen transitions are in progress.
  base::Optional<base::ScopedClosureRunner> home_screen_blur_disabler_;

  base::ObserverList<AppListControllerObserver> observers_;

  // Observed to update notification badging on app list items. Also used to get
  // initial notification badge information when app list items are added.
  apps::AppRegistryCache* cache_ = nullptr;

  // Whether the notification indicator flag is enabled.
  const bool is_notification_indicator_enabled_;

  // Observes user profile prefs for the app list.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Whether the pref for notification badging is enabled.
  base::Optional<bool> notification_badging_pref_enabled_;

  // Whether the wallpaper is being previewed. The home screen should be hidden
  // during wallpaper preview.
  bool in_wallpaper_preview_ = false;

  // Whether we're currently in a window dragging process.
  bool in_window_dragging_ = false;

  // The last overview mode exit type - cached when the overview exit starts, so
  // it can be used to decide how to update home screen when overview mode exit
  // animations are finished (at which point this information will not be
  // available).
  base::Optional<OverviewEnterExitType> overview_exit_type_;

  // Responsible for recording smoothness related UMA stats for home screen
  // animations.
  base::Optional<ui::ThroughputTracker> smoothness_tracker_;

  base::ScopedObservation<SplitViewController, SplitViewObserver>
      split_view_observation_{this};

  base::WeakPtrFactory<AppListControllerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppListControllerImpl);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_CONTROLLER_IMPL_H_
