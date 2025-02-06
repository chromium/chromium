// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_ENABLING_H_
#define CHROME_BROWSER_GLIC_GLIC_ENABLING_H_

#include "base/types/expected.h"

class Profile;

namespace glic {
// Enum for signalling the reason why glic was not enabled.
enum class GlicEnabledStatus {
  kEnabled = 0,
  kGlicFeatureFlagDisabled = 1,
  kTabstripComboButtonDisabled = 2,
  kMaxValue = 2
};
}  // namespace glic

// This class provides a central location for checking if GLIC is enabled. It
// allows for future expansion to include other ways the feature may be disabled
// such as based on user preferences or system settings.
//
// There are multiple notions of "enabled". The highest level is
// IsEnabledByFlags which controls whether any global-Glic infrastructure is
// created. If flags are off, nothing Glic-related should be created.
//
// If flags are enabled, various global objects are created as well as a
// GlicKeyedService for each "eligible" profile. Eligible profiles exclude
// incognito, guest mode, system profile, etc. i.e. include only real user
// profiles. An eligible profile will create various Glic decorations and views
// for the profile's browser windows, regardless of whether Glic is actually
// "enabled" for the given profile. If disabled, those decorations should remain
// inert.  The GlicKeyedService is created for all eligible profiles so it can
// listen for changes to prefs which control the per-profile Glic-Enabled state.
//
// Finally, an eligible profile may be Glic-Enabled. In this state, Glic UI is
// visible and usable by the user. This state can change at runtime so Glic
// entry points should depend on this state.
class GlicEnabling {
 public:
  // Returns whether the global Glic feature is enabled for Chrome. This status
  // will not change at runtime.
  static bool IsEnabledByFlags();

  // Some profiles - such as incognito, guest, system profile, etc. - are never
  // eligible to use Glic. This function returns true if a profile is eligible
  // for Glic, that is, it can potentially be enabled, regardless of whether it
  // is currently enabled or not. Always returns false if IsEnabledByFlags is
  // off. This will never change for a given profile.
  static bool IsProfileEligible(const Profile* profile);

  // Returns true if the given profile has Glic enabled. True implies that
  // IsEnabledByFlags is on and IsProfileEligible(profile) is also true. This
  // value can change at runtime.
  static bool IsEnabledForProfile(const Profile* profile);

 private:
  // Private helper function that returns enabled status for fine grain logging
  // if desired.
  static glic::GlicEnabledStatus CheckEnabling();
};

#endif  // CHROME_BROWSER_GLIC_GLIC_ENABLING_H_
