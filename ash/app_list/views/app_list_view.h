// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_VIEW_H_

#include <memory>
#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_view_state.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura {
class Window;
}

namespace display {
class Display;
}

namespace ui {
class AnimationMetricsReporter;
}  // namespace ui

namespace app_list {
class AppsContainerView;
class ApplicationDragAndDropHost;
class AppListBackgroundShieldView;
class AppListMainView;
class AppListModel;
class AppsGridView;
class HideViewAnimationObserver;
class PaginationModel;
class SearchBoxView;
class SearchModel;
class TransitionAnimationObserver;

// AppListView is the top-level view and controller of app list UI. It creates
// and hosts a AppsGridView and passes AppListModel to it for display.
// TODO(newcomer|weidongg): Organize the cc file to match the order of
// definitions in this header.
class APP_LIST_EXPORT AppListView : public views::WidgetDelegateView {
 public:
  class TestApi {
   public:
    explicit TestApi(AppListView* view);
    ~TestApi();

    AppsGridView* GetRootAppsGridView();

   private:
    AppListView* const view_;
    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // Number of the size of shelf. Used to determine the opacity of items in the
  // app list during dragging.
  static constexpr float kNumOfShelfSize = 2.0;

  // The opacity of the app list background.
  static constexpr float kAppListOpacity = 0.95;

  // The opacity of the app list background with blur.
  static constexpr float kAppListOpacityWithBlur = 0.7;

  // The preferred blend alpha with wallpaper color for background.
  static constexpr int kAppListColorDarkenAlpha = 178;

  // The defualt color of the app list background.
  static constexpr SkColor kDefaultBackgroundColor = SK_ColorBLACK;

  // The duration the AppListView ignores scroll events which could transition
  // its state.
  static constexpr int kScrollIgnoreTimeMs = 500;

  struct InitParams {
    gfx::NativeView parent = nullptr;
    int initial_apps_page = 0;
    bool is_tablet_mode = false;
    // Whether the shelf alignment is on the side of the display.
    bool is_side_shelf = false;
  };

  // Does not take ownership of |delegate|.
  explicit AppListView(AppListViewDelegate* delegate);
  ~AppListView() override;

  // Prevents handling input events for the |window| in context of handling in
  // app list.
  static void ExcludeWindowFromEventHandling(aura::Window* window);

  static void SetShortAnimationForTesting(bool enabled);
  static bool ShortAnimationsForTesting();

  // Initializes the widget as a bubble or fullscreen view depending on if the
  // fullscreen app list feature is set.
  void Initialize(const InitParams& params);

  // If |drag_and_drop_host| is not NULL it will be called upon drag and drop
  // operations outside the application list. This has to be called after
  // Initialize was called since the app list object needs to exist so that
  // it can set the host.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Shows the UI when there are no pending icon loads. Otherwise, starts a
  // timer to show the UI when a maximum allowed wait time has expired.
  void ShowWhenReady();

  // Dismisses the UI, cleans up and sets the state to CLOSED.
  void Dismiss();

  // Closes opened folder or search result page if they are opened. Returns
  // whether the action was handled.
  bool CloseOpenedPage();

  // Performs the 'back' action for the active page.
  void Back();

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;
  bool CanProcessEventsWithinSubtree() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void Layout() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // WidgetDelegate:
  ax::mojom::Role GetAccessibleWindowRole() const override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Called when tablet mode starts and ends.
  void OnTabletModeChanged(bool started);

  // Called when the wallpaper colors change.
  void OnWallpaperColorsChanged();

  // Handles scroll events from various sources.
  bool HandleScroll(int offset, ui::EventType type);

  // Changes the app list state.
  void SetState(AppListViewState new_state);

  // Starts the close animation.
  void StartCloseAnimation(base::TimeDelta animation_duration);

  // Changes the app list state depending on the current |app_list_state_| and
  // whether the search box is empty.
  void SetStateFromSearchBoxView(bool search_box_is_empty,
                                 bool triggered_by_contents_change);

  // Updates y position and opacity of app list during dragging.
  void UpdateYPositionAndOpacity(int y_position_in_screen,
                                 float background_opacity);

  // Offsets the y position of the app list (above the screen)
  void OffsetYPositionOfAppList(int offset);

  // Update Y position and opacity of this view's child views during dragging.
  void UpdateChildViewsYPositionAndOpacity();

  // The search box cannot actively listen to all key events. To control and
  // input into the search box when it does not have focus, we need to redirect
  // necessary key events to the search box.
  void RedirectKeyEventToSearchBox(ui::KeyEvent* event);

  // Called when on-screen keyboard's visibility is changed.
  void OnScreenKeyboardShown(bool shown);

  // Called when parent window's bounds is changed.
  void OnParentWindowBoundsChanged();

  // If the on-screen keyboard is shown, hide it. Return whether keyboard was
  // hidden
  bool CloseKeyboardIfVisible();

  // Sets |is_in_drag_| and updates the visibility of app list items.
  void SetIsInDrag(bool is_in_drag);

  // Gets the PaginationModel owned by this view's apps grid.
  PaginationModel* GetAppsPaginationModel();

  // Gets the content bounds of the app info dialog of the app list in the
  // screen coordinates.
  gfx::Rect GetAppInfoDialogBounds() const;

  // Gets current screen bottom.
  int GetScreenBottom() const;

  // Returns current app list height above display bottom.
  int GetCurrentAppListHeight() const;

  // The progress of app list height transitioning from closed to fullscreen
  // state. [0.0, 1.0] means the progress between closed and peeking state,
  // while [1.0, 2.0] means the progress between peeking and fullscreen state.
  float GetAppListTransitionProgress() const;

  // Returns the height of app list in fullscreen state.
  int GetFullscreenStateHeight() const;

  views::Widget* get_fullscreen_widget_for_test() const {
    return fullscreen_widget_;
  }

  gfx::NativeView parent_window() const { return parent_window_; }

  AppListViewState app_list_state() const { return app_list_state_; }

  views::Widget* search_box_widget() const { return search_box_widget_; }

  SearchBoxView* search_box_view() const { return search_box_view_; }

  AppListMainView* app_list_main_view() const { return app_list_main_view_; }

  bool is_fullscreen() const {
    return app_list_state_ == AppListViewState::FULLSCREEN_ALL_APPS ||
           app_list_state_ == AppListViewState::FULLSCREEN_SEARCH;
  }

  bool is_tablet_mode() const { return is_tablet_mode_; }

  bool is_side_shelf() const { return is_side_shelf_; }

  bool is_in_drag() const { return is_in_drag_; }

  bool drag_started_from_peeking() const { return drag_started_from_peeking_; }

  void set_onscreen_keyboard_shown(bool onscreen_keyboard_shown) {
    onscreen_keyboard_shown_ = onscreen_keyboard_shown;
  }

  // Returns true if the home launcher is enabled in tablet mode.
  bool IsHomeLauncherEnabledInTabletMode() const;

  // Returns true if the home_launcher feature is enabled.
  bool is_home_launcher_enabled() const { return is_home_launcher_enabled_; }

  views::View* GetAppListBackgroundShieldForTest();

 private:
  // A widget observer that is responsible for keeping the AppListView state up
  // to date on closing.
  // TODO(newcomer): Merge this class into AppListView once the old app list
  // view code is removed.
  class FullscreenWidgetObserver;

  void InitContents(int initial_apps_page);

  void InitChildWidgets();

  // Initializes the widget for fullscreen mode.
  void InitializeFullscreen(gfx::NativeView parent);

  // Closes the AppListView when a click or tap event propogates to the
  // AppListView.
  void HandleClickOrTap(ui::LocatedEvent* event);

  // Initializes |initial_drag_point_|.
  void StartDrag(const gfx::Point& location);

  // Updates the bounds of the widget while maintaining the relative position
  // of the top of the widget and the gesture.
  void UpdateDrag(const gfx::Point& location);

  // Handles app list state transfers. If the drag was fast enough, ignore the
  // release position and snap to the next state.
  void EndDrag(const gfx::Point& location);

  // Set child views for FULLSCREEN_ALL_APPS and PEEKING.
  void SetChildViewsForStateTransition(AppListViewState new_state);

  // Converts |state| to the fullscreen equivalent.
  void ConvertAppListStateToFullscreenEquivalent(AppListViewState* state);

  // Kicks off the proper animation for the state change. If an animation is
  // in progress it will be interrupted.
  void StartAnimationForState(AppListViewState new_state);

  // Records the state transition for UMA.
  void RecordStateTransitionForUma(AppListViewState new_state);

  // Creates an Accessibility Event if the state transition warrants one.
  void MaybeCreateAccessibilityEvent(AppListViewState new_state);

  // Gets the display nearest to the parent window.
  display::Display GetDisplayNearestView() const;

  // Gets the apps container view owned by this view.
  AppsContainerView* GetAppsContainerView();

  // Gets the root apps grid view owned by this view.
  AppsGridView* GetRootAppsGridView();

  // Gets the apps grid view within the folder view owned by this view.
  AppsGridView* GetFolderAppsGridView();

  // Gets the AppListStateTransitionSource for |app_list_state_| to
  // |target_state|. If we are not interested in recording a state transition
  // (ie. PEEKING->PEEKING) then return kMaxAppListStateTransition. If this is
  // modified, histograms will be affected.
  AppListStateTransitionSource GetAppListStateTransitionSource(
      AppListViewState target_state) const;

  // Overridden from views::WidgetDelegateView:
  views::View* GetInitiallyFocusedView() override;

  // Gets app list background opacity during dragging.
  float GetAppListBackgroundOpacityDuringDragging();

  void GetWallpaperProminentColors(
      AppListViewDelegate::GetWallpaperProminentColorsCallback callback);
  void SetBackgroundShieldColor();

  // Records the number of folders, and the number of items in folders for UMA
  // histograms.
  void RecordFolderMetrics();

  // Returns true if scroll events should be ignored.
  bool ShouldIgnoreScrollEvents();

  // Returns preferred y of fullscreen widget bounds in parent window for the
  // specified state.
  int GetPreferredWidgetYForState(AppListViewState state);

  // Returns preferred fullscreen widget bounds in parent window for the
  // specified state. Note that this function should only be called after the
  // widget is initialized.
  gfx::Rect GetPreferredWidgetBoundsForState(AppListViewState state);

  // Updates y position of |app_list_background_shield_| based on the
  // |app_list_state_| and |is_in_drag_|.
  void UpdateAppListBackgroundYPosition();

  AppListViewDelegate* delegate_;    // Weak. Owned by AppListService.
  AppListModel* const model_;        // Not Owned.
  SearchModel* const search_model_;  // Not Owned.

  AppListMainView* app_list_main_view_ = nullptr;
  views::Widget* fullscreen_widget_ = nullptr;  // Owned by AppListView.
  gfx::NativeView parent_window_ = nullptr;

  views::View* search_box_focus_host_ =
      nullptr;  // Owned by the views hierarchy.
  views::Widget* search_box_widget_ =
      nullptr;                                // Owned by the app list's widget.
  SearchBoxView* search_box_view_ = nullptr;  // Owned by |search_box_widget_|.
  // Owned by the app list's widget. Null if the fullscreen app list is not
  // enabled.
  AppListBackgroundShieldView* app_list_background_shield_ = nullptr;

  // Whether tablet mode is active.
  bool is_tablet_mode_ = false;
  // Whether the shelf is oriented on the side.
  bool is_side_shelf_ = false;

  // True if the user is in the process of gesture-dragging on opened app list,
  // or dragging the app list from shelf.
  bool is_in_drag_ = false;

  // Y position of the app list in screen space coordinate during dragging.
  int app_list_y_position_in_screen_ = 0;

  // The opacity of app list background during dragging.
  float background_opacity_ = 0.f;

  // The location of initial gesture event in screen coordinates.
  gfx::Point initial_drag_point_;

  // The rectangle of initial widget's window in screen coordinates.
  gfx::Rect initial_window_bounds_;

  // The velocity of the gesture event.
  float last_fling_velocity_ = 0;
  // Whether the background blur is enabled.
  const bool is_background_blur_enabled_;
  // The state of the app list, controlled via SetState().
  AppListViewState app_list_state_ = AppListViewState::PEEKING;

  // A widget observer that sets the AppListView state when the widget is
  // closed.
  std::unique_ptr<FullscreenWidgetObserver> widget_observer_;

  std::unique_ptr<HideViewAnimationObserver> hide_view_animation_observer_;

  std::unique_ptr<TransitionAnimationObserver> transition_animation_observer_;

  // The mask used to clip the |app_list_background_shield_|.
  std::unique_ptr<ui::LayerOwner> app_list_background_shield_mask_;

  // For UMA and testing. If non-null, triggered when the app list is painted.
  base::Closure next_paint_callback_;

  // True if the dragging started from PEEKING state.
  bool drag_started_from_peeking_ = false;

  // Accessibility announcement dialogue.
  base::string16 state_announcement_;

  // Metric reporter for state change animations.
  const std::unique_ptr<ui::AnimationMetricsReporter>
      state_animation_metrics_reporter_;

  // Whether the on-screen keyboard is shown.
  bool onscreen_keyboard_shown_ = false;

  // Whether the home launcher feature is enabled.
  const bool is_home_launcher_enabled_;

  // True if new style launcher feature is enabled.
  const bool is_new_style_launcher_enabled_;

  base::WeakPtrFactory<AppListView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_VIEW_H_
