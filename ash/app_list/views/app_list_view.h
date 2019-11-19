// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_VIEW_H_

#include <memory>
#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/color_palette.h"
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
class ImplicitAnimationObserver;
}  // namespace ui

namespace ash {
class AppsContainerView;
class ApplicationDragAndDropHost;
class AppListBackgroundShieldView;
class AppListConfig;
class AppListMainView;
class AppListModel;
class AppsGridView;
class BoundsAnimationObserver;
class PaginationModel;
class SearchBoxView;
class SearchModel;

FORWARD_DECLARE_TEST(AppListControllerImplTest,
                     CheckAppListViewBoundsWhenVKeyboardEnabled);
FORWARD_DECLARE_TEST(AppListControllerImplTest,
                     CheckAppListViewBoundsWhenDismissVKeyboard);
FORWARD_DECLARE_TEST(AppListControllerImplMetricsTest,
                     PresentationTimeRecordedForDragInTabletMode);

namespace {

// The fraction of app list height that the app list must be released at in
// order to transition to the next state.
constexpr int kAppListThresholdDenominator = 3;

}  // namespace

// AppListView is the top-level view and controller of app list UI. It creates
// and hosts a AppsGridView and passes AppListModel to it for display.
// TODO(newcomer|weidongg): Organize the cc file to match the order of
// definitions in this header.
class APP_LIST_EXPORT AppListView : public views::WidgetDelegateView,
                                    public aura::WindowObserver {
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
  static constexpr float kAppListOpacityWithBlur = 0.74;

  // The preferred blend alpha with wallpaper color for background.
  static constexpr int kAppListColorDarkenAlpha = 178;

  // The defualt color of the app list background.
  static constexpr SkColor kDefaultBackgroundColor = gfx::kGoogleGrey900;

  // The duration the AppListView ignores scroll events which could transition
  // its state.
  static constexpr int kScrollIgnoreTimeMs = 500;

  // The snapping threshold for dragging app list from shelf in tablet mode,
  // measured in DIPs.
  static constexpr int kDragSnapToFullscreenThreshold = 320;

  // The snapping thresholds for dragging app list from shelf in laptop mode,
  // measured in DIPs.
  static constexpr int kDragSnapToClosedThreshold = 120;
  static constexpr int kDragSnapToPeekingThreshold = 561;

  // The velocity the app list must be dragged from the shelf in order to
  // transition to the next state, measured in DIPs/event.
  static constexpr int kDragVelocityFromShelfThreshold = 120;

  // The velocity the app list must be dragged in order to transition to the
  // next state, measured in DIPs/event.
  static constexpr int kDragVelocityThreshold = 6;

  // The animation duration for app list movement.
  static constexpr int kAppListAnimationDurationMs = 200;
  static constexpr int kAppListAnimationDurationFromFullscreenMs = 250;

  // Does not take ownership of |delegate|.
  explicit AppListView(AppListViewDelegate* delegate);
  ~AppListView() override;

  // Prevents handling input events for the |window| in context of handling in
  // app list.
  static void ExcludeWindowFromEventHandling(aura::Window* window);

  static void SetShortAnimationForTesting(bool enabled);
  static bool ShortAnimationsForTesting();

  // Returns the app list transition progress value associated with a app list
  // view state. This matches the values GetAppListTransitionProgress() is
  // expected to return when app list view is exactly in the provided state.
  static float GetTransitionProgressForState(ash::AppListViewState state);

  // Initializes the view, only done once per session.
  void InitView(bool is_tablet_mode,
                gfx::NativeView parent,
                base::RepeatingClosure on_bounds_animation_ended_callback);

  // Initializes the contents of the view.
  void InitContents(bool is_tablet_mode);

  // Initializes this view's widget.
  void InitWidget(gfx::NativeView parent);

  // Initializes the SearchBox's widget.
  void InitChildWidget();

  // Sets the state of all child views to be re-shown, then shows the view.
  void Show(bool is_side_shelf, bool is_tablet_mode);

  // If |drag_and_drop_host| is not nullptr it will be called upon drag and drop
  // operations outside the application list. This has to be called after
  // Initialize was called since the app list object needs to exist so that
  // it can set the host.
  void SetDragAndDropHostOfCurrentAppList(
      ApplicationDragAndDropHost* drag_and_drop_host);

  // Dismisses the UI, cleans up and sets the state to CLOSED.
  void Dismiss();

  // Resets the child views before showing the AppListView.
  void ResetForShow();

  // Closes opened folder or search result page if they are opened.
  void CloseOpenedPage();

  // If a folder is open, close it. Returns whether an opened folder was closed.
  bool HandleCloseOpenFolder();

  // If a search box is open, close it. Returns whether an open search box was
  // closed.
  bool HandleCloseOpenSearchBox();

  // Performs the 'back' action for the active page.
  bool Back();

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void Layout() override;

  // WidgetDelegate:
  ax::mojom::Role GetAccessibleWindowRole() override;

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
  bool HandleScroll(const gfx::Vector2d& offset, ui::EventType type);

  // Changes the app list state.
  void SetState(ash::AppListViewState new_state);

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

  // Home launcher can become the focused window without being reset when all
  // open windows are closed in tablet mode. Reset the view in this case.
  void OnHomeLauncherGainingFocusWithoutAnimation();

  // Gets the PaginationModel owned by this view's apps grid.
  ash::PaginationModel* GetAppsPaginationModel();

  // Gets the content bounds of the app info dialog of the app list in the
  // screen coordinates.
  gfx::Rect GetAppInfoDialogBounds() const;

  // Gets current screen bottom.
  int GetScreenBottom() const;

  // Returns current app list height above display bottom.
  int GetCurrentAppListHeight() const;

  // Flags that can be passed to GetAppListTransitionProgress(). For more
  // details, see GetAppListTransitionProgress() documentation.
  static constexpr int kProgressFlagNone = 0;
  static constexpr int kProgressFlagSearchResults = 1;
  static constexpr int kProgressFlagWithTransform = 1 << 1;

  // The progress of app list height transitioning from closed to fullscreen
  // state. [0.0, 1.0] means the progress between closed and peeking state,
  // while [1.0, 2.0] means the progress between peeking and fullscreen state.
  //
  // By default, this calculates progress for drag operation while app list
  // is AppListState::kApps state, relative to the current app list view bounds.
  // The |flags| argument can be used to amend this behavior:
  // *   Use |kProgressFlagNone| for default behavior.
  // *   If |kProgressFlagSearchResult| flag is set, the progress will be
  //     calculated using kHalf state height as baseline. This should be used
  //     when calculating contents layout for search results state.
  // *   If |kProgressFlagWithTransform| is set, the progress will be calculated
  //     for the app list height offset by the current app list view transform.
  //     This should be used when setting up transform animations for views
  //     whose bounds depend on the app list height - in particular when the
  //     animation is implemented by setting up target bounds first, and then
  //     animating view layer transform from one that matches current bounds to
  //     an identity transform. This flag is needed to properly calculate the
  //     initial animation transform.
  float GetAppListTransitionProgress(int flags) const;

  // Returns the height of app list in fullscreen state.
  int GetFullscreenStateHeight() const;

  // Calculates and returns the app list view state after dragging from shelf
  // ends.
  ash::AppListViewState CalculateStateAfterShelfDrag(
      const ui::LocatedEvent& event_in_screen,
      float launcher_above_shelf_bottom_amount) const;

  // Returns a animation metrics reportre for state transition.
  ui::AnimationMetricsReporter* GetStateTransitionMetricsReporter();

  // Called when drag in tablet mode starts/proceeds/ends.
  void OnHomeLauncherDragStart();
  void OnHomeLauncherDragInProgress();
  void OnHomeLauncherDragEnd();

  // Resets the animation metrics reporter for state transition.
  void ResetTransitionMetricsReporter();

  // WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // Called when state transition animation is completed.
  void OnStateTransitionAnimationCompleted();

  void OnTabletModeAnimationTransitionNotified(
      TabletModeAnimationTransition animation_transition);

  // Called at the end of dragging AppList from Shelf.
  void EndDragFromShelf(ash::AppListViewState app_list_state);

  // Moves the AppListView off screen and calls a layout if needed.
  void OnBoundsAnimationCompleted();

  // Returns the expected tile bounds in screen coordinates the provided app
  // grid item ID , if the item is in the first apps grid page. Otherwise, it
  // returns 1x1 rectangle in the apps grid center.
  gfx::Rect GetItemScreenBoundsInFirstGridPage(const std::string& id) const;

  gfx::NativeView parent_window() const { return parent_window_; }

  ash::AppListViewState app_list_state() const { return app_list_state_; }

  SearchBoxView* search_box_view() const { return search_box_view_; }

  AppListMainView* app_list_main_view() const { return app_list_main_view_; }

  views::View* announcement_view() const { return announcement_view_; }

  bool is_fullscreen() const {
    return app_list_state_ == ash::AppListViewState::kFullscreenAllApps ||
           app_list_state_ == ash::AppListViewState::kFullscreenSearch;
  }

  bool is_tablet_mode() const { return is_tablet_mode_; }

  bool is_side_shelf() const { return is_side_shelf_; }

  void SetShelfHasRoundedCorners(bool shelf_has_rounded_corners);

  bool shelf_has_rounded_corners() const { return shelf_has_rounded_corners_; }

  bool is_in_drag() const { return is_in_drag_; }

  void set_onscreen_keyboard_shown(bool onscreen_keyboard_shown) {
    onscreen_keyboard_shown_ = onscreen_keyboard_shown;
  }

  views::View* GetAppListBackgroundShieldForTest();

  // Gets the current app list configuration. Should not be used before the app
  // list content has been initialized.
  const AppListConfig& GetAppListConfig() const;

  SkColor GetAppListBackgroundShieldColorForTest();

 private:
  FRIEND_TEST_ALL_PREFIXES(ash::AppListControllerImplTest,
                           CheckAppListViewBoundsWhenVKeyboardEnabled);
  FRIEND_TEST_ALL_PREFIXES(ash::AppListControllerImplTest,
                           CheckAppListViewBoundsWhenDismissVKeyboard);
  FRIEND_TEST_ALL_PREFIXES(ash::AppListControllerImplMetricsTest,
                           PresentationTimeRecordedForDragInTabletMode);

  class StateAnimationMetricsReporter;

  // Updates the app list configuration that should be used by this app list
  // view.
  // |parent_window|: The window that contains the app list widget.
  void UpdateAppListConfig(aura::Window* parent_window);

  // Updates the widget to be shown.
  void UpdateWidget();

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

  // Set child views for |target_state|.
  void SetChildViewsForStateTransition(ash::AppListViewState target_state);

  // Converts |state| to the fullscreen equivalent.
  void ConvertAppListStateToFullscreenEquivalent(ash::AppListViewState* state);

  // Gets the animation duration that transition to |taget_state| should have.
  base::TimeDelta GetStateTransitionAnimationDuration(
      ash::AppListViewState target_state);

  // Kicks off the proper animation for the state change. If an animation is
  // in progress it will be interrupted.
  void StartAnimationForState(ash::AppListViewState new_state);

  void MaybeIncreaseAssistantPrivacyInfoRowShownCount(
      ash::AppListViewState new_state);

  // Applies a bounds animation on this views layer.
  void ApplyBoundsAnimation(ash::AppListViewState target_state,
                            base::TimeDelta duration_ms);

  // Records the state transition for UMA.
  void RecordStateTransitionForUma(ash::AppListViewState new_state);

  // Creates an Accessibility Event if the state transition warrants one.
  void MaybeCreateAccessibilityEvent(ash::AppListViewState new_state);

  // Ensures that the app list widget bounds are set to the preferred bounds for
  // the current app list view state - intended to be called when the
  // display bounds available to the app list view change.
  void EnsureWidgetBoundsMatchCurrentState();

  // Returns the remaining vertical distance for the bounds movement
  // animation.
  int GetRemainingBoundsAnimationDistance() const;

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
      ash::AppListViewState target_state) const;

  // Overridden from views::WidgetDelegateView:
  views::View* GetInitiallyFocusedView() override;

  // Gets app list background opacity during dragging.
  float GetAppListBackgroundOpacityDuringDragging();

  const std::vector<SkColor>& GetWallpaperProminentColors();
  void SetBackgroundShieldColor();

  // Records the number of folders, and the number of items in folders for UMA
  // histograms.
  void RecordFolderMetrics();

  // Returns true if scroll events should be ignored.
  bool ShouldIgnoreScrollEvents();

  // Returns true if the Embedded Assistant UI is currently being shown.
  bool IsShowingEmbeddedAssistantUI() const;

  // Returns preferred y of fullscreen widget bounds in parent window for the
  // specified state.
  int GetPreferredWidgetYForState(ash::AppListViewState state) const;

  // Returns preferred fullscreen widget bounds in parent window for the
  // specified state. Note that this function should only be called after the
  // widget is initialized.
  gfx::Rect GetPreferredWidgetBoundsForState(ash::AppListViewState state);

  // Updates y position of |app_list_background_shield_| based on the
  // |state| and |is_in_drag_|.
  void UpdateAppListBackgroundYPosition(ash::AppListViewState state);

  AppListViewDelegate* delegate_;    // Weak. Owned by AppListService.
  AppListModel* const model_;        // Not Owned.
  SearchModel* const search_model_;  // Not Owned.

  AppListMainView* app_list_main_view_ = nullptr;
  gfx::NativeView parent_window_ = nullptr;

  views::Widget* search_box_widget_ =
      nullptr;                                // Owned by the app list's widget.
  SearchBoxView* search_box_view_ = nullptr;  // Owned by |search_box_widget_|.
  // Owned by the app list's widget. Used to show the darkened AppList
  // background.
  AppListBackgroundShieldView* app_list_background_shield_ = nullptr;

  // The time the AppListView was requested to be shown. Used for metrics.
  base::Optional<base::Time> time_shown_;

  // Whether tablet mode is active.
  bool is_tablet_mode_ = false;
  // Whether the shelf is oriented on the side.
  bool is_side_shelf_ = false;

  // Whether the shelf has rounded corners.
  bool shelf_has_rounded_corners_ = false;

  // True if the user is in the process of gesture-dragging on opened app list,
  // or dragging the app list from shelf.
  bool is_in_drag_ = false;

  // Whether the view is being built.
  bool is_building_ = false;

  // Y position of the app list in screen space coordinate during dragging.
  int app_list_y_position_in_screen_ = 0;

  // The opacity of app list background during dragging. This ensures a gradual
  // opacity shift from the shelf opacity while dragging to show the AppListView
  // from the shelf.
  float background_opacity_in_drag_ = 0.f;

  // The location of initial gesture event in screen coordinates.
  gfx::Point initial_drag_point_;

  // The rectangle of initial widget's window in screen coordinates.
  gfx::Rect initial_window_bounds_;

  // The location of the initial mouse event in view coordinates.
  gfx::Point initial_mouse_drag_point_;

  // The velocity of the gesture event.
  float last_fling_velocity_ = 0;
  // Whether the background blur is enabled.
  const bool is_background_blur_enabled_;
  // The state of the app list, controlled via SetState().
  ash::AppListViewState app_list_state_ = ash::AppListViewState::kClosed;

  // The timestamp when the ongoing animation ends.
  base::TimeTicks animation_end_timestamp_;

  // An observer to notify AppListView of bounds animation completion.
  std::unique_ptr<BoundsAnimationObserver> bounds_animation_observer_;

  // For UMA and testing. If non-null, triggered when the app list is painted.
  base::Closure next_paint_callback_;

  // Metric reporter for state change animations.
  const std::unique_ptr<StateAnimationMetricsReporter>
      state_animation_metrics_reporter_;

  // Whether the on-screen keyboard is shown.
  bool onscreen_keyboard_shown_ = false;

  // Whether the app list has been translated up to ensure app list folder
  // view header is visible when onscreen keyboard is shown.
  bool offset_to_show_folder_with_onscreen_keyboard_ = false;

  // View used to announce:
  // 1. state transition for peeking and fullscreen
  // 2. folder opening and closing.
  // 3. app dragging in AppsGridView.
  views::View* announcement_view_ = nullptr;  // Owned by AppListView.

  // Records the presentation time for app launcher dragging.
  std::unique_ptr<ash::PresentationTimeRecorder> presentation_time_recorder_;

  // If set, the app list config that should be used within the app list view
  // instead of the default instance.
  std::unique_ptr<AppListConfig> app_list_config_;

  // Callback which is run when the bounds animation of the widget is ended.
  base::RepeatingClosure on_bounds_animation_ended_callback_;

  base::WeakPtrFactory<AppListView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppListView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_VIEW_H_
