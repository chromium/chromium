// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_DEVICE_ID_FETCHER_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_DEVICE_ID_FETCHER_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

// This class has been followed out and now simply hosts a profile pref.
// TODO(crbug.com/1152871): Remove or migrate those, too.
class DeviceIDFetcher {
 public:
  DeviceIDFetcher() = delete;

  // Called to register the |kEnableDRM| and |kDRMSalt| preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* prefs);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_DEVICE_ID_FETCHER_H_
