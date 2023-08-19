// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_MANAGER_ANDROID_H_
#define CHROME_BROWSER_PROFILES_PROFILE_MANAGER_ANDROID_H_

#include <jni.h>

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

class ProfileManagerAndroid : public ProfileManagerObserver {
 public:
  explicit ProfileManagerAndroid(ProfileManager* manager);
  ~ProfileManagerAndroid() override;

  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;

 private:
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_MANAGER_ANDROID_H_
