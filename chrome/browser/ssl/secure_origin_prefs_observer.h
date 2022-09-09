// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SECURE_ORIGIN_PREFS_OBSERVER_H_
#define CHROME_BROWSER_SSL_SECURE_ORIGIN_PREFS_OBSERVER_H_

#include "components/prefs/pref_change_registrar.h"

class PrefService;

// Helper for translating prefs::kUnsafelyTreatInsecureOriginAsSecure (and
// therefore also the enterprise policies corresponding to this pref) into
// network::SecureOriginAllowlist state.
class SecureOriginPrefsObserver {
 public:
  explicit SecureOriginPrefsObserver(PrefService* pref_service);

  SecureOriginPrefsObserver(const SecureOriginPrefsObserver&) = delete;
  SecureOriginPrefsObserver& operator=(const SecureOriginPrefsObserver&) =
      delete;

 private:
  void OnChangeInSecureOriginPref();

  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_SSL_SECURE_ORIGIN_PREFS_OBSERVER_H_
