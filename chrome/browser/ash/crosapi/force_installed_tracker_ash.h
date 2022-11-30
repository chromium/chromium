// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FORCE_INSTALLED_TRACKER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FORCE_INSTALLED_TRACKER_ASH_H_

#include "base/observer_list.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chromeos/crosapi/mojom/force_installed_tracker.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi interface for tracking force-installed extensions.
class ForceInstalledTrackerAsh : public mojom::ForceInstalledTracker {
 public:
  ForceInstalledTrackerAsh();
  ForceInstalledTrackerAsh(const ForceInstalledTrackerAsh&) = delete;
  ForceInstalledTrackerAsh& operator=(const ForceInstalledTrackerAsh&) = delete;
  ~ForceInstalledTrackerAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::ForceInstalledTracker> receiver);

  // crosapi::mojom::ForceInstalledTracker:
  void OnForceInstalledExtensionsReady() override;

  // Add an observer.
  void AddObserver(extensions::ForceInstalledTracker::Observer* observer);

  // Remove an observer.
  void RemoveObserver(extensions::ForceInstalledTracker::Observer* observer);

  // Check whether all force-installed extensions are ready in Lacros. This
  // method returns true if ash-chrome has been notified.
  bool IsReady() const;

 private:
  bool is_ready_ = false;

  // Any number of crosapi clients can connect to this class.
  mojo::ReceiverSet<mojom::ForceInstalledTracker> receivers_;

  // Registered observers.
  base::ObserverList<extensions::ForceInstalledTracker::Observer> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FORCE_INSTALLED_TRACKER_ASH_H_
