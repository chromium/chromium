// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_PREFS_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_PREFS_H_

#include "base/memory/raw_ptr.h"

class PrefRegistrySimple;
class PrefService;

// Handler for Fast Checkout related prefs.
class FastCheckoutPrefs {
 public:
  explicit FastCheckoutPrefs(PrefService* pref_service);
  ~FastCheckoutPrefs() = default;
  FastCheckoutPrefs(const FastCheckoutPrefs&) = delete;
  FastCheckoutPrefs& operator=(const FastCheckoutPrefs&) = delete;

  // Sets Fast Checkout's profile pref for if a user has declined onboarding to
  // `true`.
  void DeclineOnboarding();

  // Returns the current value of Fast Checkout's profile pref for if a user has
  // declined onboarding.
  bool IsOnboardingDeclined();

  // Register Fast Checkout related profile prefs in `registry`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  const raw_ptr<PrefService> pref_service_;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_PREFS_H_
