// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_

#include <set>
#include <string>

#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

class PrefService;
class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionRegistry;

// Used to track installation of force-installed extensions for the profile
// and report stats to UMA.
// ExtensionService owns this class and outlives it.
class InstallationTracker : public ExtensionRegistryObserver {
 public:
  InstallationTracker(ExtensionRegistry* registry,
                      Profile* profile,
                      std::unique_ptr<base::OneShotTimer> timer =
                          std::make_unique<base::OneShotTimer>());

  ~InstallationTracker() override;

  // ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;

 private:
  // Loads list of force-installed extensions if available.
  void OnForcedExtensionsPrefChanged();

  // If |succeeded| report time elapsed for extensions load,
  // otherwise amount of not yet loaded extensions and reasons
  // why they were not installed.
  void ReportResults(bool succeeded);

  // Unowned, but guaranteed to outlive this object.
  ExtensionRegistry* registry_;
  Profile* profile_;
  // Unowned, but guaranteed to outlive this object.
  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;

  // Moment when the class was initialized.
  base::Time start_time_;

  // Set of all extensions requested to be force installed.
  std::set<std::string> forced_extensions_;

  // Set of not yet loaded force installed extensions.
  std::set<std::string> pending_forced_extensions_;

  // Tracks whether stats were already reported for the session.
  bool reported_ = false;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver> observer_;

  // Tracks installation reporting timeout.
  std::unique_ptr<base::OneShotTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(InstallationTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_
