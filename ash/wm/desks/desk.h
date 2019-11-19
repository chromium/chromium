// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_H_
#define ASH_WM_DESKS_DESK_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
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
// the desk is in active, those containers are hidden.
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
  };

  explicit Desk(int associated_container_id);
  ~Desk();

  int container_id() const { return container_id_; }

  const std::vector<aura::Window*>& windows() const { return windows_; }

  bool is_active() const { return is_active_; }

  bool should_notify_content_changed() const {
    return should_notify_content_changed_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void OnRootWindowAdded(aura::Window* root);
  void OnRootWindowClosing(aura::Window* root);

  void AddWindowToDesk(aura::Window* window);
  void RemoveWindowFromDesk(aura::Window* window);

  base::AutoReset<bool> GetScopedNotifyContentChangedDisabler();

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

  // Moves all the windows on this desk to |target_desk|.
  void MoveWindowsToDesk(Desk* target_desk);

  // Moves a single |window| from this desk to |target_desk|. |window| must
  // belong to this desk.
  void MoveWindowToDesk(aura::Window* window, Desk* target_desk);

  aura::Window* GetDeskContainerForRoot(aura::Window* root) const;

  void NotifyContentChanged();

  // Updates the backdrop availability and visibility on the containers (on all
  // roots) associated with this desk.
  void UpdateDeskBackdrops();

 private:
  void MoveWindowToDeskInternal(aura::Window* window, Desk* target_desk);

  // The associated container ID with this desk.
  const int container_id_;

  // Windows tracked on this desk. Clients of the DesksController can use this
  // list when they're notified of desk change events.
  // TODO(afakhry): Change this to track MRU windows on this desk.
  std::vector<aura::Window*> windows_;

  // Maps all root windows to observer objects observing the containers
  // associated with this desk on those root windows.
  base::flat_map<aura::Window*, std::unique_ptr<DeskContainerObserver>>
      roots_to_containers_observers_;

  base::ObserverList<Observer> observers_;

  // TODO(afakhry): Consider removing this.
  bool is_active_ = false;

  // If false, observers won't be notified of desk's contents changes. This is
  // used to throttle those notifications when we add or remove many windows,
  // and we want to notify observers only once.
  bool should_notify_content_changed_ = true;

  DISALLOW_COPY_AND_ASSIGN(Desk);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_H_
