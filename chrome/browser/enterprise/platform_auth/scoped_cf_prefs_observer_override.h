// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_SCOPED_CF_PREFS_OBSERVER_OVERRIDE_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_SCOPED_CF_PREFS_OBSERVER_OVERRIDE_H_

#include <memory>

#include "base/functional/callback_forward.h"

namespace enterprise_auth {

class CFPreferencesObserver;

// A RAII helper to override the creation of `CFPreferencesObserver` in tests.
//
// When instantiated, it registers a factory callback that will be used to
// create `CFPreferencesObserver` instances instead of the default production
// logic. The override is automatically removed when this object goes out of
// scope.
class ScopedCFPreferenceObserverOverride {
 public:
  explicit ScopedCFPreferenceObserverOverride(
      base::RepeatingCallback<std::unique_ptr<CFPreferencesObserver>()>
          cf_prefs_observer_override);
  ~ScopedCFPreferenceObserverOverride();

 private:
  static bool instance_exists_;
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_SCOPED_CF_PREFS_OBSERVER_OVERRIDE_H_
