// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_PROFILE_CONFIGURATION_H_
#define CHROME_BROWSER_GLIC_GLIC_PROFILE_CONFIGURATION_H_

#include "base/memory/raw_ref.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class Profile;

namespace glic {

// This class manages interaction with the prefs system for Glic per-profile
// settings. For launcher settings, which are per-installation, see
// GlicLauncherConfiguration.
class GlicProfileConfiguration {
 public:
  explicit GlicProfileConfiguration(Profile* profile);
  ~GlicProfileConfiguration();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  bool IsEnabledByPolicy() const;
  bool HasCompletedFre() const;

 private:
  void OnEnabledByPolicyChanged();

  // raw_ref since this class is owned by a keyed service tied to the Profile it
  // will be outlived by it.
  const raw_ref<Profile> profile_;

  PrefChangeRegistrar pref_registrar_;
};
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_PROFILE_CONFIGURATION_H_
