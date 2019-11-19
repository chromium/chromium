// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_ISOLATION_PREFS_OBSERVER_H_
#define CHROME_BROWSER_SITE_ISOLATION_PREFS_OBSERVER_H_

#include "base/macros.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

// Helper for translating Site-Isolation-related prefs (and enterprise policies
// mapped to prefs) into appropriate runtime settings.
class SiteIsolationPrefsObserver {
 public:
  explicit SiteIsolationPrefsObserver(PrefService* pref_service);

 private:
  void OnChangeInIsolatedOriginsPref();

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationPrefsObserver);
};

#endif  // CHROME_BROWSER_SITE_ISOLATION_PREFS_OBSERVER_H_
