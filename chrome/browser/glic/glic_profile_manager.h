// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_
#define CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"

class Profile;

namespace glic {

// GlicProfileManager is a GlobalFeature that manages multi-profile Glic state.
// Among other things it is used for determining which profile to launch from an
// OS Entry point and ensuring that just one panel is shown across all profiles.
class GlicProfileManager : public ProfileManagerObserver,
                           public ProfileObserver {
 public:

  GlicProfileManager();
  ~GlicProfileManager() override;

  // Returns the global instance.
  static GlicProfileManager* GetInstance();

  GlicProfileManager(const GlicProfileManager&) = delete;
  GlicProfileManager& operator=(const GlicProfileManager&) = delete;

  // Return the profile that should be used to open glic. May be null if there
  // is no eligible profile.
  Profile* GetProfileForLaunch() const;

  // Used in GlicMultiInstance. Called when a GlicFloatingUi is shown and closes
  // any previous existing floating glic. Resets the tracked glic if a null
  // profile is passed.
  void SetCurrentDetachedGlic(Profile* profile);

  // Called by GlobalFeatures.
  void Shutdown();

  // Opens the panel if the "glic-open-on-startup" command line switch was used
  // and glic has not already opened like this.
  void MaybeAutoOpenGlicPanel();

  void ShowProfilePicker();

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* profile) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Static in order to permit setting forced values before the manager is
  // constructed.
  static void ForceProfileForLaunchForTesting(std::optional<Profile*> profile);

  base::WeakPtr<GlicProfileManager> GetWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(GlicProfileManagerDidSelectProfileTest,
                           DidSelectProfile_NoConsent);
  FRIEND_TEST_ALL_PREFIXES(GlicProfileManagerDidSelectProfileTest,
                           DidSelectProfile_Consented);

  // Callback from ProfilePicker::Show().
  void DidSelectProfile(Profile* profile);

  // Used in GlicMultiInstance to track the GlicKeyedService of the current
  // detached glic, if any.
  base::WeakPtr<GlicKeyedService> current_detached_glic_;
  bool did_auto_open_ = false;

  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observations_{this};

  base::WeakPtrFactory<GlicProfileManager> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_
