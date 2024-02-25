// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_MANAGER_OBSERVER_H_

#include "base/observer_list_types.h"

class Profile;
enum class ProfileKeepAliveOrigin;

class ProfileManagerObserver : public base::CheckedObserver {
 public:
  // Called when a Profile is added to the manager, the profile is fully created
  // and registered with the ProfileManager. This is only called for normal
  // (on-the-record) profiles as the ProfileManager doesn't own the OTR profile.
  // For OTR profile creation, see
  // ProfileObserver::OnOffTheRecordProfileCreated().
  // Unlike ProfileAttributesStorage::Observer::OnProfileAdded(), which is only
  // called when a new user is first created, this is called once on every run
  // of Chrome, provided that the Profile is in use.
  virtual void OnProfileAdded(Profile* profile) {}

  // Called when the user deletes a profile and all associated data should be
  // erased. Note that the Profile object will not be destroyed until Chrome
  // shuts down. See https://crbug.com/88586
  virtual void OnProfileMarkedForPermanentDeletion(Profile* profile) {}

  // Called when the profile manager is destroying. As the `ProfileManager` is
  // owned by the `BrowserProcessImpl`, this will only be called during
  // shutdown.
  virtual void OnProfileManagerDestroying() {}

  // Called when a keep alive is added to a profile, with the respective keep
  // alive origin.
  virtual void OnKeepAliveAdded(const Profile* profile,
                                ProfileKeepAliveOrigin keep_alive_origin) {}

  // Called when a new profile is being created. This is called at earlier
  // stage of Profile creation, i.e., we should not assume Profile
  // initialization is completed.
  // In most cases, `OnProfileAdded()` is what you want, instead.
  virtual void OnProfileCreationStarted(Profile* profile) {}
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_MANAGER_OBSERVER_H_
