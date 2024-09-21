// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_VIEW_H_

#include <memory>
#include <optional>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/metrics_util.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura {
class Window;
}

namespace display {
class Display;
}

namespace ui {
class ImplicitAnimationObserver;
}  // namespace ui

namespace ash {
class AppListA11yAnnouncer;
class AppsContainerView;
class AppListMainView;
class AppsGridView;
class PagedAppsGridView;
class PaginationModel;
class SearchBoxView;

FORWARD_DECLARE_TEST(AppListControllerImplTest,
                     CheckAppListViewBoundsWhenDismissVKeyboard);

// AppListView is the top-level view and controller of app list UI. It creates
// and hosts a AppsGridView and passes AppListModel to it for display.
// TODO(newcomer|weidongg): Organize the cc file to match the order of
// definitions in this header.
class ASH_EXPORT AppListView : public views::WidgetDelegateView,
                               public aura::WindowObserver {
  METADATA_HEADER(AppListView, views::WidgetDelegateView)
 public:
  class TestApi {
   public:
    explicit TestApi(AppListView* view);

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    ~TestApi();

    PagedAppsGridView* GetRootAppsGridView();

   private:
    const raw_ptr<AppListView> view_;
  };

  class ASH_EXPORT ScopedAccessibilityAnnouncementLock {
   public:
    explicit ScopedAccessibilityAnnouncementLock(AppListView* view)
        : view_(view) {
      ++view_->accessibility_event_disablers_;
    }

    ~ScopedAccessibilityAnnouncementLock() {
      --view_->accessibility_event_disablers_;
    }

   private:
    const raw_ptr<AppListView> view_;
  };

  // Used to prevent the app list contents from being reset when the app list
  // shows. Only one instance can exist at a time. This class is useful when:
  // (1) the app list close animation is reversed, and
  // (2) the contents before the close animation starts should be kept.
  class ScopedContentsResetDisabler {
   public:
    explicit ScopedContentsResetDisabler(AppListView* view);
    ScopedContentsResetDisabler(const ScopedContentsResetDisabler&) = delete;
    ScopedContentsResetDisabler& operator=(const ScopedContentsResetDisabler&) =
        delete;
    ~ScopedContentsResetDisabler();

   private:
    const raw_ptr<AppListView> view_;
  };

  // Does not take ownership of |delegate|.
  explicit AppListView(AppListViewDelegate* delegate);

  AppListView(const AppListView&) = delete;
  AppListView& operator=(const AppListView&) = delete;

  ~AppListView() override;

  // Used for testing, allows the page reset timer to be fired immediately
  // after starting.
  static void SetSkipPageResetTimerForTesting(bool enabled);

  // Initializes the view, only done once per session.
  void InitView(gfx::NativeView parent);

  // Initializes the contents of the view.
  void InitContents();

  // Initializes this view's widget.
  void InitWidget(gfx::NativeView parent);

  // Sets the state of all child views to be re-shown, then shows the view.
  // |preferred_state| - The initial app list view state.
  void Show(AppListViewState preferred_state);

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
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void Layout(PassKey) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Handles scroll events from various sources.
  bool HandleScroll(const gfx::Point& location,
                    const gfx::Vector2d& offset,
                    ui::EventType type);

  // Changes the app list state.
  void SetState(AppListViewState new_state);

  // Changes the app list state depending on the current |app_list_state_| and
  // whether the search box is empty.
  void SetStateFromSearchBoxView(bool search_box_is_empty,
                                 bool triggered_by_contents_change);

  // Offsets the y position of the app list (above the screen)
  void OffsetYPositionOfAppList(int offset);

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

  // Home launcher can become the focused window without being reset when all
  // open windows are closed in tablet mode. Ensures that correct initial view
  // is focused in this case.
  void OnHomeLauncherGainingFocusWithoutAnimation();

  // Ensures that apps grid pagination model has selected the initial page.
  void SelectInitialAppsPage();

  // Gets the PaginationModel owned by this view's apps grid.
  PaginationModel* GetAppsPaginationModel();

  // Returns the height of app list in fullscreen state.
  int GetFullscreenStateHeight() const;

  // Returns a animation metrics reporting callback  for state transition.
  metrics_util::SmoothnessCallback GetStateTransitionMetricsReportCallback();

  // Resets the animation metrics reporter for state transition.
  void ResetTransitionMetricsReporter();

  // WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override;

  void OnTabletModeAnimationTransitionNotified(
      TabletModeAnimationTransition animation_transition);

  AppListViewState app_list_state() const { return app_list_state_; }

  SearchBoxView* search_box_view() const { return search_box_view_; }

  AppListMainView* app_list_main_view() const { return app_list_main_view_; }

  AppListA11yAnnouncer* a11y_announcer() { return a11y_announcer_.get(); }

  bool is_fullscreen() const {
    return app_list_state_ == AppListViewState::kFullscreenAllApps ||
           app_list_state_ == AppListViewState::kFullscreenSearch;
  }

  void set_onscreen_keyboard_shown(bool onscreen_keyboard_shown) {
    onscreen_keyboard_shown_ = onscreen_keyboard_shown;
  }

  // Returns true if the Embedded Assistant UI is currently being shown.
  bool IsShowingEmbeddedAssistantUI() const;

  // Returns true if a folder is being renamed.
  bool IsFolderBeingRenamed();

  // Starts or stops a timer which will reset the app list to the initial apps
  // page. Called when the app list's visibility changes.
  void UpdatePageResetTimer(bool app_list_visibility);

  // Updates the title of the window that contains the launcher.
  void UpdateWindowTitle();

  // Called when app list visibility changed.
  void OnAppListVisibilityWillChange(bool visible);
  void OnAppListVisibilityChanged(bool shown);

 private:
  FRIEND_TEST_ALL_PREFIXES(AppListControllerImplTest,
                           CheckAppListViewBoundsWhenDismissVKeyboard);

  class StateAnimationMetricsReporter;

  // Returns insets that should be added to app list content to avoid overlap
  // with the shelf.
  gfx::Insets GetMainViewInsetsForShelf() const;

  // Updates the widget to be shown.
  void UpdateWidget();

  // Closes the AppListView when a click or tap event propogates to the
  // AppListView.
  void HandleClickOrTap(ui::LocatedEvent* event);

  // Set child views for |target_state|.
  void SetChildViewsForStateTransition(AppListViewState target_state);

  // Records the state transition for UMA.
  void RecordStateTransitionForUma(AppListViewState new_state);

  // Creates an Accessibility Event if the state transition warrants one.
  void MaybeCreateAccessibilityEvent(AppListViewState new_state);

  // Ensures that the app list widget bounds are set to the preferred bounds for
  // the current app list view state - intended to be called when the
  // display bounds available to the app list view change.
  void EnsureWidgetBoundsMatchCurrentState();

  // Gets the display nearest to the parent window.
  display::Display GetDisplayNearestView() const;

  // Gets the apps container view owned by this view.
  AppsContainerView* GetAppsContainerView();

  // Gets the root apps grid view owned by this view.
  PagedAppsGridView* GetRootAppsGridView();

  // Overridden from views::WidgetDelegateView:
  views::View* GetInitiallyFocusedView() override;

  // Returns true if scroll events should be ignored.
  bool ShouldIgnoreScrollEvents();

  // Returns preferred fullscreen widget bounds in parent window. Note that this
  // function should only be called after the widget is initialized.
  gfx::Rect GetPreferredWidgetBounds();

  // Reset the subpixel position offset of the |layer| so that it's DP origin
  // is snapped.
  void ResetSubpixelPositionOffset(ui::Layer* layer);

  const raw_ptr<AppListViewDelegate> delegate_;

  // Keeps track of the number of locks that prevent the app list view
  // from creating app list transition accessibility events. This is used to
  // prevent A11Y announcements when showing the assistant UI.
  int accessibility_event_disablers_ = 0;
  raw_ptr<AppListMainView, DanglingUntriaged> app_list_main_view_ = nullptr;

  raw_ptr<SearchBoxView, DanglingUntriaged> search_box_view_ =
      nullptr;  // Owned by views hierarchy.

  // The time the AppListView was requested to be shown. Used for metrics.
  std::optional<base::Time> time_shown_;

  // Whether the view is being built.
  bool is_building_ = false;

  // The state of the app list, controlled via SetState().
  AppListViewState app_list_state_ = AppListViewState::kClosed;
  // Set to target app list state while `SetState()` is being handled.
  AppListViewState target_app_list_state_ = AppListViewState::kClosed;

  // Metric reporter for state change animations.
  const std::unique_ptr<StateAnimationMetricsReporter>
      state_animation_metrics_reporter_;

  // Whether the on-screen keyboard is shown.
  bool onscreen_keyboard_shown_ = false;

  // Whether the app list has been translated up to ensure app list folder
  // view header is visible when onscreen keyboard is shown.
  bool offset_to_show_folder_with_onscreen_keyboard_ = false;

  // Used for announcing accessibility alerts.
  std::unique_ptr<AppListA11yAnnouncer> a11y_announcer_;

  // If true, the contents view is not reset when showing the app list.
  bool disable_contents_reset_when_showing_ = false;

  // A timer which will reset the app list to the initial page. This timer only
  // goes off when the app list is not visible after a set amount of time.
  base::OneShotTimer page_reset_timer_;

  // Used to cancel in progress `SetState()` request if `SetState()` gets called
  // again. Updating children state during app list view state update may cause
  // `SetState()` to get called again - for example, if exiting search results
  // page causes virtual keyboard to get hidden, and work area bounds available
  // to the app list change.
  // When calling methods that may cause nested `SetState()` call, `SetState()`
  // will bind a weak ptr to this factory, and it will bail out early if it
  // detects that `SetState()` got called again (in which case the weak ptr will
  // be invalidated).
  base::WeakPtrFactory<AppListView> set_state_weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_VIEW_H_
