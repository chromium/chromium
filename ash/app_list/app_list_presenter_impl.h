// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_PRESENTER_IMPL_H_
#define ASH_APP_LIST_APP_LIST_PRESENTER_IMPL_H_

#include <stdint.h>

#include <memory>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_observer.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace ui {
class ScopedLayerAnimationSettings;
}  // namespace ui

namespace ash {
class AppListControllerImpl;
class AppListPresenterEventFilter;
class AppListView;
enum class AppListViewState;

// Manages app list UI. Creates AppListView and schedules showing/hiding
// animation. While the UI is visible, it monitors things such as app list
// activation state and mouse/touch events to dismiss the UI. Updates the shelf
// launcher icon state.
class ASH_EXPORT AppListPresenterImpl
    : public aura::client::FocusChangeObserver,
      public views::WidgetObserver,
      public display::DisplayObserver,
      public ShelfObserver {
 public:
  static constexpr std::array<int, 8> kIdsOfContainersThatWontHideAppList = {
      kShellWindowId_AppListContainer,
      kShellWindowId_HelpBubbleContainer,
      kShellWindowId_HomeScreenContainer,
      kShellWindowId_MenuContainer,
      kShellWindowId_PowerMenuContainer,
      kShellWindowId_SettingBubbleContainer,
      kShellWindowId_ShelfBubbleContainer,
      kShellWindowId_ShelfContainer};

  // Callback which fills out the passed settings object. Used by
  // UpdateYPositionAndOpacityForHomeLauncher so different callers can do
  // similar animations with different settings.
  using UpdateHomeLauncherAnimationSettingsCallback =
      base::RepeatingCallback<void(ui::ScopedLayerAnimationSettings* settings)>;

  // |controller| must outlive |this|.
  explicit AppListPresenterImpl(AppListControllerImpl* controller);

  AppListPresenterImpl(const AppListPresenterImpl&) = delete;
  AppListPresenterImpl& operator=(const AppListPresenterImpl&) = delete;

  ~AppListPresenterImpl() override;

  // Returns app list window or nullptr if it is not visible.
  aura::Window* GetWindow() const;

  // Returns app list view if one exists, or nullptr otherwise.
  AppListView* GetView() { return view_; }
  const AppListView* GetView() const { return view_; }

  // Show the app list window on the display with the given id. If
  // |event_time_stamp| is not 0, it means |Show()| was triggered by one of the
  // AppListShowSources: kSearchKey, kShelfButton, or kSwipeFromShelf.
  void Show(AppListViewState preferred_state,
            int64_t display_id,
            base::TimeTicks event_time_stamp,
            std::optional<AppListShowSource> show_source);

  // Hide the open app list window. This may leave the view open but hidden.
  // If |event_time_stamp| is not 0, it means |Dismiss()| was triggered by
  // one AppListShowSource or focusing out side of the launcher.
  void Dismiss(base::TimeTicks event_time_stamp);

  // Sets the app list view visibility (without updating the app list window
  // visibility). No-op if the app list view does not exist.
  void SetViewVisibility(bool visible);

  // If app list has an opened folder, close it. Returns whether an opened
  // folder was closed.
  bool HandleCloseOpenFolder();

  // Handles `AppListController::UpdateAppListWithNewSortingOrder()` for the
  // app list presenter.
  void UpdateForNewSortingOrder(
      const std::optional<AppListSortOrder>& new_order,
      bool animate,
      base::OnceClosure update_position_closure);

  // Updates the continue section visibility based on user preference.
  void UpdateContinueSectionVisibility();

  // Returns current visibility of the app list. Deprecated, use
  // |IsAtLeastPartiallyVisible| instead.
  bool IsVisibleDeprecated() const;

  // Returns whether the app list is visible. This will only return false if
  // the app list is entirely occluded.
  bool IsAtLeastPartiallyVisible() const;

  // Returns target visibility. This may differ from IsVisible() if a visibility
  // transition is in progress.
  bool GetTargetVisibility() const;

  // Scales the home launcher view maintaining the view center point, and
  // updates its opacity. If |callback| is non-null, the update should be
  // animated, and the |callback| should be called with the animation settings.
  // |transition| - The tablet mode animation type. Used to report animation
  // metrics if the home launcher change is animated. Should be set only if
  // |callback| is non-null. If not set, the animation smoothness metrics will
  // not be reported.
  void UpdateScaleAndOpacityForHomeLauncher(
      float scale,
      float opacity,
      std::optional<TabletModeAnimationTransition> transition,
      UpdateHomeLauncherAnimationSettingsCallback callback);

  // Shows or hides the Assistant page.
  // |show| is true to show and false to hide.
  void ShowEmbeddedAssistantUI(bool show);

  // Returns current visibility of the Assistant page.
  bool IsShowingEmbeddedAssistantUI() const;

 private:
  // Sets the app list view and attempts to show it.
  void SetView(AppListView* view);

  // Forgets the view.
  void ResetView();

  // Returns the id of the display containing the app list, if visible. If not
  // visible returns kInvalidDisplayId.
  int64_t GetDisplayId() const;

  void OnVisibilityChanged(bool visible, int64_t display_id);
  void OnVisibilityWillChange(bool visible, int64_t display_id);

  // Called when the widget is hidden or destroyed.
  void OnClosed();

  // aura::client::FocusChangeObserver overrides:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // DisplayObserver overrides:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ShelfObserver overrides:
  void OnShelfShuttingDown() override;

  // Registers a callback that is run when the next frame successfully makes it
  // to the screen.
  void RequestPresentationTime(int64_t display_id,
                               base::TimeTicks event_time_stamp);

  // Snaps the app list window bounds to fit the screen size. (See
  // https://crbug.com/884889).
  void SnapAppListBoundsToDisplayEdge();

  // Called when the reorder animation completes.
  void OnAppListReorderAnimationDone();

  // Called when the tablet <-> clamshell transition animation completes.
  // Hides the `AppListView`'s window if `target_visibility == false`.
  void OnTabletToClamshellTransitionAnimationDone(bool target_visibility,
                                                  bool aborted);

  // Owns |this|.
  const raw_ptr<AppListControllerImpl> controller_;

  // Closes the app list when the user clicks outside its bounds.
  std::unique_ptr<AppListPresenterEventFilter> event_filter_;

  // An observer that notifies AppListView when the display has changed.
  display::ScopedDisplayObserver display_observer_{this};

  // An observer that notifies AppListView when the shelf state has changed.
  base::ScopedObservation<Shelf, ShelfObserver> shelf_observer_{this};

  // The target visibility of the AppListView, true if the target visibility is
  // shown.
  bool is_target_visibility_show_ = false;

  // The AppListView this class manages, owned by its widget.
  raw_ptr<AppListView> view_ = nullptr;

  // Whether the presenter is currently changing app list view state to shown.
  // TODO(https://crbug.com/1307871): Remove this when the linked crash gets
  // diagnosed.
  bool showing_app_list_ = false;

  base::WeakPtrFactory<AppListPresenterImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_PRESENTER_IMPL_H_
