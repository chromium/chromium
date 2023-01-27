// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_EXTENSION_WATCHER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_EXTENSION_WATCHER_H_

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"

class ProfileManager;

namespace performance_manager {

// Sets the `PageType::kExtension` type on `PageNodes` hosting extension
// background pages.
class ExtensionWatcher : public ProfileManagerObserver,
                         public extensions::ProcessManagerObserver {
 public:
  ExtensionWatcher();
  ~ExtensionWatcher() override;

 private:
  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // extensions::ProcessManagerObserver:
  void OnBackgroundHostCreated(extensions::ExtensionHost* host) override;
  void OnProcessManagerShutdown(extensions::ProcessManager* manager) override;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};

  base::ScopedMultiSourceObservation<extensions::ProcessManager,
                                     extensions::ProcessManagerObserver>
      extension_process_manager_observation_{this};
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_EXTENSION_WATCHER_H_
