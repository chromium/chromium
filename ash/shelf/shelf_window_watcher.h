// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_WINDOW_WATCHER_H_
#define ASH_SHELF_SHELF_WINDOW_WATCHER_H_

#include <set>

#include "ash/shell_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

class ShelfModel;

// ShelfWindowWatcher manages ShelfItems for dialogs in the default container
// with valid ShelfItemType and ShelfID window properties (ie. task manager).
// ShelfWindowWatcher also tracks the active shelf item via window activation.
class ShelfWindowWatcher : public ::wm::ActivationChangeObserver,
                           public ShellObserver {
 public:
  explicit ShelfWindowWatcher(ShelfModel* model);
  ~ShelfWindowWatcher() override;

  static const char kDefaultShelfIdPrefix[];

 private:
  // Observes for windows being added to a root window's default container.
  class ContainerWindowObserver : public aura::WindowObserver {
   public:
    explicit ContainerWindowObserver(ShelfWindowWatcher* window_watcher);
    ~ContainerWindowObserver() override;

   private:
    // aura::WindowObserver:
    void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
    void OnWindowDestroying(aura::Window* window) override;

    ShelfWindowWatcher* window_watcher_;

    DISALLOW_COPY_AND_ASSIGN(ContainerWindowObserver);
  };

  // Observes individual user windows to detect when they are closed or when
  // their shelf item properties have changed.
  class UserWindowObserver : public aura::WindowObserver {
   public:
    explicit UserWindowObserver(ShelfWindowWatcher* window_watcher);
    ~UserWindowObserver() override;

   private:
    // aura::WindowObserver:
    void OnWindowPropertyChanged(aura::Window* window,
                                 const void* key,
                                 intptr_t old) override;
    void OnWindowDestroying(aura::Window* window) override;
    void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
    void OnWindowTitleChanged(aura::Window* window) override;

    ShelfWindowWatcher* window_watcher_;

    DISALLOW_COPY_AND_ASSIGN(UserWindowObserver);
  };

  // Creates a ShelfItem for |window|.
  void AddShelfItem(aura::Window* window);

  // Removes a ShelfItem for |window|.
  void RemoveShelfItem(aura::Window* window);

  // Cleans up observers on |container|.
  void OnContainerWindowDestroying(aura::Window* container);

  // Adds a shelf item for new windows added to the default container that have
  // valid ShelfItemType and ShelfID property values.
  void OnUserWindowAdded(aura::Window* window);

  // Adds, updates or removes the shelf item based on a property change.
  void OnUserWindowPropertyChanged(aura::Window* window);

  // Removes the shelf item when a window closes.
  void OnUserWindowDestroying(aura::Window* window);

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;

  ShelfModel* model_;

  ContainerWindowObserver container_window_observer_{this};
  UserWindowObserver user_window_observer_{this};

  ScopedObserver<aura::Window, aura::WindowObserver>
      observed_container_windows_;
  ScopedObserver<aura::Window, aura::WindowObserver> observed_user_windows_;

  // The set of windows with shelf items managed by this ShelfWindowWatcher.
  std::set<aura::Window*> user_windows_with_items_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWindowWatcher);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_WINDOW_WATCHER_H_
