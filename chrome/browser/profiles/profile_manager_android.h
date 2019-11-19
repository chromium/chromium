// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_MANAGER_ANDROID_H_
#define CHROME_BROWSER_PROFILES_PROFILE_MANAGER_ANDROID_H_

#include <jni.h>

#include "chrome/browser/profiles/profile_manager_observer.h"

class ProfileManagerAndroid : public ProfileManagerObserver {
 public:
  ProfileManagerAndroid();
  ~ProfileManagerAndroid() override;

  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_MANAGER_ANDROID_H_
