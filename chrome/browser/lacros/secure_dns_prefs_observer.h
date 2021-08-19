// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SECURE_DNS_PREFS_OBSERVER_H_
#define CHROME_BROWSER_LACROS_SECURE_DNS_PREFS_OBSERVER_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "chrome/browser/lacros/crosapi_pref_observer.h"
#include "components/prefs/pref_service.h"

// Observes ash-chrome for changes in the secure DNS preferences.
class SecureDnsPrefsObserver {
 public:
  explicit SecureDnsPrefsObserver(PrefService* local_state);
  SecureDnsPrefsObserver(const SecureDnsPrefsObserver&) = delete;
  SecureDnsPrefsObserver& operator=(const SecureDnsPrefsObserver&) = delete;
  ~SecureDnsPrefsObserver();

  void Init();

 private:
  FRIEND_TEST_ALL_PREFIXES(SecureDnsPrefsObserver, LocalStateUpdatedOnChange);

  void OnDnsOverHttpsModeChanged(base::Value value);
  void OnDnsOverHttpsTemplatesChanged(base::Value value);

  PrefService* local_state_{nullptr};
  std::unique_ptr<CrosapiPrefObserver> doh_mode_observer_;
  std::unique_ptr<CrosapiPrefObserver> doh_templates_observer_;
};

#endif  // CHROME_BROWSER_LACROS_SECURE_DNS_PREFS_OBSERVER_H_
