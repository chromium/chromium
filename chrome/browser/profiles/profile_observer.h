// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_OBSERVER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_OBSERVER_H_

#include "base/observer_list_types.h"

class Profile;

class ProfileObserver : public base::CheckedObserver {
 public:
  // The observed profile has spawned a new off the record profile (whether
  // owned or independent). This is called just before ownership of the new
  // OTR profile is taken by the original profile or the
  // IndependentOTRProfileManager, so |original_profile->GetOffRecordProfile()|
  // should not be called, but |off_the_record->GetOriginalProfile()| will
  // return |original_profile|.
  virtual void OnOffTheRecordProfileCreated(Profile* off_the_record) {}

  // The observed profile will be destroyed soon. All KeyedServices are still
  // valid. The shutdown sequence for a profile is:
  //   1. BrowserContext related shutdown occurs via
  //      BrowserContext::NotifyWillBeDestroyed()
  //   2. OnProfileWillBeDestroyed called for |profile|
  //   3. OTR profile (if any) goes through shutdown in same sequence
  //   4. KeyedServices are shut down for |profile|
  virtual void OnProfileWillBeDestroyed(Profile* profile) {}
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_OBSERVER_H_
