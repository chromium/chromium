// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_
#define CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/glic_keyed_service.h"

class Profile;

namespace glic {

// GlicProfileManager is a GlobalFeature that is responsible for determining
// which profile to use for launching the glic panel and for ensuring just one
// panel is shown across all profiles.
class GlicProfileManager {
 public:
  GlicProfileManager();
  ~GlicProfileManager();

  // Returns the global instance.
  static GlicProfileManager* GetInstance();

  GlicProfileManager(const GlicProfileManager&) = delete;
  GlicProfileManager& operator=(const GlicProfileManager&) = delete;

  // Close the Glic window, if one exists.
  void CloseGlicWindow();

  // Return the profile that should be used to open glic. May be null if there
  // is no eligible profile.
  Profile* GetProfileForLaunch();

  // Called by GlicKeyedService.
  void OnUILaunching(GlicKeyedService* glic);

 private:
  base::WeakPtr<GlicKeyedService> active_glic_;
};
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_PROFILE_MANAGER_H_
