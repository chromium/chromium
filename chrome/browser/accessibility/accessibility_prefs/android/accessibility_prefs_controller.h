// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_PREFS_ANDROID_ACCESSIBILITY_PREFS_CONTROLLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_PREFS_ANDROID_ACCESSIBILITY_PREFS_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "components/prefs/pref_service.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class Profile;

namespace accessibility {

// AccessibilityPrefsController is for managing accessibility related prefs for
// the browser.
class AccessibilityPrefsController : public ProfileManagerObserver {
 public:
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  explicit AccessibilityPrefsController(PrefService* local_state_prefs);
  ~AccessibilityPrefsController() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

 private:
  void Init();
#if BUILDFLAG(IS_ANDROID)
  void OnAccessibilityPerformanceFilteringAllowedChanged();
#endif
  void OnRendererAccessibilityEnabledChanged();

  raw_ptr<PrefService> local_state_prefs_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<PrefChangeRegistrar> profile_pref_change_registrar_;
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
};

}  // namespace accessibility

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_PREFS_ANDROID_ACCESSIBILITY_PREFS_CONTROLLER_H_
