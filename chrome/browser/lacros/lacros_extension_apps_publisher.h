// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_PUBLISHER_H_
#define CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_PUBLISHER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom-forward.h"

// This class tracks extension-based apps running in Lacros [e.g. chrome apps,
// aka v2 packaged apps]. This class forwards metadata about these apps to Ash,
// specifically StandaloneBrowserExtensionApps. This latter class is an
// AppService publisher, which in turn will glue these apps into the App Service
// infrastructure.
//
// The main sublety of this class is that it observes all Lacros profiles for
// installed/running extensions-based apps, whereas Ash itself will only ever
// run a single (login) profile. As such, this class is also responsible for
// [de]muxing responses to form this many : one relationship.
//
// This class only tracks apps added to non-incognito profiles. As such, it only
// needs to observe ProfileManager, not the profiles themselves for creation of
// incognito profiles.
class LacrosExtensionAppsPublisher : public ProfileManagerObserver {
 public:
  LacrosExtensionAppsPublisher();
  ~LacrosExtensionAppsPublisher() override;

  LacrosExtensionAppsPublisher(const LacrosExtensionAppsPublisher&) = delete;
  LacrosExtensionAppsPublisher& operator=(const LacrosExtensionAppsPublisher&) =
      delete;

  // This class does nothing until Initialize is called. This provides an
  // opportunity for this class and subclasses to finish constructing before
  // pointers get passed and used in inner classes.
  void Initialize();

 protected:
  // Publishes differential app updates to the app_service in Ash via crosapi.
  // Virtual for testing.
  virtual void Publish(std::vector<crosapi::mojom::AppPtr> apps);

 private:
  // An inner class that tracks extension-based app activity scoped to a
  // profile.
  class ProfileTracker;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // A map that maintains a single ProfileTracker per Profile.
  std::map<Profile*, std::unique_ptr<ProfileTracker>> profile_trackers_;

  // Scoped observer for the ProfileManager.
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

#endif  // CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_PUBLISHER_H_
