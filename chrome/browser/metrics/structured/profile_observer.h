// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_PROFILE_OBSERVER_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_PROFILE_OBSERVER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

class ProfileManager;
class Profile;

namespace metrics::structured {

// Wrapper class for general profile observation in Structured Metrics. It
// handles validation and setting up the observation with the ProfileManager.
//
// This is providing logic for checking whether the profile is a regular
// profile.
class ProfileObserver : public ProfileManagerObserver {
 public:
  ProfileObserver();

  ~ProfileObserver() override;

  // Called once the profile has been validated.
  //
  // The profile has been validated to be a regular profile and the observes can
  // now be notified.
  virtual void ProfileAdded(const Profile& profile) = 0;

 private:
  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_PROFILE_OBSERVER_H_
