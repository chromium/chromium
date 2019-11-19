// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_CONTROLLER_H_
#define ASH_WM_DESKS_DESKS_CONTROLLER_H_

#include <memory>
#include <queue>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/desks_helper.h"
#include "ash/session/session_observer.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/account_id/account_id.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class Desk;

// Defines a controller for creating, destroying and managing virtual desks and
// their windows.
class ASH_EXPORT DesksController : public DesksHelper,
                                   public wm::ActivationChangeObserver,
                                   public SessionObserver {
 public:
  class Observer {
   public:
    // Called when |desk| has been created and added to
    // `DesksController::desks_`.
    virtual void OnDeskAdded(const Desk* desk) = 0;

    // Called when |desk| has been removed from `DesksController::desks_`.
    // However |desk| is kept alive temporarily and will be destroyed after all
    // observers have been notified with this.
    virtual void OnDeskRemoved(const Desk* desk) = 0;

    // Called when the |activated| desk gains activation from the |deactivated|
    // desk.
    virtual void OnDeskActivationChanged(const Desk* activated,
                                         const Desk* deactivated) = 0;

    // Called when the desk switch animations is launching.
    virtual void OnDeskSwitchAnimationLaunching() = 0;

    // Called when the desk switch animations on all root windows finish.
    virtual void OnDeskSwitchAnimationFinished() = 0;

   protected:
    virtual ~Observer() = default;
  };

  DesksController();
  ~DesksController() override;

  // Convenience method for returning the DesksController instance. The actual
  // instance is created and owned by Shell.
  static DesksController* Get();

  const std::vector<std::unique_ptr<Desk>>& desks() const { return desks_; }

  const Desk* active_desk() const { return active_desk_; }

  // Destroys any pending animations in preparation for shutdown.
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

  // Returns the next / previous desks to the currently active desk. Returns
  // nullptr if the active desk is the first on the left or the last on the
  // right, and previous and next desks are requested respectively.
  Desk* GetNextDesk() const;
  Desk* GetPreviousDesk() const;

  // Creates a new desk. CanCreateDesks() must be checked before calling this.
  void NewDesk(DesksCreationRemovalSource source);

  // Removes and deletes the given |desk|. |desk| must already exist, and
  // CanRemoveDesks() must be checked before this.
  // This will trigger the `DeskRemovalAnimation` if the active desk is being
  // removed outside of overview.
  void RemoveDesk(const Desk* desk, DesksCreationRemovalSource source);

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

  // Moves |window| (which must belong to the currently active desk) to
  // |target_desk| (which must be a different desk). If |window| is minimized,
  // it will be unminimized after it's moved to |target_desk|.
  // Returns true on success, false otherwise (e.g. if |window| doesn't belong
  // to the active desk).
  bool MoveWindowFromActiveDeskTo(aura::Window* window,
                                  Desk* target_desk,
                                  DesksMoveWindowFromActiveDeskSource source);

  // Called explicitly by the RootWindowController when a root window has been
  // added or about to be removed in order to update all the available desks.
  void OnRootWindowAdded(aura::Window* root_window);
  void OnRootWindowClosing(aura::Window* root_window);

  // DesksHelper:
  bool BelongsToActiveDesk(aura::Window* window) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gaining_active,
                          aura::Window* losing_active) override;
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

 private:
  class DeskAnimationBase;
  class DeskActivationAnimation;
  class DeskRemovalAnimation;

  void OnAnimationFinished(DeskAnimationBase* animation);

  bool HasDesk(const Desk* desk) const;

  int GetDeskIndex(const Desk* desk) const;

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

  // Returns the desk to which |window| belongs or nullptr if it doesn't belong
  // to any desk.
  const Desk* FindDeskOfWindow(aura::Window* window) const;

  // Reports the number of windows per each available desk. This called when a
  // desk switch occurs.
  void ReportNumberOfWindowsPerDeskHistogram() const;

  void ReportDesksCountHistogram() const;

  std::vector<std::unique_ptr<Desk>> desks_;

  Desk* active_desk_ = nullptr;

  // The account ID of the current active user.
  AccountId current_account_id_;

  // Stores the per-user last active desk index.
  base::flat_map<AccountId, int> user_to_active_desk_index_;

  // True when desks addition, removal, or activation change are in progress.
  // This can be checked when overview mode is active to avoid exiting overview
  // mode as a result of desks modifications.
  bool are_desks_being_modified_ = false;

  // List of on-going desks animations.
  std::vector<std::unique_ptr<DeskAnimationBase>> animations_;

  // A free list of desk container IDs to be used for newly-created desks. New
  // desks pops from this queue and removed desks's associated container IDs are
  // re-pushed on this queue.
  std::queue<int> available_container_ids_;

  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(DesksController);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_CONTROLLER_H_
