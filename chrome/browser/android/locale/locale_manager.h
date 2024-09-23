// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOCALE_LOCALE_MANAGER_H_
#define CHROME_BROWSER_ANDROID_LOCALE_LOCALE_MANAGER_H_

#include <string>

// Provides access to the locale specific customizations on Android.
class LocaleManager {
 public:
  LocaleManager() = delete;
  LocaleManager(const LocaleManager&) = delete;
  LocaleManager& operator=(const LocaleManager&) = delete;

  static std::string GetYandexReferralID();
  static std::string GetMailRUReferralID();
};

#endif  // CHROME_BROWSER_ANDROID_LOCALE_LOCALE_MANAGER_H_
