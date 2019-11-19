// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_LOCALE_LOCALE_MANAGER_H_
#define CHROME_BROWSER_ANDROID_LOCALE_LOCALE_MANAGER_H_

#include <jni.h>
#include <string>

#include "base/macros.h"

// Provides access to the locale specific customizations on Android.
class LocaleManager {
 public:
  static std::string GetYandexReferralID();
  static std::string GetMailRUReferralID();
  static void RecordUserTypeMetrics();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(LocaleManager);
};

#endif  // CHROME_BROWSER_ANDROID_LOCALE_LOCALE_MANAGER_H_
