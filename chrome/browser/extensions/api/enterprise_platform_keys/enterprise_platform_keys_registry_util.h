// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_REGISTRY_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_REGISTRY_UTIL_H_

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace extensions {

namespace platform_keys {

void EnterprisePlatformKeysRegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry);

}  // namespace platform_keys

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_PLATFORM_KEYS_ENTERPRISE_PLATFORM_KEYS_REGISTRY_UTIL_H_
