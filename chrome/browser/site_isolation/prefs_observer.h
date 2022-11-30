// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_ISOLATION_PREFS_OBSERVER_H_
#define CHROME_BROWSER_SITE_ISOLATION_PREFS_OBSERVER_H_

#include "components/prefs/pref_change_registrar.h"

class PrefService;

// Helper for translating Site-Isolation-related prefs (and enterprise policies
// mapped to prefs) into appropriate runtime settings.
class SiteIsolationPrefsObserver {
 public:
  explicit SiteIsolationPrefsObserver(PrefService* pref_service);

  SiteIsolationPrefsObserver(const SiteIsolationPrefsObserver&) = delete;
  SiteIsolationPrefsObserver& operator=(const SiteIsolationPrefsObserver&) =
      delete;

 private:
  void OnChangeInIsolatedOriginsPref();

  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_SITE_ISOLATION_PREFS_OBSERVER_H_
