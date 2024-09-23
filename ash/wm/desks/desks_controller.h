// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_CONTROLLER_H_
#define ASH_WM_DESKS_DESKS_CONTROLLER_H_

#include <memory>
#include <queue>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/autotest_desks_api.h"
#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "ash/wm/desks/templates/restore_data_collector.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/restore_data.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Determines how a desk will be closed when it is removed in the `RemoveDesk`
// and `RemoveDeskInternal` functions. This allows for the desk removal
// functions to support a range of different close methods, such as combining
// desks and closing desks with windows, as well as closing desks with windows
// and providing an undo toast when done manually.
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// `DeskCloseType` in src/tools/metrics/histograms/metadata/ash/enums.xml.
enum class DeskCloseType {
  // Closes the target desk and moves its windows to another desk.
  kCombineDesks = 0,
  // Closes the target desk and all of its windows.
  kCloseAllWindows = 1,
  // Closes the target desk, saves its data to the `temporary_removed_desk_`
  // member variable, and creates a toast that will fully destroy the desk if
  // the user does not interact with it before it expires.
  kCloseAllWindowsAndWait = 2,
  kMaxValue = kCloseAllWindowsAndWait,
};

class Desk;
class DeskAnimationBase;
class DeskBarController;
class DeskTemplate;

// Defines a controller for creating, destroying and managing virtual desks and
// their windows.
class ASH_EXPORT DesksController : public chromeos::DesksHelper,
                                   public wm::ActivationChangeObserver,
                                   public SessionObserver,
                                   public DeskProfilesDelegate::Observer {
 public:
  using GetDeskTemplateCallback =
      base::OnceCallback<void(std::unique_ptr<DeskTemplate>)>;

  class Observer {
   public:
    // Called when `desk` has been created and added to
    // `DesksController::desks_`. It's important to note that `desk` can be
    // added at any position in `DesksController::desks_`.  Is also called
    // when a desk is added from undoing a desk removal, in which case
    // `from_undo` is true.
    virtual void OnDeskAdded(const Desk* desk, bool from_undo) {}

    // Called when `desk` has been removed from `DesksController::desks_`.
    // However `desk` is kept alive temporarily and will be destroyed after all
    // observers have been notified with this.
    virtual void OnDeskRemoved(const Desk* desk) {}

    // Called when `desk` has been been removed from `DesksController::desks_`
    // and past the buffer so that it can no longer be revived.
    virtual void OnDeskRemovalFinalized(const base::Uuid& uuid) {}

    // Called when the desk at |old_index| is reordered to |new_index|.
    virtual void OnDeskReordered(int old_index, int new_index) {}

    // Called when the `activated` desk gains activation from the `deactivated`
    // desk.
    virtual void OnDeskActivationChanged(const Desk* activated,
                                         const Desk* deactivated) {}

    // Called when the desk switch animations is launching.
    virtual void OnDeskSwitchAnimationLaunching() {}

    // Called when the desk switch animations on all root windows finish.
    virtual void OnDeskSwitchAnimationFinished() {}

    // Called when the desk's name is changed, including when the name is set on
    // a newly created desk if we are not using name user nudges.
    virtual void OnDeskNameChanged(const Desk* desk,
                                   const std::u16string& new_name) {}

   protected:
    virtual ~Observer() = default;
  };

  DesksController();

  DesksController(const DesksController&) = delete;
  DesksController& operator=(const DesksController&) = delete;

  ~DesksController() override;

  // Convenience method for returning the DesksController instance.
  static DesksController* Get();

  // Returns the default name for a desk at `desk_index`.
  static std::u16string GetDeskDefaultName(size_t desk_index);

  const std::vector<std::unique_ptr<Desk>>& desks() const { return desks_; }

  const Desk* active_desk() const { return active_desk_; }

  const base::flat_set<raw_ptr<aura::Window, CtnExperimental>>&
  visible_on_all_desks_windows() const {
    return visible_on_all_desks_windows_;
  }

  DeskAnimationBase* animation() const { return animation_.get(); }

  DeskBarController* desk_bar_controller() const {
    return desk_bar_controller_.get();
  }

  // Finds and returns the name of the desk that `desk` would be combined with
  // when the user clicks or presses the combine desks button or context menu
  // item.
  const std::u16string& GetCombineDesksTargetName(const Desk* desk) const;

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

  // Returns the desk that matches the desk_uuid, and returns null if no matches
  // found.
  Desk* GetDeskByUuid(const base::Uuid& desk_uuid) const;

  // Returns the desk index of the desk that matches the desk_uuid, and returns
  // -1 if no match is found.
  int GetDeskIndexByUuid(const base::Uuid& desk_uuid) const;

  // Creates a new desk. `CanCreateDesks()` must be checked before calling this.
  void NewDesk(DesksCreationRemovalSource source);
  void NewDesk(DesksCreationRemovalSource source, std::u16string name);

  bool HasDesk(const Desk* desk) const;

  // Gives the desk with the specified index.
  Desk* GetDeskAtIndex(size_t index) const;

  // Removes and deletes the given `desk`. `desk` must already exist, and
  // CanRemoveDesks() must be checked before this.
  // This will trigger the `DeskRemovalAnimation` if the active desk is being
  // removed outside of overview.
  // `close_type` determines how the desk will be closed. See the
  // `DeskCloseType` enum for details on what each value does.
  void RemoveDesk(const Desk* desk,
                  DesksCreationRemovalSource source,
                  DeskCloseType close_type);

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

  // Used to indicate that the all-desk `window` has moved to a new root.
  void NotifyAllDeskWindowMovedToNewRoot(aura::Window* window);

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

  // Sets the `uuid_` of the desk at `index` to the supplied `guid`.
  void RestoreGuidOfDeskAtIndex(base::Uuid guid, size_t index);

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

  // Captures the active desk and returns it as a saved desk (of type
  // `template_type`) containing necessary information that can be used to
  // create a same desk via provided `callback`, `root_window_to_show` is used
  // to determine which monitor to show saved desk related dialog.
  void CaptureActiveDeskAsSavedDesk(GetDeskTemplateCallback callback,
                                    DeskTemplateType template_type,
                                    aura::Window* root_window_to_show) const;

  // Creates a new desk and optionally activates it depending on
  // `template_type`. If `customized_desk_name` is provided, desk name will be
  // `customized_desk_name` or `customized_desk_name
  // ({counter})` to resolve naming conflicts. CanCreateDesks() must be checked
  // before calling this.
  Desk* CreateNewDeskForSavedDesk(
      DeskTemplateType template_type,
      const std::u16string& customized_desk_name = std::u16string());

  // Called when an app with `app_id` is a single instance app which is about to
  // get launched from a saved desk. Moves the existing app instance to the
  // active desk without animation if it exists. Returns true if we should
  // launch the app (i.e. the app was not found and thus should be launched),
  // and false otherwise. Optional launch parameters may be present in
  // `launch_list`.
  bool OnSingleInstanceAppLaunchingFromSavedDesk(
      const std::string& app_id,
      const app_restore::RestoreData::LaunchList& launch_list);

  // Updates the default names (e.g. "Desk 1", "Desk 2", ... etc.) given to the
  // desks. This is called when desks are added, removed or reordered to update
  // the names based on the desks order.
  void UpdateDesksDefaultNames();

  // Cancels the desk removal toast and then triggers `UndoDeskRemoval()` if
  // there is a desk removal in progress.
  void MaybeCancelDeskRemoval();

  // Cancels the desk removal toast if there is currently a
  // `temporary_removed_desk_` and
  // `temporary_removed_desk_->is_toast_persistent()` is true.
  void MaybeDismissPersistentDeskRemovalToast();

  // Requests focus on the undo desk toast's dismiss button.
  bool RequestFocusOnUndoDeskRemovalToast();

  // Adds focus highlight to an active toast to undo desk removal if one is
  // active and the toast is not already highlighted. Otherwise, it removes the
  // highlight from an active toast and returns false.
  bool MaybeToggleA11yHighlightOnUndoDeskRemovalToast();

  // Activates the undo button on a highlighted toast to undo desk removal if
  // one is active. Returns true if the activation was successful.
  bool MaybeActivateDeskRemovalUndoButtonOnHighlightedToast();

  // Returns true if it's possible to enter or exit overview mode in the current
  // configuration. This can be false at certain times, such as when there is an
  // active desk animation.
  bool CanEnterOverview() const;
  bool CanEndOverview() const;

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

  // DeskProfilesDelegate::Observer:
  void OnProfileRemoved(uint64_t profile_id) override;

  // Fires the timer used for recording desk traversals immediately.
  void FireMetricsTimerForTesting();

  // Resets the animation if there is any ongiong one.
  void ResetAnimation();

  // Generates a unique desk name. If `base` already existed, returns a
  // desk name with format of `base({counter})` to resolve naming conflicts.
  // Otherwise returns `base`.
  std::u16string CreateUniqueDeskName(const std::u16string& base) const;

  // Saves metrics and resets `temporary_removed_desk_` if `toast_id` is empty
  // or it matches the toast ID stored in `temporary_removed_desk_`.
  void MaybeCommitPendingDeskRemoval(
      const std::string& toast_id = std::string());

  // Returns true if there is an active toast for undoing desk removal and that
  // toast's dismiss button is currently being focused.
  bool IsUndoToastFocused() const;

  // Tracks/untracks the z-order of `window` on all desks. Should only be called
  // when per-desk z-order is enabled.
  void TrackWindowOnAllDesks(aura::Window* window);
  void UntrackWindowFromAllDesks(aura::Window* window);

 private:
  class DeskTraversalsMetricsHelper;
  class RemovedDeskData;
  friend class DeskAnimationBase;
  friend class DeskActivationAnimation;
  friend class DeskRemovalAnimation;
  friend class DesksTestApi;

  void OnAnimationFinished(DeskAnimationBase* animation);

  bool HasDeskWithName(const std::u16string& desk_name) const;

  // Activates the given `desk` and deactivates the currently active one. `desk`
  // has to be an existing desk. If `update_window_activation` is true, the
  // active window on the deactivated desk will be deactivated, and the most-
  // recently used window on the newly-activated desk will be activated. This
  // parameter is almost always true except when the active desk is being
  // removed while in overview mode. In that case, windows from the active desk
  // will move to another desk and remain in the overview grid, and no
  // activation or deactivation should be done in order to keep overview mode
  // active.
  void ActivateDeskInternal(const Desk* desk, bool update_window_activation);

  // Removes `desk` without animation.
  // `close_type` determines how the desk will be closed. See `DeskCloseType`
  // for more information on what each value does. `desk_switched` indicates
  // that the desk switch animation has already moved activation from the
  // removing desk.
  void RemoveDeskInternal(const Desk* desk,
                          DesksCreationRemovalSource source,
                          DeskCloseType close_type,
                          bool desk_switched);

  // Inserts the desk contained in `temporary_removed_desk_->desk()` back into
  // its original position of `temporary_removed_desk_->index()`. Activates the
  // removed desk if it was active before.
  void UndoDeskRemoval();

  // Records and reports metrics on the desk contained in `removed_desk_data`
  // and closes all of its windows. Because all app windows would already be
  // moved to another desk during a combine desk operation, the action of
  // closing all windows in the desk would become a no-op, so we can still use
  // this function in the combine desks process.
  void FinalizeDeskRemoval(RemovedDeskData* removed_desk_data);

  // Forcefully cleans up app windows that should be closed.
  void CleanUpClosedAppWindowsTask(
      std::unique_ptr<aura::WindowTracker> closing_window_tracker);

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

  // Report the number of windows being closed when close_all action are
  // finalized per each desk removal source.
  void ReportClosedWindowsCountPerSourceHistogram(
      DesksCreationRemovalSource source,
      int windows_closed) const;

  // Reports custom desk name metrics for the number of desks with custom names
  // and the percentage of the user's desks with custom names.
  void ReportCustomDeskNames() const;

  static base::TimeDelta GetCloseAllWindowCloseTimeoutForTest();
  static base::AutoReset<base::TimeDelta> SetCloseAllWindowCloseTimeoutForTest(
      base::TimeDelta interval);

  std::vector<std::unique_ptr<Desk>> desks_;

  raw_ptr<Desk> active_desk_ = nullptr;

  // Target desk if in middle of desk activation, `nullptr` otherwise.
  raw_ptr<Desk> desk_to_activate_ = nullptr;

  // The account ID of the current active user.
  AccountId current_account_id_;

  // Stores the per-user last active desk index.
  // TODO(b/284482035): Clean this up.
  base::flat_map<AccountId, int> user_to_active_desk_index_;

  // Stores visible on all desks windows, that is normal type windows with
  // normal z-ordering and are visible on all workspaces. Store here to prevent
  // repeatedly retrieving these windows on desk switches.
  base::flat_set<raw_ptr<aura::Window, CtnExperimental>>
      visible_on_all_desks_windows_;

  // True when desks addition, removal, or activation change are in progress.
  // This can be checked when overview mode is active to avoid exiting overview
  // mode as a result of desks modifications.
  bool are_desks_being_modified_ = false;

  // Not null if there is an on-going desks animation.
  std::unique_ptr<DeskAnimationBase> animation_;

  // A free list of desk container IDs to be used for newly-created desks. New
  // desks pops from this queue and removed desks's associated container IDs are
  // re-pushed on this queue.
  std::queue<int> available_container_ids_;

  // Responsible for tracking and writing number of desk traversals one has
  // done within a span of X seconds.
  std::unique_ptr<DeskTraversalsMetricsHelper> metrics_helper_;

  // Holds a desk when it has been removed but we are still waiting for the user
  // to confirm that they want the desk to be removed.
  std::unique_ptr<RemovedDeskData> temporary_removed_desk_;

  // Dedicated controller for the desk bars.
  std::unique_ptr<DeskBarController> desk_bar_controller_;

  base::ObserverList<Observer>::Unchecked observers_;

  // Scheduler for reporting the weekly active desks metric.
  base::OneShotTimer weekly_active_desks_scheduler_;

  // Does the job for the `CaptureActiveDeskAsSavedDesk()` method.
  mutable RestoreDataCollector restore_data_collector_;

  base::ScopedObservation<DeskProfilesDelegate, DeskProfilesDelegate::Observer>
      desk_profiles_observer_{this};

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DesksController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_CONTROLLER_H_
