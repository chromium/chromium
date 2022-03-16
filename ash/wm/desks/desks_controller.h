// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_CONTROLLER_H_
#define ASH_WM_DESKS_DESKS_CONTROLLER_H_

#include <memory>
#include <queue>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/autotest_desks_api.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/restore_data.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class Desk;
class DeskAnimationBase;
class DeskTemplate;

// Defines a controller for creating, destroying and managing virtual desks and
// their windows.
class ASH_EXPORT DesksController : public chromeos::DesksHelper,
                                   public wm::ActivationChangeObserver,
                                   public SessionObserver {
 public:
  using GetDeskTemplateCallback =
      base::OnceCallback<void(std::unique_ptr<DeskTemplate>)>;

  class Observer {
   public:
    // Called when |desk| has been created and added to
    // `DesksController::desks_`.
    virtual void OnDeskAdded(const Desk* desk) = 0;

    // Called when |desk| has been removed from `DesksController::desks_`.
    // However |desk| is kept alive temporarily and will be destroyed after all
    // observers have been notified with this.
    virtual void OnDeskRemoved(const Desk* desk) = 0;

    // Called when the desk at |old_index| is reordered to |new_index|.
    virtual void OnDeskReordered(int old_index, int new_index) = 0;

    // Called when the |activated| desk gains activation from the |deactivated|
    // desk.
    virtual void OnDeskActivationChanged(const Desk* activated,
                                         const Desk* deactivated) = 0;

    // Called when the desk switch animations is launching.
    virtual void OnDeskSwitchAnimationLaunching() = 0;

    // Called when the desk switch animations on all root windows finish.
    virtual void OnDeskSwitchAnimationFinished() = 0;

    // Called when the desk's name is changed, including when the name is set on
    // a newly created desk if we are not using name user nudges.
    virtual void OnDeskNameChanged(const Desk* desk,
                                   const std::u16string& new_name) = 0;

   protected:
    virtual ~Observer() = default;
  };

  DesksController();

  DesksController(const DesksController&) = delete;
  DesksController& operator=(const DesksController&) = delete;

  ~DesksController() override;

  // Convenience method for returning the DesksController instance.
  static DesksController* Get();

  // Returns the default name for a desk at |desk_index|.
  static std::u16string GetDeskDefaultName(size_t desk_index);

  const std::vector<std::unique_ptr<Desk>>& desks() const { return desks_; }

  const Desk* active_desk() const { return active_desk_; }

  const base::flat_set<aura::Window*>& visible_on_all_desks_windows() const {
    return visible_on_all_desks_windows_;
  }

  bool disable_app_id_check_for_desk_templates() {
    return disable_app_id_check_for_desk_templates_;
  }

  DeskAnimationBase* animation() const { return animation_.get(); }

  // Returns the current |active_desk()| or the soon-to-be active desk if a desk
  // switch animation is in progress.
  const Desk* GetTargetActiveDesk() const;

  // Returns the visible on all desks windows that reside on |root_window|.
  base::flat_set<aura::Window*> GetVisibleOnAllDesksWindowsOnRoot(
      aura::Window* root_window) const;

  // Restores the primary user's activate desk at active_desk_index.
  void RestorePrimaryUserActiveDeskIndex(int active_desk_index);

  // Restacks the visible on all desks windows on the active desks and notifies
  // all desks of content change. Should be called during user switch when the
  // new user's windows have been shown.
  void OnNewUserShown();

  // Destroys any pending animations in preparation for shutdown and save desk
  // metrics.
  void Shutdown();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if desks are being modified due to desk creation, removal, or
  // activation. It also returns true while the desk switch animation is in
  // progress.
  bool AreDesksBeingModified() const;

  // Returns true if we haven't reached the maximum allowed number of desks.
  bool CanCreateDesks() const;

  // Returns true as long as there are two or more desks. It is required that
  // there is at least one single desk at any time.
  bool CanRemoveDesks() const;

  // Returns the next / previous desks to the target / currently active desk.
  // Returns nullptr if the active desk is the first on the left or the last on
  // the right, and previous and next desks are requested respectively.
  Desk* GetNextDesk(bool use_target_active_desk = true) const;
  Desk* GetPreviousDesk(bool use_target_active_desk = true) const;

  // Creates a new desk. CanCreateDesks() must be checked before calling this.
  void NewDesk(DesksCreationRemovalSource source);

  // Removes and deletes the given |desk|. |desk| must already exist, and
  // CanRemoveDesks() must be checked before this.
  // This will trigger the `DeskRemovalAnimation` if the active desk is being
  // removed outside of overview.
  void RemoveDesk(const Desk* desk, DesksCreationRemovalSource source);

  // Reorder the desk at |old_index| to |new_index|.
  void ReorderDesk(int old_index, int new_index);

  // Performs the desk switch animation on all root windows to activate the
  // given |desk| and to deactivate the currently active one. |desk| has to be
  // an existing desk. The active window on the currently active desk will be
  // deactivated, and the most-recently used window from the newly-activated
  // desk will be activated.
  // This will trigger the `DeskActivationAnimation`.
  void ActivateDesk(const Desk* desk, DesksSwitchSource source);

  // Activates the desk to the left or right of the current desk, if it exists.
  // Performs a hit the wall animation if there is no desk to activate. Returns
  // false if there is already a desk animation active. This function will then
  // do nothing, no desk switch or hit the wall animation.
  bool ActivateAdjacentDesk(bool going_left, DesksSwitchSource source);

  // Functions used by WmGestureHandler to modify the current touchpad desk
  // animation, if it exists. StartSwipeAnimation starts a new animation to
  // an adjacent desk, or replaces an existing swipe animation. It returns
  // true if either of those were successful, false otherwise.
  bool StartSwipeAnimation(bool move_left);
  void UpdateSwipeAnimation(float scroll_delta_x);
  void EndSwipeAnimation();

  // Moves |window| (which must belong to the currently active desk) to
  // |target_desk| (which must be a different desk).
  // |target_root| is provided if |window| is desired to be moved to another
  // desk on another display, otherwise, you can just provide
  // |window->GetRootWindow()| if the window should stay on the same display.
  // If |window| is minimized and isn't visible on all desks, it will be
  // unminimized after it's moved to |target_desk|. Returns true on success,
  // false otherwise (e.g. if |window| doesn't belong to the active desk or
  // |window| is visible on all desks and user is manually moving it). If
  // |window| is visible on all desks and |source| is kShortcut, it will be made
  // not visible on all desks.
  bool MoveWindowFromActiveDeskTo(aura::Window* window,
                                  Desk* target_desk,
                                  aura::Window* target_root,
                                  DesksMoveWindowFromActiveDeskSource source);

  // Adds |window| to |visible_on_all_desks_windows_|.
  void AddVisibleOnAllDesksWindow(aura::Window* window);

  // Removes |window| if it is in |visible_on_all_desks_windows_|.
  void MaybeRemoveVisibleOnAllDesksWindow(aura::Window* window);

  // Notifies each desk in |desks_| that their contents has changed.
  void NotifyAllDesksForContentChanged();

  void NotifyDeskNameChanged(const Desk* desk, const std::u16string& new_name);

  // Reverts the name of the given |desk| to the default value (i.e. "Desk 1",
  // "Desk 2", ... etc.) according to its position in the |desks_| list, as if
  // it was never modified by users.
  void RevertDeskNameToDefault(Desk* desk);

  // Restores the desk at |index| to the given |name|. This is only for
  // user-modified desk names, and hence |name| should never be empty since
  // users are not allowed to set empty names.
  void RestoreNameOfDeskAtIndex(std::u16string name, size_t index);

  // Restores the creation time of the desk at |index|.
  void RestoreCreationTimeOfDeskAtIndex(base::Time creation_time, size_t index);

  // Restores the visited metrics of the desk at |index|. If it has been more
  // than one day since |last_day_visited|, record and reset the consecutive
  // daily visits metrics.
  void RestoreVisitedMetricsOfDeskAtIndex(int first_day_visited,
                                          int last_day_visited,
                                          size_t index);

  // Restores the |interacted_with_this_week_| field of the desk at |index|.
  void RestoreWeeklyInteractionMetricOfDeskAtIndex(
      bool interacted_with_this_week,
      size_t index);

  // Restores the metrics related to tracking a user's weekly active desks.
  // Records and resets these metrics if the current time is past |report_time|.
  void RestoreWeeklyActiveDesksMetrics(int weekly_active_desks,
                                       base::Time report_time);

  // Returns the time when |weekly_active_desks_scheduler_| is scheduled to go
  // off.
  base::Time GetWeeklyActiveReportTime() const;

  // Called explicitly by the RootWindowController when a root window has been
  // added or about to be removed in order to update all the available desks.
  void OnRootWindowAdded(aura::Window* root_window);
  void OnRootWindowClosing(aura::Window* root_window);

  int GetDeskIndex(const Desk* desk) const;

  // Gets the container of the desk at |desk_index| in a specific screen with a
  // |target_root|. If desk_index is invalid, it returns nullptr.
  aura::Window* GetDeskContainer(aura::Window* target_root, int desk_index);

  // chromeos::DesksHelper:
  bool BelongsToActiveDesk(aura::Window* window) override;
  int GetActiveDeskIndex() const override;
  std::u16string GetDeskName(int index) const override;
  int GetNumberOfDesks() const override;
  void SendToDeskAtIndex(aura::Window* window, int desk_index) override;

  // Captures the active desk and returns it as a desk template containing
  // necessary information that can be used to create a same desk via provided
  // `callback`, `root_window_to_show` is used to determine which monitor to
  // show template related dialog.
  void CaptureActiveDeskAsTemplate(
      GetDeskTemplateCallback callback,
      aura::Window* root_window_to_show = nullptr) const;

  // Creates and activates a new desk for a template with name `template_name`
  // or `template_name ({counter})` to resolve naming conflicts. Runs `callback`
  // with true if creation was successful, false otherwise.
  void CreateAndActivateNewDeskForTemplate(
      const std::u16string& template_name,
      base::OnceCallback<void(bool)> callback);

  // Called when an app with `app_id` is a single instance app which is about to
  // get launched from a saved template. Moves the existing app instance to the
  // active desk without animation if it exists. Returns true if we should
  // launch the app (i.e. the app was not found and thus should be launched),
  // and false otherwise. Optional launch parameters may be present in
  // `launch_list`.
  bool OnSingleInstanceAppLaunchingFromTemplate(
      const std::string& app_id,
      const app_restore::RestoreData::LaunchList& launch_list);

  // Updates the default names (e.g. "Desk 1", "Desk 2", ... etc.) given to the
  // desks. This is called when desks are added, removed or reordered to update
  // the names based on the desks order.
  void UpdateDesksDefaultNames();

  // ::wm::ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gaining_active,
                          aura::Window* losing_active) override;
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnFirstSessionStarted() override;

  // Fires the timer used for recording desk traversals immediately.
  void FireMetricsTimerForTesting();

 private:
  class DeskTraversalsMetricsHelper;
  friend class DeskAnimationBase;
  friend class DeskActivationAnimation;
  friend class DeskRemovalAnimation;
  friend class DesksTemplatesTest;

  // Keeps the state for the asynchronous call for AppLaunchData to the clients.
  struct Call {
    Call();
    Call(Call&&);
    Call& operator=(Call&&);
    ~Call();

    std::vector<aura::Window*> unsupported_apps;
    std::unique_ptr<app_restore::RestoreData> data;
    uint32_t pending_request_count = 0;
    GetDeskTemplateCallback callback;
  };

  void set_disable_app_id_check_for_desk_templates(
      bool disable_app_id_check_for_desk_templates) {
    disable_app_id_check_for_desk_templates_ =
        disable_app_id_check_for_desk_templates;
  }

  void OnAnimationFinished(DeskAnimationBase* animation);

  bool HasDesk(const Desk* desk) const;

  bool HasDeskWithName(const std::u16string& desk_name) const;

  // Activates the given |desk| and deactivates the currently active one. |desk|
  // has to be an existing desk. If |update_window_activation| is true,
  // the active desk on the deactivated desk will be deactivated, and the most-
  // recently used window on the newly-activated desk will be deactivated. This
  // parameter is almost always true except when the active desk is being
  // removed while in overview mode. In that case, windows from the active desk
  // will move to another desk and remain in the overview grid, and no
  // activation or deactivation should be done in order to keep overview mode
  // active.
  void ActivateDeskInternal(const Desk* desk, bool update_window_activation);

  // Removes `desk` without animation.
  void RemoveDeskInternal(const Desk* desk, DesksCreationRemovalSource source);

  // Moves all the windows that are visible on all desks that currently
  // reside on |active_desk_| to |new_desk|.
  void MoveVisibleOnAllDesksWindowsFromActiveDeskTo(Desk* new_desk);

  // Checks if the fullscreen state has changed after desks were switched and
  // notifies shell if needed. For e.g Desk 1 has a window in fullscreen while
  // Desk 2 does not, this function would notify shell of a fullscreen state
  // change when switching between Desk 1 and 2 in that case.
  void NotifyFullScreenStateChangedAcrossDesksIfNeeded(
      const Desk* previous_active_desk);

  // Iterates through the visible on all desks windows on the active desk
  // and restacks them based on their position in the global MRU tracker. This
  // should be called after desk activation.
  void RestackVisibleOnAllDesksWindowsOnActiveDesk();

  // Returns the desk to which |window| belongs or nullptr if it doesn't belong
  // to any desk.
  const Desk* FindDeskOfWindow(aura::Window* window) const;

  // Reports the number of windows per each available desk. This called when a
  // desk switch occurs.
  void ReportNumberOfWindowsPerDeskHistogram() const;

  void ReportDesksCountHistogram() const;

  // Records the Desk class' global |g_weekly_active_desks| and also resets it
  // to 1, accounting for the current active desk. Also resets the
  // |interacted_with_this_week_| field for each inactive desk in |desks_|.
  void RecordAndResetNumberOfWeeklyActiveDesks();

  // Receives the AppLaunchInfo from the single client and puts it into the
  // RestoreData record where data from all clients is accumulated.  If all data
  // is collected, invokes SendAppLaunchData().
  // TODO(crbug.com/1268741): extract this, together with the `calls_` and the
  // relevant methods, into a separate class.
  void OnAppLaunchDataReceived(
      uint32_t serial,
      const std::string app_id,
      const int32_t window_id,
      std::unique_ptr<app_restore::WindowInfo> window_info,
      std::unique_ptr<app_restore::AppLaunchInfo> app_launch_info) const;

  // Sends the RestoreData to the consumer after all clients deliver their
  // AppLaunchInfo.
  void SendRestoreData(uint32_t serial,
                       aura::Window* root_window_to_show) const;

  std::vector<std::unique_ptr<Desk>> desks_;

  Desk* active_desk_ = nullptr;

  // The account ID of the current active user.
  AccountId current_account_id_;

  // Stores the per-user last active desk index.
  base::flat_map<AccountId, int> user_to_active_desk_index_;

  // Stores visible on all desks windows, that is normal type windows with
  // normal z-ordering and are visible on all workspaces. Store here to prevent
  // repeatedly retrieving these windows on desk switches.
  base::flat_set<aura::Window*> visible_on_all_desks_windows_;

  // True when desks addition, removal, or activation change are in progress.
  // This can be checked when overview mode is active to avoid exiting overview
  // mode as a result of desks modifications.
  bool are_desks_being_modified_ = false;

  // In ash unittests, the FullRestoreSaveHandler isn't hooked up so initialized
  // windows lack an app id. If a window doesn't have a valid app id, then it
  // won't be tracked by Desk as a supported window and those windows will be
  // deemed unsupported for Desk Templates. If
  // `disable_app_id_check_for_desk_templates_` is true, then this check is
  // omitted so we can test Desk Templates.
  bool disable_app_id_check_for_desk_templates_ = false;

  // Not null if there is an on-going desks animation.
  std::unique_ptr<DeskAnimationBase> animation_;

  // A free list of desk container IDs to be used for newly-created desks. New
  // desks pops from this queue and removed desks's associated container IDs are
  // re-pushed on this queue.
  std::queue<int> available_container_ids_;

  // Responsible for tracking and writing number of desk traversals one has
  // done within a span of X seconds.
  std::unique_ptr<DeskTraversalsMetricsHelper> metrics_helper_;

  base::ObserverList<Observer>::Unchecked observers_;

  // Scheduler for reporting the weekly active desks metric.
  base::OneShotTimer weekly_active_desks_scheduler_;

  // Data to put into desk template.  Mutable because it is not part of the
  // state but it can live between asynchronous calls.
  // Because gathering the data is asynchronous, we maintain a map of requests
  // identified by serial number of the request that comes from the UI.
  mutable uint32_t serial_ = 0;
  mutable base::flat_map<uint32_t, Call> calls_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_CONTROLLER_H_
