// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_PERSISTENT_DESKS_BAR_CONTROLLER_H_
#define ASH_WM_DESKS_PERSISTENT_DESKS_BAR_CONTROLLER_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_controller_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class PersistentDesksBarView;
enum class AppListViewState;

// Controller for the persistent desks bar. One per display, because each
// display has its own persistent desks bar widget and view hierarchy, different
// settings to show or hide the bar as well.
class ASH_EXPORT PersistentDesksBarController
    : public SessionObserver,
      public OverviewObserver,
      public DesksController::Observer,
      public TabletModeObserver,
      public ShellObserver,
      public AppListControllerObserver,
      public AccessibilityObserver,
      public display::DisplayObserver {
 public:
  constexpr static int kBarHeight = 40;

  PersistentDesksBarController();
  PersistentDesksBarController(const PersistentDesksBarController&) = delete;
  PersistentDesksBarController& operator=(const PersistentDesksBarController&) =
      delete;
  ~PersistentDesksBarController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns true if it satisfies the prerequisites to show the persistent
  // desks bar. `kBentoBar` feature is running as an experiment now. And we will
  // only enable it for a specific group of existing desks users, see
  // `kUserHasUsedDesksRecently` for more details. But we also want to enable it
  // if the user has explicitly enabled `kBentoBar` from chrome://flags or from
  // the command line. Even though the user is not in the group of existing
  // desks users.
  static bool ShouldPersistentDesksBarBeVisible();

  const views::Widget* persistent_desks_bar_widget() const {
    return persistent_desks_bar_widget_.get();
  }

  const PersistentDesksBarView* persistent_desks_bar_view() const {
    return persistent_desks_bar_view_;
  }

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // OverviewObserver:
  void OnOverviewModeWillStart() override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override;
  void OnDeskRemoved(const Desk* desk) override;
  void OnDeskReordered(int old_index, int new_index) override;
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;
  void OnDeskNameChanged(const Desk* desk,
                         const std::u16string& new_name) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // AppListControllerObserver:
  void OnViewStateChanged(AppListViewState state) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Returns the value of the pref 'kBentoBarEnabled'.
  bool IsEnabled() const;

  // Toggles the value of `is_enabled_` and destroys the bar if it is togggled
  // to false.
  void ToggleEnabledState();

  // Initializes and shows the widget that contains the PersistentDesksBarView
  // contents. Creates the widget only if ShouldPersistentDesksBarBeCreated()
  // returns true and it hasn't been created already. Only refreshes the
  // contents and shows the widget if it has been created.
  void MaybeInitBarWidget();

  // Destroys `persistent_desks_bar_widget_` and `persistent_desks_bar_view_`.
  void DestroyBarWidget();

  // Updates the bar's status on window state changes.
  void UpdateBarOnWindowStateChanges(aura::Window* window);

  // Updates the bar's status when the given `window` is destroying.
  void UpdateBarOnWindowDestroying(aura::Window* window);

 private:
  // Returns true if the persistent desks bar should be created and shown.
  bool ShouldPersistentDesksBarBeCreated() const;

  // Updates bar's state on the pref `kBentoBarEnabled` changes.
  void UpdateBarStateOnPrefChanges();

  views::UniqueWidgetPtr persistent_desks_bar_widget_;
  // The contents view of the above |persistent_desks_bar_widget_| if created.
  PersistentDesksBarView* persistent_desks_bar_view_ = nullptr;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  PrefService* active_user_pref_service_ = nullptr;

  // Indicates if overview mode will start. This is used to guarantee the work
  // area will be updated on the bento bar’s visibility changes before entering
  // overview mode. Since the work area will not be updated once we are already
  // in overview mode. Note, this will be set to true when overview mode will
  // start and set to false until overview mode ends.
  bool overview_mode_in_progress_ = false;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_PERSISTENT_DESKS_BAR_CONTROLLER_H_
