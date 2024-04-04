// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_CHROME_APP_WINDOW_TRACKER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_CHROME_APP_WINDOW_TRACKER_ASH_H_

#include <map>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chromeos/crosapi/mojom/app_window_tracker.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace crosapi {

// Implements the crosapi interface for ChromeAppWindowTracker Ash-Chrome on the
// UI thread. This is responsible for tracking windows associated with chrome
// apps, and populating the shelf with their metadata.
//
// There are two different IPC channels whose information must be combined to
// create a shelf item: aura::Window* via Wayland and app_id via crosapi. This
// class tracks both pending aura::Windows and pending app_ids. Once both are
// present, this class hands off tracking/ownership to
// StandaloneBrowserExtensionAppShelfItemDelegate.
class ChromeAppWindowTrackerAsh : public mojom::AppWindowTracker,
                                  public aura::EnvObserver,
                                  public aura::WindowObserver {
 public:
  ChromeAppWindowTrackerAsh();
  ChromeAppWindowTrackerAsh(const ChromeAppWindowTrackerAsh&) = delete;
  ChromeAppWindowTrackerAsh& operator=(const ChromeAppWindowTrackerAsh&) =
      delete;
  ~ChromeAppWindowTrackerAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::AppWindowTracker> receiver);

  // mojom::AppWindowTracker overrides:
  void OnAppWindowAdded(const std::string& app_id,
                        const std::string& window_id) override;
  void OnAppWindowRemoved(const std::string& app_id,
                          const std::string& window_id) override;

  // aura::EnvObserver overrides:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override;

  // public and virtual for testing.
  // Given a newly created chrome app window, either creates a shelf item
  // controller or updates the existing shelf item controller.
  virtual void UpdateShelf(const std::string& app_id, aura::Window* window);

 protected:
  // Holds metadata associated with a chrome app window. Once both pieces of
  // metadata are available, this class has sufficient information to either
  // create a StandaloneBrowserExtensionAppShelfItemController, or else update
  // an existing one.
  struct WindowData {
    std::string app_id;
    raw_ptr<aura::Window> window = nullptr;
  };

 private:
  // If both pieces of metadata are present, then stop tracking the window as
  // it's no longer pending.
  void CheckWindowNoLongerPending(const std::string& window_id);

  // A map from |window_id| to WindowData. Note that Lacros windows that are not
  // associated with chrome apps will also be present in this map. They will be
  // removed when the window is closed.
  std::map<std::string, WindowData> pending_window_ids_;

  // Observers aura::Env for newly created windows.
  base::ScopedObservation<aura::Env, EnvObserver> env_observation_{this};

  // Observes windows in |pending_window_ids_| for destruction.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};

  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::AppWindowTracker> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_CHROME_APP_WINDOW_TRACKER_ASH_H_
