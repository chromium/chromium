// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_SCOPED_CF_PREFS_OBSERVER_OVERRIDE_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_SCOPED_CF_PREFS_OBSERVER_OVERRIDE_H_

#include <CoreFoundation/CoreFoundation.h>

#include <memory>
#include <string>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"

namespace enterprise_auth {

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

// Serves the MDM config provided by `read_config` instead of reading it from
// the OS. The update callback the prefs handler subscribes with is stored in
// `on_update`, so tests can simulate a config change by running it.
class FakeCFPreferencesObserver : public CFPreferencesObserver {
 public:
  FakeCFPreferencesObserver(
      base::RepeatingClosure& on_update,
      base::RepeatingCallback<CFPreferencesObserver::RawConfig()> read_config);
  ~FakeCFPreferencesObserver() override;

  // CFPreferencesObserver:
  void Subscribe(base::RepeatingClosure on_update) override;
  void Unsubscribe() override;
  base::OnceCallback<CFPreferencesObserver::RawConfig()> GetReadConfigCallback()
      override;

 private:
  const raw_ref<base::RepeatingClosure> on_update_;
  const base::RepeatingCallback<CFPreferencesObserver::RawConfig()>
      read_config_;
};

// Converts `values` into a `CFArray` of `CFString`s, as found in the MDM
// payload.
base::apple::ScopedCFTypeRef<CFPropertyListRef> StringsToPropList(
    const std::vector<std::string>& values);

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_SCOPED_CF_PREFS_OBSERVER_OVERRIDE_H_
