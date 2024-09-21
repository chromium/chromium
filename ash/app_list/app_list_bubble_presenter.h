// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_BUBBLE_PRESENTER_H_
#define ASH_APP_LIST_APP_LIST_BUBBLE_PRESENTER_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/display/display_observer.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace ash {

class AppListBubbleEventFilter;
class AppListBubbleView;
class AppListControllerImpl;
enum class AppListSortOrder;

// Manages the UI for the bubble launcher used in clamshell mode. Handles
// showing and hiding the UI, as well as bounds computations. Only one bubble
// can be visible at a time, across all displays.
class ASH_EXPORT AppListBubblePresenter : public views::WidgetObserver,
                                          public wm::ActivationChangeObserver,
                                          public display::DisplayObserver {
 public:
  explicit AppListBubblePresenter(AppListControllerImpl* controller);
  AppListBubblePresenter(const AppListBubblePresenter&) = delete;
  AppListBubblePresenter& operator=(const AppListBubblePresenter&) = delete;
  ~AppListBubblePresenter() override;

  // Closes the bubble if it is open and prepares for shutdown.
  void Shutdown();

  // Shows the bubble on the display with `display_id`. The bubble is shown
  // asynchronously (after a delay) because the continue suggestions need to be
  // refreshed before the bubble views can be created and animated. This delay
  // is skipped in unit tests (see TestAppListClient) for convenience. Larger
  // tests (e.g. browser_tests) may need to wait for the window to open.
  void Show(int64_t display_id);

  // Shows or hides the bubble on the display with `display_id`. Returns the
  // appropriate ShelfAction to indicate whether the bubble was shown or hidden.
  ShelfAction Toggle(int64_t display_id);

  // Closes and destroys the bubble.
  void Dismiss();

  // Returns the bubble window or nullptr if it is not open.
  aura::Window* GetWindow() const;

  // Returns true if the bubble is showing on any display.
  bool IsShowing() const;

  // Returns true if the assistant page is showing.
  bool IsShowingEmbeddedAssistantUI() const;

  // Switches to the assistant page. Requires the bubble to be open.
  void ShowEmbeddedAssistantUI();

  // Updates the continue section visibility based on user preference.
  void UpdateContinueSectionVisibility();

  // Handles `AppListController::UpdateAppListWithNewSortingOrder()` for the
  // bubble launcher.
  void UpdateForNewSortingOrder(
      const std::optional<AppListSortOrder>& new_order,
      bool animate,
      base::OnceClosure update_position_closure);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gaining_active,
                          aura::Window* losing_active) override {}
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Returns the preferred width for the bubble launcher for the |root_window|.
  int GetPreferredBubbleWidth(aura::Window* root_window) const;

  views::Widget* bubble_widget_for_test() { return bubble_widget_; }
  AppListBubbleView* bubble_view_for_test() { return bubble_view_; }

 private:
  // Callback for zero state search update. Builds the bubble widget and views
  // on display `display_id` and triggers the show animation.
  void OnZeroStateSearchDone(int64_t display_id);

  // Callback for AppListBubbleEventFilter, used to notify this of presses
  // outside the bubble.
  void OnPressOutsideBubble(const ui::LocatedEvent& event);

  // Gets the display id for the display `bubble_widget_` is shown on. Returns
  // kInvalidDisplayId if not shown.
  int64_t GetDisplayId() const;

  // Callback for the hide animation.
  void OnHideAnimationEnded();

  const raw_ptr<AppListControllerImpl> controller_;

  // Whether the view is showing or animating to show. Note that the
  // `bubble_widget_` may be null during the zero state search called in
  // `Show()`.
  bool is_target_visibility_show_ = false;

  // Owned by native widget.
  raw_ptr<views::Widget> bubble_widget_ = nullptr;

  // Owned by views.
  raw_ptr<AppListBubbleView> bubble_view_ = nullptr;

  // The page to show after the views are constructed.
  AppListBubblePage target_page_ = AppListBubblePage::kApps;

  // Closes the widget when the user clicks outside of it.
  std::unique_ptr<AppListBubbleEventFilter> bubble_event_filter_;

  // Observes display configuration changes.
  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtrFactory<AppListBubblePresenter> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_BUBBLE_PRESENTER_H_
