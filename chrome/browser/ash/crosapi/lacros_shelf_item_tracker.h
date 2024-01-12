// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_CROSAPI_LACROS_SHELF_ITEM_TRACKER_H_
#define CHROME_BROWSER_ASH_CROSAPI_LACROS_SHELF_ITEM_TRACKER_H_

#include <map>
#include <optional>
#include <string>

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chromeos/crosapi/mojom/lacros_shelf_item_tracker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {
class ShelfItemDelegate;
struct ShelfID;
}  // namespace ash

namespace crosapi {

// Receives Lacros windows and tracks them.
// Also tracks the destruction of the windows.
// Based on the tracked windows, this class can add new items to the Shelf and
// remove existing items from the Shelf.
class LacrosShelfItemTracker : public mojom::LacrosShelfItemTracker,
                               public aura::EnvObserver,
                               public aura::WindowObserver {
 public:
  LacrosShelfItemTracker();
  ~LacrosShelfItemTracker() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::LacrosShelfItemTracker> receiver);

  // mojom::LacrosShelfItemTracker override:
  void AddOrUpdateWindow(mojom::WindowDataPtr window_data) override;

  // aura::WindowObserver override:
  void OnWindowDestroying(aura::Window* window) override;

  // aura::EnvObserver override:
  void OnWindowInitialized(aura::Window* window) override;

 protected:
  // Protected virtual for testing.
  virtual ash::ShelfItemDelegate* AddOrUpdateShelfItemAndReturnDelegate(
      mojom::WindowDataPtr window_data);

  // Protected virtual for testing.
  virtual void RemoveFromShelf(const ash::ShelfID& shelf_id);

 private:
  struct WindowInfo {
    WindowInfo();
    ~WindowInfo();
    raw_ptr<aura::Window> window;
    std::optional<mojom::WindowDataPtr> window_data;
  };

  // If |window_id| is present in both |all_lacros_windows_| and
  // |tracked_windows_| then we add the window to shelf.
  void MaybeAddToShelf(const std::string& window_id);

  // Virtual for testing.
  virtual std::unique_ptr<ash::ShelfItemDelegate> CreateDelegateByInstanceType(
      const ash::ShelfID& shelf_id,
      mojom::InstanceType instance_type);

  // A map for all windows whose app id starts with |LacrosAppIdPrefix| when
  // initialized.
  std::map<std::string, WindowInfo> lacros_prefixed_windows_;

  // A map for all windows associated with a Shelf item this instance has
  // created. Key: unique ID for a Shelf item. Value: a set of all window ID
  // associated with the item.
  std::map<std::string, std::set<std::string>> shelf_item_windows_;

  // Observes all Lacros windows for destruction.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      lacros_prefixed_windows_observations_{this};

  // Observes aura::Env for newly created windows.
  base::ScopedObservation<aura::Env, EnvObserver> env_observation_{this};

  // Supports multiple receivers.
  mojo::ReceiverSet<mojom::LacrosShelfItemTracker> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_LACROS_SHELF_ITEM_TRACKER_H_
