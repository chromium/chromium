// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_PUBLISHER_H_
#define CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_PUBLISHER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "chrome/browser/lacros/for_which_extension_type.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/mojom/types.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

// This class tracks Chrome apps [i.e. extension-based apps, AKA v2 packaged
// apps] or extensions running in Lacros, and forwards metadata about these
// these apps / extensions to two classes in Ash:
//
// (1) StandaloneBrowserExtensionApps is an AppService publisher, which in turn
// will glue these Chrome apps / extensions into the App Service infrastructure.
//
// (2) A yet unnamed class is responsible for tracking windows and gluing them
// into the ash shelf.
//
// The main sublety of this class is that it observes all Lacros profiles for
// installed/running Chrome apps / extensions, whereas Ash itself will only ever
// run a single (login) profile. As such, this class is also responsible for
// muxing responses to form this many : one relationship.
//
// This class only tracks Chrome apps / extensions added to non-incognito
// profiles. As such, it only needs to observe ProfileManager, not the profiles
// themselves for creation of incognito profiles.
//
// See LacrosExtensionAppsController for the class responsible for receiving
// events from Ash.
class LacrosExtensionAppsPublisher : public ProfileManagerObserver {
 public:
  static std::unique_ptr<LacrosExtensionAppsPublisher> MakeForChromeApps();
  static std::unique_ptr<LacrosExtensionAppsPublisher> MakeForExtensions();

  // Should not be directly called. Normally this should be private, but then
  // this would require friending std::make_unique.
  explicit LacrosExtensionAppsPublisher(
      const ForWhichExtensionType& which_type);
  ~LacrosExtensionAppsPublisher() override;

  LacrosExtensionAppsPublisher(const LacrosExtensionAppsPublisher&) = delete;
  LacrosExtensionAppsPublisher& operator=(const LacrosExtensionAppsPublisher&) =
      delete;

  // This class does nothing until Initialize is called. This provides an
  // opportunity for this class and subclasses to finish constructing before
  // pointers get passed and used in inner classes.
  void Initialize();

  // Updates app's window mode and republishes the app.
  void UpdateAppWindowMode(const std::string& app_id,
                           apps::WindowMode window_mode);

  // Exposed so that LacrosExtensionAppsController can initialize its receiver.
  mojo::Remote<crosapi::mojom::AppPublisher>& publisher() { return publisher_; }

 protected:
  // Publishes differential updates to the App_Service in Ash via crosapi.
  // Virtual for testing.
  virtual void Publish(std::vector<apps::AppPtr> apps);

  // Notifies Ash's app window tracker of an app window construction. For Chrome
  // apps only. Virtual for testing.
  virtual void OnAppWindowAdded(const std::string& app_id,
                                const std::string& window_id);

  // Notifies Ash's app window tracker of an app window destruction. For Chrome
  // apps only. Virtual for testing.
  virtual void OnAppWindowRemoved(const std::string& app_id,
                                  const std::string& window_id);

  // Virtual for testing. Sets up the crosapi connection. Returns false on
  // failure.
  virtual bool InitializeCrosapi();

 private:
  // An inner class that tracks Chrome app / extension activity scoped to a
  // profile.
  class ProfileTracker;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // State to decide which extension type (e.g., Chrome Apps vs. Extensions)
  // to support.
  const ForWhichExtensionType which_type_;

  // A map that maintains a single ProfileTracker per Profile.
  std::map<Profile*, std::unique_ptr<ProfileTracker>> profile_trackers_;

  // Mojo endpoint that's responsible for sending app publisher messages to Ash.
  mojo::Remote<crosapi::mojom::AppPublisher> publisher_;

  // Scoped observer for the ProfileManager.
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

#endif  // CHROME_BROWSER_LACROS_LACROS_EXTENSION_APPS_PUBLISHER_H_
