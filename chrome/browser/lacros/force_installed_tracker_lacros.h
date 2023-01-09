// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_FORCE_INSTALLED_TRACKER_LACROS_H_
#define CHROME_BROWSER_LACROS_FORCE_INSTALLED_TRACKER_LACROS_H_

#include "base/time/time.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chromeos/crosapi/mojom/force_installed_tracker.mojom.h"

// Provider of APIs to track the installation status of extensions (listed in
// ExtensionInstallForceList policy). This class is also responsible for
// notifying ash-chrome when the status is changed.
class ForceInstalledTrackerLacros
    : public extensions::ForceInstalledTracker::Observer {
 public:
  ForceInstalledTrackerLacros();
  ForceInstalledTrackerLacros(const ForceInstalledTrackerLacros&) = delete;
  ForceInstalledTrackerLacros& operator=(const ForceInstalledTrackerLacros&) =
      delete;
  ~ForceInstalledTrackerLacros() override;

  // Start the force-installed tracker. ForceInstalledTracker mojom APIs
  // will be called if the corresponding observer methods are triggered.
  void Start();

  // extensions::ForceInstalledTracker::Observer:
  void OnForceInstalledExtensionsReady() override;

 protected:
  // Determine whether the current service is available or not by checking the
  // API version.
  // Virtual for testing.
  virtual bool IsServiceAvailable() const;

  // Get the `extensions::ForceInstalledTracker` instance for the primary user
  // profile. The extension service should be launched before this method is
  // called.
  // Virtual for testing.
  virtual extensions::ForceInstalledTracker*
  GetExtensionForceInstalledTracker();

 private:
  base::ScopedObservation<extensions::ForceInstalledTracker,
                          extensions::ForceInstalledTracker::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_LACROS_FORCE_INSTALLED_TRACKER_LACROS_H_
