// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_PREFS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_PREFS_H_

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace extensions::login_api {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace extensions::login_api

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_PREFS_H_
