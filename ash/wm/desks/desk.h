// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_H_
#define ASH_WM_DESKS_DESK_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/aura/window_observer.h"

namespace ash {

class DeskContainerObserver;

// Represents a virtual desk, tracking the windows that belong to this desk.
// In a multi display scenario, desks span all displays (i.e. if desk 1 is
// active, it is active on all displays at the same time). Each display is
// associated with a |container_id_|. This is the ID of all the containers on
// all root windows that are associated with this desk. So the mapping is: one
// container per display (root window) per each desk.
// Those containers are parent windows of the windows that belong to the
// associated desk. When the desk is active, those containers are shown, when
// the desk is inactive, those containers are hidden.
class ASH_EXPORT Desk {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the desk's content change as a result of windows addition or
    // removal. Note that some windows are added or removed, but are not
    // considered a content change, such as the windows created by overview
    // mode.
    virtual void OnContentChanged() = 0;

    // Called when Desk is at the end of its destructor. Desk automatically
    // removes its Observers before calling this.
    virtual void OnDeskDestroyed(const Desk* desk) = 0;

    // Called when the desk's name changes.
    virtual void OnDeskNameChanged(const std::u16string& new_name) = 0;
  };

  // Tracks stacking order for a window that is visible on all desks. This is
  // used to support per-desk z-orders for all-desk windows. Entries are stored
  // in ascending `order`.
  struct AllDeskWindowStackingData {
    aura::Window* window = nullptr;
    // The z-order of the window.
    // Note: this is reversed from how child windows are ordered in
    // `aura::Window`, so an entry with `order == 0` means topmost.
    // Note: this order ignores non-normal windows.
    size_t order = 0;
  };

  explicit Desk(int associated_container_id, bool desk_being_restored = false);

  Desk(const Desk&) = delete;
  Desk& operator=(const Desk&) = delete;

  ~Desk();

  static void SetWeeklyActiveDesks(int weekly_active_desks);
  static int GetWeeklyActiveDesks();

  int container_id() const { return container_id_; }

  const base::GUID& uuid() const { return uuid_; }

  const std::vector<aura::Window*>& windows() const { return windows_; }

  const std::u16string& name() const { return name_; }

  bool is_active() const { return is_active_; }

  bool should_notify_content_changed() const {
    return should_notify_content_changed_;
  }

  bool is_name_set_by_user() const { return is_name_set_by_user_; }

  bool is_desk_being_removed() const { return is_desk_being_removed_; }
  void set_is_desk_being_removed(bool is_desk_being_removed) {
    is_desk_being_removed_ = is_desk_being_removed;
  }

  const base::Time& creation_time() const { return creation_time_; }
  void set_creation_time(base::Time creation_time) {
    creation_time_ = creation_time;
  }

  int first_day_visited() const { return first_day_visited_; }
  void set_first_day_visited(int first_day_visited) {
    first_day_visited_ = first_day_visited;
  }

  int last_day_visited() const { return last_day_visited_; }
  void set_last_day_visited(int last_day_visited) {
    last_day_visited_ = last_day_visited;
  }

  bool interacted_with_this_week() const { return interacted_with_this_week_; }
  void set_interacted_with_this_week(bool interacted_with_this_week) {
    interacted_with_this_week_ = interacted_with_this_week;
  }

  const base::flat_map<aura::Window*, std::vector<AllDeskWindowStackingData>>&
  all_desk_window_stacking() const {
    return all_desk_window_stacking_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void OnRootWindowAdded(aura::Window* root);
  void OnRootWindowClosing(aura::Window* root);

  void AddWindowToDesk(aura::Window* window);
  void RemoveWindowFromDesk(aura::Window* window);

  void WillRemoveWindowFromDesk(aura::Window* window);

  base::AutoReset<bool> GetScopedNotifyContentChangedDisabler();

  bool ContainsAppWindows() const;

  // Sets the desk's name to |new_name| and updates the observers.
  // |set_by_user| should be true if this name was given to the desk by the user
  // from its mini view in overview mode.
  void SetName(std::u16string new_name, bool set_by_user);

  // Prepares for the animation to activate this desk (i.e. this desk is not
  // active yet), by showing its containers on all root windows while setting
  // their opacities to 0. Calling Activate() during the animation will set the
  // opacities back to 1.
  void PrepareForActivationAnimation();

  // Activates this desk. All windows on this desk (if any) will become visible
  // (by means of showing this desk's associated containers on all root
  // windows). If |update_window_activation| is true, the most recently
  // used one of them will be activated.
  void Activate(bool update_window_activation);

  // Deactivates this desk. All windows on this desk (if any) will become hidden
  // (by means of hiding this desk's associated containers on all root windows),
  // If |update_window_activation| is true, the currently active window
  // on this desk will be deactivated.
  void Deactivate(bool update_window_activation);

  // Moves non-app overview windows (such as the Desks Bar, the Save Desk
  // button, and the "No Windows" label) from this desk to `target_desk`. This
  // allows us to keep the app windows in a closing desk until it is either
  // restored or destroyed, and also to move these windows back to the desk if
  // it is being restored in an active state.
  void MoveNonAppOverviewWindowsToDesk(Desk* target_desk);

  // In preparation for removing this desk, moves all the windows on this desk
  // to `target_desk` such that they become last in MRU order across all desks,
  // and they will be stacked at the bottom among the children of
  // `target_desk`'s container.
  // Note that from a UX stand point, removing a desk is viewed as the user is
  // now done with this desk, and therefore its windows are demoted and
  // deprioritized.
  void MoveWindowsToDesk(Desk* target_desk);

  // Moves a single |window| from this desk to |target_desk|, possibly moving it
  // to a different display, depending on |target_root|. |window| must belong to
  // this desk. If |unminimize| is true, the window is unminimized after it has
  // been moved.
  void MoveWindowToDesk(aura::Window* window,
                        Desk* target_desk,
                        aura::Window* target_root,
                        bool unminimize);

  aura::Window* GetDeskContainerForRoot(aura::Window* root) const;

  // Notifies observers that the desk's contents (list of application windows on
  // the desk) have changed.
  void NotifyContentChanged();

  // Update (even if overview is active) the backdrop availability and
  // visibility on the containers (on all roots) associated with this desk.
  void UpdateDeskBackdrops();

  // Records the lifetime of the desk based on its desk `index`. Should be
  // called when this desk is removed by the user.
  void RecordLifetimeHistogram(int index);

  // Returns whether the difference between |last_day_visited_| and the current
  // day is less than or equal to 1 or |last_day_visited_| is not set.
  bool IsConsecutiveDailyVisit() const;

  // Records the consecutive daily visits for |this| and resets
  // |last_day_visited_| and |first_day_visited_|. |last_day_visited_| must be
  // greater than or equal to |first_day_visited_|. If |being_removed| and
  // |is_active_| is true, then set |last_day_visited_| to the current day. This
  // accounts for cases where the user removes the active desk.
  void RecordAndResetConsecutiveDailyVisits(bool being_removed);

  // Gets all app windows on this desk that should be closed.
  std::vector<aura::Window*> GetAllAppWindows() const;

  // Gets desk windows including floated window (if any).
  // Note that floated window isn't tracked in `windows_` but still "belongs" to
  // this desk, it's stored in the float container and managed by
  // `FloatController`.
  std::vector<aura::Window*> GetAllAssociatedWindows() const;

  // Construct stacking data for windows that appear on all desks. This is done
  // just as a desk becomes inactive. The stacking data is then later used by
  // `RestackAllDeskWindows` if the desk becomes active again.
  void BuildAllDeskStackingData();

  // Uses the data from `BuildAllDeskStackingData` to re-stack all-desk
  // windows. This is a no-op if there is no data for the current desk.
  void RestackAllDeskWindows();

  // Called when an all-desk window has been added.
  void AddAllDeskWindow(aura::Window* window);

  // Called when an all-desk window has been removed (either from being closed
  // or not longer being all-desk).
  void RemoveAllDeskWindow(aura::Window* window);

 private:
  friend class DesksTestApi;

  void MoveWindowToDeskInternal(aura::Window* window,
                                Desk* target_desk,
                                aura::Window* target_root);

  // Returns true if per-desk z-order tracking is enabled and this desk is
  // currently *not* active. We do not track changes to the active desk since we
  // will rebuild stacking data when the desk becomes inactive (see
  // `BuildAllDeskStackingData`).
  bool ShouldUpdateAllDeskStackingData();

  // If `PrepareForActivationAnimation()` was called during the animation to
  // activate this desk, this function is called from `Activate()` to reset the
  // opacities of the containers back to 1, and returns true. Otherwise, returns
  // false.
  bool MaybeResetContainersOpacities();

  // If |this| has not been interacted with yet this week, increment
  // |g_weekly_active_desks| and set |this| to interacted with.
  void MaybeIncrementWeeklyActiveDesks();

  // Uniquely identifies the desk.
  const base::GUID uuid_;

  // The associated container ID with this desk.
  const int container_id_;

  // Windows tracked on this desk. Clients of the DesksController can use this
  // list when they're notified of desk change events.
  // TODO(afakhry): Change this to track MRU windows on this desk.
  std::vector<aura::Window*> windows_;

  // The name given to this desk.
  std::u16string name_;

  // Maps all root windows to observer objects observing the containers
  // associated with this desk on those root windows.
  base::flat_map<aura::Window*, std::unique_ptr<DeskContainerObserver>>
      roots_to_containers_observers_;

  base::ObserverList<Observer> observers_;

  bool is_active_ = false;

  // If false, observers won't be notified of desk's contents changes. This is
  // used to throttle those notifications when we add or remove many windows,
  // and we want to notify observers only once.
  bool should_notify_content_changed_ = true;

  // True if the `PrepareForActivationAnimation()` was called, and this desk's
  // containers are shown while their layer opacities are temporarily set to 0.
  bool started_activation_animation_ = false;

  // True if this desk's |name_| was set by the user, false if it's one of the
  // default automatically assigned names (e.g. "Desk 1", "Desk 2", ... etc.)
  // based on the desk's position in the list. Those default names change as
  // desks are added/removed if this desk changes position, whereas names that
  // are set by the user don't change.
  bool is_name_set_by_user_ = false;

  // True if the desk is being removed.
  bool is_desk_being_removed_ = false;

  // The time this desk was created at. Used to record desk lifetimes.
  base::Time creation_time_;

  // The first and last day this desk was visited in a string of consecutive
  // daily visits. These values store the time in days since local epoch as an
  // integer. They are used to record the number of consecutive daily visits to
  // |this|. If their values are -1, then |this| has not been visited since
  // creation.
  int first_day_visited_ = -1;
  int last_day_visited_ = -1;

  // Stacking data for all all-desk windows. Ordered from topmost and
  // down. Keyed by root window.
  base::flat_map<aura::Window*, std::vector<AllDeskWindowStackingData>>
      all_desk_window_stacking_;

  // Tracks whether |this| has been interacted with this week. This value is
  // reset by the DesksController.
  bool interacted_with_this_week_ = false;

  // A timer for marking |this| as interacted with only if the user remains on
  // |this| for a brief period of time.
  base::OneShotTimer active_desk_timer_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_H_
