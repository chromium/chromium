// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_PREFS_ANDROID_ACCESSIBILITY_PREFS_CONTROLLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_PREFS_ANDROID_ACCESSIBILITY_PREFS_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "components/prefs/pref_service.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace accessibility {

// AccessibilityPrefsController is for managing accessibility related prefs for
// the browser.
class AccessibilityPrefsController {
 public:
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
  explicit AccessibilityPrefsController(PrefService* local_state_prefs);
  ~AccessibilityPrefsController();

 private:
  void OnAccessibilityPerformanceFilteringAllowedChanged();

  raw_ptr<PrefService> local_state_prefs_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace accessibility

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_PREFS_ANDROID_ACCESSIBILITY_PREFS_CONTROLLER_H_
